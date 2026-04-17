#pragma once

#include <cstdlib>
#include <fstream>
#include <hegel/hegel.h>
#include <hegel/json.h>
#include <nlohmann/json.hpp>
#include <string>

namespace conformance {

    inline std::ofstream& get_metrics_file() {
        static std::ofstream file;
        static bool initialized = false;
        if (!initialized) {
            if (const char* path = std::getenv("CONFORMANCE_METRICS_FILE")) {
                file.open(path, std::ios::app);
            }
            initialized = true;
        }
        return file;
    }

    inline int get_test_cases() {
        if (const char* val = std::getenv("CONFORMANCE_TEST_CASES")) {
            return std::atoi(val);
        }
        return 50; // default
    }

    // Write a complete metrics object for one test case
    inline void write_metrics(const nlohmann::json& metrics) {
        auto& f = get_metrics_file();
        if (f.is_open()) {
            f << metrics.dump() << "\n";
            f.flush();
        }
    }

    // Wrap a generator in a trivial filter so it loses its schema,
    // forcing the compositional fallback path.
    template <typename T>
    hegel::generators::Generator<T>
    make_non_basic(hegel::generators::Generator<T> gen) {
        return gen.filter([](const T&) { return true; });
    }

    inline std::string get_mode(const nlohmann::json& args) {
        if (args.contains("mode") && args["mode"].is_string()) {
            return args["mode"].get<std::string>();
        }
        return "basic";
    }

} // namespace conformance
