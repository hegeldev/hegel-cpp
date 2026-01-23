#include <gtest/gtest.h>

#include <hegel/detail/base64.hpp>
#include <hegel/hegel.hpp>

using namespace hegel::detail;
using namespace hegel::st;

TEST(Base64, RoundTrip) {
  hegel::hegel(
      [] {
        auto input = binary().generate();
        std::string encoded = base64_encode(input);
        std::vector<uint8_t> decoded = base64_decode(encoded);
        ASSERT_EQ(input, decoded);
      },
      hegel::HegelOptions{.test_cases = 100});
}

TEST(Base64, KnownValues) {
  // RFC 4648 test vectors
  ASSERT_EQ(base64_encode({}), "");
  ASSERT_EQ(base64_encode({'f'}), "Zg==");
  ASSERT_EQ(base64_encode({'f', 'o'}), "Zm8=");
  ASSERT_EQ(base64_encode({'f', 'o', 'o'}), "Zm9v");
  ASSERT_EQ(base64_encode({'f', 'o', 'o', 'b'}), "Zm9vYg==");
  ASSERT_EQ(base64_encode({'f', 'o', 'o', 'b', 'a'}), "Zm9vYmE=");
  ASSERT_EQ(base64_encode({'f', 'o', 'o', 'b', 'a', 'r'}), "Zm9vYmFy");
}
