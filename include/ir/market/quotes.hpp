#pragma once
#include <string>
#include <utility>
#include <optional>
#include <unordered_map>

#include "ir/core/date.hpp"
#include "ir/core/ids.hpp"   // IndexId
#include "ir/core/result.hpp"


namespace ir::market {

	using QuoteId = std::string;

	enum class QuoteType { Rate, Spread, Price, Vol };

	struct Quote {
		QuoteType type{ QuoteType::Rate };
		double value{ 0.0 };
	};

	class FixingStore {
	public:
		// Adds/overwrites fixing
		void add(const ir::IndexId& index, const ir::Date& d, double fixing);

		// Returns empty if missing
		std::optional<double> get(const ir::IndexId& index, const ir::Date& d) const;

	private:
		// Key = index.value + "|" + YYYY-MM-DD (simple, deterministic)
		static std::string key(const ir::IndexId& index, const ir::Date& d);

		std::unordered_map<std::string, double> fixings_;
	};

} // namespace ir::market
