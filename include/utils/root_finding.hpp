#pragma once
#include <functional>
#include "core/result.hpp"
#include "core/types.hpp"

namespace ir::utils {

	struct RootFindOptions {
		int max_iter = 100;
		double tol_abs = 1e-12;
		double tol_rel = 1e-10;
	};

	struct RootFindReport {
		int iterations = 0;
		double f_at_root = 0.0;
		bool converged = false;
	};

	struct RootFindResult {
		double root = 0.0;
		RootFindReport report;
	};

	// Brent's method on [a,b] with f(a)*f(b) <= 0.
	Result<RootFindResult> brent(
		const std::function<double(double)>& f,
		double a,
		double b,
		const RootFindOptions& opts = {}
	);

} // namespace ir::utils
