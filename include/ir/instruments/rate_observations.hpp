#pragma once
#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/ids.hpp"
#include "ir/core/conventions.hpp"

namespace ir::instruments {

	// IBOR-style observation
	struct IborObservation {
		ir::IndexId index;         // e.g. "USD-LIBOR-3M" or "USD-SOFR-TERM-3M"
		ir::Date fixing_date;      // date when fixing is observed
		ir::Date accrual_start;
		ir::Date accrual_end;
		ir::DayCount accrual_dc{ ir::DayCount::ACT360 };
	};

	// RFR daily comp observation set (for OIS / RFR compounded coupons)
	struct RfrObservation {
		ir::IndexId index;         // e.g. "SOFR", "ESTR", "SONIA"
		ir::Date start;            // accrual start
		ir::Date end;              // accrual end (exclusive or inclusive—define convention later)
		ir::DayCount accrual_dc{ ir::DayCount::ACT360 };

		// In v1 keep compounding convention minimal.
		// Later I can add: lookback, lockout, payment lag, averaging vs compounding.
	};

} // namespace ir::instruments