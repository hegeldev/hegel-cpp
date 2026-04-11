#include "hegel/hegel.h"
#include "metrics.h"

int main(int argc, char* argv[]) {
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        []() {
            auto gen = hegel::generators::booleans();
            auto value = hegel::draw(gen);
            conformance::write_metrics({{"value", value}});
        },
        {.test_cases = test_cases});

    return 0;
}
