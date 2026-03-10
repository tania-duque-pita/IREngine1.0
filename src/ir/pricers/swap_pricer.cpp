#include "ir/pricers/swap_pricer.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "ir/core/date.hpp"
#include "ir/core/error.hpp"

namespace ir::pricers {

    // ------------------------ Small helpers ------------------------

    double leg_sign(ir::instruments::PayReceive dir) {
        return (dir == ir::instruments::PayReceive::Pay) ? -1.0 : +1.0;
    }

    std::optional<double> signed_amount_if_known(
        const ir::instruments::Cashflow& cf,
        ir::instruments::PayReceive dir,
        const ir::market::FixingStore& fixings,
        const ir::Date& valuation_date,
        const ir::market::ForwardCurve* rfr_forward) {

        if (cf.type() == ir::instruments::CashflowType::RfrCoupon) {
            const auto* rfr_cf = dynamic_cast<const ir::instruments::RfrCompoundCoupon*>(&cf);
            if (!rfr_cf) {
                return std::nullopt;
            }

            auto a = rfr_cf->amount_if_known(&fixings, valuation_date, rfr_forward);
            if (!a.has_value()) return std::nullopt;
            return leg_sign(dir) * a.value();
        }

        auto a = cf.amount_if_known(&fixings);
        if (!a.has_value()) return std::nullopt;
        return leg_sign(dir) * a.value();
    }

