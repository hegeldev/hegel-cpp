#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

#include "hegel/hegel.h"
#include "metrics.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
        return 1;
    }

    auto args = nlohmann::json::parse(argv[1]);
    size_t min_size = args["min_size"].get<size_t>();
    std::optional<size_t> max_size =
        args["max_size"].is_null()
            ? std::nullopt
            : std::optional<size_t>(args["max_size"].get<size_t>());
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            auto gen =
                hegel::strategies::binary({.min_size = min_size, .max_size = max_size});
            std::vector<uint8_t> value = gen.generate();
            conformance::write_metrics({{"length", value.size()}});
        },
        {.test_cases = test_cases});

    return 0;
}
