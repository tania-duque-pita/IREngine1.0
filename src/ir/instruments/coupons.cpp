#include "ir/instruments/coupons.hpp"

#include <chrono>
#include <cmath>

#include "ir/core/date.hpp"
#include "ir/core/error.hpp"

namespace ir::instruments {

    // ===================== FixedCoupon =====================

    FixedCoupon::FixedCoupon(ir::Date pay_date, double amount)
        : pay_date_(pay_date), amount_(amount) {
    }

    std::optional<double> FixedCoupon::amount_if_known(const ir::market::FixingStore* fixings) const {
        return amount_; // always deterministic
    }

    // ===================== IborCoupon =====================

    IborCoupon::IborCoupon(ir::Date pay_date,
        double notional,
        double spread,
        IborObservation obs)
        : pay_date_(pay_date),
        notional_(notional),
        spread_(spread),
        obs_(std::move(obs)) {
    }

    std::optional<double> IborCoupon::amount_if_known(const ir::market::FixingStore* fixings) const {
        if (!fixings) return std::nullopt; // Now we can check for nullptr!

        auto fx = fixings->get(obs_.index, obs_.fixing_date);
        if (!fx.has_value()) return std::nullopt;

        const double tau = ir::year_fraction(obs_.accrual_start, obs_.accrual_end, obs_.accrual_dc);
        if (!(tau > 0.0)) return std::nullopt;

        const double rate = fx.value() + spread_;
        return notional_ * rate * tau;
    }

    // ===================== RfrCompoundCoupon =====================
    // v1 simplifying assumptions:
    // - requires a fixing for *every calendar day* in [start, end)
    // - no weekend/holiday lookback logic (TODO later)
    // - compounding: ?(1 + r_i * dt_i)
    // - equivalent compounded rate: (compound_factor - 1) / tau_total
    // - amount: notional * (comp_rate + spread) * tau_total
    //
    // If any fixing is missing, returns nullopt.

    RfrCompoundCoupon::RfrCompoundCoupon(ir::Date pay_date,
        double notional,
        double spread,
        RfrObservation obs)
        : pay_date_(pay_date),
        notional_(notional),
        spread_(spread),
        obs_(std::move(obs)) {
    }

    std::optional<double> RfrCompoundCoupon::amount_if_known(const ir::market::FixingStore* fixings) const {
        const auto start = obs_.start;
        const auto end = obs_.end;
        if (!fixings) return std::nullopt; // Now we can check for nullptr!

        const double tau_total = ir::year_fraction(start, end, obs_.accrual_dc);
        if (!(tau_total > 0.0)) return std::nullopt;

        double compound = 1.0;

        // iterate day by day: [d, d+1)
        for (ir::Date d = start; d < end; d = d + std::chrono::days{ 1 }) {
            const ir::Date d_next = d + std::chrono::days{ 1 };
            if (!(d_next <= end)) break; // safety

            auto fx = fixings->get(obs_.index, d);
            if (!fx.has_value()) return std::nullopt;

            const double dt = ir::year_fraction(d, d_next, obs_.accrual_dc);
            if (dt < 0.0) return std::nullopt;

            compound *= (1.0 + fx.value() * dt);
        }

        const double comp_rate = (compound - 1.0) / tau_total;
        const double total_rate = comp_rate + spread_;

        return notional_ * total_rate * tau_total;
    }

} // namespace ir::instruments