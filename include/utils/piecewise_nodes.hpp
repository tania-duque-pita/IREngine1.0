#pragma once
#include <vector>
#include <cstddef>
#include "core/types.hpp"
#include "core/result.hpp"

namespace ir::utils {

	// Represents piecewise-defined curve nodes at times t_i.
	struct Nodes1D {
		std::vector<double> t;   // increasing
		std::vector<double> v;   // node values (e.g., DF or zero rate)

		// std::size_t size() const { return t.size(); }
		// bool empty() const { return t.empty(); }

		// void clear() { t.clear(); v.clear(); }

		// Append a node; expects monotonic t.
		Result<int> push_back(double ti, double vi);

		// Replace last value (common during solve iterations).
		Result<int> set_last_value(double vi);
	};

	Result<int> validate_nodes(const Nodes1D& n);

} // namespace ir::utils

