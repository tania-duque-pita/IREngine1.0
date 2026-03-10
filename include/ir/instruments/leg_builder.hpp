#pragma once
#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"
#include "ir/core/ids.hpp"
#include "ir/instruments/leg.hpp"
#include "ir/instruments/coupons.hpp"
#include "ir/instruments/rate_observations.hpp"

namespace ir::instruments {

    struct FixedLegConfig {
        double notional{ 1.0 };
        double fixed_rate{ 0.0 };                // for swaps (rate * notional * accrual)
        ir::DayCount dc{ ir::DayCount::ACT365 };
    };

    struct IborLegConfig {
        double notional{ 1.0 };
        double spread{ 0.0 };
        ir::IndexId index{ ir::IndexId{"UNSET"} };
        ir::DayCount dc{ ir::DayCount::ACT360 };
        int fixing_lag_days{ 2 };                // v1: simple fixing lag
    };

    struct RfrLegConfig {
        double notional{ 1.0 };
        double spread{ 0.0 };
        ir::IndexId index{ ir::IndexId{"UNSET"} };
        ir::DayCount dc{ ir::DayCount::ACT360 };
        // v1: no lookback/lockout; add later
    };

    class LegBuilder {
    public:
        static Leg build_fixed_leg(PayReceive dir,
            const ir::Schedule& schedule,
            const FixedLegConfig& cfg);

        static Leg build_ibor_leg(PayReceive dir,
            const ir::Schedule& schedule,
            const IborLegConfig& cfg,
            const ir::Calendar& cal,
            ir::BusinessDayConvention bdc);

        static Leg build_rfr_compound_leg(PayReceive dir,
            const ir::Schedule& schedule,
            const RfrLegConfig& cfg);
    };

} // namespace ir::instruments