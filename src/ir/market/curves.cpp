#include "ir/market/curves.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "ir/utils/interpolation.hpp"
#include "ir/utils/piecewise_nodes.hpp"

namespace ir::market {

    // =============================
    // PiecewiseDiscountCurve
    // =============================

    PiecewiseDiscountCurve::PiecewiseDiscountCurve(const ir::Date& asof, Config cfg)
        : DiscountCurve(asof), cfg_(std::move(cfg)) {
    }

    ir::Result<int> PiecewiseDiscountCurve::set_nodes(ir::utils::Nodes1D nodes_df) {
        // Build interpolator on DF
        ir::utils::Interp1DData data;
        data.x = nodes_df.t;
        data.y = nodes_df.v;

        // validate_xy returns Result<void>, but LogLinearInterpolator ctor may also validate.
        auto vxy = ir::utils::validate_xy(data);
        if (!vxy.has_value()) return vxy;

        nodes_df_ = std::move(nodes_df);
        interp_ = std::make_unique<ir::utils::LogLinearInterpolator>(
            std::move(data)
        );

        return 0;
    }

    double PiecewiseDiscountCurve::df(const ir::Date& d) const {
        const double t = ir::year_fraction(asof_, d, cfg_.dc);
        return df(t);
    }

    double PiecewiseDiscountCurve::df(double t) const {
        // Convention: df(0)=1.0
        if (t <= 0.0) return 1.0;

        if (!interp_) {
            // If called before set_nodes, this is a programmer error.
            throw std::runtime_error("PiecewiseDiscountCurve::df: curve has no nodes/interpolator.");
        }

        const double out = (*interp_)(t);
        // Safety: should remain positive due to log-linear
        return out;
    }

    // =============================
    // PiecewiseForwardCurve (pseudo-discount curve)
    // =============================

    PiecewiseForwardCurve::PiecewiseForwardCurve(const ir::Date& asof, Config cfg)
        : ForwardCurve(asof), cfg_(std::move(cfg)) {
    }

    ir::Result<int> PiecewiseForwardCurve::set_nodes(ir::utils::Nodes1D nodes_pf) {
        ir::utils::Interp1DData data;
        data.x = nodes_pf.t;
        data.y = nodes_pf.v;

        auto vxy = ir::utils::validate_xy(data);
        if (!vxy.has_value()) return vxy;

        nodes_pf_ = std::move(nodes_pf);
        interp_ = std::make_unique<ir::utils::LogLinearInterpolator>(
            std::move(data));

        return 0;
    }

    double PiecewiseForwardCurve::pf(double t) const {
        if (t <= 0.0) return 1.0;

        if (!interp_) {
            throw std::runtime_error("PiecewiseForwardCurve::pf: curve has no nodes/interpolator.");
        }
        return (*interp_)(t);
    }

    double PiecewiseForwardCurve::forward_rate(const ir::Date& start,
        const ir::Date& end,
        ir::DayCount dc) const {
        // Compute forward over [start,end] via pseudo DFs:
        // F = (P_f(t1)/P_f(t2) - 1) / tau
        const double t1 = ir::year_fraction(asof_, start, cfg_.dc);
        const double t2 = ir::year_fraction(asof_, end, cfg_.dc);

        const double tau = ir::year_fraction(start, end, dc);
        if (tau <= 0.0) {
            // You can throw or return 0; throwing is safer to detect bugs.
            throw std::runtime_error("PiecewiseForwardCurve::forward_rate: non-positive accrual tau.");
        }

        const double p1 = pf(t1);
        const double p2 = pf(t2);

        return (p1 / p2 - 1.0) / tau;
    }

} // namespace ir::market
