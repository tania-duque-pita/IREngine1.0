#include "ir/io/deal_io.hpp"

#include <stdexcept>

#include "ir/core/error.hpp"
#include "ir/io/csv_io.hpp"

namespace ir::io {

    static ir::Result<ir::Date> parse_date(const std::string& s) {
        auto d = ir::Date::parse_iso(s);
        if (!d.has_value()) return d.error();
        return d.value();
    }

    static ir::Result<double> parse_double(const std::string& s) {
        try {
            if (s.empty()) return 0.0;
            return std::stod(s);
        }
        catch (...) {
            return ir::Error::make(ir::ErrorCode::ParseError, "Failed to parse double: " + s);
        }
    }

    static ir::Result<LegType> parse_leg_type(const std::string& s) {
        if (s == "FIXED") return LegType::Fixed;
        if (s == "IBOR")  return LegType::Ibor;
        if (s == "RFR")   return LegType::Rfr;
        return ir::Error::make(ir::ErrorCode::ParseError, "Unknown LegType: " + s);
    }

    static ir::Result<PayReceive> parse_dir(const std::string& s) {
        if (s == "PAY") return PayReceive::Pay;
        if (s == "RECEIVE") return PayReceive::Receive;
        return ir::Error::make(ir::ErrorCode::ParseError, "Unknown PayReceive: " + s);
    }

    static ir::Result<ir::DayCount> parse_dc(const std::string& s) {
        if (s == "ACT360") return ir::DayCount::ACT360;
        if (s == "ACT365") return ir::DayCount::ACT365;
        return ir::Error::make(ir::ErrorCode::ParseError, "Unknown Day_Convention: " + s);
    }

    ir::Result<DealSpec> read_deal_data_csv(const std::string& path) {
        auto t = ir::io::read_csv_file(path);
        if (!t.has_value()) return t.error();

        DealSpec deal;

        for (const auto& r : t.value().rows) {
            LegSpec s;

            s.leg_id = r.at("Leg_ID");

            auto lt = parse_leg_type(r.at("LegType"));
            if (!lt.has_value()) return lt.error();
            s.type = lt.value();

            auto dr = parse_dir(r.at("PayReceive"));
            if (!dr.has_value()) return dr.error();
            s.dir = dr.value();

            auto n = parse_double(r.at("Notional")); if (!n.has_value()) return n.error(); s.notional = n.value();
            auto fr = parse_double(r.at("FixedRate")); if (!fr.has_value()) return fr.error(); s.fixed_rate = fr.value();
            auto sp = parse_double(r.at("Spread")); if (!sp.has_value()) return sp.error(); s.spread = sp.value();

            s.index = ir::IndexId{ r.at("IndexId") };
            s.discount_curve = ir::CurveId{ r.at("DiscountCurveID") };
            s.fwd_curve = ir::CurveId{ r.at("FwdCurveID") };
            s.fixings_id = r.at("FixingsID");

            auto cob = parse_date(r.at("COB")); if (!cob.has_value()) return cob.error(); s.cob = cob.value();
            auto st = parse_date(r.at("Start_Date")); if (!st.has_value()) return st.error(); s.start = st.value();
            auto en = parse_date(r.at("End_Date")); if (!en.has_value()) return en.error(); s.end = en.value();

            s.frequency = r.at("Frequency");
            if (s.frequency.empty()) {
                return ir::Error::make(ir::ErrorCode::ParseError, "Frequency is mandatory.");
            }

            auto dc = parse_dc(r.at("Day_Convention"));
            if (!dc.has_value()) return dc.error();
            s.dc = dc.value();

            // Basic validation
            if (s.notional == 0.0) {
                return ir::Error::make(ir::ErrorCode::InvalidArgument, "Notional cannot be 0 for leg: " + s.leg_id);
            }
            if (!(s.start < s.end)) {
                return ir::Error::make(ir::ErrorCode::InvalidArgument, "Start_Date must be < End_Date for leg: " + s.leg_id);
            }

            deal.legs.push_back(std::move(s));
        }

        return deal;
    }

} // namespace ir::io