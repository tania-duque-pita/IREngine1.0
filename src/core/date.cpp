#include "core/date.hpp"
#include "core/error.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

namespace ir {

    // Operator overloading
    Date operator+(const Date& d, std::chrono::days dd) { return Date(d.raw() + dd); }
    bool operator<(const Date& a, const Date& b) { return a.raw() < b.raw(); };
    std::chrono::days operator-(const Date& a, const Date& b) { return static_cast<std::chrono::days>(a.raw() - b.raw()); }


    // Minimal stub for parse_iso; leave unimplemented or implement later.
    Result<Date> Date::parse_iso(const std::string_view& iso)
    {
        std::string segment;
        std::vector<int> seglist{};
        Error e;


        std::stringstream ss(static_cast<std::string>(iso));
        while (std::getline(ss, segment, '-'))
        {
            try {
                seglist.push_back(std::stoi(segment));
            }
            catch (...) {
                e.code = ErrorCode::ParseError;
                e.message = "Non-numeric date segment";
                return e;
            }
        }

        if (seglist.size()!=3 ||
            (seglist[0] < 0 || seglist[1] < 1 || seglist[1]>12 || seglist[2] < 1 || seglist[2]>31))
        {
            e.code = ErrorCode::InvalidDate;
            e.message = "The date does not follow format 'YYYY-mm-dd'";
            return e;
        }

        return Date{ std::chrono::year{seglist[0]} / seglist[1] / seglist[2] };
    }



    int Date::year() const
    {
        std::chrono::year_month_day ymd{ d_ };
        return static_cast<int>(ymd.year());
    }

    unsigned Date::month() const
    {
        std::chrono::year_month_day ymd{ d_ };
        return static_cast<unsigned>(ymd.month());
    }

    unsigned Date::day() const
    {
        std::chrono::year_month_day ymd{ d_ };
        return static_cast<unsigned>(ymd.day());
    }


    // Tenor

    Result<Tenor> Tenor::parse(std::string_view s)
    {
        Error e;

        std::size_t loc_unit;
        char s_unit;
        int n_amount;
        size_t s_size = s.size();
        Tenor t;

        if (s_size < 2)
        {
            e.code = ErrorCode::ParseError;
            e.message = "Tenor string too short.";
            return e;
        }

        loc_unit = s.find_first_of("dDwWmMyY");

        if (loc_unit == std::string::npos|| loc_unit==0)
        {
            e.code = ErrorCode::ParseError;
            e.message = "Tenor string does not consist of numeric tenor amount and tenor unit (D/W/M/Y)";
            return e;
        }

        try{
            n_amount = std::stoi(static_cast<std::string>(s.substr(0, loc_unit)));
        }
        catch (...) {
            e.code = ErrorCode::ParseError;
            e.message = "Tenor string does not consist of numeric tenor amount and tenor unit (D/W/M/Y)";
            return e;
        }
        
        s_unit = s[s_size-1];

        //Setting Tenor amount
        t.n = n_amount;

        //Setting Tenor unit
        switch (s_unit) {
        case 'D':
        case 'd':
            t.unit = TenorUnit::Days;
            break;
        case 'W':
        case 'w':
            t.unit = TenorUnit::Weeks;
            break;
        case 'M':
            t.unit = TenorUnit::Months;
            break;
        case 'Y':
        case 'y':
            t.unit = TenorUnit::Years;
            break;
        default:
            e.code = ErrorCode::ParseError;
            e.message = "Unknown tenor unit (expected D,W,M,Y)";
            return e;
        }

        return t;
    }


    // Calendar implementation (v1: weekends-only)
    bool Calendar::is_business_day(const Date& d) const
    {
        return !is_weekend(d);
    }

    Date Calendar::adjust(const Date& d, BusinessDayConvention bdc) const
    {
        if (is_business_day(d)) return d;

        using namespace std::chrono;

        if (bdc == BusinessDayConvention::Following || bdc == BusinessDayConvention::ModifiedFollowing) {
            Date cur = d;
            while (!is_business_day(cur)) {
                cur = Date{ cur.raw() + days{1} };
            }

            if (bdc == BusinessDayConvention::ModifiedFollowing) {
                year_month_day orig{ d.raw() };
                year_month_day adj{ cur.raw() };
                if (adj.month() != orig.month()) {
                    // Fall back to preceding
                    Date cur2 = d;
                    while (!is_business_day(cur2)) {
                        cur2 = Date{ cur2.raw() - days{1} };
                    }
                    return cur2;
                }
            }
            return cur;
        }
        else { // Preceding
            Date cur = d;
            while (!is_business_day(cur)) {
                cur = Date{ cur.raw() - std::chrono::days{1} };
            }
            return cur;
        }
    }


