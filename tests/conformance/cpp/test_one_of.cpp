#include <hegel/internal.h>
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
    std::string mode = args.value("mode", "basic");

    // Precompute a fallback even value from the first range. When the filter
    // exhausts its retry limit, mark_complete is still sent (status=INVALID)
    // and the server writes a metric, so the client must write one too.
    int dummy_even_value = 0;
    if (mode == "filter_even" && !args["ranges"].empty()) {
        int lo = args["ranges"][0]["min_value"].get<int>();
        dummy_even_value = lo % 2 == 0 ? lo : lo + 1;
    }

    // Build generators from non-overlapping integer ranges
    std::vector<gs::Generator<int>> gens;
    for (const auto& range : args["ranges"]) {
        int lo = range["min_value"].get<int>();
        int hi = range["max_value"].get<int>();
        auto gen = gs::integers<int>({.min_value = lo, .max_value = hi});

        if (mode == "map_negate") {
            gens.push_back(gen.map([](int x) { return -x; }));
        } else if (mode == "filter_even") {
            gens.push_back(gen.filter([](int x) { return x % 2 == 0; }));
        } else {
            gens.push_back(gen);
        }
    }

    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [&](hegel::TestCase& tc) {
            try {
                auto value = tc.draw(gs::one_of(gens));
                conformance::write_metrics({{"value", value}});
            } catch (const hegel::internal::HegelReject&) {
                // Filter exhaustion path: the runner will mark_complete
                // with status=INVALID, so the server writes a metric.
                // Write a matching client metric so counts stay in sync.
                // HegelStopTest (server OVERRUN / abandoned iteration) is
                // not caught here and skips mark_complete, so neither
                // side writes.
                if (mode == "filter_even") {
                    conformance::write_metrics({{"value", dummy_even_value}});
                }
                throw;
            }
        },
        {.test_cases = static_cast<uint64_t>(test_cases)});

    return 0;
}
