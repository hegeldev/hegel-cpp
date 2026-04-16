#include <crc32.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace hegel::impl {

    static constexpr uint32_t make_crc_entry(uint32_t c) {
        for (int k = 0; k < 8; ++k) {
            if (c & 1)
                c = 0xEDB88320 ^ (c >> 1);
            else
                c >>= 1;
        }
        return c;
    }

    static constexpr auto make_crc_table() {
        std::array<uint32_t, 256> table{};
        for (uint32_t i = 0; i < 256; ++i) {
            table[i] = make_crc_entry(i);
        }
        return table;
    }

    static constexpr auto CRC_TABLE = make_crc_table();

    uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc = CRC_TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFF;
    }

} // namespace hegel::impl
