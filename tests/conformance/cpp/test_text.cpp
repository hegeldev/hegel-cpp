#include <hegel/json.h>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "hegel/hegel.h"
#include "metrics.h"

#include "../../../src/json_impl.h"
using hegel::internal::json::ImplUtil;

// Extract Unicode codepoints from a UTF-8 string
std::vector<uint32_t> extract_codepoints(const std::string& s) {
    std::vector<uint32_t> codepoints;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp = 0;
        size_t len = 0;
        if ((c & 0x80) == 0) {
            cp = c;
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
        } else {
            i += 1; // Invalid UTF-8, skip byte
            continue;
        }
        for (size_t j = 1; j < len && (i + j) < s.size(); ++j) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + j]) & 0x3F);
        }
        codepoints.push_back(cp);
        i += len;
    }
    return codepoints;
}

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

    // Read character filtering params
    std::optional<std::string> codec =
        args.contains("codec") && !args["codec"].is_null()
            ? std::optional<std::string>(args["codec"].get<std::string>())
            : std::nullopt;
    std::optional<uint32_t> min_codepoint =
        args.contains("min_codepoint") && !args["min_codepoint"].is_null()
            ? std::optional<uint32_t>(args["min_codepoint"].get<uint32_t>())
            : std::nullopt;
    std::optional<uint32_t> max_codepoint =
        args.contains("max_codepoint") && !args["max_codepoint"].is_null()
            ? std::optional<uint32_t>(args["max_codepoint"].get<uint32_t>())
            : std::nullopt;
    std::optional<std::vector<std::string>> categories;
    if (args.contains("categories") && !args["categories"].is_null()) {
        std::vector<std::string> cats;
        for (const auto& c : args["categories"]) {
            cats.push_back(c.get<std::string>());
        }
        categories = cats;
    }
    std::optional<std::vector<std::string>> exclude_categories;
    if (args.contains("exclude_categories") &&
        !args["exclude_categories"].is_null()) {
        std::vector<std::string> cats;
        for (const auto& c : args["exclude_categories"]) {
            cats.push_back(c.get<std::string>());
        }
        exclude_categories = cats;
    }
    std::optional<std::string> include_characters =
        args.contains("include_characters") &&
                !args["include_characters"].is_null()
            ? std::optional<std::string>(
                  args["include_characters"].get<std::string>())
            : std::nullopt;
    std::optional<std::string> exclude_characters =
        args.contains("exclude_characters") &&
                !args["exclude_characters"].is_null()
            ? std::optional<std::string>(
                  args["exclude_characters"].get<std::string>())
            : std::nullopt;

    std::string mode = conformance::get_mode(args);
    int test_cases = conformance::get_test_cases();

    hegel::hegel(
        [=]() {
            auto gen = hegel::generators::text(
                {.min_size = min_size,
                 .max_size = max_size,
                 .codec = codec,
                 .min_codepoint = min_codepoint,
                 .max_codepoint = max_codepoint,
                 .categories = categories,
                 .exclude_categories = exclude_categories,
                 .include_characters = include_characters,
                 .exclude_characters = exclude_characters});
            auto value = mode == "non_basic"
                             ? hegel::draw(conformance::make_non_basic(gen))
                             : hegel::draw(gen);
            auto cps = extract_codepoints(value);
            nlohmann::json cp_array = nlohmann::json::array();
            for (auto cp : cps) {
                cp_array.push_back(cp);
            }
            conformance::write_metrics({{"codepoints", cp_array}});
        },
        {.test_cases = test_cases});

    return 0;
}
