#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>

#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"
#include "ir/market/market_data.hpp"
#include "ir/market/curves.hpp"
#include "ir/market/quotes.hpp"
#include "ir/instruments/products.hpp"
#include "ir/instruments/leg.hpp"
#include "ir/instruments/coupons.hpp"
#include "ir/instruments/rate_observations.hpp"
#include "ir/pricers/swap_pricer.hpp"

using ir::Date;
using ir::DayCount;

namespace {

    // Simple flat discount curve for tests: df(t)=exp(-r*t)
    class FlatDiscountCurve final : public ir::market::DiscountCurve {
    public:
        FlatDiscountCurve(const Date& asof, double r)
            : ir::market::DiscountCurve(asof), r_(r) {}

        double df(const Date& d) const override {
            const double t = ir::year_fraction(asof_, d, DayCount::ACT365);
            return df(t);
        }

        double df(double t) const override {
            if (t <= 0.0) return 1.0;
            return std::exp(-r_ * t);
        }

    private:
        double r_{ 0.0 };
    };

    class FlatForwardCurve final : public ir::market::ForwardCurve {
    public:
        FlatForwardCurve(const Date& asof, double f)
            : ir::market::ForwardCurve(asof), f_(f) {}

        double forward_rate(const Date&, const Date&, DayCount) const override {
            return f_;
        }

    private:
        double f_{ 0.0 };
    };

} // namespace

