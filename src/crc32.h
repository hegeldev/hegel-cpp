#pragma once

#include <cstddef>
#include <cstdint>

namespace hegel::impl {

    /// Compute the CRC32 checksum of a byte buffer (IEEE 802.3 polynomial).
    uint32_t crc32(const uint8_t* data, size_t len);

} // namespace hegel::impl
