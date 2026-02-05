#include "ir/utils/piecewise_nodes.hpp"

#include <cmath>
#include "ir/core/error.hpp"
#include "ir/utils/math.hpp"

namespace ir::utils {

    Result<int> validate_nodes(const Nodes1D& n) {
        if (n.t.size() != n.v.size()) {
            return Error::make(ErrorCode::InvalidArgument, "Nodes1D: t and v sizes differ.");
        }
        for (std::size_t i = 0; i < n.t.size(); ++i) {
            if (!std::isfinite(n.t[i]) || !std::isfinite(n.v[i])) {
                return Error::make(ErrorCode::InvalidArgument, "Nodes1D: non-finite t or v.");
            }
            if (i > 0 && !(n.t[i] > n.t[i - 1])) {
                return Error::make(ErrorCode::InvalidArgument, "Nodes1D: t must be strictly increasing.");
            }
        }
        return Result<int>(0); //ok
    }

    Result<int> Nodes1D::push_back(double ti, double vi) {
        if (!std::isfinite(ti) || !std::isfinite(vi)) {
            return Error::make(ErrorCode::InvalidArgument, "Nodes1D::push_back: non-finite input.");
        }
        if (!t.empty() && !(ti > t.back())) {
            return Error::make(ErrorCode::InvalidArgument, "Nodes1D::push_back: ti must be > last t.");
        }
        t.push_back(ti);
        v.push_back(vi);
        return Result<int>(0); //ok
    }

    Result<int> Nodes1D::set_last_value(double vi) {
        if (v.empty()) {
            return Error::make(ErrorCode::InvalidArgument, "Nodes1D::set_last_value: no nodes.");
        }
        if (!std::isfinite(vi)) {
            return Error::make(ErrorCode::InvalidArgument, "Nodes1D::set_last_value: non-finite value.");
        }
        v.back() = vi;
        return Result<int>(0); //ok
    }

} // namespace ir::utils
