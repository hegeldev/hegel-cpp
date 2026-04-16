#include "hegel/hegel.h"
#include "metrics.h"

namespace gs = hegel::generators;

int main(int argc, char* argv[]) {
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [](hegel::TestCase& tc) {
            auto gen = gs::booleans();
            auto value = tc.draw(gen);
            conformance::write_metrics({{"value", value}});
        },
        {.test_cases = test_cases});

    return 0;
}
