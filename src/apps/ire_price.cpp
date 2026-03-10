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

static int to_int_pay_receive(ir::io::PayReceive d) {
    return (d == ir::io::PayReceive::Pay) ? -1 : 1;
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

        const fs::path parent = p.parent_path();
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

    const std::string arg1 = argv[1];

    if (arg1 == "--example") {
        if (argc < 3) {
            throw std::runtime_error("Usage: ire_price --example <folder_name>");
        }

        const std::string folder_name = argv[2];
        const fs::path repo_root = find_repo_root_from_cwd();
        const fs::path folder = repo_root / "example" / folder_name;

        if (!fs::exists(folder)) {
            throw std::runtime_error(
                "Example folder does not exist: " + folder.string());
        }

        return folder;
    }

    return fs::path(arg1);
}

struct BuiltLegEntry {
    ir::io::LegSpec spec;
    ir::instruments::Leg leg;
};

struct ResultCashflowRow {
    std::string leg_id;
    std::string leg_type;
    int pay_receive{1};
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
    const std::vector<std::pair<std::string, double>>& leg_pvs,
    std::size_t num_cashflows) {

    std::ofstream out(out_file);
    if (!out) {
        throw std::runtime_error(
            "Could not open output file for writing: " + out_file.string());
    }

    out << "ValuationDate,NumLegs,NumCashflows,PV\n";
    out << valuation_date.to_iso() << ","
        << leg_pvs.size() << ","
        << num_cashflows << ","
        << total_pv << "\n";

    out << "\nLeg_ID,Leg_PV\n";
    for (const auto& [leg_id, pv] : leg_pvs) {
        out << leg_id << "," << pv << "\n";
    }
}

