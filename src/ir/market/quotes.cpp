#include "ir/market/quotes.hpp"

#include <string>

namespace ir::market {

	std::string FixingStore::key(const ir::IndexId& index, const ir::Date& d) {
		// Deterministic, easy-to-debug key.
		// Requires Date::to_iso() in your core/date.hpp
		return index.value + "|" + d.to_iso();
	}

	void FixingStore::add(const ir::IndexId& index, const ir::Date& d, double fixing) {
		fixings_[key(index, d)] = fixing;
	}

	std::optional<double> FixingStore::get(const ir::IndexId& index, const ir::Date& d) const {
		const auto k = key(index, d);
		const auto it = fixings_.find(k);
		if (it == fixings_.end()) return std::nullopt;
		return it->second;
	}

} // namespace ir::market
