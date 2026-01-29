#include "../../include/core/date.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>


using namespace ir;

TEST_CASE("Date::") {
	Date date = Date::from_ymd(2026, 1, 25);
	Date date2= Date::from_ymd(2026, 1, 27);
	std::chrono::days delta_d{ 2 };

	SECTION("Date::operators") {
		REQUIRE((date+ delta_d) == date2);
		REQUIRE(date<date2);
		REQUIRE(date2 - date == delta_d);
	}

	SECTION("Date::parse_iso") {
		Result<Date> date_iso = Date::parse_iso("2026-01-25");
		REQUIRE(date_iso.has_value());
		REQUIRE(date_iso.value() == date);

		Result<Date> date_error = Date::parse_iso("20250123");
		REQUIRE_FALSE(date_error.has_value());
		REQUIRE(date_error.error().code == ErrorCode::InvalidDate);

		Result<Date> date_error2 = Date::parse_iso("01-Nov-2025");
		REQUIRE_FALSE(date_error2.has_value());
		REQUIRE(date_error2.error().code == ErrorCode::ParseError);
	}

	SECTION("Date::year/month/day") {
		REQUIRE(date.year() == 2026);
		REQUIRE(date.month() == static_cast<unsigned>(1));
		REQUIRE(date.day() == static_cast<unsigned>(25));
	}
}

TEST_CASE("Tenor::parse") {
	Tenor tenor{ 5,TenorUnit::Days };

	// Parse Tenor
	Result<Tenor> my_tenor = Tenor::parse("2W");
	REQUIRE(my_tenor.value().n == 2);
	REQUIRE(my_tenor.value().unit == TenorUnit::Weeks);

	my_tenor = Tenor::parse("5Y");
	REQUIRE(my_tenor.value().n == 5);
	REQUIRE(my_tenor.value().unit == TenorUnit::Years);

	// Check errors
	Result<Tenor> my_tenor_error = Tenor::parse("2");
	REQUIRE_FALSE(my_tenor_error.has_value());
	REQUIRE(my_tenor_error.error().code == ErrorCode::ParseError);
	REQUIRE(my_tenor_error.error().message == "Tenor string too short.");

	my_tenor_error = Tenor::parse("TW");
	REQUIRE_FALSE(my_tenor_error.has_value());
	REQUIRE(my_tenor_error.error().code == ErrorCode::ParseError);
	REQUIRE(my_tenor_error.error().message == "Tenor string does not consist of numeric tenor amount and tenor unit (D/W/M/Y)");

}


TEST_CASE("Calendar::is_weekend via is_business_day") {
    Calendar cal;

    // 2026-01-25 is Sunday -> weekend
    REQUIRE_FALSE(cal.is_business_day(Date::from_ymd(2026, 1, 25)));

    // 2026-01-26 is Monday -> business day
    REQUIRE(cal.is_business_day(Date::from_ymd(2026, 1, 26)));

    // 2026-01-24 is Saturday -> weekend
    REQUIRE_FALSE(cal.is_business_day(Date::from_ymd(2026, 1, 24)));
}

TEST_CASE("Calendar::adjust (Following / Preceding / ModifiedFollowing)") {
    Calendar cal;

    // Following: Sunday 2026-01-25 -> next business day 2026-01-26 (Monday)
    {
        Date src = Date::from_ymd(2026, 1, 25);
        Date adj = cal.adjust(src, BusinessDayConvention::Following);
        REQUIRE(adj == Date::from_ymd(2026, 1, 26));
    }

    // Preceding: Sunday 2026-01-25 -> previous business day 2026-01-23 (Friday)
    {
        Date src = Date::from_ymd(2026, 1, 25);
        Date adj = cal.adjust(src, BusinessDayConvention::Preceding);
        REQUIRE(adj == Date::from_ymd(2026, 1, 23));
    }

    // ModifiedFollowing: month roll example
    // 2026-01-31 is Saturday -> Following would move to next month (Mon 2026-02-02),
    // ModifiedFollowing should fall back to preceding business day in the same month -> 2026-01-30 (Friday).
    {
        Date src = Date::from_ymd(2026, 1, 31);
        Date adj = cal.adjust(src, BusinessDayConvention::ModifiedFollowing);
        REQUIRE(adj == Date::from_ymd(2026, 1, 30));
    }
}

