#pragma once

#include <optional>
#include "ir/instruments/cashflow.hpp"
#include "ir/instruments/rate_observations.hpp"

namespace ir::market {
    class ForwardCurve;
}

namespace ir::instruments {

    class FixedCoupon final : public Cashflow {
    public:
        FixedCoupon(ir::Date pay_date, double amount);
        CashflowType type() const override { return CashflowType::Fixed; }
        ir::Date pay_date() const override { return pay_date_; }
        std::optional<double> amount_if_known(const ir::market::FixingStore* fixings) const override;
        double amount() const { return amount_; }

    private:
        ir::Date pay_date_;
        double amount_{ 0.0 };
    };

    class IborCoupon final : public Cashflow {
    public:
        IborCoupon(ir::Date pay_date, double notional, double spread, IborObservation obs);
        CashflowType type() const override { return CashflowType::IborCoupon; }
        ir::Date pay_date() const override { return pay_date_; }
        std::optional<double> amount_if_known(const ir::market::FixingStore* fixings) const override;
        double notional() const { return notional_; }
        double spread() const { return spread_; }
        const IborObservation& observation() const { return obs_; }

    private:
        ir::Date pay_date_;
        double notional_{ 0.0 };
        double spread_{ 0.0 };
        IborObservation obs_;
    };

    class RfrCompoundCoupon final : public Cashflow {
    public:
        RfrCompoundCoupon(ir::Date pay_date, double notional, double spread, RfrObservation obs);

        CashflowType type() const override { return CashflowType::RfrCoupon; }
        ir::Date pay_date() const override { return pay_date_; }

        // Existing full-known behavior: only succeeds if all required fixings exist.
        std::optional<double> amount_if_known(const ir::market::FixingStore* fixings) const override;

        // New hybrid behavior:
        // - for d < cutoff: use realized fixings
        // - for d >= cutoff: use forward projection
        std::optional<double> amount_if_known(
            const ir::market::FixingStore* fixings,
            const ir::Date& cutoff,
            const ir::market::ForwardCurve* forward) const;

        double notional() const { return notional_; }
        double spread() const { return spread_; }
        const RfrObservation& observation() const { return obs_; }

    private:
        ir::Date pay_date_;
        double notional_{ 0.0 };
        double spread_{ 0.0 };
        RfrObservation obs_;
    };

} // namespace ir::instruments