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

    std::optional<double> signed_amount_if_known(const ir::instruments::Cashflow& cf,
        ir::instruments::PayReceive dir,
        const ir::market::FixingStore& fixings) {
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

    struct LegPVResult {
        double pv{ 0.0 };
        std::vector<CashflowPVLine> lines{};
    };

    // Project + discount each cashflow in a leg.
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

            // Try known amount from fixings (or deterministic fixed coupon)
            auto known = cfptr->amount_if_known(&fixings);
            double amt = 0.0;

            if (known.has_value()) {
                amt = known.value();
            }
            else {
                // Need projection based on CF type
                switch (cfptr->type()) {
                case ir::instruments::CashflowType::IborCoupon: {
                    auto cpn = std::dynamic_pointer_cast<ir::instruments::IborCoupon>(cfptr);
                    if (!cpn) {
                        return ir::Error::make(ir::ErrorCode::InvalidArgument,
                            "SingleCurve pricer: cashflow type mismatch (IborCoupon).");
                    }
                    const auto& obs = cpn->observation();
                    const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
                    const double fwd = forward_from_discount_curve(disc, obs.accrual_start, obs.accrual_end, obs.accrual_dc);
                    amt = cpn->notional() * (fwd + cpn->spread()) * tau;
                    break;
                }
                case ir::instruments::CashflowType::RfrCoupon: {
                    auto cpn = std::dynamic_pointer_cast<ir::instruments::RfrCompoundCoupon>(cfptr);
                    if (!cpn) {
                        return ir::Error::make(ir::ErrorCode::InvalidArgument,
                            "SingleCurve pricer: cashflow type mismatch (RfrCompoundCoupon).");
                    }
                    // v1 approximation: use simple forward from DF over the accrual period
                    const auto& obs = cpn->observation();
                    const double tau = ir::year_fraction(obs.start, obs.end, obs.accrual_dc);
                    const double fwd = forward_from_discount_curve(disc, obs.start, obs.end, obs.accrual_dc);
                    amt = cpn->notional() * (fwd + cpn->spread()) * tau;
                    break;
                }
                case ir::instruments::CashflowType::Fixed:
                default:
                    return ir::Error::make(ir::ErrorCode::InvalidArgument,
                        "SingleCurve pricer: amount unknown for cashflow.");
                }
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
                (cfptr->type() == ir::instruments::CashflowType::IborCoupon) ? "IBOR" : "RFR";
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

            auto known = cfptr->amount_if_known(&fixings);
            double amt = 0.0;

            if (known.has_value()) {
                amt = known.value();
            }
            else {
                switch (cfptr->type()) {
                case ir::instruments::CashflowType::IborCoupon: {
                    if (!ibor_fwd) {
                        return ir::Error::make(ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: IBOR forward curve is null.");
                    }
                    auto cpn = std::dynamic_pointer_cast<ir::instruments::IborCoupon>(cfptr);
                    if (!cpn) {
                        return ir::Error::make(ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: cashflow type mismatch (IborCoupon).");
                    }
                    const auto& obs = cpn->observation();
                    const double tau = ir::year_fraction(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
                    const double fwd = ibor_fwd->forward_rate(obs.accrual_start, obs.accrual_end, obs.accrual_dc);
                    amt = cpn->notional() * (fwd + cpn->spread()) * tau;
                    break;
                }
                case ir::instruments::CashflowType::RfrCoupon: {
                    if (!rfr_fwd) {
                        return ir::Error::make(ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: RFR forward curve is null.");
                    }
                    auto cpn = std::dynamic_pointer_cast<ir::instruments::RfrCompoundCoupon>(cfptr);
                    if (!cpn) {
                        return ir::Error::make(ir::ErrorCode::InvalidArgument,
                            "MultiCurve pricer: cashflow type mismatch (RfrCompoundCoupon).");
                    }
                    // v1 approximation: use forward over accrual period (simple)
                    const auto& obs = cpn->observation();
                    const double tau = ir::year_fraction(obs.start, obs.end, obs.accrual_dc);
                    const double fwd = rfr_fwd->forward_rate(obs.start, obs.end, obs.accrual_dc);
                    amt = cpn->notional() * (fwd + cpn->spread()) * tau;
                    break;
                }
                case ir::instruments::CashflowType::Fixed:
                default:
                    return ir::Error::make(ir::ErrorCode::InvalidArgument,
                        "MultiCurve pricer: amount unknown for cashflow.");
                }
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
                (cfptr->type() == ir::instruments::CashflowType::IborCoupon) ? "IBOR" : "RFR";
            line.leg_id = leg.leg_id;
            out.lines.push_back(std::move(line));
        }

        return out;
    }

    // ------------------------ DiscountingSwapPricer ------------------------

    ir::Result<PricingResult>
        DiscountingSwapPricer::price(const ir::instruments::InterestRateSwap& swap,
            const ir::market::MarketData& md,
            const PricingContext& ctx) const {
        auto discR = get_discount_curve(md, ctx.discount_curve);
        if (!discR.has_value()) return discR.error();
        const auto& disc = *discR.value();

        auto fixedLeg = pv_leg_single_curve(swap.fixed_leg(), disc, *(md.fixings()), ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto floatLeg = pv_leg_single_curve(swap.float_leg(), disc, *(md.fixings()), ctx);
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
        auto discR = get_discount_curve(md, ctx.discount_curve);
        if (!discR.has_value()) return discR.error();
        const auto& disc = *discR.value();

        auto fixedLeg = pv_leg_single_curve(swap.fixed_leg(), disc, *(md.fixings()), ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto rfrLeg = pv_leg_single_curve(swap.rfr_leg(), disc, *(md.fixings()), ctx);
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
        auto discR = get_discount_curve(md, ctx.discount_curve);
        if (!discR.has_value()) return discR.error();
        const auto& disc = *discR.value();

        auto iborR = get_forward_curve(md, ctx.ibor_forward_curve);
        if (!iborR.has_value()) return iborR.error();

        const auto* ibor = iborR.value();

        auto fixedLeg = pv_leg_multi_curve(swap.fixed_leg(), disc, ibor, nullptr, *(md.fixings()), ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto floatLeg = pv_leg_multi_curve(swap.float_leg(), disc, ibor, nullptr, *(md.fixings()), ctx);
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
        auto discR = get_discount_curve(md, ctx.discount_curve);
        if (!discR.has_value()) return discR.error();
        const auto& disc = *discR.value();

        auto rfrR = get_forward_curve(md, ctx.rfr_forward_curve);
        if (!rfrR.has_value()) return rfrR.error();
        const auto* rfr = rfrR.value();

        auto fixedLeg = pv_leg_multi_curve(swap.fixed_leg(), disc, nullptr, rfr, *(md.fixings()), ctx);
        if (!fixedLeg.has_value()) return fixedLeg.error();

        auto rfrLeg = pv_leg_multi_curve(swap.rfr_leg(), disc, nullptr, rfr, *(md.fixings()), ctx);
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