#pragma once

/**
 * @file formats.h
 * @brief Format string generator functions: emails, urls, domains, etc.
 */

#include <string>

#include "hegel/core.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for domains() strategy.
     */
    struct DomainsParams {
        size_t max_length = 255; ///< Maximum domain name length
    };

    /**
     * @brief Parameters for ip_addresses() strategy.
     */
    struct IpAddressesParams {
        std::optional<int> v; ///< IP version: 4, 6, or nullopt for both
    };

    // =============================================================================
    // Strategy declarations
    // =============================================================================

    /// @name Format String Strategies
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
     *   be in the range [4, 255]; values outside this range throw
     *   `std::invalid_argument`.
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
     *   Other values throw `std::invalid_argument`.
     * @return Generator producing IP-address strings.
     */
    Generator<std::string> ip_addresses(IpAddressesParams params = {});

    /**
     * @brief Generate calendar dates.
     *
     * Generates a date between January 01, 0001 through December 31, 9999 in
     * the proleptic Gregorian calendar) and returns the ISO 8601 serialization
     * (`YYYY-MM-DD`). Values shrink towards January 1st, 2000.
     *
     * @return Generator producing ISO 8601 date strings.
     */
    Generator<std::string> dates();

    /**
     * @brief Generate times of day.
     *
     * Generates a time between 00:00:00 and 23:59:59.999999 and returns the
     * ISO 8601 serialization (`HH:MM:SS[.ffffff]`). Values shrink towards
     * midnight. No timezone component is requested; generated values are naive.
     *
     * @return Generator producing ISO 8601 time strings.
     */
    Generator<std::string> times();

    /**
     * @brief Generate datetimes.
     *
     * Generates datetimes between January 01, 0001 at 00:00:00 and December 31,
     * 9999 at 23:59:59.999999 and returns the ISO 8601 serialization
     * (`YYYY-MM-DDTHH:MM:SS[.ffffff]`).
     * No timezone is requested; generated values are naive. Examples from this
     * strategy shrink towards midnight on January 1st 2000.
     *
     * @return Generator producing ISO 8601 datetime strings.
     */
    Generator<std::string> datetimes();

    /// @}

} // namespace hegel::generators
