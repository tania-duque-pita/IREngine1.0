#pragma once
#include <vector>

#include "ir/instruments/cashflow.hpp"

namespace ir::instruments {

	enum class PayReceive { Pay, Receive };

	struct Leg {
		PayReceive direction{ PayReceive::Pay };
		std::vector<CashflowPtr> cashflows;
	};

} // namespace ir::instruments