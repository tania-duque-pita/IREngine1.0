#pragma once
#include <memory>
#include <vector>

#include "ir/core/date.hpp"
#include "ir/core/result.hpp"
#include "ir/core/conventions.hpp"
#include "ir/core/ids.hpp"

#include "ir/market/market_data.hpp"
#include "ir/market/curves.hpp"

namespace ir::market {

    // For bootstrapping, we typically solve for the last node value.
    // So each helper must be able to compute implied quote using the curves provided.
    class RateHelper {
    public:
        virtual ~RateHelper() = default;
        virtual ir::Date maturity() const = 0;
        virtual double market_quote() const = 0;
    };

    // ---------- OIS helper (discount curve bootstrap) ----------
    class OisSwapHelper : public RateHelper {
    public:
        struct Config {
            ir::DayCount fixed_dc{ ir::DayCount::ACT360 };
            ir::Frequency fixed_freq{ ir::Frequency::Annual };
            ir::BusinessDayConvention bdc{ ir::BusinessDayConvention::ModifiedFollowing };
            ir::Calendar calendar{};
        };

        OisSwapHelper(ir::Date start,
            ir::Date end,
            double par_rate,
            Config cfg) : start_(start), end_(end), par_rate_(par_rate), cfg_(std::move(cfg)) 
        {
        }

        ir::Date maturity() const override { return end_; }
        double market_quote() const override { return par_rate_; }

        // Implied par rate given a candidate discount curve
        ir::Result<double> implied_par_rate(const PiecewiseDiscountCurve& disc) const;

    private:
        ir::Date start_;
        ir::Date end_;
        double par_rate_;
        Config cfg_;
    };

    // ---------- FRA helper (forward curve bootstrap) ----------
    class FraHelper : public RateHelper {
    public:
        struct Config {
            ir::DayCount dc{ ir::DayCount::ACT360 };
        };

        FraHelper(ir::Date start,
            ir::Date end,
            double fra_rate,
            Config cfg) 
            : start_(start), end_(end), par_fra_rate_(fra_rate), cfg_(std::move(cfg)) 
        {
        }

        ir::Date maturity() const override { return end_; }
        double market_quote() const override { return par_fra_rate_; }

        // Implied FRA rate given forward curve (pseudo-DF) and discount curve (for PV consistency if needed)
        ir::Result<double> implied_fra_rate(const PiecewiseForwardCurve& fwd) const;

    private:
        ir::Date start_;
        ir::Date end_;
        double par_fra_rate_;
        Config cfg_;
    };

    // ---------- IRS helper (forward curve bootstrap using fixed discount curve) ----------
    class IrsHelper final : public RateHelper {
    public:
        struct Config {
            ir::DayCount fixed_dc{ ir::DayCount::ACT365 };
            ir::Frequency fixed_freq{ ir::Frequency::Annual };
            ir::DayCount float_dc{ ir::DayCount::ACT360 };
            ir::Frequency float_freq{ ir::Frequency::Quarterly };
            ir::BusinessDayConvention bdc{ ir::BusinessDayConvention::ModifiedFollowing };
            ir::Calendar calendar{};
        };

        IrsHelper(ir::Date start,
            ir::Date end,
            double par_rate,
            Config cfg) 
            : start_(start), end_(end), par_rate_(par_rate), cfg_(std::move(cfg)) {
        }

        ir::Date maturity() const override { return end_; }
        double market_quote() const override { return par_rate_; }

        // Implied par rate given fixed discount + candidate forward curve
        ir::Result<double> implied_par_rate(const PiecewiseDiscountCurve& disc,
            const PiecewiseForwardCurve& fwd) const;

    private:
        ir::Date start_;
        ir::Date end_;
        double par_rate_;
        Config cfg_;
    };

} // namespace ir::market