TEST_CASE("MultiCurveSwapPricer: projects IBOR coupon when fixing missing", "[pricers][swap]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_fixings(&fixings);
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, 0.0));
    md.set_forward_curve(ir::CurveId{ "FWD_IBOR" }, std::make_shared<FlatForwardCurve>(asof, 0.05));

    ir::instruments::IborObservation obs{
        ir::IndexId{ "USD-LIBOR-6M" },
        asof,
        asof,
        Date::from_ymd(2026, 7, 1),
        DayCount::ACT360
    };

    auto ibor_cf = std::make_shared<ir::instruments::IborCoupon>(
        obs.accrual_end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg float_leg;
    float_leg.direction = ir::instruments::PayReceive::Receive;
    float_leg.leg_id = "L_FLOAT";
    float_leg.cashflows = { ibor_cf };

    ir::instruments::Leg fixed_leg;
    fixed_leg.direction = ir::instruments::PayReceive::Pay;
    fixed_leg.leg_id = "L_FIXED";

    ir::instruments::TradeInfo ti;
    ti.start_date = asof;
    ti.end_date = obs.accrual_end;

    ir::instruments::InterestRateSwap swap(ti, fixed_leg, float_leg);

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::MultiCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };
    ctx.ibor_forward_curve = ir::CurveId{ "FWD_IBOR" };

    ir::pricers::MultiCurveSwapPricer pricer;
    auto res = pricer.price(swap, md, ctx);

    REQUIRE(res.has_value());

    const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
    const double expected = 1'000'000.0 * 0.05 * tau;

    REQUIRE_THAT(res.value().pv, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE_THAT(res.value().pv_float_leg, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE_THAT(res.value().pv_fixed_leg, Catch::Matchers::WithinAbs(0.0, 1e-12));
}

TEST_CASE("MultiCurveSwapPricer: uses fixing when available (overrides projection)", "[pricers][swap]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, 0.0));
    md.set_forward_curve(ir::CurveId{ "FWD_IBOR" }, std::make_shared<FlatForwardCurve>(asof, 0.05));

    const auto idx = ir::IndexId{ "USD-LIBOR-6M" };
    fixings.add(idx, asof, 0.04);
    md.set_fixings(&fixings);

    ir::instruments::IborObservation obs{
        idx,
        asof,
        asof,
        Date::from_ymd(2026, 7, 1),
        DayCount::ACT360
    };

    auto ibor_cf = std::make_shared<ir::instruments::IborCoupon>(
        obs.accrual_end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg float_leg;
    float_leg.direction = ir::instruments::PayReceive::Receive;
    float_leg.leg_id = "L_FLOAT";
    float_leg.cashflows = { ibor_cf };

    ir::instruments::Leg fixed_leg;
    fixed_leg.direction = ir::instruments::PayReceive::Pay;
    fixed_leg.leg_id = "L_FIXED";

    ir::instruments::TradeInfo ti;
    ti.start_date = asof;
    ti.end_date = obs.accrual_end;

    ir::instruments::InterestRateSwap swap(ti, fixed_leg, float_leg);

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::MultiCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };
    ctx.ibor_forward_curve = ir::CurveId{ "FWD_IBOR" };

    ir::pricers::MultiCurveSwapPricer pricer;
    auto res = pricer.price(swap, md, ctx);

    REQUIRE(res.has_value());

    const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
    const double expected = 1'000'000.0 * 0.04 * tau;

    REQUIRE_THAT(res.value().pv, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE_THAT(res.value().pv_float_leg, Catch::Matchers::WithinAbs(expected, 1e-10));
}

TEST_CASE("MultiCurveSwapPricer: OIS/RFR leg projects using RFR forward curve (v1 simple)", "[pricers][swap]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_fixings(&fixings);
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, 0.0));
    md.set_forward_curve(ir::CurveId{ "FWD_RFR" }, std::make_shared<FlatForwardCurve>(asof, 0.03));

    ir::instruments::RfrObservation obs{
        ir::IndexId{ "SOFR" },
        asof,
        Date::from_ymd(2026, 2, 1),
        DayCount::ACT360
    };

    auto rfr_cf = std::make_shared<ir::instruments::RfrCompoundCoupon>(
        obs.end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg rfr_leg;
    rfr_leg.direction = ir::instruments::PayReceive::Receive;
    rfr_leg.leg_id = "L_RFR";
    rfr_leg.cashflows = { rfr_cf };

    ir::instruments::Leg fixed_leg;
    fixed_leg.direction = ir::instruments::PayReceive::Pay;
    fixed_leg.leg_id = "L_FIXED";

    ir::instruments::TradeInfo ti;
    ti.start_date = asof;
    ti.end_date = obs.end;

    ir::instruments::OisSwap swap(ti, fixed_leg, rfr_leg);

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::MultiCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };
    ctx.rfr_forward_curve = ir::CurveId{ "FWD_RFR" };

    ir::pricers::MultiCurveSwapPricer pricer;
    auto res = pricer.price(swap, md, ctx);

    REQUIRE(res.has_value());

    const double tau = ir::year_fraction(obs.start, obs.end, obs.accrual_dc);
    const double expected = 1'000'000.0 * 0.03 * tau;

    REQUIRE_THAT(res.value().pv, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE_THAT(res.value().pv_float_leg, Catch::Matchers::WithinAbs(expected, 1e-10));
}

TEST_CASE("MultiCurveSwapPricer::price_leg prices IBOR receive leg directly", "[pricers][swap][leg]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_fixings(&fixings);
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, 0.0));
    md.set_forward_curve(ir::CurveId{ "FWD_IBOR" }, std::make_shared<FlatForwardCurve>(asof, 0.05));

    ir::instruments::IborObservation obs{
        ir::IndexId{ "USD-LIBOR-6M" },
        asof,
        asof,
        Date::from_ymd(2026, 7, 1),
        DayCount::ACT360
    };

    auto ibor_cf = std::make_shared<ir::instruments::IborCoupon>(
        obs.accrual_end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg leg;
    leg.leg_id = "IBOR_LEG";
    leg.direction = ir::instruments::PayReceive::Receive;
    leg.cashflows = { ibor_cf };

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::MultiCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };
    ctx.ibor_forward_curve = ir::CurveId{ "FWD_IBOR" };

    ir::pricers::MultiCurveSwapPricer pricer;
    auto res = pricer.price_leg(leg, md, ctx);

    REQUIRE(res.has_value());

    const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
    const double expected = 1'000'000.0 * 0.05 * tau;

    REQUIRE_THAT(res.value().pv, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE(res.value().lines.size() == 1);
    REQUIRE(res.value().lines[0].leg_id == "IBOR_LEG");
    REQUIRE(res.value().lines[0].label == "IBOR");
    REQUIRE_THAT(res.value().lines[0].amount, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE_THAT(res.value().lines[0].pv, Catch::Matchers::WithinAbs(expected, 1e-10));
}

TEST_CASE("MultiCurveSwapPricer::price_leg prices RFR pay leg directly", "[pricers][swap][leg]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_fixings(&fixings);
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, 0.0));
    md.set_forward_curve(ir::CurveId{ "FWD_RFR" }, std::make_shared<FlatForwardCurve>(asof, 0.03));

    ir::instruments::RfrObservation obs{
        ir::IndexId{ "SOFR" },
        asof,
        Date::from_ymd(2026, 2, 1),
        DayCount::ACT360
    };

    auto rfr_cf = std::make_shared<ir::instruments::RfrCompoundCoupon>(
        obs.end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg leg;
    leg.leg_id = "RFR_LEG";
    leg.direction = ir::instruments::PayReceive::Pay;
    leg.cashflows = { rfr_cf };

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::MultiCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };
    ctx.rfr_forward_curve = ir::CurveId{ "FWD_RFR" };

    ir::pricers::MultiCurveSwapPricer pricer;
    auto res = pricer.price_leg(leg, md, ctx);

    REQUIRE(res.has_value());

    const double tau = ir::year_fraction(obs.start, obs.end, obs.accrual_dc);
    const double expected = -1'000'000.0 * 0.03 * tau;

    REQUIRE_THAT(res.value().pv, Catch::Matchers::WithinAbs(expected, 1e-10));
    REQUIRE(res.value().lines.size() == 1);
    REQUIRE(res.value().lines[0].leg_id == "RFR_LEG");
    REQUIRE(res.value().lines[0].label == "RFR");
    REQUIRE_THAT(res.value().lines[0].amount, Catch::Matchers::WithinAbs(expected, 1e-10));
}

