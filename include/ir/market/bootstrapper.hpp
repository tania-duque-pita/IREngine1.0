#pragma once
#include <memory>
#include <vector>

#include "ir/core/result.hpp"
#include "ir/core/date.hpp"
#include "ir/market/curves.hpp"
#include "ir/market/rate_helpers.hpp"
#include "ir/utils/root_finding.hpp"

namespace ir::market {

    struct BootstrapOptions {
        ir::utils::RootFindOptions solver{};
        double df_min = 1e-8;      // bracket lower bound for DF
        double df_max = 1.0;       // bracket upper bound for DF
    };

    class CurveBootstrapper {
    public:
        // Discount curve from OIS helpers
        ir::Result<std::shared_ptr<PiecewiseDiscountCurve>>
            bootstrap_discount_curve(const ir::Date& asof,
                PiecewiseDiscountCurve::Config cfg,
                const std::vector<std::shared_ptr<OisSwapHelper>>& helpers,
                const BootstrapOptions& opts = {}) const;

        // Forward curve from FRA/IRS helpers, given discount curve
        ir::Result<std::shared_ptr<PiecewiseForwardCurve>>
            bootstrap_forward_curve(const ir::Date& asof,
                PiecewiseForwardCurve::Config cfg,
                const PiecewiseDiscountCurve& discount_curve,
                const std::vector<std::shared_ptr<RateHelper>>& helpers,
                const BootstrapOptions& opts = {}) const;
    };

} // namespace ir::market
