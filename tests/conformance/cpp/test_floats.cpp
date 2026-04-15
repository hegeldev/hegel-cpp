#include <cmath>
#include <hegel/json.h>
#include <iostream>
#include <limits>
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
    std::optional<double> min_value =
        args["min_value"].is_null()
            ? std::nullopt
            : std::optional<double>(args["min_value"].get<double>());
    std::optional<double> max_value =
        args["max_value"].is_null()
            ? std::nullopt
            : std::optional<double>(args["max_value"].get<double>());
    bool exclude_min = args.value("exclude_min", false);
    bool exclude_max = args.value("exclude_max", false);
    std::optional<bool> allow_nan =
        args["allow_nan"].is_null()
            ? std::nullopt
            : std::optional<bool>(args["allow_nan"].get<bool>());
    std::optional<bool> allow_infinity =
        args["allow_infinity"].is_null()
            ? std::nullopt
            : std::optional<bool>(args["allow_infinity"].get<bool>());
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            // When both bounds are excluded, check that a valid float exists
            // strictly between them. Adjacent IEEE-754 doubles have no float
            // between them; asking the server to generate such a value would
            // raise InvalidArgument on the Python side and crash the server.
            // In that case, write a dummy NaN metric and skip the draw.
            if (min_value.has_value() && max_value.has_value() && exclude_min &&
                exclude_max) {
                double lo = *min_value;
                double hi = *max_value;
                double next =
                    std::nextafter(lo, std::numeric_limits<double>::infinity());
                if (next >= hi) {
                    conformance::write_metrics({{"value", 0.0},
                                                {"is_nan", true},
                                                {"is_infinite", false}});
                    return;
                }
            }

            auto gen = hegel::generators::floats<double>({
                .min_value = min_value,
                .max_value = max_value,
                .exclude_min = exclude_min,
                .exclude_max = exclude_max,
                .allow_nan = allow_nan,
                .allow_infinity = allow_infinity,
            });
            auto value = hegel::draw(gen);
            conformance::write_metrics({
                {"value", value},
                {"is_nan", std::isnan(value)},
                {"is_infinite", std::isinf(value)},
            });
        },
        {.test_cases = test_cases});

    return 0;
}
