#pragma once
#include <vector>

#include "ir/instruments/cashflow.hpp"

namespace ir::instruments {

	enum class PayReceive { Pay, Receive };

	struct Leg {
		PayReceive direction{ PayReceive::Pay };
		std::vector<CashflowPtr> cashflows;
		std::string leg_id{};
	};

} // namespace ir::instruments