    static ir::Result<const ir::market::DiscountCurve*> get_discount_curve(
        const ir::market::MarketData& md, const ir::CurveId& id) {
        try {
            return &md.discount_curve(id);
        }
        catch (const std::exception& e) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                std::string("SwapPricer: missing discount curve: ") + e.what());
        }
    }

    static ir::Result<const ir::market::ForwardCurve*> get_forward_curve(
        const ir::market::MarketData& md, const ir::CurveId& id) {
        try {
            return &md.forward_curve(id);
        }
        catch (const std::exception& e) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                std::string("SwapPricer: missing forward curve: ") + e.what());
        }
    }

    // Single-curve projection from discount factors:
    // F(t1,t2) = (DF(t1)/DF(t2)-1)/tau
    static double forward_from_discount_curve(const ir::market::DiscountCurve& disc,
        const ir::Date& start,
        const ir::Date& end,
        ir::DayCount dc) {
        const double tau = ir::year_fraction(start, end, dc);
        if (!(tau > 0.0)) {
            throw std::runtime_error("forward_from_discount_curve: non-positive tau.");
        }
        const double df1 = disc.df(start);
        const double df2 = disc.df(end);
        return (df1 / df2 - 1.0) / tau;
    }

    static bool is_payable_after_valuation(const ir::Date& pay, const ir::Date& val) {
        // v1: ignore cashflows on/before valuation_date
        return (val < pay);
    }

    static std::optional<double> rfr_amount_single_curve_with_cutoff(
        const ir::instruments::RfrCompoundCoupon& cf,
        const ir::market::FixingStore& fixings,
        const ir::Date& valuation_date,
        const ir::market::DiscountCurve& disc) {

        const auto& obs = cf.observation();
        const ir::Date start = obs.start;
        const ir::Date end = obs.end;

        const double tau_total = ir::year_fraction(start, end, obs.accrual_dc);
        if (!(tau_total > 0.0)) {
            return std::nullopt;
        }

        double compound = 1.0;

        for (ir::Date d = start; d < end; d = d + std::chrono::days{ 1 }) {
            ir::Date d_next = d + std::chrono::days{ 1 };
            if (end < d_next) {
                d_next = end;
            }

            const double dt = ir::year_fraction(d, d_next, obs.accrual_dc);
            if (dt < 0.0) {
                return std::nullopt;
            }

            double r = 0.0;

            if (d < valuation_date) {
                auto fx = fixings.get(obs.index, d);
                if (!fx.has_value()) {
                    return std::nullopt;
                }
                r = fx.value();
            }
            else {
                r = forward_from_discount_curve(disc, d, d_next, obs.accrual_dc);
            }

            compound *= (1.0 + r * dt);
        }

        return cf.notional() * (compound - 1.0)
            + cf.notional() * cf.spread() * tau_total;
    }


    static ir::Result<LegPVResult> pv_leg_single_curve(
        const ir::instruments::Leg& leg,
        const ir::market::DiscountCurve& disc,
        const ir::market::FixingStore& fixings,
        const PricingContext& ctx) {

        LegPVResult out;
        const double sgn = leg_sign(leg.direction);

        for (const auto& cfptr : leg.cashflows) {
            if (!cfptr) continue;

            const auto pay = cfptr->pay_date();
            if (!is_payable_after_valuation(pay, ctx.valuation_date)) continue;

            const double df = disc.df(pay);
            double amt = 0.0;

            switch (cfptr->type()) {

            case ir::instruments::CashflowType::Fixed: {
                auto known = cfptr->amount_if_known(&fixings);
                if (!known.has_value()) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "SingleCurve pricer: fixed cashflow amount unknown.");
                }
                amt = known.value();
                break;
            }

            case ir::instruments::CashflowType::IborCoupon: {
                auto known = cfptr->amount_if_known(&fixings);

                if (known.has_value()) {
                    amt = known.value();
                }
                else {
                    auto cpn = std::dynamic_pointer_cast<ir::instruments::IborCoupon>(cfptr);
                    if (!cpn) {
                        return ir::Error::make(
                            ir::ErrorCode::InvalidArgument,
                            "SingleCurve pricer: cashflow type mismatch (IborCoupon).");
                    }

                    const auto& obs = cpn->observation();
                    const double tau =
                        ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);

                    if (!(tau > 0.0)) {
                        return ir::Error::make(
                            ir::ErrorCode::InvalidArgument,
                            "SingleCurve pricer: invalid IBOR accrual year fraction.");
                    }

                    const double fwd =
                        forward_from_discount_curve(
                            disc,
                            obs.accrual_start,
                            obs.accrual_end,
                            obs.accrual_dc);

                    amt = cpn->notional() * (fwd + cpn->spread()) * tau;
                }
                break;
            }

            case ir::instruments::CashflowType::RfrCoupon: {
                auto cpn = std::dynamic_pointer_cast<ir::instruments::RfrCompoundCoupon>(cfptr);
                if (!cpn) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "SingleCurve pricer: cashflow type mismatch (RfrCompoundCoupon).");
                }

                const auto& obs = cpn->observation();
                const ir::Date start = obs.start;
                const ir::Date end = obs.end;

                const double tau_total =
                    ir::year_fraction(start, end, obs.accrual_dc);

                if (!(tau_total > 0.0)) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "SingleCurve pricer: invalid RFR accrual year fraction.");
                }

                double compound = 1.0;

                for (ir::Date d = start; d < end; d = d + std::chrono::days{ 1 }) {
                    ir::Date d_next = d + std::chrono::days{ 1 };
                    if (end < d_next) {
                        d_next = end;
                    }

                    const double dt = ir::year_fraction(d, d_next, obs.accrual_dc);
                    if (dt < 0.0) {
                        return ir::Error::make(
                            ir::ErrorCode::InvalidArgument,
                            "SingleCurve pricer: invalid RFR daily accrual fraction.");
                    }

                    double r = 0.0;

                    if (d < ctx.valuation_date) {
                        auto fx = fixings.get(obs.index, d);
                        if (!fx.has_value()) {
                            return ir::Error::make(
                                ir::ErrorCode::InvalidArgument,
                                "SingleCurve pricer: missing historical RFR fixing.");
                        }
                        r = fx.value();
                    }
                    else {
                        r = forward_from_discount_curve(disc, d, d_next, obs.accrual_dc);
                    }

                    compound *= (1.0 + r * dt);
                }

                amt = cpn->notional() * (compound - 1.0)
                    + cpn->notional() * cpn->spread() * tau_total;

                break;
            }

            default:
                return ir::Error::make(
                    ir::ErrorCode::InvalidArgument,
                    "SingleCurve pricer: unsupported cashflow type.");
            }

            const double signed_amt = sgn * amt;
            const double pv = signed_amt * df;

            out.pv += pv;

            CashflowPVLine line;
            line.pay_date = pay;
            line.amount = signed_amt;
            line.df = df;
            line.pv = pv;
            line.label = (cfptr->type() == ir::instruments::CashflowType::Fixed) ? "FIXED" :
                (cfptr->type() == ir::instruments::CashflowType::IborCoupon) ? "IBOR" :
                "RFR";
            line.leg_id = leg.leg_id;
            out.lines.push_back(std::move(line));
        }

        return out;
    }

    static ir::Result<LegPVResult> pv_leg_multi_curve(
        const ir::instruments::Leg& leg,
        const ir::market::DiscountCurve& disc,
        const ir::market::ForwardCurve* ibor_fwd,
        const ir::market::ForwardCurve* rfr_fwd,
        const ir::market::FixingStore& fixings,
        const PricingContext& ctx) {

        LegPVResult out;
        const double sgn = leg_sign(leg.direction);

        for (const auto& cfptr : leg.cashflows) {
            if (!cfptr) continue;

            const auto pay = cfptr->pay_date();
            if (!is_payable_after_valuation(pay, ctx.valuation_date)) continue;

            const double df = disc.df(pay);
            double amt = 0.0;

            switch (cfptr->type()) {

            case ir::instruments::CashflowType::Fixed: {
                auto known = cfptr->amount_if_known(&fixings);
                if (!known.has_value()) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "MultiCurve pricer: fixed cashflow amount unknown.");
                }
                amt = known.value();
                break;
            }

            case ir::instruments::CashflowType::IborCoupon: {
                auto known = cfptr->amount_if_known(&fixings);

                if (known.has_value()) {
                    amt = known.value();
                }
                else {
                    auto cpn = std::dynamic_pointer_cast<ir::instruments::IborCoupon>(cfptr);
                    if (!cpn) {
                        return ir::Error::make(
                            ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: cashflow type mismatch (IborCoupon).");
                    }

                    if (!ibor_fwd) {
                        return ir::Error::make(
                            ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: IBOR forward curve is null.");
                    }

                    const auto& obs = cpn->observation();
                    const double tau =
                        ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);

                    if (!(tau > 0.0)) {
                        return ir::Error::make(
                            ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: invalid IBOR accrual year fraction.");
                    }

                    const double fwd =
                        ibor_fwd->forward_rate(obs.accrual_start, obs.accrual_end, obs.accrual_dc);

                    amt = cpn->notional() * (fwd + cpn->spread()) * tau;
                }
                break;
            }

            case ir::instruments::CashflowType::RfrCoupon: {
                auto cpn = std::dynamic_pointer_cast<ir::instruments::RfrCompoundCoupon>(cfptr);
                if (!cpn) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "MultiCurve pricer: cashflow type mismatch (RfrCompoundCoupon).");
                }

                if (!rfr_fwd) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "MultiCurve pricer: RFR forward curve is null.");
                }

                // Hybrid logic:
                // - use fixings for d < valuation_date
                // - use forward projection for d >= valuation_date
                auto hybrid_amt = cpn->amount_if_known(&fixings, ctx.valuation_date, rfr_fwd);

                if (!hybrid_amt.has_value()) {
                    return ir::Error::make(
                        ir::ErrorCode::InvalidArgument,
                        "MultiCurve pricer: unable to determine/project RFR coupon amount.");
                }

                amt = hybrid_amt.value();
                break;
            }

            default:
                return ir::Error::make(
                    ir::ErrorCode::InvalidArgument,
                    "MultiCurve pricer: unsupported cashflow type.");
            }

            const double signed_amt = sgn * amt;
            const double pv = signed_amt * df;

            out.pv += pv;

            CashflowPVLine line;
            line.pay_date = pay;
            line.amount = signed_amt;
            line.df = df;
            line.pv = pv;
            line.label = (cfptr->type() == ir::instruments::CashflowType::Fixed) ? "FIXED" :
                (cfptr->type() == ir::instruments::CashflowType::IborCoupon) ? "IBOR" :
                "RFR";
            line.leg_id = leg.leg_id;
            out.lines.push_back(std::move(line));
        }

        return out;
    }

    // ------------------------ Leg Pricer -----------------------------------
    ir::Result<LegPVResult>
        DiscountingSwapPricer::price_leg(const ir::instruments::Leg& leg,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {
        auto discR = get_discount_curve(md, ctx.discount_curve);
        if (!discR.has_value()) return discR.error();

        if (md.fixings() == nullptr) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "DiscountingSwapPricer::price_leg: MarketData fixings store is null.");
        }

        return pv_leg_single_curve(leg, *discR.value(), *(md.fixings()), ctx);
    }

    ir::Result<LegPVResult>
        MultiCurveSwapPricer::price_leg(const ir::instruments::Leg& leg,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {
        auto discR = get_discount_curve(md, ctx.discount_curve);
        if (!discR.has_value()) return discR.error();

        if (md.fixings() == nullptr) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "MultiCurveSwapPricer::price_leg: MarketData fixings store is null.");
        }

        const ir::market::ForwardCurve* ibor = nullptr;
        const ir::market::ForwardCurve* rfr = nullptr;

        bool has_ibor = false;
        bool has_rfr = false;

        for (const auto& cfptr : leg.cashflows) {
            if (!cfptr) continue;

            if (cfptr->type() == ir::instruments::CashflowType::IborCoupon) {
                has_ibor = true;
            }
            else if (cfptr->type() == ir::instruments::CashflowType::RfrCoupon) {
                has_rfr = true;
            }
        }

        if (has_ibor && has_rfr) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "MultiCurveSwapPricer::price_leg: mixed IBOR/RFR coupons in one leg are not supported.");
        }

        if (has_ibor) {
            auto iborR = get_forward_curve(md, ctx.ibor_forward_curve);
            if (!iborR.has_value()) return iborR.error();
            ibor = iborR.value();
        }

        if (has_rfr) {
            auto rfrR = get_forward_curve(md, ctx.rfr_forward_curve);
            if (!rfrR.has_value()) return rfrR.error();
            rfr = rfrR.value();
        }

        return pv_leg_multi_curve(leg, *discR.value(), ibor, rfr, *(md.fixings()), ctx);
    }


    // ------------------------ DiscountingSwapPricer ------------------------

    ir::Result<PricingResult>
        DiscountingSwapPricer::price(const ir::instruments::InterestRateSwap& swap,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {

        auto fixedLeg = price_leg(swap.fixed_leg(), md, ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto floatLeg = price_leg(swap.float_leg(), md, ctx);
        if (!floatLeg.has_value()) return floatLeg.error();

        PricingResult res;
        res.pv_fixed_leg = fixedLeg.value().pv;
        res.pv_float_leg = floatLeg.value().pv;
        res.pv = res.pv_fixed_leg + res.pv_float_leg;

        res.lines.reserve(fixedLeg.value().lines.size() + floatLeg.value().lines.size());
        res.lines.insert(res.lines.end(), fixedLeg.value().lines.begin(), fixedLeg.value().lines.end());
        res.lines.insert(res.lines.end(), floatLeg.value().lines.begin(), floatLeg.value().lines.end());

        return res;
    }

    ir::Result<PricingResult>
        DiscountingSwapPricer::price(const ir::instruments::OisSwap& swap,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {

        auto fixedLeg = price_leg(swap.fixed_leg(), md, ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto rfrLeg = price_leg(swap.rfr_leg(), md, ctx);
        if (!rfrLeg.has_value()) return rfrLeg.error();

        PricingResult res;
        res.pv_fixed_leg = fixedLeg.value().pv;
        res.pv_float_leg = rfrLeg.value().pv;
        res.pv = res.pv_fixed_leg + res.pv_float_leg;

        res.lines.reserve(fixedLeg.value().lines.size() + rfrLeg.value().lines.size());
        res.lines.insert(res.lines.end(), fixedLeg.value().lines.begin(), fixedLeg.value().lines.end());
        res.lines.insert(res.lines.end(), rfrLeg.value().lines.begin(), rfrLeg.value().lines.end());

        return res;
    }

    // ------------------------ MultiCurveSwapPricer ------------------------

    ir::Result<PricingResult>
        MultiCurveSwapPricer::price(const ir::instruments::InterestRateSwap& swap,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {

        auto fixedLeg = price_leg(swap.fixed_leg(), md, ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto floatLeg = price_leg(swap.float_leg(), md, ctx);
        if (!floatLeg.has_value()) return floatLeg.error();

        PricingResult res;
        res.pv_fixed_leg = fixedLeg.value().pv;
        res.pv_float_leg = floatLeg.value().pv;
        res.pv = res.pv_fixed_leg + res.pv_float_leg;

        res.lines.reserve(fixedLeg.value().lines.size() + floatLeg.value().lines.size());
        res.lines.insert(res.lines.end(), fixedLeg.value().lines.begin(), fixedLeg.value().lines.end());
        res.lines.insert(res.lines.end(), floatLeg.value().lines.begin(), floatLeg.value().lines.end());

        return res;
    }

    ir::Result<PricingResult>
        MultiCurveSwapPricer::price(const ir::instruments::OisSwap& swap,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {

        auto fixedLeg = price_leg(swap.fixed_leg(), md, ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto rfrLeg = price_leg(swap.rfr_leg(), md, ctx);
        if (!rfrLeg.has_value()) return rfrLeg.error();

        PricingResult res;
        res.pv_fixed_leg = fixedLeg.value().pv;
        res.pv_float_leg = rfrLeg.value().pv;
        res.pv = res.pv_fixed_leg + res.pv_float_leg;

        res.lines.reserve(fixedLeg.value().lines.size() + rfrLeg.value().lines.size());
        res.lines.insert(res.lines.end(), fixedLeg.value().lines.begin(), fixedLeg.value().lines.end());
        res.lines.insert(res.lines.end(), rfrLeg.value().lines.begin(), rfrLeg.value().lines.end());

        return res;
    }

} // namespace ir::pricers