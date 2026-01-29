#pragma once
#include <vector>
#include <stdexcept>
//#include "core/types.hpp"
#include "../core/result.hpp"
#include "../core/types.hpp"

namespace ir::utils {

    enum class InterpType {
        Flat,        // constant beyond ends
        Linear,       // linear interpolation
        LogLinear      // 
    };

    struct Interp1DData {
        std::vector<double> x;  // strictly increasing
        std::vector<double> y;  // same size as x
    };

    class IInterpolator1D {
    public:
        virtual ~IInterpolator1D() = default;

        virtual double value(double x) const = 0;
        double operator()(double x) const { return value(x); }

    protected:
        std::vector<double> xs_;
        std::vector<double> ys_;
        InterpType ex_;

        //// Optional: for diagnostics / risk
        //virtual double left_endpoint() const = 0;
        //virtual double right_endpoint() const = 0;
    };

    // ------------------------- Linear -------------------------
    class LinearInterpolator : public IInterpolator1D {
    public:
        LinearInterpolator(Interp1DData data);

        double value(double x) const override;
        //double left_endpoint() const override { return xs_.front(); }
        //double right_endpoint() const override { return xs_.back(); }

    };

    // ---------------------- Log-Linear ------------------------
    // Interpolate y in log-space: y(x) = exp( linear_interp( log(y) ) )
    // Use for discount factors to preserve positivity.
    class LogLinearInterpolator : public IInterpolator1D {
    public:
        // requires y_i > 0
        LogLinearInterpolator(Interp1DData data);

        double value(double x) const override;
        //double left_endpoint() const override { return xs_.front(); }
        //double right_endpoint() const override { return xs_.back(); }

    private:
        std::vector<double> log_ys_;
    };

    // Validation helper (used in constructors)
    Result<int> validate_xy(const Interp1DData& data);

} // namespace ir::utils