TEST_CASE("Calendar::advance (Days, Weeks, Months, Years) with adjustment") {
    Calendar cal;

    // Advance by 2 days: 2026-01-25 + 2D -> 2026-01-27
    {
        Date src = Date::from_ymd(2026, 1, 25);
        Tenor t{ 2, TenorUnit::Days };
        Date adv = cal.advance(src, t, BusinessDayConvention::Following);
        REQUIRE(adv == Date::from_ymd(2026, 1, 27));
    }

    // Advance by 1 week: 2026-01-25 + 1W -> 2026-02-01 (Sun) -> Following -> 2026-02-02 (Mon)
    {
        Date src = Date::from_ymd(2026, 1, 25);
        Tenor t{ 1, TenorUnit::Weeks };
        Date adv = cal.advance(src, t, BusinessDayConvention::Following);
        REQUIRE(adv == Date::from_ymd(2026, 2, 2));
    }

    // Advance by 1 month from a mid-month business day:
    // 2026-01-26 + 1M -> 2026-02-26 (valid)
    {
        Date src = Date::from_ymd(2026, 1, 26);
        Tenor t{ 1, TenorUnit::Months };
        Date adv = cal.advance(src, t, BusinessDayConvention::Following);
        REQUIRE(adv == Date::from_ymd(2026, 2, 26));
    }

    // Advance by 1 year: 2026-01-25 + 1Y -> 2027-01-25
    {
        Date src = Date::from_ymd(2026, 1, 25);
        Tenor t{ 1, TenorUnit::Years };
        Date adv = cal.advance(src, t, BusinessDayConvention::Following);
        REQUIRE(adv == Date::from_ymd(2027, 1, 25));
    }

    // Advance month from end-of-month: 2026-01-31 + 1M -> 2026-02-28 (last day preserved),
    // then adjust with Following/ModifiedFollowing - ensure advance uses the supplied bdc.
    {
        Date src = Date::from_ymd(2026, 1, 31);
        Tenor t = {1, TenorUnit::Months};

        // With Following: result is Feb 28 (then adjusted if weekend). We expect calendar.adjust to
        // move to the next business day if Feb 28 is weekend; to avoid depending on weekday here we
        // use ModifiedFollowing which in our implementation preserves last-business-day semantics.
        Date adv_mod = cal.advance(src, t, BusinessDayConvention::ModifiedFollowing);

        // Expect a date in February 2026 that is a valid calendar day; the implementation preserves
        // "end of month" -> should be some day in Feb (Feb 28 for 2026).
        REQUIRE(adv_mod == Date::from_ymd(2026, 2, 27));

    }

    // Advance by 1 months (falling on EOM)
    {
        Date src = Date::from_ymd(2026, 2, 28);
        Tenor t{ 1, TenorUnit::Months };
        Date adv = cal.advance(src, t, BusinessDayConvention::Preceding);
        REQUIRE(adv == Date::from_ymd(2026, 3, 31));
    }


}

TEST_CASE("year_fraction: basic day count conventions") {
    Date d1 = Date::from_ymd(2026, 1, 1);
    Date d2 = Date::from_ymd(2026, 4, 1); // 90 days later (Jan31+Feb28+Mar31 = 90)

    // ACT/360
    {
        double yf = year_fraction(d1, d2, DayCount::ACT360);
        REQUIRE_THAT(yf, Catch::Matchers::WithinAbs(90.0 / 360.0, 1e-12));
    }

    // ACT/365F
    {
        double yf = year_fraction(d1, d2, DayCount::ACT365F);
        REQUIRE_THAT(yf, Catch::Matchers::WithinAbs(90.0 / 365.0,1e-12));
    }

    // THIRTY/360 (simple US rule) -- use dates that exercise D1!=31 rule
    {
        Date a = Date::from_ymd(2026, 1, 30);
        Date b = Date::from_ymd(2026, 2, 28);
        // According to implementation: Yd = 0, Md = 1, Dd = 28-30 => 30 -2 = 28 days360
        double yf = year_fraction(a, b, DayCount::THIRTY360);
        REQUIRE_THAT(yf, Catch::Matchers::WithinAbs(28.0 / 360.0, 1e-12));
    }

    // sanity: same day -> zero
    {
        double yf = year_fraction(d1, d1, DayCount::ACT365F);
        REQUIRE(yf == 0.0);
    }
}

TEST_CASE("make_schedule: forward and backward monthly schedules and EOM behavior") {
    Calendar cal;

    // Helper Tenor
    Tenor t1m = {1,TenorUnit::Months};

    // Forward monthly schedule from 2026-01-01 to 2026-04-01
    {
        ScheduleConfig cfg;
        cfg.start = Date::from_ymd(2026, 1, 1);
        cfg.end = Date::from_ymd(2026, 4, 1);
        cfg.tenor = t1m;
        cfg.calendar = cal;
        cfg.bdc = BusinessDayConvention::Following;
        cfg.rule = DateGenerationRule::Forward;

        auto check = Date::from_ymd(2026, 4, 1); //REMOVE

        Schedule s = make_schedule(cfg);
        REQUIRE(s.dates.size() == 4);
        REQUIRE(s.dates[0] == Date::from_ymd(2026, 1, 1));
        REQUIRE(s.dates[1] == Date::from_ymd(2026, 2, 2));
        REQUIRE(s.dates[2] == Date::from_ymd(2026, 3, 2));
        REQUIRE(s.dates[3] == Date::from_ymd(2026, 4, 1));
    }

    // Backward generation should produce same set when start/end align
    {
        ScheduleConfig cfg;
        cfg.start = Date::from_ymd(2026, 1, 1);
        cfg.end = Date::from_ymd(2026, 4, 1);
        cfg.tenor = t1m;
        cfg.calendar = cal;
        cfg.bdc = BusinessDayConvention::Following;
        cfg.rule = DateGenerationRule::Backward;

        Schedule s = make_schedule(cfg);
        // should include start and end and the same number of points
        REQUIRE(s.dates.front() == Date::from_ymd(2026, 1, 1));
        REQUIRE(s.dates.back() == Date::from_ymd(2026, 4, 1));
        REQUIRE(s.dates.size() == 4);
    }

    // End-of-month preservation: 2026-01-31 -> 2026-02-(last) -> 2026-03-31
    {
        ScheduleConfig cfg;
        cfg.start = Date::from_ymd(2026, 1, 31);
        cfg.end = Date::from_ymd(2026, 3, 31);
        cfg.tenor = t1m;
        cfg.calendar = cal;
        cfg.bdc = BusinessDayConvention::ModifiedFollowing;
        cfg.rule = DateGenerationRule::Forward;

        Schedule s = make_schedule(cfg);

        // Expect three entries: Jan31, Feb(last day), Mar31
        REQUIRE(s.dates.size() == 3);
        REQUIRE(s.dates[0] == Date::from_ymd(2026, 1, 30));
        REQUIRE(s.dates[1] == Date::from_ymd(2026, 2, 27));
        REQUIRE(s.dates[2] == Date::from_ymd(2026, 3, 31));
    }
}