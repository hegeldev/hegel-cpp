#include <cstdint>
#include <hegel/json.h>
#include <iostream>
#include <optional>
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
    size_t min_size = args["min_size"].get<size_t>();
    std::optional<size_t> max_size =
        args["max_size"].is_null()
            ? std::nullopt
            : std::optional<size_t>(args["max_size"].get<size_t>());
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=](hegel::TestCase& tc) {
            auto gen = gs::binary({.min_size = min_size, .max_size = max_size});
            std::vector<uint8_t> value = tc.draw(gen);
            conformance::write_metrics({{"length", value.size()}});
        },
        {.test_cases = test_cases});

    return 0;
}
