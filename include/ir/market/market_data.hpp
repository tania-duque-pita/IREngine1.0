#pragma once
#include <memory>
#include <unordered_map>
#include <optional>

#include "ir/core/ids.hpp"
#include "ir/core/result.hpp"
#include "ir/market/quotes.hpp"
#include "ir/market/curves.hpp"


namespace ir::market {

	class MarketData {
	public:
		explicit MarketData(ir::Date asof);

		ir::Date asof() const { return asof_; }

		// Curves
		void set_discount_curve(const ir::CurveId& id, std::shared_ptr<DiscountCurve> c);
		void set_forward_curve(const ir::CurveId& id, std::shared_ptr<ForwardCurve> c);

		const DiscountCurve& discount_curve(const ir::CurveId& id) const;
		const ForwardCurve& forward_curve(const ir::CurveId& id) const;

		// Quotes
		void set_quote(const QuoteId& id, Quote q);
		std::optional<Quote> quote(const QuoteId& id) const;

		// Fixings
		FixingStore& fixings() { return fixings_; }
		const FixingStore& fixings() const { return fixings_; }

	private:
		ir::Date asof_;

		std::unordered_map<std::string, std::shared_ptr<DiscountCurve>> discount_;
		std::unordered_map<std::string, std::shared_ptr<ForwardCurve>> forward_;
		std::unordered_map<QuoteId, Quote> quotes_;

		FixingStore fixings_;
	};

} // namespace ir::market
