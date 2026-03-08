#include <catch2/catch_test_macros.hpp>

#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"

#include "ir/instruments/leg_builder.hpp"
#include "ir/instruments/coupons.hpp"

using ir::Date;

static ir::Schedule make_simple_schedule() {
    // Jan 1 -> Apr 1, 2026 with monthly tenor (3 periods)
    ir::Schedule s;
    s.dates = {
      Date::from_ymd(2026, 1, 1),
      Date::from_ymd(2026, 2, 1),
      Date::from_ymd(2026, 3, 1),
      Date::from_ymd(2026, 4, 1)
    };
    return s;
}

TEST_CASE("LegBuilder: build_fixed_leg creates N-1 FixedCoupons with pay dates = period end",
    "[instruments][leg_builder]") {
    const auto sched = make_simple_schedule();

    ir::instruments::FixedLegConfig cfg;
    cfg.notional = 1'000'000.0;
    cfg.fixed_rate = 0.05;
    cfg.dc = ir::DayCount::ACT365;

    auto leg = ir::instruments::LegBuilder::build_fixed_leg(ir::instruments::PayReceive::Receive, sched, cfg);

    REQUIRE(leg.cashflows.size() == sched.dates.size() - 1);

    // Pay dates should match accrual end dates
    for (std::size_t i = 1; i < sched.dates.size(); ++i) {
        REQUIRE(leg.cashflows[i - 1]->pay_date() == sched.dates[i]);
        REQUIRE(leg.cashflows[i - 1]->type() == ir::instruments::CashflowType::Fixed);
    }
}

TEST_CASE("LegBuilder: build_ibor_leg sets fixing_date = accrual_start - lag adjusted by calendar",
    "[instruments][leg_builder]") {
    const auto sched = make_simple_schedule();

    ir::instruments::IborLegConfig cfg;
    cfg.notional = 1'000'000.0;
    cfg.spread = 0.001;
    cfg.index = ir::IndexId{ "USD-LIBOR-1M" };
    cfg.dc = ir::DayCount::ACT360;
    cfg.fixing_lag_days = 2;

    ir::Calendar cal; // weekends-only in v1
    auto leg = ir::instruments::LegBuilder::build_ibor_leg(
        ir::instruments::PayReceive::Pay,
        sched,
        cfg,
        cal,
        ir::BusinessDayConvention::ModifiedFollowing
    );

    REQUIRE(leg.cashflows.size() == sched.dates.size() - 1);

    // Inspect first coupon details via dynamic cast
    auto c0 = std::dynamic_pointer_cast<ir::instruments::IborCoupon>(leg.cashflows[0]);
    REQUIRE(c0 != nullptr);

    // First accrual starts 2026-01-01. Fixing date should be 2025-12-30 then adjusted.
    // 2025-12-30 is Tuesday, business day -> unchanged in weekends-only calendar.
    REQUIRE(c0->observation().accrual_start == Date::from_ymd(2026, 1, 1));
    REQUIRE(c0->observation().fixing_date == Date::from_ymd(2025, 12, 30));
    REQUIRE(c0->pay_date() == Date::from_ymd(2026, 2, 1));
}

TEST_CASE("LegBuilder: build_rfr_compound_leg creates RfrCompoundCoupon per period",
    "[instruments][leg_builder]") {
    const auto sched = make_simple_schedule();

    ir::instruments::RfrLegConfig cfg;
    cfg.notional = 1'000'000.0;
    cfg.spread = 0.0;
    cfg.index = ir::IndexId{ "SOFR" };
    cfg.dc = ir::DayCount::ACT360;

    auto leg = ir::instruments::LegBuilder::build_rfr_compound_leg(
        ir::instruments::PayReceive::Receive, sched, cfg);

    REQUIRE(leg.cashflows.size() == sched.dates.size() - 1);

    auto c0 = std::dynamic_pointer_cast<ir::instruments::RfrCompoundCoupon>(leg.cashflows[0]);
    REQUIRE(c0 != nullptr);

    REQUIRE(c0->observation().start == Date::from_ymd(2026, 1, 1));
    REQUIRE(c0->observation().end == Date::from_ymd(2026, 2, 1));
    REQUIRE(c0->pay_date() == Date::from_ymd(2026, 2, 1));
}