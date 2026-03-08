#pragma once
#include <memory>

#include "ir/core/date.hpp"
#include "ir/market/quotes.hpp"   // FixingStore lives here in your repo

namespace ir::instruments {

	enum class CashflowType {
		Fixed,          // deterministic amount
		IborCoupon,     // needs IBOR fixing/projection
		RfrCoupon       // needs RFR daily comp/projection
	};

	class Cashflow {
	public:
		virtual ~Cashflow() = default;

		virtual CashflowType type() const = 0;
		virtual ir::Date pay_date() const = 0;

		// If the cashflow can be fully determined from fixings, return amount.
		// If not (needs projection), return std::nullopt.
		virtual std::optional<double> amount_if_known(const ir::market::FixingStore* fixings) const = 0;
	};

	using CashflowPtr = std::shared_ptr<Cashflow>;

} // namespace ir::instruments