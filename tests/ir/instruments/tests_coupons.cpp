#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"
#include "ir/market/quotes.hpp"           // FixingStore
#include "ir/instruments/coupons.hpp"
#include "ir/instruments/rate_observations.hpp"

using ir::Date;
using ir::DayCount;
using ir::IndexId;

TEST_CASE("FixedCoupon: amount_if_known always returns amount", "[instruments][coupons]") {
	ir::market::FixingStore fixings;

	Date pay = Date::from_ymd(2026, 1, 10);
	ir::instruments::FixedCoupon c(pay, 123.45);

	auto amt = c.amount_if_known(&fixings);
	REQUIRE(amt.has_value());
	REQUIRE_THAT(amt.value(), Catch::Matchers::WithinAbs(123.45, 1e-10));
}

TEST_CASE("IborCoupon: amount_if_known is nullopt if fixing missing", "[instruments][coupons]") {
	ir::market::FixingStore fixings;

	ir::instruments::IborObservation obs{
		IndexId{ "USD-LIBOR-3M" },
		Date::from_ymd(2026, 1, 2),
		Date::from_ymd(2026, 1, 2),
		Date::from_ymd(2026, 4, 2),
		DayCount::ACT360
	};

	ir::instruments::IborCoupon c(Date::from_ymd(2026, 4, 2), 1'000'000.0, 0.0010, obs);

	auto amt = c.amount_if_known(&fixings);
	REQUIRE_FALSE(amt.has_value());
}

TEST_CASE("IborCoupon: amount_if_known uses fixing + spread", "[instruments][coupons]") {
	ir::market::FixingStore fixings;

	const auto idx = IndexId{ "USD-LIBOR-3M" };
	const Date fix = Date::from_ymd(2026, 1, 2);
	fixings.add(idx, fix, 0.0300); // 3%

	ir::instruments::IborObservation obs{
		idx,
		fix,
		Date::from_ymd(2026, 1, 2),
		Date::from_ymd(2026, 4, 2),
		DayCount::ACT360
	};

	const double notional = 1'000'000.0;
	const double spread = 0.0010; // 10bp
	ir::instruments::IborCoupon c(Date::from_ymd(2026, 4, 2), notional, spread, obs);

	auto amt = c.amount_if_known(&fixings);
	REQUIRE(amt.has_value());

	const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
	const double expected = notional * (0.0300 + spread) * tau;
	REQUIRE_THAT(amt.value(), Catch::Matchers::WithinAbs(expected, 1e-10));
}

TEST_CASE("RfrCompoundCoupon: nullopt if any daily fixing missing", "[instruments][coupons]") {
	ir::market::FixingStore fixings;

	const auto idx = IndexId{ "SOFR" };
	const Date start = Date::from_ymd(2026, 1, 1);
	const Date end = Date::from_ymd(2026, 1, 4); // days: 1->2, 2->3, 3->4

	// Add only 2 fixings out of 3 needed
	fixings.add(idx, Date::from_ymd(2026, 1, 1), 0.0100);
	fixings.add(idx, Date::from_ymd(2026, 1, 2), 0.0100);
	// Missing 2026-01-03

	ir::instruments::RfrObservation obs{
		idx,
		start,
		end,
		DayCount::ACT360
	};

	ir::instruments::RfrCompoundCoupon c(end, 1'000'000.0, 0.0, obs);

	auto amt = c.amount_if_known(&fixings);
	REQUIRE_FALSE(amt.has_value());
}

TEST_CASE("RfrCompoundCoupon: realized amount matches daily compounding (v1)", "[instruments][coupons]") {
	ir::market::FixingStore fixings;

	const auto idx = IndexId{ "SOFR" };
	const Date start = Date::from_ymd(2026, 1, 1);
	const Date end = Date::from_ymd(2026, 1, 4); // 3 daily periods

	// Constant daily rate = 1%
	fixings.add(idx, Date::from_ymd(2026, 1, 1), 0.0100);
	fixings.add(idx, Date::from_ymd(2026, 1, 2), 0.0100);
	fixings.add(idx, Date::from_ymd(2026, 1, 3), 0.0100);

	ir::instruments::RfrObservation obs{
		idx,
		start,
		end,
		DayCount::ACT360 };

	const double notional = 1'000'000.0;
	const double spread = 0.0;
	ir::instruments::RfrCompoundCoupon c(end, notional, spread, obs);

	auto amt = c.amount_if_known(&fixings);
	REQUIRE(amt.has_value());

	// Expected: compound = Π(1 + r*dt), comp_rate = (compound-1)/tau_total
	const double dt = ir::year_fraction(Date::from_ymd(2026, 1, 1), Date::from_ymd(2026, 1, 2), DayCount::ACT360);
	const double tau_total = ir::year_fraction(start, end, DayCount::ACT360);

	const double compound = (1.0 + 0.0100 * dt) * (1.0 + 0.0100 * dt) * (1.0 + 0.0100 * dt);
	const double comp_rate = (compound - 1.0) / tau_total;
	const double expected = notional * comp_rate * tau_total;

	REQUIRE_THAT(amt.value(), Catch::Matchers::WithinAbs(expected, 1e-10));
}