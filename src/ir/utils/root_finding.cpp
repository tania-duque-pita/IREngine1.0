#include "ir/utils/root_finding.hpp"

#include <cmath>
#include <algorithm>
#include "ir/core/error.hpp"
#include "ir/utils/math.hpp"
#include "ir/core/result.hpp"

namespace ir::utils {

    static bool close_enough(double a, double b, const RootFindOptions& opts) {
        const double tol = std::max(opts.tol_abs, opts.tol_rel * std::max(std::fabs(a), std::fabs(b)));
        return std::fabs(a - b) <= tol;
    }

    Result<RootFindResult> brent(
        const std::function<double(double)>& f,
        double a,
        double b,
        const RootFindOptions& opts
    ) {
        if (!(a < b)) {
            return Error::make(ErrorCode::InvalidArgument, "brent: require a < b.");
        }

        double fa = f(a);
        double fb = f(b);
        if (!std::isfinite(fa) || !std::isfinite(fb)) {
            return Error::make(ErrorCode::InvalidArgument, "brent: f(a) or f(b) non-finite.");
        }
        if (fa == 0.0) return RootFindResult{ a, RootFindReport{0, fa, true} };
        if (fb == 0.0) return RootFindResult{ b, RootFindReport{0, fb, true} };

        if (fa * fb > 0.0) {
            return Error::make(ErrorCode::InvalidArgument, "brent: root not bracketed (f(a)*f(b) > 0).");
        }

        double c = a;
        double fc = fa;

        double d = b - a;
        double e = d;

        RootFindReport rep{ 0, 0.0, false };

        for (int iter = 1; iter <= opts.max_iter; ++iter) {
            // Ensure |fb| <= |fc|
            if (std::fabs(fc) < std::fabs(fb)) {
                a = b;  b = c;  c = a;
                fa = fb; fb = fc; fc = fa;
            }

            const double tol = std::max(opts.tol_abs, opts.tol_rel * std::fabs(b));
            const double m = 0.5 * (c - b);

            if (std::fabs(m) <= tol || fb == 0.0) {
                rep.iterations = iter;
                rep.f_at_root = fb;
                rep.converged = true;
                return RootFindResult{ b, rep };
            }

            double p = 0.0, q = 1.0;
            bool use_interp = false;

            if (std::fabs(e) > tol && std::fabs(fa) > std::fabs(fb)) {
                // Attempt interpolation
                use_interp = true;
                double s = fb / fa;

                if (a == c) {
                    // Secant
                    p = 2.0 * m * s;
                    q = 1.0 - s;
                }
                else {
                    // Inverse quadratic interpolation
                    double r = fb / fc;
                    double t = fa / fc;
                    p = s * (2.0 * m * t * (t - r) - (b - a) * (r - 1.0));
                    q = (t - 1.0) * (r - 1.0) * (s - 1.0);
                }

                if (p > 0.0) q = -q;
                p = std::fabs(p);

                // Check acceptability
                const double min1 = 3.0 * m * q - std::fabs(tol * q);
                const double min2 = std::fabs(e * q);

                if (!(2.0 * p < std::min(min1, min2))) {
                    use_interp = false;
                }
            }

            if (!use_interp) {
                // Bisection
                d = m;
                e = m;
            }
            else {
                e = d;
                d = p / q;
            }

            a = b;
            fa = fb;

            if (std::fabs(d) > tol) b += d;
            else b += (m > 0 ? tol : -tol);

            fb = f(b);
            if (!std::isfinite(fb)) {
                return Error::make(ErrorCode::InvalidArgument, "brent: f(x) became non-finite.");
            }

            // Maintain bracketing
            if ((fb > 0 && fc > 0) || (fb < 0 && fc < 0)) {
                c = a;
                fc = fa;
                e = d = b - a;
            }
        }

        rep.iterations = opts.max_iter;
        rep.f_at_root = fb;
        rep.converged = false;
        return RootFindResult{ b, rep }; // return best effort (or return Error if you prefer)
    }

} // namespace ir::utils
