#pragma once

#include <cstdio>
#include <ostream>
#include <string>
#include <tuple>

namespace hegel {

    /**
     * @brief A calendar date in the proleptic Gregorian calendar.
     *
     * Produced by @ref hegel::generators::dates "dates()" and as the date
     * part of DateTime. Plain aggregate: construct one with
     * `hegel::Date{2024, 2, 29}`.
     */
    struct Date {
        int year;  ///< Year. Generated dates span [1, 9999].
        int month; ///< Month of the year, in [1, 12].
        int day;   ///< Day of the month, in [1, days-in-month].

        /**
         * @brief The ISO 8601 serialization (`YYYY-MM-DD`).
         *
         * The year is zero-padded to four digits, month and day to two, so
         * every Date has exactly one serialization.
         */
        std::string to_string() const {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
            return buf;
        }
    };

    /// @name Date comparisons (chronological order)
    /// @{
    inline bool operator==(const Date& a, const Date& b) {
        return std::tie(a.year, a.month, a.day) ==
               std::tie(b.year, b.month, b.day);
    }
    inline bool operator!=(const Date& a, const Date& b) { return !(a == b); }
    inline bool operator<(const Date& a, const Date& b) {
        return std::tie(a.year, a.month, a.day) <
               std::tie(b.year, b.month, b.day);
    }
    inline bool operator>(const Date& a, const Date& b) { return b < a; }
    inline bool operator<=(const Date& a, const Date& b) { return !(b < a); }
    inline bool operator>=(const Date& a, const Date& b) { return !(a < b); }
    /// @}

    /// @brief Prints Date::to_string().
    inline std::ostream& operator<<(std::ostream& os, const Date& d) {
        return os << d.to_string();
    }

    /**
     * @brief A time of day with microsecond precision. No timezone; values
     *        are naive.
     *
     * Produced by @ref hegel::generators::times "times()" and as the time
     * part of DateTime. Plain aggregate: construct one with
     * `hegel::Time{23, 59, 59, 999999}`.
     */
    struct Time {
        int hour;        ///< Hour of the day, in [0, 23].
        int minute;      ///< Minute of the hour, in [0, 59].
        int second;      ///< Second of the minute, in [0, 59].
        int microsecond; ///< Microsecond of the second, in [0, 999999].

        /**
         * @brief The ISO 8601 serialization (`HH:MM:SS.ffffff`).
         *
         * The fractional-seconds field is always present and zero-padded to
         * six digits — including when `microsecond` is zero — so every Time
         * has exactly one serialization. (This differs from Python's
         * `isoformat()`, which drops the fractional part when it is zero.)
         */
        std::string to_string() const {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%06d", hour, minute,
                          second, microsecond);
            return buf;
        }
    };

    /// @name Time comparisons (chronological order)
    /// @{
    inline bool operator==(const Time& a, const Time& b) {
        return std::tie(a.hour, a.minute, a.second, a.microsecond) ==
               std::tie(b.hour, b.minute, b.second, b.microsecond);
    }
    inline bool operator!=(const Time& a, const Time& b) { return !(a == b); }
    inline bool operator<(const Time& a, const Time& b) {
        return std::tie(a.hour, a.minute, a.second, a.microsecond) <
               std::tie(b.hour, b.minute, b.second, b.microsecond);
    }
    inline bool operator>(const Time& a, const Time& b) { return b < a; }
    inline bool operator<=(const Time& a, const Time& b) { return !(b < a); }
    inline bool operator>=(const Time& a, const Time& b) { return !(a < b); }
    /// @}

    /// @brief Prints Time::to_string().
    inline std::ostream& operator<<(std::ostream& os, const Time& t) {
        return os << t.to_string();
    }

    /**
     * @brief A naive datetime: a Date plus a Time, with no timezone.
     *
     * Produced by @ref hegel::generators::datetimes "datetimes()". Plain
     * aggregate: construct one with
     * `hegel::DateTime{{2024, 2, 29}, {12, 30, 0, 0}}`.
     */
    struct DateTime {
        Date date; ///< The calendar date.
        Time time; ///< The time of day.

        /**
         * @brief The ISO 8601 serialization
         *        (`YYYY-MM-DDTHH:MM:SS.ffffff`).
         *
         * Combines Date::to_string() and Time::to_string() with the `T`
         * separator, so the fractional-seconds field is always present and
         * every DateTime has exactly one serialization.
         */
        std::string to_string() const {
            return date.to_string() + "T" + time.to_string();
        }
    };

    /// @name DateTime comparisons (chronological order)
    /// @{
    inline bool operator==(const DateTime& a, const DateTime& b) {
        return a.date == b.date && a.time == b.time;
    }
    inline bool operator!=(const DateTime& a, const DateTime& b) {
        return !(a == b);
    }
    inline bool operator<(const DateTime& a, const DateTime& b) {
        if (a.date != b.date) {
            return a.date < b.date;
        }
        return a.time < b.time;
    }
    inline bool operator>(const DateTime& a, const DateTime& b) {
        return b < a;
    }
    inline bool operator<=(const DateTime& a, const DateTime& b) {
        return !(b < a);
    }
    inline bool operator>=(const DateTime& a, const DateTime& b) {
        return !(a < b);
    }
    /// @}

    /// @brief Prints DateTime::to_string().
    inline std::ostream& operator<<(std::ostream& os, const DateTime& dt) {
        return os << dt.to_string();
    }

} // namespace hegel
