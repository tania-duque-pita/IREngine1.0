#pragma once
namespace ir {

	enum class DayCount { ACT360, ACT365, THIRTY360}; // ACTACT_ISDA
	enum class BusinessDayConvention { Following, ModifiedFollowing, Preceding };
	enum class Frequency { Annual = 1, SemiAnnual = 2, Quarterly = 4, Monthly = 12 , Weekly = 52, Daily = 365 };
	enum class Compounding { Simple, Compounded, Continuous };
	enum class DateGenerationRule { Forward, Backward };
	// enum class StubType { None, ShortFront, LongFront, ShortBack, LongBack };

}