TEST_CASE("DiscountingSwapPricer::price_leg projects IBOR from discount curve in single-curve mode", "[pricers][swap][leg]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_fixings(&fixings);

    const double flat_r = 0.05;
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, flat_r));

    ir::instruments::IborObservation obs{
        ir::IndexId{ "USD-LIBOR-6M" },
        asof,
        asof,
        Date::from_ymd(2026, 7, 1),
        DayCount::ACT360
    };

    auto ibor_cf = std::make_shared<ir::instruments::IborCoupon>(
        obs.accrual_end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg leg;
    leg.leg_id = "SC_IBOR_LEG";
    leg.direction = ir::instruments::PayReceive::Receive;
    leg.cashflows = { ibor_cf };

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::SingleCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };

    ir::pricers::DiscountingSwapPricer pricer;
    auto res = pricer.price_leg(leg, md, ctx);

    REQUIRE(res.has_value());
    REQUIRE(res.value().lines.size() == 1);
    REQUIRE(res.value().lines[0].label == "IBOR");
    REQUIRE(res.value().lines[0].leg_id == "SC_IBOR_LEG");

    const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
    const double df1 = 1.0;
    const double df2 = std::exp(-flat_r * ir::year_fraction(asof, obs.accrual_end, DayCount::ACT365));
    const double fwd = (df1 / df2 - 1.0) / tau;
    const double expected_amount = 1'000'000.0 * fwd * tau;
    const double expected_pv = expected_amount * df2;

    REQUIRE_THAT(res.value().lines[0].amount, Catch::Matchers::WithinAbs(expected_amount, 1e-10));
    REQUIRE_THAT(res.value().lines[0].df, Catch::Matchers::WithinAbs(df2, 1e-10));
    REQUIRE_THAT(res.value().pv, Catch::Matchers::WithinAbs(expected_pv, 1e-10));
}

TEST_CASE("MultiCurveSwapPricer::price_leg returns error when required IBOR forward curve is missing", "[pricers][swap][leg]") {
    const Date asof = Date::from_ymd(2026, 1, 1);

    ir::market::FixingStore fixings{};
    ir::market::MarketData md(asof);
    md.set_fixings(&fixings);
    md.set_discount_curve(ir::CurveId{ "DISCOUNT" }, std::make_shared<FlatDiscountCurve>(asof, 0.0));

    ir::instruments::IborObservation obs{
        ir::IndexId{ "USD-LIBOR-6M" },
        asof,
        asof,
        Date::from_ymd(2026, 7, 1),
        DayCount::ACT360
    };

    auto ibor_cf = std::make_shared<ir::instruments::IborCoupon>(
        obs.accrual_end, 1'000'000.0, 0.0, obs);

    ir::instruments::Leg leg;
    leg.leg_id = "BROKEN_IBOR_LEG";
    leg.direction = ir::instruments::PayReceive::Receive;
    leg.cashflows = { ibor_cf };

    ir::pricers::PricingContext ctx;
    ctx.valuation_date = asof;
    ctx.framework = ir::pricers::PricingFramework::MultiCurve;
    ctx.discount_curve = ir::CurveId{ "DISCOUNT" };
    ctx.ibor_forward_curve = ir::CurveId{ "MISSING_FWD" };

    ir::pricers::MultiCurveSwapPricer pricer;
    auto res = pricer.price_leg(leg, md, ctx);

    REQUIRE_FALSE(res.has_value());
}