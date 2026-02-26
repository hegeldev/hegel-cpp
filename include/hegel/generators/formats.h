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

    /// Generate valid email addresses
    Generator<std::string> emails();

    /**
     * @brief Generate valid domain names.
     * @param params Length constraints
     * @return Generator producing domain names
     */
    Generator<std::string> domains(DomainsParams params = {});

    /// Generate valid URLs
    Generator<std::string> urls();

    /**
     * @brief Generate IP addresses.
     * @param params Version constraints (v=4, v=6, or both)
     * @return Generator producing IP address strings
     */
    Generator<std::string> ip_addresses(IpAddressesParams params = {});

    /// Generate dates in ISO 8601 format (YYYY-MM-DD)
    Generator<std::string> dates();

    /// Generate times in ISO 8601 format (HH:MM:SS)
    Generator<std::string> times();

    /// Generate datetimes in ISO 8601 format
    Generator<std::string> datetimes();

    /// @}

} // namespace hegel::generators
