#include "hegel/hegel.hpp"
#include "metrics.hpp"

int main(int argc, char* argv[]) {
  int test_cases = conformance::get_test_cases();

  hegel::hegel(
      []() {
        auto gen = hegel::st::booleans();
        auto value = gen.generate();
        conformance::write_metrics({{"value", value}});
      },
      hegel::HegelOptions{.test_cases = test_cases});

  return 0;
}
