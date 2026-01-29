#include "utils/interpolation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/error.hpp"
#include "utils/math.hpp"

namespace ir::utils {

    Result<int> validate_xy(const Interp1DData& data) {
        if (data.x.size() != data.y.size()) {
            return Error::make(ErrorCode::InvalidArgument, "Interpolation: x and y sizes differ.");
        }
        if (data.x.size() < 2) {
            return Error::make(ErrorCode::InvalidArgument, "Interpolation: need at least 2 points.");
        }
        for (std::size_t i = 0; i < data.x.size(); ++i) {
            if (!std::isfinite(data.x[i]) || !std::isfinite(data.y[i])) {
                return Error::make(ErrorCode::InvalidArgument, "Interpolation: non-finite x/y.");
            }
            if (i > 0 && !(data.x[i] > data.x[i - 1])) {
                return Error::make(ErrorCode::InvalidArgument, "Interpolation: x must be strictly increasing.");
            }
        } // ok


        return Result<int>(0);
    }

    // -------- LinearInterpolator --------//

    LinearInterpolator::LinearInterpolator(Interp1DData data)
        {
        ex_ = InterpType::Linear;

        auto ok = validate_xy(data);
        if (!ok.has_value()) {
            // Keep ctor noexcept-free: throw or store error? Minimal: throw.
            // If you prefer Result-returning factory, change design accordingly.
            throw std::runtime_error(ok.error().message);
        }
        xs_ = std::move(data.x);
        ys_ = std::move(data.y);
    }

    double LinearInterpolator::value(double x) const {
        // Flat extrapolation if we are outside of domain
        if (x <= xs_.front()) return ys_.front();
        if (x >= xs_.back())  return ys_.back();

        std::vector<double>::const_iterator it = std::lower_bound(xs_.begin(), xs_.end(), x);
        it--;

        std::vector<double>::const_iterator it_y = ys_.begin() + (it - xs_.begin());

        double x0{ *it };
        double y0{ *it_y };
        it++;
        it_y++;
        double x1{ *it };
        double y1{ *it_y };


        const double w = ((x - x0) / (x1 - x0));
        return y0 + w * (y1 - y0);

    }

    // -------- LogLinearInterpolator --------

    LogLinearInterpolator::LogLinearInterpolator(Interp1DData data)
        {
        ex_ = InterpType::LogLinear;

        auto ok = validate_xy(data);
        if (!ok.has_value()) {
            throw std::runtime_error(ok.error().message);
        }
        for (double yi : data.y) {
            if (!(yi > 0.0)) {
                throw std::runtime_error("LogLinearInterpolation: y must be > 0.");
            }
        }

        xs_ = std::move(data.x);
        log_ys_.reserve(data.y.size());
        for (double yi : data.y) log_ys_.push_back(std::log(yi));
    }

    double LogLinearInterpolator::value(double x) const {
        // Flat extrapolation
        if (x <= xs_.front()) return std::exp(log_ys_.front());
        if (x >= xs_.back())  return std::exp(log_ys_.back());

        std::vector<double>::const_iterator it = std::lower_bound(xs_.begin(), xs_.end(), x);
        it--;

        std::vector<double>::const_iterator it_y = log_ys_.begin() + (it - xs_.begin());

        double x0{ *it };
        double ly0{ *it_y };
        it++;
        it_y++;
        double x1{ *it };
        double ly1{ *it_y };

        const double w = static_cast<double>((x - x0) / (x1 - x0));
        const double ly = ly0 + w * (ly1 - ly0);
        return std::exp(ly);
    }

} // namespace ir::utils
