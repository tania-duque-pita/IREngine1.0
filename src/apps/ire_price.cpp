#include <filesystem>
#include <iostream>
#include <fstream>
#include <memory>
#include <unordered_map>

#include "ir/core/conventions.hpp"
#include "ir/core/date.hpp"


#include "ir/io/deal_io.hpp"
#include "ir/io/market_io.hpp"
#include "ir/io/csv_io.hpp"

#include "ir/market/market_data.hpp"
#include "ir/pricers/swap_pricer.hpp"
#include "ir/instruments/leg_builder.hpp"
#include "ir/instruments/products.hpp"

namespace fs = std::filesystem;

static ir::Tenor parse_tenor_or_throw(const std::string& s) {
    auto t = ir::Tenor::parse(s);
    if (!t.has_value()) throw std::runtime_error(t.error().message);
    return t.value();
}

static ir::instruments::PayReceive to_instr_dir(ir::io::PayReceive d) {
    return (d == ir::io::PayReceive::Pay)
        ? ir::instruments::PayReceive::Pay
        : ir::instruments::PayReceive::Receive;
}

static ir::DayCount to_dc(ir::DayCount dc) { return dc; }

static std::string raw_github_url(const std::string& owner_repo, const std::string& folder, const std::string& file) {
    // https://raw.githubusercontent.com/<owner_repo>/main/<folder>/<file>
    return "https://raw.githubusercontent.com/" + owner_repo + "/main/" + folder + "/" + file;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage:\n"
                << "  ire_price <folder_path>\n"
                << "  ire_price --github <owner/repo> <folder_in_repo>\n";
            return 1;
        }

        bool github_mode = false;
        std::string owner_repo, repo_folder;
        fs::path folder;

        if (std::string(argv[1]) == "--github") {
            if (argc < 4) {
                std::cerr << "Usage: ire_price --github <owner/repo> <folder_in_repo>\n";
                return 1;
            }
            github_mode = true;
            owner_repo = argv[2];
            repo_folder = argv[3];
        }
        else {
            folder = fs::path(argv[1]);
        }

        // -------- Load deal_data.csv --------
        ir::io::DealSpec deal;

        if (!github_mode) {
            auto dealRes = ir::io::read_deal_data_csv((folder / "deal_data.csv").string());
            if (!dealRes.has_value()) throw std::runtime_error(dealRes.error().message);
            deal = dealRes.value();
        }
        else {
            auto text = ir::io::fetch_text_from_url(raw_github_url(owner_repo, repo_folder, "deal_data.csv"));
            if (!text.has_value()) throw std::runtime_error(text.error().message);
            auto tab = ir::io::read_csv_text(text.value());
            if (!tab.has_value()) throw std::runtime_error(tab.error().message);

            // quick reuse: write to temp + parse (keeps it minimal)
            const std::string tmp = "ire_tmp_deal.csv";
            { std::ofstream out(tmp); out << text.value(); }
            auto dealRes = ir::io::read_deal_data_csv(tmp);
            if (!dealRes.has_value()) throw std::runtime_error(dealRes.error().message);
            deal = dealRes.value();
        }

        if (deal.legs.empty()) throw std::runtime_error("Deal has no legs.");

        // Use COB from first leg as MarketData asof
        ir::Date asof = deal.legs.front().cob;
        ir::market::MarketData md(asof);
        ir::market::FixingStore fixings;

        // -------- Load discount curve (mandatory) --------
        const auto& discount_id = deal.legs.front().discount_curve;

        if (!github_mode) {
            auto r = ir::io::load_discount_curve_nodes_csv((folder / "discount.csv").string(), discount_id, md);
            if (!r.has_value()) throw std::runtime_error(r.error().message);
        }
        else {
            auto txt = ir::io::fetch_text_from_url(raw_github_url(owner_repo, repo_folder, "discount.csv"));
            if (!txt.has_value()) throw std::runtime_error(txt.error().message);
            const std::string tmp = "ire_tmp_discount.csv";
            { std::ofstream out(tmp); out << txt.value(); }
            auto r = ir::io::load_discount_curve_nodes_csv(tmp, discount_id, md);
            if (!r.has_value()) throw std::runtime_error(r.error().message);
        }

        // -------- Load forward curves and fixings (as needed) --------
        // Map FixingsID -> IndexId (from deal leg)
        std::unordered_map<std::string, ir::IndexId> fixings_map;

        for (const auto& leg : deal.legs) {
            if (leg.type == ir::io::LegType::Ibor || leg.type == ir::io::LegType::Rfr) {
                // forward curve mandatory for float
                if (leg.fwd_curve.value.empty()) {
                    throw std::runtime_error("Floating leg missing FwdCurveID: " + leg.leg_id);
                }

                // Load forward curve file named "<FwdCurveID>.csv" or "fwd_1.csv"? Choose your convention:
                // Here: we assume file name equals FwdCurveID lowercased is tricky,
                // so simplest: FwdCurveID is the filename without extension, e.g. "fwd_1".
                const std::string fwd_file = leg.fwd_curve.value + ".csv";

                // Load once
                static std::unordered_map<std::string, bool> loaded_fwd;
                if (!loaded_fwd[leg.fwd_curve.value]) {
                    if (!github_mode) {
                        auto r = ir::io::load_forward_curve_nodes_csv((folder / fwd_file).string(), leg.fwd_curve, md);
                        if (!r.has_value()) throw std::runtime_error(r.error().message);
                    }
                    else {
                        auto txt = ir::io::fetch_text_from_url(raw_github_url(owner_repo, repo_folder, fwd_file));
                        if (!txt.has_value()) throw std::runtime_error(txt.error().message);
                        const std::string tmp = "ire_tmp_" + leg.fwd_curve.value + ".csv";
                        { std::ofstream out(tmp); out << txt.value(); }
                        auto r = ir::io::load_forward_curve_nodes_csv(tmp, leg.fwd_curve, md);
                        if (!r.has_value()) throw std::runtime_error(r.error().message);
                    }
                    loaded_fwd[leg.fwd_curve.value] = true;
                }

                if (!leg.fixings_id.empty()) {
                    fixings_map[leg.fixings_id] = leg.index;
                }
            }
        }

        // Load fixings if file exists (optional)
        for (const auto& [fix_id, index] : fixings_map) {
            const std::string fx_file = fix_id + ".csv";

            if (!github_mode) {
                fs::path p = folder / fx_file;
                if (fs::exists(p)) {
                    auto r = ir::io::load_fixings_csv(p.string(), index, fixings);
                    if (!r.has_value()) throw std::runtime_error(r.error().message);
                }
            }
            else {
                // try fetch; if missing, ignore
                auto txt = ir::io::fetch_text_from_url(raw_github_url(owner_repo, repo_folder, fx_file));
                if (txt.has_value()) {
                    const std::string tmp = "ire_tmp_" + fix_id + ".csv";
                    { std::ofstream out(tmp); out << txt.value(); }
                    auto r = ir::io::load_fixings_csv(tmp, index, fixings);
                    if (!r.has_value()) throw std::runtime_error(r.error().message);
                }
            }
        }
        md.set_fixings(&fixings);

        // -------- Build schedules and legs --------
        ir::Calendar cal;
        auto bdc = ir::BusinessDayConvention::ModifiedFollowing;

        std::vector<std::pair<std::string, ir::instruments::Leg>> built_legs;

        for (const auto& leg : deal.legs) {
            ir::ScheduleConfig sc;
            sc.start = leg.start;
            sc.end = leg.end;
            sc.tenor = parse_tenor_or_throw(leg.frequency);
            sc.calendar = cal;
            sc.bdc = bdc;
            sc.rule = ir::DateGenerationRule::Backward;

            auto sched = ir::make_schedule(sc);
            //if (!sched) throw std::runtime_error();

            if (leg.type == ir::io::LegType::Fixed) {
                ir::instruments::FixedLegConfig cfg;
                cfg.notional = leg.notional;
                cfg.fixed_rate = leg.fixed_rate;
                cfg.dc = leg.dc;

                auto L = ir::instruments::LegBuilder::build_fixed_leg(
                    to_instr_dir(leg.dir), sched, cfg);

                built_legs.push_back({ leg.leg_id, std::move(L) });

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

                built_legs.push_back({ leg.leg_id, std::move(L) });

            }
            else { // RFR
                ir::instruments::RfrLegConfig cfg;
                cfg.notional = leg.notional;
                cfg.spread = leg.spread;
                cfg.index = leg.index;
                cfg.dc = leg.dc;

                auto L = ir::instruments::LegBuilder::build_rfr_compound_leg(
                    to_instr_dir(leg.dir), sched, cfg);

                built_legs.push_back({ leg.leg_id, std::move(L) });
            }
        }

        if (built_legs.size() != 2) {
            std::cerr << "Note: current demo app expects 2 legs. Found " << built_legs.size() << ".\n";
        }

        // -------- Build product (IRS or OIS) --------
        // Simple heuristic: if any leg is RFR => OisSwap, else IRS.
        bool has_rfr = false;
        for (const auto& leg : deal.legs) if (leg.type == ir::io::LegType::Rfr) has_rfr = true;

        ir::instruments::TradeInfo ti;
        ti.trade_id = "DEMO";
        ti.trade_date = asof;
        ti.start_date = deal.legs.front().start;
        ti.end_date = deal.legs.front().end;

        ir::pricers::PricingContext ctx;
        ctx.valuation_date = asof;
        ctx.discount_curve = discount_id;
        ctx.framework = ir::pricers::PricingFramework::MultiCurve;

        // Pick forward IDs from the float leg (if any)
        for (const auto& leg : deal.legs) {
            if (leg.type == ir::io::LegType::Ibor) ctx.ibor_forward_curve = leg.fwd_curve;
            if (leg.type == ir::io::LegType::Rfr)  ctx.rfr_forward_curve = leg.fwd_curve;
        }

        ir::pricers::MultiCurveSwapPricer pricer;

        ir::pricers::PricingResult result;

        if (!has_rfr) {
            ir::instruments::InterestRateSwap swap(ti, built_legs[0].second, built_legs[1].second);
            auto r = pricer.price(swap, md, ctx);
            if (!r.has_value()) throw std::runtime_error(r.error().message);
            result = std::move(r.value());
        }
        else {
            ir::instruments::OisSwap swap(ti, built_legs[0].second, built_legs[1].second);
            auto r = pricer.price(swap, md, ctx);
            if (!r.has_value()) throw std::runtime_error(r.error().message);
            result = std::move(r.value());
        }

        // -------- Print results (cashflow + pv per coupon) --------
        std::cout << "\n=== PV Summary ===\n";
        std::cout << "PV Fixed Leg: " << result.pv_fixed_leg << "\n";
        std::cout << "PV Float Leg: " << result.pv_float_leg << "\n";
        std::cout << "PV Total    : " << result.pv << "\n";

        std::cout << "\n=== Cashflows (signed) ===\n";
        std::cout << "PayDate,Label,Amount,DF,PV\n";
        for (const auto& ln : result.lines) {
            std::cout << ln.pay_date.to_iso() << ","
                << (ln.label.empty() ? "CF" : ln.label) << ","
                << ln.amount << ","
                << ln.df << ","
                << ln.pv << "\n";
        }

        std::cout << "\nDone.\n";
        return 0;

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}