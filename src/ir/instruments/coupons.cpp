#include "ir/instruments/coupons.hpp"

#include <chrono>
#include <optional>

#include "ir/core/date.hpp"
#include "ir/market/curves.hpp"

namespace ir::instruments {

    // ===================== FixedCoupon =====================

    FixedCoupon::FixedCoupon(ir::Date pay_date, double amount)
        : pay_date_(pay_date), amount_(amount) {
    }

    std::optional<double> FixedCoupon::amount_if_known(const ir::market::FixingStore* /*fixings*/) const {
        return amount_;
    }

    // ===================== IborCoupon =====================

    IborCoupon::IborCoupon(ir::Date pay_date, double notional, double spread, IborObservation obs)
        : pay_date_(pay_date), notional_(notional), spread_(spread), obs_(std::move(obs)) {
    }

    std::optional<double> IborCoupon::amount_if_known(const ir::market::FixingStore* fixings) const {
        if (!fixings) return std::nullopt;

        auto fx = fixings->get(obs_.index, obs_.fixing_date);
        if (!fx.has_value()) return std::nullopt;

        const double tau = ir::year_fraction(obs_.accrual_start, obs_.accrual_end, obs_.accrual_dc);
        if (!(tau > 0.0)) return std::nullopt;

        const double rate = fx.value() + spread_;
        return notional_ * rate * tau;
    }

    // ===================== RfrCompoundCoupon =====================

    RfrCompoundCoupon::RfrCompoundCoupon(ir::Date pay_date, double notional, double spread, RfrObservation obs)
        : pay_date_(pay_date), notional_(notional), spread_(spread), obs_(std::move(obs)) {
    }

    // Existing behavior: all dates must be fixed.
    // Delegate to the hybrid overload with cutoff=end and no forward needed.
    std::optional<double> RfrCompoundCoupon::amount_if_known(const ir::market::FixingStore* fixings) const {
        return amount_if_known(fixings, obs_.end, nullptr);
    }

    // Hybrid OIS amount:
// - realized portion compounded from historical daily fixings
// - remaining portion approximated using one simple forward rate over [cutoff_eff, end]
    std::optional<double> RfrCompoundCoupon::amount_if_known(
        const ir::market::FixingStore* fixings,
        const ir::Date& cutoff,
        const ir::market::ForwardCurve* forward) const {

        const ir::Date start = obs_.start;
        const ir::Date end = obs_.end;
        const ir::Date cutoff_eff = std::min(cutoff, end);

        const double tau_total = ir::year_fraction(start, end, obs_.accrual_dc);
        if (!(tau_total > 0.0)) {
            return std::nullopt;
        }

        double compound = 1.0;

        // Realized part: [start, cutoff_eff)
        for (ir::Date d = start; d < cutoff_eff; d = d + std::chrono::days{ 1 }) {
            ir::Date d_next = d + std::chrono::days{ 1 };
            if (cutoff_eff < d_next) {
                d_next = cutoff_eff;
            }

            const double dt = ir::year_fraction(d, d_next, obs_.accrual_dc);
            if (dt < 0.0) {
                return std::nullopt;
            }

            if (!fixings) {
                return std::nullopt;
            }

            auto fx = fixings->get(obs_.index, d);
            if (!fx.has_value()) {
                return std::nullopt;
            }

            compound *= (1.0 + fx.value() * dt);
        }

        // Projected part: [max(start, cutoff_eff), end]
        const ir::Date proj_start = (start < cutoff_eff) ? cutoff_eff : start;

        if (proj_start < end) {
            if (!forward) {
                return std::nullopt;
            }

            const double tau_f = ir::year_fraction(proj_start, end, obs_.accrual_dc);
            if (tau_f < 0.0) {
                return std::nullopt;
            }

            const double r_f = forward->forward_rate(proj_start, end, obs_.accrual_dc);
            compound *= (1.0 + r_f * tau_f);
        }

        return notional_ * (compound - 1.0) + notional_ * spread_ * tau_total;
    }

} // namespace ir::instruments