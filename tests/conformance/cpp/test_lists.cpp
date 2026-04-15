#include <hegel/json.h>
#include <iostream>
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
    bool unique = args.value("unique", false);
    std::string mode = args.value("mode", "basic");
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            auto element_gen = hegel::generators::integers<int>(
                {.min_value = min_value, .max_value = max_value});

            // In non_basic mode, wrap element generator in a map to make it
            // function-backed (non-schema)
            hegel::generators::Generator<int> gen =
                mode == "non_basic" ? element_gen.map([](int x) { return x; })
                                    : element_gen;

            auto vec = hegel::draw(
                hegel::generators::vectors(gen, {.min_size = min_size,
                                                 .max_size = max_size,
                                                 .unique = unique}));

            nlohmann::json elements_arr = nlohmann::json::array();
            for (auto v : vec) {
                elements_arr.push_back(v);
            }
            conformance::write_metrics({{"elements", elements_arr}});
        },
        {.test_cases = test_cases});

    return 0;
}
