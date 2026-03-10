#include "ir/io/market_io.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/error.hpp"
#include "ir/io/csv_io.hpp"
#include "ir/utils/piecewise_nodes.hpp"

// --- OPTIONAL: URL fetch ---
// Strategy:
// 1) If you later add libcurl, implement with curl.
// 2) For now: rely on system curl if available.
// You can keep it simple and document it in README.
#include <cstdlib>

namespace ir::io {

    static ir::Result<ir::Date> parse_iso_date(const std::string& s) {
        auto d = ir::Date::parse_iso(s);
        if (!d.has_value()) return d.error();
        return d.value();
    }

    static ir::Result<double> parse_double(const std::string& s) {
        try {
            return std::stod(s);
        }
        catch (...) {
            return ir::Error::make(ir::ErrorCode::ParseError, "Failed to parse double: " + s);
        }
    }

    ir::Result<int> load_discount_curve_nodes_csv(const std::string& path,
        const ir::CurveId& curve_id,
        ir::market::MarketData& md) {
        auto tab = ir::io::read_csv_file(path);
        if (!tab.has_value()) return tab.error();

        // Build curve
        ir::market::PiecewiseDiscountCurve::Config cfg;
        cfg.dc = ir::DayCount::ACT365;

        auto curve = std::make_shared<ir::market::PiecewiseDiscountCurve>(md.asof(), cfg);

        // Nodes (t, DF)
        std::vector<std::pair<double, double>> pts;
        for (const auto& r : tab.value().rows) {
            auto d = parse_iso_date(r.at("Date")); if (!d.has_value()) return d.error();
            auto df = parse_double(r.at("DF")); if (!df.has_value()) return df.error();

            const double t = ir::year_fraction(md.asof(), d.value(), cfg.dc);
            pts.push_back({ t, df.value() });
        }

        std::sort(pts.begin(), pts.end(), [](auto& a, auto& b) { return a.first < b.first; });

        ir::utils::Nodes1D nodes;
        auto ok0 = nodes.push_back(0.0, 1.0); if (!ok0.has_value()) return ok0.error();
        for (auto& p : pts) {
            if (p.first <= 0.0) continue;
            auto ok = nodes.push_back(p.first, p.second);
            if (!ok.has_value()) return ok.error();
        }

        auto set = curve->set_nodes(std::move(nodes));
        if (!set.has_value()) return set.error();

        md.set_discount_curve(curve_id, curve);
        return 0;
    }

    ir::Result<int> load_forward_curve_nodes_csv(const std::string& path,
        const ir::CurveId& curve_id,
        ir::market::MarketData& md) {
        auto tab = ir::io::read_csv_file(path);
        if (!tab.has_value()) return tab.error();

        ir::market::PiecewiseForwardCurve::Config cfg;
        cfg.dc = ir::DayCount::ACT365;

        auto curve = std::make_shared<ir::market::PiecewiseForwardCurve>(md.asof(), cfg);

        std::vector<std::pair<double, double>> pts;
        for (const auto& r : tab.value().rows) {
            auto d = parse_iso_date(r.at("Date")); if (!d.has_value()) return d.error();
            auto pf = parse_double(r.at("PseudoDF")); if (!pf.has_value()) return pf.error();

            const double t = ir::year_fraction(md.asof(), d.value(), cfg.dc);
            pts.push_back({ t, pf.value() });
        }

        std::sort(pts.begin(), pts.end(), [](auto& a, auto& b) { return a.first < b.first; });

        ir::utils::Nodes1D nodes;
        auto ok0 = nodes.push_back(0.0, 1.0); if (!ok0.has_value()) return ok0.error();
        for (auto& p : pts) {
            if (p.first <= 0.0) continue;
            auto ok = nodes.push_back(p.first, p.second);
            if (!ok.has_value()) return ok.error();
        }

        auto set = curve->set_nodes(std::move(nodes));
        if (!set.has_value()) return set.error();

        md.set_forward_curve(curve_id, curve);
        return 0;
    }

    ir::Result<int> load_fixings_csv(const std::string& path,
        const ir::IndexId& index,
        ir::market::FixingStore& store) {
        auto tab = ir::io::read_csv_file(path);
        if (!tab.has_value()) return tab.error();

        for (const auto& r : tab.value().rows) {
            auto d = parse_iso_date(r.at("Date")); if (!d.has_value()) return d.error();
            auto rt = parse_double(r.at("Rate")); if (!rt.has_value()) return rt.error();

            store.add(index, d.value(), rt.value());
        }

        return 0;
    }

} // namespace ir::io