#include <iostream>
#include <hegel/json.h>
#include <optional>

#include "hegel/hegel.h"
#include "metrics.h"

#include "../../../src/json_impl.h"
using hegel::internal::json::ImplUtil;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
        return 1;
    }

    auto args = ImplUtil::raw(hegel::internal::json::json::parse(argv[1]));
    std::optional<int> min_value =
        args["min_value"].is_null()
            ? std::nullopt
            : std::optional<int>(args["min_value"].get<int>());
    std::optional<int> max_value =
        args["max_value"].is_null()
            ? std::nullopt
            : std::optional<int>(args["max_value"].get<int>());
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            auto gen = hegel::generators::integers<int>(
                {.min_value = min_value, .max_value = max_value});
            auto value = hegel::draw(gen);
            conformance::write_metrics({{"value", value}});
        },
        {.test_cases = test_cases});

    return 0;
}
