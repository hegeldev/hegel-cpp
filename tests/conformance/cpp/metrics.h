#pragma once

#include <cstdlib>
#include <fstream>
#include <hegel/json.h>
#include <string>
#include <nlohmann/json.hpp>

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

} // namespace conformance
