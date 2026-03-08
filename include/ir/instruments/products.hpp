#pragma once
#include <string>

#include "ir/instruments/leg.hpp"
#include "ir/core/date.hpp"
#include "ir/core/ids.hpp"
#include "ir/instruments/coupons.hpp"

namespace ir::instruments {

    struct TradeInfo {
        std::string trade_id{ "UNSET" };
        ir::Date trade_date{};
        ir::Date start_date{};
        ir::Date end_date{};
    };

    class InterestRateSwap {
    public:
        InterestRateSwap(TradeInfo info, Leg fixed_leg, Leg float_leg)
            : info_(std::move(info)), fixed_(std::move(fixed_leg)), floating_(std::move(float_leg)) {
        }

        const TradeInfo& info() const { return info_; }
        const Leg& fixed_leg() const { return fixed_; }
        const Leg& float_leg() const { return floating_; }

    private:
        TradeInfo info_;
        Leg fixed_;
        Leg floating_;
    };

    // OIS swap: fixed vs RFR compounded float
    class OisSwap {
    public:
        OisSwap(TradeInfo info, Leg fixed_leg, Leg rfr_leg)
            : info_(std::move(info)), fixed_(std::move(fixed_leg)), rfr_(std::move(rfr_leg)) {
        }

        const TradeInfo& info() const { return info_; }
        const Leg& fixed_leg() const { return fixed_; }
        const Leg& rfr_leg() const { return rfr_; }

    private:
        TradeInfo info_;
        Leg fixed_;
        Leg rfr_;
    };

    // Optional: FRA as a single cashflow-like product (or two legs of 1 period)
    class Fra {
    public:
        Fra(TradeInfo info, IborCoupon coupon)
            : info_(std::move(info)), coupon_(std::move(coupon)) {
        }

        const TradeInfo& info() const { return info_; }
        const IborCoupon& coupon() const { return coupon_; }

    private:
        TradeInfo info_;
        IborCoupon coupon_;
    };

} // namespace ir::instruments