#pragma once

#include <chrono>
#include <format>
#include <string>
#include "result.hpp"
#include "conventions.hpp"


namespace ir {

	class Date {
	public:
		using sys_days = std::chrono::sys_days;

		Date() = default;
		//Date(int y, unsigned m, unsigned d) : d_(std::chrono::year{y} / m / d) {}
		explicit Date(sys_days d) : d_(d) {}

		static Date from_ymd(int y, unsigned m, unsigned d) { return Date{ std::chrono::year{y} / m / d }; }
		static Result<Date> parse_iso(const std::string_view& iso); // Assumes format "YYYY-MM-DD"
		std::string to_iso() const { return std::format("{:%Y-%m-%d}", d_); }

		int year() const;
		unsigned month() const;
		unsigned day() const;

		sys_days raw() const { return d_; }

		// Operator overloading
		friend bool operator==(const Date&, const Date&) = default;

	private:
		sys_days d_{};
	};

	// Operator overloading
	Date operator+(const Date& d, std::chrono::days dd);
	bool operator<(const Date& a, const Date& b);
	std::chrono::days operator-(const Date& a, const Date& b);


	// Tenor
	enum class TenorUnit { Days, Weeks, Months, Years };

	struct Tenor {
		int n{ 0 };
		TenorUnit unit{ TenorUnit::Days };

		static Result<Tenor> parse(std::string_view s); // "1D", "2W", "3M", "5Y"

		// helper queries
		bool is_zero() const { return n == 0; }
	};


	// Calendar
	class Calendar {
	public:
		// v1: weekends-only. later: store holiday set.
		bool is_business_day(const Date& d) const;
		Date adjust(const Date& d, BusinessDayConvention bdc) const;

		// advance by tenor (calendar-aware) then adjust
		Date advance(const Date& d, const Tenor& t, BusinessDayConvention bdc) const;

	private:
		static bool is_weekend(const Date& d);
	};

	// year fraction
	double year_fraction(const Date& start, const Date& end, DayCount dc);


	// Schedules

	struct ScheduleConfig {
		Date start;
		Date end;
		Tenor tenor;                       // e.g. 3M
		Calendar calendar;
		BusinessDayConvention bdc{ BusinessDayConvention::ModifiedFollowing };
		DateGenerationRule rule{ DateGenerationRule::Backward };
		//StubType stub{ StubType::None };     // v1: can mostly ignore, add later
		bool end_of_month{ false };          // v1: implement later if you like
	};

	struct Schedule {
		std::vector<Date> dates;           // includes start and end, adjusted
	};
	Schedule make_schedule(const ScheduleConfig& cfg);
	// Result<Schedule> make_schedule(const ScheduleConfig& cfg);

}