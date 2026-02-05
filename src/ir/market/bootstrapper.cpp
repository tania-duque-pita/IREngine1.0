#include "ir/market/bootstrapper.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "ir/core/error.hpp"
#include "ir/utils/piecewise_nodes.hpp"
#include "ir/utils/root_finding.hpp"

namespace ir::market {

    static bool by_maturity_discount(const std::shared_ptr<OisSwapHelper>& a,
        const std::shared_ptr<OisSwapHelper>& b) {
        return a->maturity() < b->maturity();
    }

    static bool by_maturity_any(const std::shared_ptr<RateHelper>& a,
        const std::shared_ptr<RateHelper>& b) {
        return a->maturity() < b->maturity();
    }

    ir::Result<std::shared_ptr<PiecewiseDiscountCurve>>
        CurveBootstrapper::bootstrap_discount_curve(const ir::Date& asof,
            PiecewiseDiscountCurve::Config cfg,
            const std::vector<std::shared_ptr<OisSwapHelper>>& helpers,
            const BootstrapOptions& opts) const {
        if (helpers.empty()) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "bootstrap_discount_curve: helpers is empty.");
        }

        auto sorted = helpers;
        std::sort(sorted.begin(), sorted.end(), by_maturity_discount);

        auto curve = std::make_shared<PiecewiseDiscountCurve>(asof, cfg);

        // Nodes: start with (t=0, df=1)
        ir::utils::Nodes1D nodes;
        {
        auto r0 = nodes.push_back(0.0, 1.0);
            if (!r0.has_value()) return r0.error();
        }

        // Bootstrapping loop
        for (const auto& h : sorted) {
            const double ti = ir::year_fraction(asof, h->maturity(), cfg.dc);
            if (!(ti > 0.0)) {
                return ir::Error::make(ir::ErrorCode::InvalidArgument,
                    "bootstrap_discount_curve: non-positive pillar time.");
            }

            // Add node placeholder (will be solved)
            {
                // Initial guess: slightly decaying
                double guess = std::exp(-0.02 * ti);
                auto rr = nodes.push_back(ti, guess);
                if (!rr.has_value()) return rr.error();
            }

            // Objective f(df_i) = implied_par_rate(df_i) - market_quote
            auto objective = [&](double df_i) -> double {
                ir::utils::Nodes1D trial = nodes;
                trial.v.back() = df_i;

                // Build curve for this trial
                auto s = curve->set_nodes(std::move(trial));
                if (!s.has_value()) return std::numeric_limits<double>::quiet_NaN();

                auto imp = h->implied_par_rate(*curve);
                if (!imp.has_value()) return std::numeric_limits<double>::quiet_NaN();

                return imp.value() - h->market_quote();
                };

            // Solve with brent
            auto sol = ir::utils::brent(objective, opts.df_min, opts.df_max, opts.solver);
            if (!sol.has_value()) {
                return sol.error();
            }

            const double df_star = sol.value().root;
            nodes.v.back() = df_star;

            // Finalize curve nodes at this pillar
            auto ok = curve->set_nodes(nodes);
            if (!ok.has_value()) return ok.error();
        }

        return curve;
    }

    ir::Result<std::shared_ptr<PiecewiseForwardCurve>>
        CurveBootstrapper::bootstrap_forward_curve(const ir::Date& asof,
            PiecewiseForwardCurve::Config cfg,
            const PiecewiseDiscountCurve& discount_curve,
            const std::vector<std::shared_ptr<RateHelper>>& helpers,
            const BootstrapOptions& opts) const {
        if (helpers.empty()) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "bootstrap_forward_curve: helpers is empty.");
        }

        auto sorted = helpers;
        std::sort(sorted.begin(), sorted.end(), by_maturity_any);

        auto fwd = std::make_shared<PiecewiseForwardCurve>(asof, cfg);

        // Nodes for pseudo-discount curve Pf: start at (0,1)
        ir::utils::Nodes1D nodes;
        {
            auto r0 = nodes.push_back(0.0, 1.0);
            if (!r0.has_value()) return r0.error();
        }

        for (const auto& h : sorted) {
            const double ti = ir::year_fraction(asof, h->maturity(), cfg.dc);
            if (!(ti > 0.0)) {
                return ir::Error::make(ir::ErrorCode::InvalidArgument,
                    "bootstrap_forward_curve: non-positive pillar time.");
            }

            // Placeholder node
            {
                double guess = std::exp(-0.02 * ti);
                auto rr = nodes.push_back(ti, guess);
                if (!rr.has_value()) return rr.error();
            }

            // Figure out helper type
            const auto* fra = dynamic_cast<const FraHelper*>(h.get());
            const auto* irs = dynamic_cast<const IrsHelper*>(h.get());
            if (!fra && !irs) {
                return ir::Error::make(ir::ErrorCode::InvalidArgument,
                    "bootstrap_forward_curve: unsupported helper type (not FRA/IRS).");
            }

            auto objective = [&](double pf_i) -> double {
                ir::utils::Nodes1D trial = nodes;
                trial.v.back() = pf_i;

                auto s = fwd->set_nodes(std::move(trial));
                if (!s.has_value()) return std::numeric_limits<double>::quiet_NaN();

                if (fra) {
                    auto imp = fra->implied_fra_rate(*fwd);
                    if (!imp.has_value()) return std::numeric_limits<double>::quiet_NaN();
                    return imp.value() - fra->market_quote();
                }
                else {
                    auto imp = irs->implied_par_rate(discount_curve, *fwd);
                    if (!imp.has_value()) return std::numeric_limits<double>::quiet_NaN();
                    return imp.value() - irs->market_quote();
                }
                };

            auto sol = ir::utils::brent(objective, opts.df_min, opts.df_max, opts.solver);
            if (!sol.has_value()) {
                return sol.error();
            }

            nodes.v.back() = sol.value().root;

            auto ok = fwd->set_nodes(nodes);
            if (!ok.has_value()) return ok.error();
        }

        return fwd;
    }

} // namespace ir::market
