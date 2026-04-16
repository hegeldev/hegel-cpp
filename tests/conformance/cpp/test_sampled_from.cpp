#include <hegel/json.h>
#include <iostream>
#include <vector>

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
    std::vector<int> options = args["options"].get<std::vector<int>>();
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [&](hegel::TestCase& tc) {
            auto gen = gs::sampled_from(options);
            auto value = tc.draw(gen);
            conformance::write_metrics({{"value", value}});
        },
        {.test_cases = test_cases});

    return 0;
}
