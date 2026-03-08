#include "ir/instruments/leg_builder.hpp"

#include <chrono>
#include <memory>

#include "ir/core/date.hpp"
#include "ir/core/error.hpp"

namespace ir::instruments {

    static double sign(PayReceive dir) {
        return (dir == PayReceive::Pay) ? -1.0 : +1.0;
    }

    Leg LegBuilder::build_fixed_leg(PayReceive dir,
        const ir::Schedule& schedule,
        const FixedLegConfig& cfg) {
        Leg leg;
        leg.direction = dir;

        const auto& dates = schedule.dates;
        if (dates.size() < 2) return leg;

        leg.cashflows.reserve(dates.size() - 1);

        for (std::size_t i = 1; i < dates.size(); ++i) {
            const auto& d0 = dates[i - 1];
            const auto& d1 = dates[i];

            const double tau = ir::year_fraction(d0, d1, cfg.dc);
            const double amt = cfg.notional * cfg.fixed_rate * tau;

            // Keep cashflows "unsigned" (amount positive); apply direction at pricer level if preferred.
            // If you prefer signed cashflows now, multiply by sign(dir) here.
            leg.cashflows.push_back(std::make_shared<FixedCoupon>(d1, amt));
        }

        return leg;
    }

    Leg LegBuilder::build_ibor_leg(PayReceive dir,
        const ir::Schedule& schedule,
        const IborLegConfig& cfg,
        const ir::Calendar& cal,
        ir::BusinessDayConvention bdc) {
        Leg leg;
        leg.direction = dir;

        const auto& dates = schedule.dates;
        if (dates.size() < 2) return leg;

        leg.cashflows.reserve(dates.size() - 1);

        for (std::size_t i = 1; i < dates.size(); ++i) {
            const auto& accrual_start = dates[i - 1];
            const auto& accrual_end = dates[i];
            const auto  pay_date = accrual_end;

            // fixing_date = accrual_start - fixing_lag_days, adjusted
            ir::Date fixing_date = accrual_start + std::chrono::days{ -cfg.fixing_lag_days };
            fixing_date = cal.adjust(fixing_date, bdc);

            IborObservation obs{
                cfg.index,
                fixing_date,
                accrual_start,
                accrual_end,
                cfg.dc
            };

            leg.cashflows.push_back(std::make_shared<IborCoupon>(
                pay_date, cfg.notional, cfg.spread, obs));
        }

        return leg;
    }

    Leg LegBuilder::build_rfr_compound_leg(PayReceive dir,
        const ir::Schedule& schedule,
        const RfrLegConfig& cfg) {
        Leg leg;
        leg.direction = dir;

        const auto& dates = schedule.dates;
        if (dates.size() < 2) return leg;

        leg.cashflows.reserve(dates.size() - 1);

        for (std::size_t i = 1; i < dates.size(); ++i) {
            const auto& accrual_start = dates[i - 1];
            const auto& accrual_end = dates[i];
            const auto  pay_date = accrual_end;

            RfrObservation obs{
                cfg.index,
                accrual_start,
                accrual_end,
                cfg.dc
            };

            leg.cashflows.push_back(std::make_shared<RfrCompoundCoupon>(
                pay_date, cfg.notional, cfg.spread, obs));
        }

        return leg;
    }

} // namespace ir::instruments