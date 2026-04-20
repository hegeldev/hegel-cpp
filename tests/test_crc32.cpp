#include <gtest/gtest.h>

#include <crc32.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using hegel::impl::crc32;

static uint32_t crc32_of(const std::string& s) {
    return crc32(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

TEST(CRC32, EmptyBufferIsZero) { EXPECT_EQ(crc32(nullptr, 0), 0u); }

TEST(CRC32, EmptyStringIsZero) { EXPECT_EQ(crc32_of(""), 0u); }

TEST(CRC32, KnownVectorA) {
    // CRC32/ISO-HDLC of "a"
    EXPECT_EQ(crc32_of("a"), 0xE8B7BE43u);
}

TEST(CRC32, KnownVector123456789) {
    // The standard CRC32 check vector.
    EXPECT_EQ(crc32_of("123456789"), 0xCBF43926u);
}

TEST(CRC32, LongBufferDeterministic) {
    // Exercises the loop at scale. The polynomial is fixed so the value is
    // stable across runs; we don't commit to a specific magic number here,
    // just that invoking twice is consistent.
    std::vector<uint8_t> zeros(1024 * 1024, 0);
    uint32_t v1 = crc32(zeros.data(), zeros.size());
    uint32_t v2 = crc32(zeros.data(), zeros.size());
    EXPECT_EQ(v1, v2);
    EXPECT_NE(v1, 0u);
}

TEST(CRC32, DifferentBuffersDifferentHash) {
    // Basic sanity: different inputs hash differently.
    EXPECT_NE(crc32_of("abc"), crc32_of("abd"));
}
