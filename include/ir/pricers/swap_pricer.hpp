#pragma once
#include <optional>
#include <string>
#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/ids.hpp"
#include "ir/core/result.hpp"

#include "ir/market/market_data.hpp"
#include "ir/instruments/products.hpp"
#include "ir/instruments/leg.hpp"
#include "ir/instruments/cashflow.hpp"
#include "ir/instruments/coupons.hpp"

namespace ir::pricers {

    // ------------------------ Pricing context ------------------------

    enum class PricingFramework {
        SingleCurve,   // same curve used for discounting and projecting
        MultiCurve     // OIS discount curve + forwarding curve(s)
    };

    struct PricingContext {
        ir::Date valuation_date{};

        PricingFramework framework{ PricingFramework::MultiCurve };

        // Curve selection (IDs must exist in MarketData)
        ir::CurveId discount_curve{ ir::CurveId{"DISCOUNT"} };

        // Forwarding curves used for IBOR/RFR projection
        // For v1: one ID for IBOR and one ID for RFR.
        // Later: map IndexId -> CurveId.
        ir::CurveId ibor_forward_curve{ ir::CurveId{"FWD_IBOR"} };
        ir::CurveId rfr_forward_curve{ ir::CurveId{"FWD_RFR"} };

        // Optional toggles
        bool include_accrued{ false };   // if you later implement accrued
    };

    // ------------------------ Results ------------------------

    struct CashflowPVLine {
        ir::Date pay_date{};
        double amount{ 0.0 };           // signed amount (pay/receive applied)
        double df{ 0.0 };
        double pv{ 0.0 };
        std::string label{};
        std::string leg_id{};
    };

    struct PricingResult {
        double pv{ 0.0 };
        double pv_fixed_leg{ 0.0 };
        double pv_float_leg{ 0.0 };

        // Optional diagnostics useful for interviews/debugging
        std::vector<CashflowPVLine> lines{};
    };

    // ------------------------ SwapPricer interface ------------------------

    class SwapPricer {
    public:
        virtual ~SwapPricer() = default;

        virtual ir::Result<PricingResult>
            price(const ir::instruments::InterestRateSwap& swap,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const = 0;

        virtual ir::Result<PricingResult>
            price(const ir::instruments::OisSwap& swap,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const = 0;
    };

    // ------------------------ Concrete pricers ------------------------

    // Single-curve: discount + projection from the same curve (simplified)
    class DiscountingSwapPricer final : public SwapPricer {
    public:
        ir::Result<PricingResult>
            price(const ir::instruments::InterestRateSwap& swap,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const override;

        ir::Result<PricingResult>
            price(const ir::instruments::OisSwap& swap,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const override;
    };

    // Multi-curve: discount from OIS curve, project from forwarding curves
    class MultiCurveSwapPricer final : public SwapPricer {
    public:
        ir::Result<PricingResult>
            price(const ir::instruments::InterestRateSwap& swap,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const override;

        ir::Result<PricingResult>
            price(const ir::instruments::OisSwap& swap,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const override;
    };

    // ------------------------ Shared helpers (header-only signatures) ------------------------
    // These keep the implementation clean and testable.

    double leg_sign(ir::instruments::PayReceive dir);

    // Returns signed cashflow amount if determinable from fixings.
    // For future cashflows that require projection, the pricer will compute it using curves.
    std::optional<double> signed_amount_if_known(const ir::instruments::Cashflow& cf,
        ir::instruments::PayReceive dir,
        const ir::market::FixingStore& fixings);

} // namespace ir::pricers