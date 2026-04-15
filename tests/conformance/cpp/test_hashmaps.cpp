#include <algorithm>
#include <hegel/json.h>
#include <iostream>
#include <map>
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
    size_t min_size = args["min_size"].get<size_t>();
    size_t max_size = args["max_size"].get<size_t>();
    std::string key_type = args["key_type"].get<std::string>();
    int min_key = args["min_key"].get<int>();
    int max_key = args["max_key"].get<int>();
    int min_value = args["min_value"].get<int>();
    int max_value = args["max_value"].get<int>();
    std::string mode = args.value("mode", "basic");
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            nlohmann::json metrics;

            if (key_type == "integer") {
                auto key_gen = hegel::generators::integers<int>(
                    {.min_value = min_key, .max_value = max_key});
                auto val_gen = hegel::generators::integers<int>(
                    {.min_value = min_value, .max_value = max_value});

                // In non_basic mode, wrap generators to make them
                // function-backed
                auto final_key_gen = mode == "non_basic"
                                         ? key_gen.map([](int x) { return x; })
                                         : key_gen;
                auto final_val_gen = mode == "non_basic"
                                         ? val_gen.map([](int x) { return x; })
                                         : val_gen;

                auto gen = hegel::generators::dictionaries(
                    final_key_gen, final_val_gen,
                    {.min_size = min_size, .max_size = max_size});

                auto dict = hegel::draw(gen);

                metrics["size"] = dict.size();
                if (!dict.empty()) {
                    auto [min_k, max_k] =
                        std::minmax_element(dict.begin(), dict.end(),
                                            [](const auto& a, const auto& b) {
                                                return a.first < b.first;
                                            });
                    metrics["min_key"] = min_k->first;
                    metrics["max_key"] = max_k->first;

                    auto [min_v, max_v] =
                        std::minmax_element(dict.begin(), dict.end(),
                                            [](const auto& a, const auto& b) {
                                                return a.second < b.second;
                                            });
                    metrics["min_value"] = min_v->second;
                    metrics["max_value"] = max_v->second;
                }
            } else {
                // string keys
                auto val_gen = hegel::generators::integers<int>(
                    {.min_value = min_value, .max_value = max_value});

                auto final_val_gen = mode == "non_basic"
                                         ? val_gen.map([](int x) { return x; })
                                         : val_gen;

                auto text_gen = hegel::generators::text();
                auto final_text_gen =
                    mode == "non_basic"
                        ? text_gen.map([](std::string s) { return s; })
                        : text_gen;

                auto gen = hegel::generators::dictionaries(
                    final_text_gen, final_val_gen,
                    {.min_size = min_size, .max_size = max_size});

                auto dict = hegel::draw(gen);

                metrics["size"] = dict.size();
                if (!dict.empty()) {
                    auto [min_v, max_v] =
                        std::minmax_element(dict.begin(), dict.end(),
                                            [](const auto& a, const auto& b) {
                                                return a.second < b.second;
                                            });
                    metrics["min_value"] = min_v->second;
                    metrics["max_value"] = max_v->second;
                }
            }
            conformance::write_metrics(metrics);
        },
        {.test_cases = test_cases});

    return 0;
}
