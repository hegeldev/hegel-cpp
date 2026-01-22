#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

#include "hegel/hegel.hpp"
#include "metrics.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
    return 1;
  }

  auto args = nlohmann::json::parse(argv[1]);
  std::optional<double> min_value =
      args["min_value"].is_null()
          ? std::nullopt
          : std::optional<double>(args["min_value"].get<double>());
  std::optional<double> max_value =
      args["max_value"].is_null()
          ? std::nullopt
          : std::optional<double>(args["max_value"].get<double>());
  bool exclude_min = args.value("exclude_min", false);
  bool exclude_max = args.value("exclude_max", false);
  int test_cases = conformance::get_test_cases();

  hegel::hegel(
      [=]() {
        auto gen = hegel::st::floats<double>({
            .min_value = min_value,
            .max_value = max_value,
            .exclude_min = exclude_min,
            .exclude_max = exclude_max,
        });
        auto value = gen.generate();
        conformance::write_metrics({
            {"value", value},
            {"is_nan", std::isnan(value)},
            {"is_infinite", std::isinf(value)},
        });
      },
      hegel::HegelOptions{.test_cases = test_cases});

  return 0;
}
