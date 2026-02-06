#include "ir/market/bootstrapper.hpp"
#include "ir/market/curves.hpp"
#include "ir/market/rate_helpers.hpp"
#include "ir/core/date.hpp"
#include "ir/core/error.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <vector>
#include <map>

using namespace ir;
using namespace ir::market;

TEST_CASE("CurveBootstrapper::bootstrap_discount_curve - basic OIS curve", "[bootstrapper][ois]") {
    CurveBootstrapper bootstrapper;
    Date asof = Date::from_ymd(2026, 1, 1);

    PiecewiseDiscountCurve::Config cfg;
    cfg.dc = DayCount::ACT365;
    cfg.bdc = BusinessDayConvention::ModifiedFollowing;

    // Create simple OIS helpers: 6M and 1Y
    std::vector<std::shared_ptr<OisSwapHelper>> helpers;

    OisSwapHelper::Config ois_cfg;
    ois_cfg.fixed_dc = DayCount::ACT365;
    ois_cfg.fixed_freq = Frequency::SemiAnnual;

    // 6M OIS at 2.5%
    helpers.push_back(std::make_shared<OisSwapHelper>(
        asof,
        Date::from_ymd(2026, 7, 1),
        0.025,
        ois_cfg
    ));
     
    // 1Y OIS at 3.0%
    helpers.push_back(std::make_shared<OisSwapHelper>(
        asof,
        Date::from_ymd(2027, 1, 1),
        0.030,
        ois_cfg
    ));

    SECTION("Successful bootstrap with valid helpers") {
        auto result = bootstrapper.bootstrap_discount_curve(asof, cfg, helpers);

        REQUIRE(result.has_value());
        auto curve = result.value();
        REQUIRE(curve != nullptr);
        REQUIRE(curve->asof() == asof);

        // Check that DF at t=0 is 1.0
        REQUIRE_THAT(curve->df(0.0), Catch::Matchers::WithinAbs(1.0, 1e-10));

        // Check DFs are monotonic decreasing
        double df_6m = curve->df(Date::from_ymd(2026, 7, 1));
        double df_1y = curve->df(Date::from_ymd(2027, 1, 1));


        // Expected values
        REQUIRE_THAT(df_6m, Catch::Matchers::WithinAbs(0.987756431, 1e-5));
        REQUIRE_THAT(df_1y, Catch::Matchers::WithinAbs(0.970626397, 1e-5));
    }

    SECTION("Empty helpers returns error") {
        std::vector<std::shared_ptr<OisSwapHelper>> empty_helpers;
        auto result = bootstrapper.bootstrap_discount_curve(asof, cfg, empty_helpers);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ErrorCode::InvalidArgument);
    }

    SECTION("Helpers are sorted by maturity automatically") {
        // Create helpers in reverse order
        std::vector<std::shared_ptr<OisSwapHelper>> unsorted_helpers;

        // 1Y first
        unsorted_helpers.push_back(std::make_shared<OisSwapHelper>(
            asof,
            Date::from_ymd(2027, 1, 1),
            0.030,
            ois_cfg
        ));

        // 6M second
        unsorted_helpers.push_back(std::make_shared<OisSwapHelper>(
            asof,
            Date::from_ymd(2026, 7, 1),
            0.025,
            ois_cfg
        ));

        auto result = bootstrapper.bootstrap_discount_curve(asof, cfg, unsorted_helpers);
        REQUIRE(result.has_value());

        // Should still work correctly
        auto curve = result.value();
        double df_6m = curve->df(Date::from_ymd(2026, 7, 1));
        double df_1y = curve->df(Date::from_ymd(2027, 1, 1));
        
        REQUIRE_THAT(df_6m, Catch::Matchers::WithinAbs(0.987756431, 1e-5));
        REQUIRE_THAT(df_1y, Catch::Matchers::WithinAbs(0.970626397, 1e-5));
    }
}

TEST_CASE("CurveBootstrapper::bootstrap_discount_curve - multiple pillars", "[bootstrapper][ois]") {
    CurveBootstrapper bootstrapper;
    Date asof = Date::from_ymd(2026, 1, 1);

    PiecewiseDiscountCurve::Config cfg;
    cfg.dc = DayCount::ACT365;

    OisSwapHelper::Config ois_cfg;
    ois_cfg.fixed_dc = DayCount::ACT360;
    ois_cfg.fixed_freq = Frequency::Annual;

    std::vector<std::shared_ptr<OisSwapHelper>> helpers;
    std::map<Date, double> rates_per_mat{
        {Date::from_ymd(2026, 4, 1),0.020},
        {Date::from_ymd(2026, 7, 1),0.025},
        {Date::from_ymd(2027, 1, 1), 0.030},
        {Date::from_ymd(2028, 1, 1), 0.035},
        {Date::from_ymd(2031, 1, 1), 0.040} };

    // Build a term structure: 3M, 6M, 1Y, 2Y, 5Y
    for (auto& it : rates_per_mat) {
        helpers.push_back(std::make_shared<OisSwapHelper>(
            asof, it.first, it.second, ois_cfg
        ));
    }

    auto result = bootstrapper.bootstrap_discount_curve(asof, cfg, helpers);
    REQUIRE(result.has_value());

    auto curve = result.value();

    auto it= rates_per_mat.begin();
    auto it2=helpers.begin();


    for (int i = 0; i < rates_per_mat.size(); i++) {
        ir::Result<double> implied_par = (*it2)->implied_par_rate(*curve);
        REQUIRE(implied_par.has_value());
        REQUIRE_THAT(implied_par.value(), Catch::Matchers::WithinAbs(it->second, 1e-5));
    }


}

