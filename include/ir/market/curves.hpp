#pragma once
#include "ir/core/date.hpp"
#include "ir/core/conventions.hpp"
#include "ir/core/ids.hpp"
#include "ir/core/result.hpp"
#include "ir/utils/interpolation.hpp"
#include "ir/utils/piecewise_nodes.hpp"

namespace ir::market {

	// Base class for term structures
	class Curve {
	public:
		virtual ~Curve() = default;

		ir::Date asof() const { return asof_; }

	protected:
		explicit Curve(const ir::Date& asof) : asof_(asof) {}

		ir::Date asof_;
	};

	class DiscountCurve : public Curve {
	public:
		using Curve::Curve;
		virtual ~DiscountCurve() = default;

		// Main API
		virtual double df(const ir::Date& d) const = 0;

		// Optional convenience (implemented in derived or as helper)
		virtual double df(double t) const = 0; // t in year units used by this curve
	};

	class ForwardCurve : public Curve {
	public:
		using Curve::Curve;
		virtual ~ForwardCurve() = default;

		// Simple forward rate over [start, end] using curve’s internal representation
		virtual double forward_rate(const ir::Date& start,
			const ir::Date& end,
			ir::DayCount dc) const = 0;
	};

	class PiecewiseDiscountCurve final : public DiscountCurve {
	public:
		struct Config {
			ir::DayCount dc{ ir::DayCount::ACT365F };
			ir::Calendar calendar{};
			ir::BusinessDayConvention bdc{ ir::BusinessDayConvention::ModifiedFollowing };
			// For conversion Date->t (year fraction from asof)
		};

		PiecewiseDiscountCurve(const ir::Date& asof, Config cfg);

		// Build/update nodes
		ir::Result<int> set_nodes(ir::utils::Nodes1D nodes_df); // nodes_df.v are DFs

		// DiscountCurve
		double df(const ir::Date& d) const override;
		double df(double t) const override;

		// Accessors (handy for tests/diagnostics)
		const ir::utils::Nodes1D& nodes() const { return nodes_df_; }
		const Config& config() const { return cfg_; }

	private:
		Config cfg_;
		ir::utils::Nodes1D nodes_df_;
		std::unique_ptr<ir::utils::IInterpolator1D> interp_; // log-linear on DF
	};

	class PiecewiseForwardCurve final : public ForwardCurve {
	public:
		struct Config {
			ir::DayCount dc{ ir::DayCount::ACT365F };
		};

		PiecewiseForwardCurve(const ir::Date& asof, Config cfg);

		// Nodes represent pseudo-DFs P_f(t) > 0
		ir::Result<int> set_nodes(ir::utils::Nodes1D nodes_pf);

		// ForwardCurve
		double forward_rate(const ir::Date& start,
			const ir::Date& end,
			ir::DayCount dc) const override;

		// Convenience
		double pf(double t) const;   // pseudo DF

		const ir::utils::Nodes1D& nodes() const { return nodes_pf_; }

	private:
		Config cfg_;
		ir::utils::Nodes1D nodes_pf_;
		std::unique_ptr<ir::utils::IInterpolator1D> interp_; // log-linear on pf
	};

	enum class CurveType { Discount, Forward };

	struct CurveConfig {
		ir::CurveId id{ ir::CurveId{"UNSET"} };
		CurveType type{ CurveType::Discount };

		ir::Date asof{};
		ir::DayCount dc{ ir::DayCount::ACT365F };
		ir::Calendar calendar{};
		ir::BusinessDayConvention bdc{ ir::BusinessDayConvention::ModifiedFollowing };

		// interpolation choice (for now you can hardcode in implementations)
		enum class Interp { Linear, LogLinear };
		Interp interp{ Interp::LogLinear };
	};

} // namespace ir::market
