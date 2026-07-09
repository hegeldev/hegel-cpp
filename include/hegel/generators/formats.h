#pragma once

#include <string>

#include "hegel/core.h"
#include "hegel/datetime.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for domains() generator.
     */
    struct DomainsParams {
        size_t max_length = 255; ///< Maximum domain name length
    };

    /**
     * @brief Parameters for ip_addresses() generator.
     */
    struct IpAddressesParams {
        std::optional<int> v; ///< IP version: 4, 6, or nullopt for both
    };

    /// @name Strings
    /// @{

    /**
     * @brief Generate valid email addresses.
     *
     * Generates addresses in the format specified by RFC 5322 Section 3.4.1
     * (i.e. `local-part@domain`).
     * Values shrink towards shorter local-parts and host domains.
     *
     * @return Generator producing email-address strings.
     */
    Generator<std::string> emails();

    /**
     * @brief Generate valid domain names.
     *
     * Generates RFC 1035-compliant fully qualified domain names.
     *
     * @param params Length constraints. `max_length` (default 255) must
     *   be in the range [4, 255];
     * @return Generator producing domain-name strings.
     */
    Generator<std::string> domains(DomainsParams params = {});

    /**
     * @brief Generate valid URLs.
     *
     * Generates RFC 3986-compliant URLs with either the `http` or `https`
     * scheme.
     *
     * @return Generator producing http/https URL strings.
     */
    Generator<std::string> urls();

    /// @}

    /// @name Misc
    /// @{

    /**
     * @brief Generate IP addresses.
     *
     * Generates IP addresses serialized to text:
     * - IPv4: dotted-quad form (e.g. `192.0.2.5`).
     * - IPv6: colon-hex form (e.g. `2001:db8::1`).
     *
     * Any address in the selected version's space may be produced.
     *
     *
     * @param params Version constraint: `v = 4` for IPv4 only, `v = 6`
     *   for IPv6 only, or `std::nullopt` (default) for a mix of both.
     * @return Generator producing IP-address strings.
     */
    Generator<std::string> ip_addresses(IpAddressesParams params = {});

    /// @}

    /// @name Datetime
    /// @{

    /**
     * @brief Generate calendar dates.
     *
     * Generates a hegel::Date between January 01, 0001 and December 31, 9999
     * in the proleptic Gregorian calendar. Values shrink towards January 1st,
     * 2000.
     *
     * For the ISO 8601 string form (`YYYY-MM-DD`), call Date::to_string() on
     * the drawn value, or map the generator:
     * `dates().map([](hegel::Date d) { return d.to_string(); })`.
     *
     * @return Generator producing hegel::Date values.
     */
    Generator<Date> dates();

    /**
     * @brief Generate times of day.
     *
     * Generates a hegel::Time between 00:00:00 and 23:59:59.999999. Values
     * shrink towards midnight. No timezone component is requested; generated
     * values are naive.
     *
     * For the ISO 8601 string form (`HH:MM:SS.ffffff`, fractional seconds
     * always present), call Time::to_string() on the drawn value.
     *
     * @return Generator producing hegel::Time values.
     */
    Generator<Time> times();

    /**
     * @brief Generate datetimes.
     *
     * Generates a hegel::DateTime between January 01, 0001 at 00:00:00 and
     * December 31, 9999 at 23:59:59.999999. No timezone is requested;
     * generated values are naive. Values shrink towards midnight on January
     * 1st, 2000.
     *
     * For the ISO 8601 string form (`YYYY-MM-DDTHH:MM:SS.ffffff`, fractional
     * seconds always present), call DateTime::to_string() on the drawn value.
     *
     * @return Generator producing hegel::DateTime values.
     */
    Generator<DateTime> datetimes();

    /// @}

} // namespace hegel::generators