TEST_CASE("CurveBootstrapper::bootstrap_forward_curve - basic FRA curve", "[bootstrapper][fra]") {
    CurveBootstrapper bootstrapper;
    Date asof = Date::from_ymd(2026, 1, 1);

    // First create a discount curve
    PiecewiseDiscountCurve::Config disc_cfg;
    disc_cfg.dc = DayCount::ACT365;

    OisSwapHelper::Config ois_cfg;
    ois_cfg.fixed_dc = DayCount::ACT365;
    ois_cfg.fixed_freq = Frequency::Quarterly;

    std::vector<std::shared_ptr<OisSwapHelper>> disc_helpers;
    disc_helpers.push_back(std::make_shared<OisSwapHelper>(
        asof, Date::from_ymd(2026, 7, 1), 0.025, ois_cfg
    ));
    disc_helpers.push_back(std::make_shared<OisSwapHelper>(
        asof, Date::from_ymd(2027, 1, 1), 0.030, ois_cfg
    ));

    auto disc_result = bootstrapper.bootstrap_discount_curve(asof, disc_cfg, disc_helpers);
    REQUIRE(disc_result.has_value());
    auto discount_curve = disc_result.value();

    // Now bootstrap forward curve with FRAs
    PiecewiseForwardCurve::Config fwd_cfg;
    fwd_cfg.dc = DayCount::ACT365;

    std::vector<std::shared_ptr<RateHelper>> fwd_helpers;

    FraHelper::Config fra_cfg;
    fra_cfg.dc = DayCount::ACT365;

    // 0x3 FRA at 2.8%
    fwd_helpers.push_back(std::make_shared<FraHelper>(
        Date::from_ymd(2026, 1, 1),
        Date::from_ymd(2026, 4, 1),
        0.015,
        fra_cfg
    ));

    // 3x6 FRA at 2.8%
    fwd_helpers.push_back(std::make_shared<FraHelper>(
        Date::from_ymd(2026, 4, 1),
        Date::from_ymd(2026, 7, 1),
        0.028,
        fra_cfg
    ));


    SECTION("Successful forward curve bootstrap") {
        auto result = bootstrapper.bootstrap_forward_curve(
            asof, fwd_cfg, *discount_curve, fwd_helpers
        );

        REQUIRE(result.has_value());
        auto fwd_curve = result.value();

        // Check forward rates are positive
        double fwd_3x6 = fwd_curve->forward_rate(
            Date::from_ymd(2026, 4, 1),
            Date::from_ymd(2026, 7, 1),
            DayCount::ACT365
        );

        REQUIRE_THAT(fwd_3x6, Catch::Matchers::WithinAbs(.028, 1e-5));
    }

    SECTION("Empty helpers returns error") {
        std::vector<std::shared_ptr<RateHelper>> empty_helpers;
        auto result = bootstrapper.bootstrap_forward_curve(
            asof, fwd_cfg, *discount_curve, empty_helpers
        );

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ErrorCode::InvalidArgument);
    }
}

TEST_CASE("CurveBootstrapper::bootstrap_forward_curve - IRS helpers", "[bootstrapper][irs]") {
    CurveBootstrapper bootstrapper;
    Date asof = Date::from_ymd(2026, 1, 1);

    // Create discount curve
    PiecewiseDiscountCurve::Config disc_cfg;
    disc_cfg.dc = DayCount::ACT365;

    OisSwapHelper::Config ois_cfg;
    ois_cfg.fixed_dc = DayCount::ACT365;
    ois_cfg.fixed_freq = Frequency::Annual;

    std::vector<std::shared_ptr<OisSwapHelper>> disc_helpers;
    disc_helpers.push_back(std::make_shared<OisSwapHelper>(
        asof, Date::from_ymd(2027, 1, 1), 0.025, ois_cfg
    ));
    disc_helpers.push_back(std::make_shared<OisSwapHelper>(
        asof, Date::from_ymd(2028, 1, 1), 0.030, ois_cfg
    ));
    disc_helpers.push_back(std::make_shared<OisSwapHelper>(
        asof, Date::from_ymd(2031, 1, 1), 0.035, ois_cfg
    ));

    auto disc_result = bootstrapper.bootstrap_discount_curve(asof, disc_cfg, disc_helpers);
    REQUIRE(disc_result.has_value());
    auto discount_curve = disc_result.value();

    // Bootstrap forward curve with IRS helpers
    PiecewiseForwardCurve::Config fwd_cfg;
    fwd_cfg.dc = DayCount::ACT365;

    std::vector<std::shared_ptr<RateHelper>> fwd_helpers;

    IrsHelper::Config irs_cfg;
    irs_cfg.fixed_dc = DayCount::ACT365;
    irs_cfg.fixed_freq = Frequency::Annual;
    irs_cfg.float_dc = DayCount::ACT365;
    irs_cfg.float_freq = Frequency::Annual;

    // 1Y IRS at 2.9%
    fwd_helpers.push_back(std::make_shared<IrsHelper>(
        asof,
        Date::from_ymd(2027, 1, 1),
        0.029,
        irs_cfg
    ));

    // 2Y IRS at 3.3%
    fwd_helpers.push_back(std::make_shared<IrsHelper>(
        asof,
        Date::from_ymd(2028, 1, 1),
        0.033,
        irs_cfg
    ));

    auto result = bootstrapper.bootstrap_forward_curve(
        asof, fwd_cfg, *discount_curve, fwd_helpers
    );

    REQUIRE(result.has_value());
    auto fwd_curve = result.value();
    REQUIRE(fwd_curve != nullptr);

    // Verify we can compute forward rates and validate expected value
    double fwd_1y = fwd_curve->forward_rate(
        asof,
        Date::from_ymd(2027, 1, 1),
        DayCount::ACT365
    );
    REQUIRE_THAT(fwd_1y, Catch::Matchers::WithinAbs(0.029, 1e-5));

}
