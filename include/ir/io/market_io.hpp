#pragma once
#include <string>

#include "ir/core/date.hpp"
#include "ir/core/result.hpp"
#include "ir/core/ids.hpp"
#include "ir/market/market_data.hpp"
#include "ir/market/curves.hpp"

namespace ir::io {

    // Local file mode (folder path)
    ir::Result<int> load_discount_curve_nodes_csv(const std::string& path,
        const ir::CurveId& curve_id,
        ir::market::MarketData& md);

    ir::Result<int> load_forward_curve_nodes_csv(const std::string& path,
        const ir::CurveId& curve_id,
        ir::market::MarketData& md);

    ir::Result<int> load_fixings_csv(const std::string& path,
        const ir::IndexId& index,
        ir::market::FixingStore& store);

} // namespace ir::io