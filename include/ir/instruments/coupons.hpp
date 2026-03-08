#pragma once
#include <optional>

#include "ir/instruments/cashflow.hpp"
#include "ir/instruments/rate_observations.hpp"

namespace ir::instruments {

    // ---------------- FixedCoupon ----------------
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

    // ---------------- IborCoupon ----------------
    // Amount = notional * (fixing_or_forward + spread) * accrual
    class IborCoupon final : public Cashflow {
    public:
        IborCoupon(ir::Date pay_date,
            double notional,
            double spread,
            IborObservation obs);

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

    // ---------------- RfrCompoundCoupon ----------------
    // Amount = notional * (compounded_RFR + spread) * accrual
    // (v1: if daily fixings exist, compute realized; else unknown)
    class RfrCompoundCoupon final : public Cashflow {
    public:
        RfrCompoundCoupon(ir::Date pay_date,
            double notional,
            double spread,
            RfrObservation obs);

        CashflowType type() const override { return CashflowType::RfrCoupon; }
        ir::Date pay_date() const override { return pay_date_; }

        std::optional<double> amount_if_known(const ir::market::FixingStore* fixings) const override;

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