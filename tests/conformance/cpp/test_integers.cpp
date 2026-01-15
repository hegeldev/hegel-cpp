#include <iostream>
#include <nlohmann/json.hpp>

#include "hegel/hegel.hpp"
#include "metrics.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
    return 1;
  }

  auto args = nlohmann::json::parse(argv[1]);
  int min_value = args["min_value"].get<int>();
  int max_value = args["max_value"].get<int>();
  int test_cases = conformance::get_test_cases();

  hegel::hegel(
      [=]() {
        auto gen =
            hegel::st::integers<int>({.min_value = min_value, .max_value = max_value});
        auto value = gen.generate();
        conformance::write_metrics({{"value", value}});
      },
      hegel::HegelOptions{.test_cases = test_cases});

  return 0;
}
