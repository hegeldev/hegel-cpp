#include <algorithm>
#include <hegel/json.h>
#include <iostream>
#include <optional>

#include "hegel/hegel.h"
#include "metrics.h"

#include "../../../src/json_impl.h"
using hegel::internal::json::ImplUtil;
namespace gs = hegel::generators;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
        return 1;
    }

    auto args = ImplUtil::raw(hegel::internal::json::json::parse(argv[1]));
    size_t min_size = args["min_size"].get<size_t>();
    std::optional<size_t> max_size =
        args["max_size"].is_null()
            ? std::nullopt
            : std::optional<size_t>(args["max_size"].get<size_t>());
    std::optional<int> min_value =
        args["min_value"].is_null()
            ? std::nullopt
            : std::optional<int>(args["min_value"].get<int>());
    std::optional<int> max_value =
        args["max_value"].is_null()
            ? std::nullopt
            : std::optional<int>(args["max_value"].get<int>());
    int test_cases = conformance::get_test_cases();

    hegel::test(
        [=](hegel::TestCase& tc) {
            auto gen =
                gs::vectors(gs::integers<int>({.min_value = min_value,
                                               .max_value = max_value}),
                            {.min_size = min_size, .max_size = max_size});

            auto vec = tc.draw(gen);

            nlohmann::json metrics = {{"size", vec.size()}};
            if (!vec.empty()) {
                metrics["min_element"] =
                    *std::min_element(vec.begin(), vec.end());
                metrics["max_element"] =
                    *std::max_element(vec.begin(), vec.end());
            }
            conformance::write_metrics(metrics);
        },
        {.test_cases = test_cases});

    return 0;
}
