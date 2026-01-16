#include <gtest/gtest.h>

#include <hegel/hegel.hpp>

using namespace hegel::st;

TEST(Settings, DefaultRuns100TestCases) {
  int count = 0;

  hegel::hegel([&count] {
    integers<int>().generate();
    count++;
  });

  ASSERT_EQ(count, 100);
}
