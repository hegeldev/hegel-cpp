#include <hegel/json.h>
#include <iostream>
#include <vector>

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
    std::string mode = args.value("mode", "basic");

    // Build generators from non-overlapping integer ranges
    std::vector<hegel::generators::Generator<int>> gens;
    for (const auto& range : args["ranges"]) {
        int lo = range["min_value"].get<int>();
        int hi = range["max_value"].get<int>();
        auto gen = hegel::generators::integers<int>(
            {.min_value = lo, .max_value = hi});

        if (mode == "transformed") {
            // Path 2: basic generators with identity transform
            gens.push_back(gen.map([](int x) { return x; }));
        } else if (mode == "non_basic") {
            // Path 3: force non-basic by wrapping in flat_map
            gens.push_back(
                gen.flat_map([](int x) { return hegel::generators::just(x); }));
        } else {
            // Path 1: all basic generators
            gens.push_back(gen);
        }
    }

    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            auto value = hegel::draw(hegel::generators::one_of(gens));
            conformance::write_metrics({{"value", value}});
        },
        {.test_cases = test_cases});

    return 0;
}
