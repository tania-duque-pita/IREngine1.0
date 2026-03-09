#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ir/core/conventions.hpp"
#include "ir/core/date.hpp"
#include "ir/io/csv_io.hpp"
#include "ir/io/deal_io.hpp"
#include "ir/io/market_io.hpp"
#include "ir/market/market_data.hpp"
#include "ir/pricers/swap_pricer.hpp"
#include "ir/instruments/leg_builder.hpp"

namespace fs = std::filesystem;

static ir::Tenor parse_tenor_or_throw(const std::string& s) {
    auto t = ir::Tenor::parse(s);
    if (!t.has_value()) {
        throw std::runtime_error(t.error().message);
    }
    return t.value();
}

static ir::instruments::PayReceive to_instr_dir(ir::io::PayReceive d) {
    return (d == ir::io::PayReceive::Pay)
        ? ir::instruments::PayReceive::Pay
        : ir::instruments::PayReceive::Receive;
}

static std::string to_string_leg_type(ir::io::LegType t) {
    switch (t) {
    case ir::io::LegType::Fixed: return "FIXED";
    case ir::io::LegType::Ibor:  return "IBOR";
    case ir::io::LegType::Rfr:   return "RFR";
    default:                     return "UNKNOWN";
    }
}

static std::string to_string_pay_receive(ir::io::PayReceive d) {
    return (d == ir::io::PayReceive::Pay) ? "PAY" : "RECEIVE";
}

static fs::path find_repo_root_from_cwd() {
    fs::path p = fs::current_path();

    while (!p.empty()) {
        const bool has_cmake = fs::exists(p / "CMakeLists.txt");
        const bool has_src = fs::exists(p / "src");
        const bool has_include = fs::exists(p / "include");
        const bool has_example = fs::exists(p / "example");

        if (has_cmake && has_src && has_include && has_example) {
            return p;
        }

        fs::path parent = p.parent_path();
        if (parent == p) {
            break;
        }
        p = parent;
    }

    throw std::runtime_error(
        "Could not locate repository root from current working directory.");
}

static fs::path resolve_input_folder(int argc, char** argv) {
    if (argc < 2) {
        throw std::runtime_error(
            "Usage:\n"
            "  ire_price <folder_path>\n"
            "  ire_price --example <folder_name>\n");
    }

    const std::string mode = argv[1];

    // Existing local-folder mode
    if (mode != "--example") {
        return fs::path(argv[1]);
    }

    // Repo-relative example mode
    if (argc < 3) {
        throw std::runtime_error("Usage: ire_price --example <folder_name>");
    }

    const std::string folder_name = argv[2];
    const fs::path repo_root = find_repo_root_from_cwd();
    const fs::path example_folder = repo_root / "example" / folder_name;

    if (!fs::exists(example_folder)) {
        throw std::runtime_error(
            "Example folder does not exist: " + example_folder.string());
    }

    return example_folder;
}

struct BuiltLegEntry {
    ir::io::DealSpec spec;
    ir::instruments::Leg leg;
};

struct ResultCashflowRow {
    std::string leg_id;
    std::string leg_type;
    std::string pay_receive;
    std::string pay_date;
    double amount{ 0.0 };
    double df{ 0.0 };
    double pv{ 0.0 };
};

static void write_result_cashflows_csv(
    const fs::path& out_file,
    const std::vector<ResultCashflowRow>& rows) {

    std::ofstream out(out_file);
    if (!out) {
        throw std::runtime_error(
            "Could not open output file for writing: " + out_file.string());
    }

    out << "Leg_ID,LegType,PayReceive,Pay_Date,Amount,DF,PV\n";
    for (const auto& r : rows) {
        out << r.leg_id << ","
            << r.leg_type << ","
            << r.pay_receive << ","
            << r.pay_date << ","
            << r.amount << ","
            << r.df << ","
            << r.pv << "\n";
    }
}

static void write_result_csv(
    const fs::path& out_file,
    const ir::Date& valuation_date,
    double total_pv,
    std::size_t num_legs,
    std::size_t num_cashflows) {

    std::ofstream out(out_file);
    if (!out) {
        throw std::runtime_error(
            "Could not open output file for writing: " + out_file.string());
    }

    out << "ValuationDate,NumLegs,NumCashflows,PV\n";
    out << valuation_date.to_iso() << ","
        << num_legs << ","
        << num_cashflows << ","
        << total_pv << "\n";
}

