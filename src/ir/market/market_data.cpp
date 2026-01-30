#include "ir/market/market_data.hpp"

#include <stdexcept>
#include <utility>

namespace ir::market {

    MarketData::MarketData(ir::Date asof)
        : asof_(asof) {
    }

    // -------------------- Curves --------------------

    void MarketData::set_discount_curve(const ir::CurveId& id,
        std::shared_ptr<DiscountCurve> c) {
        if (!c) {
            throw std::invalid_argument("MarketData::set_discount_curve: null curve pointer.");
        }
        discount_[id.value] = std::move(c);
    }

    void MarketData::set_forward_curve(const ir::CurveId& id,
        std::shared_ptr<ForwardCurve> c) {
        if (!c) {
            throw std::invalid_argument("MarketData::set_forward_curve: null curve pointer.");
        }
        forward_[id.value] = std::move(c);
    }

    const DiscountCurve& MarketData::discount_curve(const ir::CurveId& id) const {
        auto it = discount_.find(id.value);
        if (it == discount_.end() || !it->second) {
            throw std::out_of_range("MarketData::discount_curve: curve id not found: " + id.value);
        }
        return *(it->second);
    }

    const ForwardCurve& MarketData::forward_curve(const ir::CurveId& id) const {
        auto it = forward_.find(id.value);
        if (it == forward_.end() || !it->second) {
            throw std::out_of_range("MarketData::forward_curve: curve id not found: " + id.value);
        }
        return *(it->second);
    }

    // -------------------- Quotes --------------------

    void MarketData::set_quote(const QuoteId& id, Quote q) {
        quotes_[id] = q; // insert/overwrite
    }

    std::optional<Quote> MarketData::quote(const QuoteId& id) const {
        auto it = quotes_.find(id);
        if (it == quotes_.end()) return std::nullopt;
        return it->second;
    }

} // namespace ir::market