int main(int argc, char** argv) {
    try {
        const fs::path folder = resolve_input_folder(argc, argv);

        // -------- Load deal_data.csv --------
        auto deal_res = ir::io::read_deal_data_csv((folder / "deal_data.csv").string());
        if (!deal_res.has_value()) {
            throw std::runtime_error(deal_res.error().message);
        }

        ir::io::DealSpec deal = deal_res.value();
        if (deal.legs.empty()) {
            throw std::runtime_error("Deal has no legs.");
        }

        // -------- Market setup --------
        const ir::Date asof = deal.legs.front().cob;
        ir::market::MarketData md(asof);
        ir::market::FixingStore fixings;

        // discount.csv remains mandatory
        const auto& discount_id = deal.legs.front().discount_curve;
        auto disc_res = ir::io::load_discount_curve_nodes_csv(
            (folder / "discount.csv").string(), discount_id, md);
        if (!disc_res.has_value()) {
            throw std::runtime_error(disc_res.error().message);
        }

        // -------- Load forward curves and optional fixings --------
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
                    auto fwd_res = ir::io::load_forward_curve_nodes_csv(
                        (folder / fwd_file).string(), leg.fwd_curve, md);
                    if (!fwd_res.has_value()) {
                        throw std::runtime_error(fwd_res.error().message);
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
                auto fix_res = ir::io::load_fixings_csv(p.string(), index, fixings);
                if (!fix_res.has_value()) {
                    throw std::runtime_error(fix_res.error().message);
                }
            }
        }

        md.set_fixings(&fixings);

        // -------- Build legs --------
        ir::Calendar cal;
        const auto bdc = ir::BusinessDayConvention::ModifiedFollowing;

        std::vector<BuiltLegEntry> built_legs;
        built_legs.reserve(deal.legs.size());

        for (const auto& leg_spec : deal.legs) {
            ir::ScheduleConfig sc;
            sc.start = leg_spec.start;
            sc.end = leg_spec.end;
            sc.tenor = parse_tenor_or_throw(leg_spec.frequency);
            sc.calendar = cal;
            sc.bdc = bdc;
            sc.rule = ir::DateGenerationRule::Backward;

            auto sched = ir::make_schedule(sc);

            if (leg_spec.type == ir::io::LegType::Fixed) {
                ir::instruments::FixedLegConfig cfg;
                cfg.notional = leg_spec.notional;
                cfg.fixed_rate = leg_spec.fixed_rate;
                cfg.dc = leg_spec.dc;

                auto leg = ir::instruments::LegBuilder::build_fixed_leg(
                    to_instr_dir(leg_spec.dir), sched, cfg);

                built_legs.push_back(BuiltLegEntry{ leg_spec, std::move(leg) });
            }
            else if (leg_spec.type == ir::io::LegType::Ibor) {
                ir::instruments::IborLegConfig cfg;
                cfg.notional = leg_spec.notional;
                cfg.spread = leg_spec.spread;
                cfg.index = leg_spec.index;
                cfg.dc = leg_spec.dc;
                cfg.fixing_lag_days = 2;

                auto leg = ir::instruments::LegBuilder::build_ibor_leg(
                    to_instr_dir(leg_spec.dir), sched, cfg, cal, bdc);

                built_legs.push_back(BuiltLegEntry{ leg_spec, std::move(leg) });
            }
            else {
                ir::instruments::RfrLegConfig cfg;
                cfg.notional = leg_spec.notional;
                cfg.spread = leg_spec.spread;
                cfg.index = leg_spec.index;
                cfg.dc = leg_spec.dc;

                auto leg = ir::instruments::LegBuilder::build_rfr_compound_leg(
                    to_instr_dir(leg_spec.dir), sched, cfg);

                built_legs.push_back(BuiltLegEntry{ leg_spec, std::move(leg) });
            }
        }

        // -------- Price each leg independently --------
        ir::pricers::MultiCurveSwapPricer pricer;

        double total_pv = 0.0;
        std::vector<std::pair<std::string, double>> leg_pvs;
        std::vector<ResultCashflowRow> cashflow_rows;

        for (const auto& entry : built_legs) {
            ir::pricers::PricingContext ctx;
            ctx.valuation_date = asof;
            ctx.framework = ir::pricers::PricingFramework::MultiCurve;
            ctx.discount_curve = entry.spec.discount_curve;

            if (entry.spec.type == ir::io::LegType::Ibor) {
                ctx.ibor_forward_curve = entry.spec.fwd_curve;
            }
            else if (entry.spec.type == ir::io::LegType::Rfr) {
                ctx.rfr_forward_curve = entry.spec.fwd_curve;
            }

            auto leg_res = pricer.price_leg(entry.leg, md, ctx);
            if (!leg_res.has_value()) {
                throw std::runtime_error(
                    "Failed pricing leg " + entry.spec.leg_id + ": " +
                    leg_res.error().message);
            }

            total_pv += leg_res.value().pv;
            leg_pvs.push_back({ entry.spec.leg_id, leg_res.value().pv });

            for (const auto& line : leg_res.value().lines) {
                ResultCashflowRow row;
                row.leg_id = entry.spec.leg_id;
                row.leg_type = to_string_leg_type(entry.spec.type);
                row.pay_receive = to_int_pay_receive(entry.spec.dir);
                row.pay_date = line.pay_date.to_iso();
                row.amount = line.amount;
                row.df = line.df;
                row.pv = line.pv;
                cashflow_rows.push_back(std::move(row));
            }
        }

        // -------- Write outputs --------
        const fs::path result_cashflows_file = folder / "result_cashflows.csv";
        const fs::path result_file = folder / "result.csv";

        write_result_cashflows_csv(result_cashflows_file, cashflow_rows);
        write_result_csv(result_file, asof, total_pv, leg_pvs, cashflow_rows.size());

        std::cout << "\n=== Pricing completed ===\n";
        std::cout << "Input folder        : " << folder.string() << "\n";
        std::cout << "Number of legs      : " << built_legs.size() << "\n";
        std::cout << "Number of cashflows : " << cashflow_rows.size() << "\n";
        std::cout << "Total PV            : " << total_pv << "\n";
        std::cout << "Wrote               : " << result_cashflows_file.string() << "\n";
        std::cout << "Wrote               : " << result_file.string() << "\n";
        std::cout << "Done.\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}