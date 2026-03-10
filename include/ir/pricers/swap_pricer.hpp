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

    enum class PricingFramework {
        SingleCurve,
        MultiCurve
    };

    struct PricingContext {
        ir::Date valuation_date{};
        PricingFramework framework{ PricingFramework::MultiCurve };

        ir::CurveId discount_curve{ ir::CurveId{"DISCOUNT"} };
        ir::CurveId ibor_forward_curve{ ir::CurveId{"FWD_IBOR"} };
        ir::CurveId rfr_forward_curve{ ir::CurveId{"FWD_RFR"} };

        bool include_accrued{ false };
    };

    struct CashflowPVLine {
        ir::Date pay_date{};
        double amount{ 0.0 };
        double df{ 0.0 };
        double pv{ 0.0 };
        std::string label{};
        std::string leg_id{};
    };

    struct LegPVResult {
        double pv{ 0.0 };
        std::vector<CashflowPVLine> lines{};
    };

    struct PricingResult {
        double pv{ 0.0 };
        double pv_fixed_leg{ 0.0 };
        double pv_float_leg{ 0.0 };
        std::vector<CashflowPVLine> lines{};
    };

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

        ir::Result<LegPVResult>
            price_leg(const ir::instruments::Leg& leg,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const;
    };

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

        ir::Result<LegPVResult>
            price_leg(const ir::instruments::Leg& leg,
                const ir::market::MarketData& md,
                const PricingContext& ctx) const;
    };

    double leg_sign(ir::instruments::PayReceive dir);

    // For fixed/IBOR cashflows this behaves like before.
    // For RFR coupons it can use fixings for d < valuation_date and projection for d >= valuation_date.
    std::optional<double> signed_amount_if_known(
        const ir::instruments::Cashflow& cf,
        ir::instruments::PayReceive dir,
        const ir::market::FixingStore& fixings,
        const ir::Date& valuation_date,
        const ir::market::ForwardCurve* rfr_forward);

} // namespace ir::pricers