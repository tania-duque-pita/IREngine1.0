#include "ir/market/rate_helpers.hpp"

#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/error.hpp"
#include "ir/market/curves.hpp"

namespace ir::market {

    static ir::Result<ir::Tenor> tenor_from_frequency(ir::Frequency f) {
        // Minimal mapping for vanilla swaps
        switch (f) {
        case ir::Frequency::Annual:     return ir::Tenor{ 1, ir::TenorUnit::Years };
        case ir::Frequency::SemiAnnual: return ir::Tenor{ 6, ir::TenorUnit::Months };
        case ir::Frequency::Quarterly:  return ir::Tenor{ 3, ir::TenorUnit::Months };
        case ir::Frequency::Monthly:    return ir::Tenor{ 1, ir::TenorUnit::Months };
        default:
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "Unsupported Frequency in tenor_from_frequency.");
        }
    }

    static ir::Result<ir::Schedule> make_leg_schedule(const ir::Date& start,
        const ir::Date& end,
        const ir::Tenor& tenor,
        const ir::Calendar& cal,
        ir::BusinessDayConvention bdc) {
        ir::ScheduleConfig sc;
        sc.start = start;
        sc.end = end;
        sc.tenor = tenor;
        sc.calendar = cal;
        sc.bdc = bdc;
        sc.rule = ir::DateGenerationRule::Backward;
        //sc.stub = ir::StubType::None;
        sc.end_of_month = false;

        return ir::make_schedule(sc);
    }

    // ============================
    // OisSwapHelper
    // ============================
    

    ir::Result<double> OisSwapHelper::implied_par_rate(const PiecewiseDiscountCurve& disc) const {
        // Fixed leg schedule
        auto ten = tenor_from_frequency(cfg_.fixed_freq);
        if (!ten.has_value()) return ten.error();

        auto sched = make_leg_schedule(start_, end_, ten.value(), cfg_.calendar, cfg_.bdc);
        if (!sched.has_value()) return sched.error();

        const auto& dates = sched.value().dates;
        if (dates.size() < 2) {
            return ir::Error::make(ir::ErrorCode::ScheduleError,
                "OisSwapHelper: schedule has < 2 dates.");
        }

        // Annuity = sum DF(t_i) * tau_{i-1,i}
        double annuity = 0.0;
        for (std::size_t i = 1; i < dates.size(); ++i) {
            const auto& d0 = dates[i - 1];
            const auto& d1 = dates[i];
            const double tau = ir::year_fraction(d0, d1, cfg_.fixed_dc);
            annuity += disc.df(d1) * tau;
        }

        if (!(annuity > 0.0)) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "OisSwapHelper: non-positive annuity.");
        }

        // Float PV for par OIS (simplified): DF(start) - DF(end)
        // Works for standard par swap with no spreads, no stubs complexity.
        const double numer = disc.df(start_) - disc.df(end_);
        return numer / annuity;
    }

    // ============================
    // FraHelper
    // ============================

    ir::Result<double> FraHelper::implied_fra_rate(const PiecewiseForwardCurve& fwd) const {
        const double tau = ir::year_fraction(start_, end_, cfg_.dc);
        if (!(tau > 0.0)) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "FraHelper: non-positive accrual tau.");
        }
        // Use forward curve’s API (pseudo DF implementation)
        const double r = fwd.forward_rate(start_, end_, cfg_.dc);
        return r;
    }

    // ============================
    // IrsHelper
    // ============================


    ir::Result<double> IrsHelper::implied_par_rate(const PiecewiseDiscountCurve& disc,
        const PiecewiseForwardCurve& fwd) const {
        // Fixed leg schedule
        auto fixTen = tenor_from_frequency(cfg_.fixed_freq);
        if (!fixTen.has_value()) return fixTen.error();
        auto fixSched = make_leg_schedule(start_, end_, fixTen.value(), cfg_.calendar, cfg_.bdc);
        if (!fixSched.has_value()) return fixSched.error();

        // Float leg schedule
        auto fltTen = tenor_from_frequency(cfg_.float_freq);
        if (!fltTen.has_value()) return fltTen.error();
        auto fltSched = make_leg_schedule(start_, end_, fltTen.value(), cfg_.calendar, cfg_.bdc);
        if (!fltSched.has_value()) return fltSched.error();

        const auto& fd = fixSched.value().dates;
        const auto& ld = fltSched.value().dates;

        if (fd.size() < 2 || ld.size() < 2) {
            return ir::Error::make(ir::ErrorCode::ScheduleError,
                "IrsHelper: schedule has < 2 dates.");
        }

        // Fixed annuity
        double annuity = 0.0;
        for (std::size_t i = 1; i < fd.size(); ++i) {
            const double tau = ir::year_fraction(fd[i - 1], fd[i], cfg_.fixed_dc);
            annuity += disc.df(fd[i]) * tau;
        }
        if (!(annuity > 0.0)) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "IrsHelper: non-positive fixed annuity.");
        }

        // Float PV: sum DF(pay) * F(reset,pay) * tau
        double pv_float = 0.0;
        for (std::size_t i = 1; i < ld.size(); ++i) {
            const auto& d0 = ld[i - 1];
            const auto& d1 = ld[i];
            const double tau = ir::year_fraction(d0, d1, cfg_.float_dc);
            const double F = fwd.forward_rate(d0, d1, cfg_.float_dc);
            pv_float += disc.df(d1) * F * tau;
        }

        // Par rate = PV_float / annuity
        return pv_float / annuity;
    }

} // namespace ir::market
