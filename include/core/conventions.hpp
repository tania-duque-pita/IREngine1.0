#pragma once
namespace core {

	enum class DayCount { ACT360, ACT365F, THIRTY360}; // ACTACT_ISDA
	enum class BusinessDayConvention { Following, ModifiedFollowing, Preceding };
	enum class Frequency { Annual = 1, SemiAnnual = 2, Quarterly = 4, Monthly = 12 };
	enum class Compounding { Simple, Compounded, Continuous };
	enum class DateGenerationRule { Forward, Backward };
	// enum class StubType { None, ShortFront, LongFront, ShortBack, LongBack };

}
