#pragma once
#include <string>

namespace ir{

	enum class ErrorCode {
		Ok = 0,
		InvalidArgument,
		ParseError,
		InvalidDate,
		CalendarError,
		ScheduleError
	};

	struct Error {
		ErrorCode code{ ErrorCode::Ok };
		std::string message{};

		static Error ok() { return {}; }
		static Error make(ErrorCode c, std::string msg) { return Error{ c, std::move(msg) }; }
	};

}
