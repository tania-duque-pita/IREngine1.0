#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"
#include "ir/market/bootstrapper.hpp"
#include "ir/market/rate_helpers.hpp"
#include "ir/market/curves.hpp"     // PiecewiseDiscountCurve

int main() {
    using ir::Date;
    using ir::DayCount;
    using ir::Frequency;
    using ir::BusinessDayConvention;

    using ir::market::CurveBootstrapper;
    using ir::market::BootstrapOptions;
    using ir::market::OisSwapHelper;
    using ir::market::PiecewiseDiscountCurve;

    std::cout << "=== IREngine1.0 :: OIS Discount Curve Bootstrap Demo ===\n";

    // ----------------------------
    // As-of and conventions
    // ----------------------------
    Date asof = Date::from_ymd(2026, 1, 2);

    OisSwapHelper::Config oisCfg;
    oisCfg.fixed_dc = DayCount::ACT360;
    oisCfg.fixed_freq = Frequency::Annual;
    oisCfg.bdc = BusinessDayConvention::ModifiedFollowing;
    // oisCfg.calendar default = weekends-only

    PiecewiseDiscountCurve::Config discCfg;
    discCfg.dc = DayCount::ACT365; // time axis for curve

    // ----------------------------
    // Hardcoded market quotes
    // ----------------------------
    // Par OIS rates for maturities, annual fixed leg, simplified par formula.
    // (These are just demo inputs; not meant to match any specific market date.)
    struct QuoteRow { int y; int m; int d; double par; };

    std::vector<QuoteRow> quotes = {
      {2027, 1, 2, 0.0300}, // 1Y
      {2028, 1, 2, 0.0320}, // 2Y
      {2029, 1, 2, 0.0330}, // 3Y
      {2031, 1, 2, 0.0340}, // 5Y
      {2036, 1, 2, 0.0350}  // 10Y
    };

    std::vector<std::shared_ptr<OisSwapHelper>> helpers;
    helpers.reserve(quotes.size());

    for (const auto& q : quotes) {
        Date end = Date::from_ymd(q.y, q.m, q.d);
        helpers.push_back(std::make_shared<OisSwapHelper>(asof, end, q.par, oisCfg));
    }

    // ----------------------------
    // Bootstrap
    // ----------------------------
    CurveBootstrapper bs;
    BootstrapOptions opts;
    opts.df_min = 1e-6;
    opts.df_max = 1.0;

    auto res = bs.bootstrap_discount_curve(asof, discCfg, helpers, opts);
    if (!res.has_value()) {
        std::cerr << "Bootstrap failed: " << res.error().message << "\n";
        return 1;
    }
    auto curve = res.value();

    // ----------------------------
    // Print results
    // ----------------------------
    std::cout << "\nBootstrapped pillars (t, DF):\n";
    std::cout << std::fixed << std::setprecision(8);

    const auto& nodes = curve->nodes();
    // nodes include (0,1) as first node
    for (std::size_t i = 0; i < nodes.t.size(); ++i) {
        std::cout << "  i=" << i
            << "  t=" << std::setw(4) << nodes.t[i]
            << "  DF=" << std::setw(5) << nodes.v[i]
            << "\n";
    }

    // Example: query a non-pillar date
    Date mid = Date::from_ymd(2030, 1, 2);
    double df_mid = curve->df(mid);
    auto days = (mid - asof).count(); // std::chrono::days::count()
    double t = static_cast<double>(days) / 365.0;

    std::cout << "\nExample query:\n";
    std::cout << "  Maturity Date: " << mid.to_iso() << "\n";
    std::cout << "  DF(" << t << ") = " << df_mid << "\n";

    std::cout << "\nDone.\n";
    return 0;
}
