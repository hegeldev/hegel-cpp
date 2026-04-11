#include <hegel/json.h>
#include <iostream>

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
    std::string mode = conformance::get_mode(args);
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            auto gen = hegel::generators::booleans();
            auto value = mode == "non_basic"
                             ? hegel::draw(conformance::make_non_basic(gen))
                             : hegel::draw(gen);
            conformance::write_metrics({{"value", value}});
        },
        {.test_cases = test_cases});

    return 0;
}
