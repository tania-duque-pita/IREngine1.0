#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace ir::utils {

	inline constexpr double kEps = 1e-12;

	inline bool approx_equal(double a, double b, double tol = 1e-10) {
		return std::fabs(a - b) <= tol * (1.0 + std::max(std::fabs(a), std::fabs(b)));
	}

	inline int sign(double x) {
		return (x > 0) - (x < 0);
	}

	template <class T>
	inline T clamp(T x, T lo, T hi) {
		return std::max(lo, std::min(x, hi));
	}

}
