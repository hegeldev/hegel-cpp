#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

#include "hegel/hegel.h"
#include "metrics.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
        return 1;
    }

    auto args = nlohmann::json::parse(argv[1]);
    std::vector<int> options = args["options"].get<std::vector<int>>();
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [&]() {
            auto gen = hegel::st::sampled_from(options);
            auto value = gen.generate();
            conformance::write_metrics({{"value", value}});
        },
        hegel::HegelOptions{.test_cases = test_cases});

    return 0;
}