    Date Calendar::advance(const Date& d, const Tenor& t, BusinessDayConvention bdc) const
    {
        using namespace std::chrono;

        sys_days sd = d.raw();
        sys_days new_sd = sd;

        switch (t.unit) {
        case TenorUnit::Days:
            new_sd = sd + days{ t.n };
            break;
        case TenorUnit::Weeks:
            new_sd = sd + days{ 7 * t.n };
            break;
        case TenorUnit::Months: {
            year_month_day ymd{ sd };
            year_month_day new_ymd{ sd };
            year_month_day_last ymd_lastday{ ymd.year(),std::chrono::month_day_last(ymd.month()) };
            
            std::chrono::months delta_m(t.n);
            new_ymd += delta_m;
            std::chrono::month new_month{ new_ymd.month() };
            std::chrono::year new_year{ new_ymd.year() };

            if (ymd == ymd_lastday)
            {            
                new_ymd = year_month_day_last{ new_year, std::chrono::month_day_last(new_month) };
                new_sd = sys_days{ new_ymd };
                break;
            }

            // Add days
            int day=static_cast<unsigned>(new_ymd.day());

            int new_day = day;
            //year_month_day new_ymd{ std::chrono::year(new_year), std::chrono::month(new_month), std::chrono::day(new_day) };
            while (!new_ymd.ok() && new_day > 0) {
                --new_day;
                new_ymd = year_month_day{new_year, new_month, std::chrono::day{static_cast<unsigned>(new_day)} };
            }
            new_sd = sys_days{ new_ymd };
            break;
        }
        case TenorUnit::Years: {
            year_month_day ymd{ sd };
            year_month_day new_ymd{ sd };
            year_month_day_last ymd_lastday{ ymd.year(),std::chrono::month_day_last(ymd.month()) };
            std::chrono::years delta_y(t.n);
            new_ymd += delta_y;
            int day = static_cast<unsigned>(new_ymd.day());
            // std::chrono::day d{ new_ymd.day() };
            std::chrono::month new_month{ new_ymd.month() };
            std::chrono::year new_year{ new_ymd.year() };

            if (ymd == ymd_lastday)
            {
                new_ymd = year_month_day_last{ new_year, std::chrono::month_day_last(new_month) };
                new_sd = sys_days{ new_ymd };
                break;
            }

            int new_day = day;
            while (!new_ymd.ok() && new_day > 0) {
                --new_day;
                new_ymd = year_month_day{ new_year, new_month, std::chrono::day{static_cast<unsigned>(new_day)} };
            }
            new_sd = sys_days{ new_ymd };
            break;
        }
        } // switch

        Date advanced{ new_sd };
        return adjust(advanced, bdc);
    }

    bool Calendar::is_weekend(const Date& d)
    {
        using namespace std::chrono;
        weekday wd{ d.raw() };            // c_encoding(): 0 == Sunday, 6 == Saturday
        unsigned w = wd.c_encoding();
        return (w == 0 || w == 6);
    }

    // year fraction implementations
    double year_fraction(const Date& start, const Date& end, DayCount dc)
    {
        using namespace std::chrono;

        sys_days s = start.raw();
        sys_days e = end.raw();

        if (e == s) return 0.0;
        if (e < s) return -year_fraction(end, start, dc);

        auto days = (e - s).count(); // number of days as integer

        switch (dc) {
        case DayCount::ACT360:
            return static_cast<double>(days) / 360.0;
        case DayCount::ACT365F:
            return static_cast<double>(days) / 365.0;
        case DayCount::THIRTY360: {
            year_month_day y1{ s }, y2{ e };
            int Y1 = static_cast<int>(y1.year());
            int Y2 = static_cast<int>(y2.year());
            int M1 = static_cast<unsigned>(y1.month());
            int M2 = static_cast<unsigned>(y2.month());
            int D1 = static_cast<unsigned>(y1.day());
            int D2 = static_cast<unsigned>(y2.day());

            // simple 30/360 US rule
            if (D1 == 31) D1 = 30;
            if (D2 == 31 && D1 == 30) D2 = 30;

            int days360 = (Y2 - Y1) * 360 + (M2 - M1) * 30 + (D2 - D1);
            return static_cast<double>(days360) / 360.0;
        }

        default:
            // fallback
            return static_cast<double>(days) / 365.0;
        }
    }

    // Simple schedule generator
    Schedule make_schedule(const ScheduleConfig& cfg)
    {
        Schedule sched;
        if (cfg.start.raw() > cfg.end.raw()) return sched;

        Calendar cal = cfg.calendar; // copy
        Tenor t = cfg.tenor;
        if (t.n == 0) {
            // degenerate: only start and end
            sched.dates.push_back(cal.adjust(cfg.start, cfg.bdc));
            if (cfg.end.raw() != cfg.start.raw())
                sched.dates.push_back(cal.adjust(cfg.end, cfg.bdc));
            return sched;
        }

        // Safety guard to avoid infinite loops
        const int MAX_STEPS = 1024;

        if (cfg.rule == DateGenerationRule::Backward) {
            // generate backward from end
            std::vector<Date> tmp;
            Tenor neg = t;
            tmp.push_back(cal.adjust(cfg.end, cfg.bdc));
            for (int i = 1; i < MAX_STEPS; ++i) {
                neg.n = -t.n*i;
                Date next = cal.advance(cfg.end, neg, cfg.bdc);
                if (next.raw() < cfg.start.raw()) break;
                tmp.push_back(next);
                if (next.raw() == cfg.start.raw()) break;
            }
            // ensure start included
            if (tmp.empty() || tmp.back().raw() != cfg.start.raw()) {
                tmp.push_back(cal.adjust(cfg.start, cfg.bdc));
            }
            std::reverse(tmp.begin(), tmp.end());
            // remove duplicates (if any) and assign
            tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end()); // [](const Date& a, const Date& b) { return a.raw() == b.raw(); }
            sched.dates = std::move(tmp);
        }
        else {
            // Forward generation
            std::vector<Date> tmp;
            Tenor pos = t;
            tmp.push_back(cal.adjust(cfg.start, cfg.bdc));
            for (int i = 1; i < MAX_STEPS; ++i) {
                pos.n = i * t.n;
                Date next = cal.advance(cfg.start, pos, cfg.bdc);
                if (next.raw() > cfg.end.raw()) break;
                tmp.push_back(next);
                if (next.raw() == cfg.end.raw()) break;
            }
            if (tmp.empty() || tmp.back().raw() != cfg.end.raw()) {
                tmp.push_back(cal.adjust(cfg.end, cfg.bdc));
            }
            tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end()); // [](const Date& a, const Date& b) { return a.raw() == b.raw(); }
            sched.dates = std::move(tmp);
        }

        return sched;
    }


} // namespace ir