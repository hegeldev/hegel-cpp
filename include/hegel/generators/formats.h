#pragma once

#include <string>

#include "hegel/core.h"

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
     * @return Generator producing email address strings.
     */
    Generator<std::string> emails();

    /**
     * @brief Generate valid domain names.
     * @param params Length constraints
     * @return Generator producing domain names
     */
    Generator<std::string> domains(DomainsParams params = {});

    /**
     * @brief Generate valid URLs.
     * @return Generator producing URL strings.
     */
    Generator<std::string> urls();

    /// @}

    /// @name Misc
    /// @{

    /**
     * @brief Generate IP addresses.
     * @param params Version constraints (v=4, v=6, or both)
     * @return Generator producing IP address strings
     */
    Generator<std::string> ip_addresses(IpAddressesParams params = {});

    /// @}

    /// @name Datetime
    /// @{

    /**
     * @brief Generate dates in ISO 8601 format (YYYY-MM-DD).
     * @return Generator producing date strings.
     */
    Generator<std::string> dates();

    /**
     * @brief Generate times in ISO 8601 format (HH:MM:SS).
     * @return Generator producing time strings.
     */
    Generator<std::string> times();

    /**
     * @brief Generate datetimes in ISO 8601 format.
     * @return Generator producing datetime strings.
     */
    Generator<std::string> datetimes();

    /// @}

} // namespace hegel::generators
