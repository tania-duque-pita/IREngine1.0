#pragma once
#include <string>
#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"
#include "ir/core/ids.hpp"
#include "ir/core/result.hpp"

namespace ir::io {

	enum class LegType { Fixed, Ibor, Rfr };
	enum class PayReceive { Pay, Receive };

	struct LegSpec {
		std::string leg_id;
		LegType type{ LegType::Fixed };
		PayReceive dir{ PayReceive::Pay };

		double notional{ 0.0 };
		double fixed_rate{ 0.0 };     // only for FIXED
		double spread{ 0.0 };         // float spread

		ir::IndexId index{ ir::IndexId{"UNSET"} };     // for float legs
		ir::CurveId discount_curve{ ir::CurveId{"DISCOUNT"} };
		ir::CurveId fwd_curve{ ir::CurveId{"FWD"} };   // for float legs
		std::string fixings_id;                      // optional id like "fixings_1"

		ir::Date cob{};
		ir::Date start{};
		ir::Date end{};

		std::string frequency;       // "3M" "6M" "1Y"
		ir::DayCount dc{ ir::DayCount::ACT365 };
	};

	struct DealSpec {
		std::vector<LegSpec> legs;
	};

	ir::Result<DealSpec> read_deal_data_csv(const std::string& path);

} // namespace ir::io