int main(int argc, char** argv) {
    try {
        const fs::path folder = resolve_input_folder(argc, argv);

        // -------- Load deal_data.csv --------
        auto dealRes = ir::io::read_deal_data_csv((folder / "deal_data.csv").string());
        if (!dealRes.has_value()) {
            throw std::runtime_error(dealRes.error().message);
        }
        ir::io::DealSpec deal = dealRes.value();

        if (deal.legs.empty()) {
            throw std::runtime_error("Deal has no legs.");
        }

        // -------- Market setup --------
        const ir::Date asof = deal.legs.front().cob;
        ir::market::MarketData md(asof);
        ir::market::FixingStore fixings;

        const auto& discount_id = deal.legs.front().discount_curve;

        {
            auto r = ir::io::load_discount_curve_nodes_csv(
                (folder / "discount.csv").string(), discount_id, md);
            if (!r.has_value()) {
                throw std::runtime_error(r.error().message);
            }
        }

        std::unordered_map<std::string, ir::IndexId> fixings_map;
        std::unordered_map<std::string, bool> loaded_fwd;

        for (const auto& leg : deal.legs) {
            if (leg.type == ir::io::LegType::Ibor || leg.type == ir::io::LegType::Rfr) {
                if (leg.fwd_curve.value.empty()) {
                    throw std::runtime_error(
                        "Floating leg missing FwdCurveID: " + leg.leg_id);
                }

                const std::string fwd_file = leg.fwd_curve.value + ".csv";

                if (!loaded_fwd[leg.fwd_curve.value]) {
                    auto r = ir::io::load_forward_curve_nodes_csv(
                        (folder / fwd_file).string(), leg.fwd_curve, md);
                    if (!r.has_value()) {
                        throw std::runtime_error(r.error().message);
                    }
                    loaded_fwd[leg.fwd_curve.value] = true;
                }

                if (!leg.fixings_id.empty()) {
                    fixings_map[leg.fixings_id] = leg.index;
                }
            }
        }

        for (const auto& [fix_id, index] : fixings_map) {
            const fs::path p = folder / (fix_id + ".csv");
            if (fs::exists(p)) {
                auto r = ir::io::load_fixings_csv(p.string(), index, fixings);
                if (!r.has_value()) {
                    throw std::runtime_error(r.error().message);
                }
            }
        }

        md.set_fixings(&fixings);

        // -------- Build all legs --------
        ir::Calendar cal;
        auto bdc = ir::BusinessDayConvention::ModifiedFollowing;

        std::vector<BuiltLegEntry> built_legs;
        built_legs.reserve(deal.legs.size());

        for (const auto& leg : deal.legs) {
            ir::ScheduleConfig sc;
            sc.start = leg.start;
            sc.end = leg.end;
            sc.tenor = parse_tenor_or_throw(leg.frequency);
            sc.calendar = cal;
            sc.bdc = bdc;
            sc.rule = ir::DateGenerationRule::Backward;

            auto sched = ir::make_schedule(sc);

            if (leg.type == ir::io::LegType::Fixed) {
                ir::instruments::FixedLegConfig cfg;
                cfg.notional = leg.notional;
                cfg.fixed_rate = leg.fixed_rate;
                cfg.dc = leg.dc;

                auto L = ir::instruments::LegBuilder::build_fixed_leg(
                    to_instr_dir(leg.dir), sched, cfg);

                built_legs.push_back(BuiltLegEntry{ leg, std::move(L) });
            }
            else if (leg.type == ir::io::LegType::Ibor) {
                ir::instruments::IborLegConfig cfg;
                cfg.notional = leg.notional;
                cfg.spread = leg.spread;
                cfg.index = leg.index;
                cfg.dc = leg.dc;
                cfg.fixing_lag_days = 2;

                auto L = ir::instruments::LegBuilder::build_ibor_leg(
                    to_instr_dir(leg.dir), sched, cfg, cal, bdc);

                built_legs.push_back(BuiltLegEntry{ leg, std::move(L) });
            }
            else {
                ir::instruments::RfrLegConfig cfg;
                cfg.notional = leg.notional;
                cfg.spread = leg.spread;
                cfg.index = leg.index;
                cfg.dc = leg.dc;

                auto L = ir::instruments::LegBuilder::build_rfr_compound_leg(
                    to_instr_dir(leg.dir), sched, cfg);

                built_legs.push_back(BuiltLegEntry{ leg, std::move(L) });
            }
        }

        // -------- Price each leg independently --------
        ir::pricers::MultiCurveSwapPricer pricer;

        double total_pv = 0.0;
        std::vector<ResultCashflowRow> cashflow_rows;

        for (const auto& entry : built_legs) {
            ir::pricers::PricingContext ctx;
            ctx.valuation_date = asof;
            ctx.framework = ir::pricers::PricingFramework::MultiCurve;
            ctx.discount_curve = entry.spec.discount_curve;

            if (entry.spec.type == ir::io::LegType::Ibor) {
                ctx.ibor_forward_curve = entry.spec.fwd_curve;
            }
            if (entry.spec.type == ir::io::LegType::Rfr) {
                ctx.rfr_forward_curve = entry.spec.fwd_curve;
            }

            auto leg_res = pricer.price_leg(entry.leg, md, ctx);
            if (!leg_res.has_value()) {
                throw std::runtime_error(
                    "Failed pricing leg " + entry.spec.leg_id + ": " +
                    leg_res.error().message);
            }

            total_pv += leg_res.value().pv;

            for (const auto& ln : leg_res.value().lines) {
                ResultCashflowRow row;
                row.leg_id = entry.spec.leg_id;
                row.leg_type = to_string_leg_type(entry.spec.type);
                row.pay_receive = to_string_pay_receive(entry.spec.dir);
                row.pay_date = ln.pay_date.to_iso();
                row.amount = ln.amount;
                row.df = ln.df;
                row.pv = ln.pv;
                cashflow_rows.push_back(std::move(row));
            }
        }

        // -------- Write outputs --------
        const fs::path out_cashflows = folder / "result_cashflows.csv";
        const fs::path out_result = folder / "result.csv";

        write_result_cashflows_csv(out_cashflows, cashflow_rows);
        write_result_csv(out_result, asof, total_pv, built_legs.size(), cashflow_rows.size());

        // -------- Console summary --------
        std::cout << "\n=== Pricing completed ===\n";
        std::cout << "Input folder          : " << folder.string() << "\n";
        std::cout << "Number of legs        : " << built_legs.size() << "\n";
        std::cout << "Number of cashflows   : " << cashflow_rows.size() << "\n";
        std::cout << "Total PV              : " << total_pv << "\n";
        std::cout << "Wrote                 : " << out_cashflows.string() << "\n";
        std::cout << "Wrote                 : " << out_result.string() << "\n";
        std::cout << "Done.\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}