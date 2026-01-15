#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "hegel/hegel.hpp"
#include "metrics.hpp"

// Count Unicode codepoints in a UTF-8 string
size_t count_codepoints(const std::string& s) {
  size_t count = 0;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i];
    if ((c & 0x80) == 0) {
      i += 1;
    } else if ((c & 0xE0) == 0xC0) {
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      i += 3;
    } else if ((c & 0xF8) == 0xF0) {
      i += 4;
    } else {
      i += 1;  // Invalid UTF-8, skip byte
    }
    count++;
  }
  return count;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " '<json_params>'" << std::endl;
    return 1;
  }

  auto args = nlohmann::json::parse(argv[1]);
  size_t min_length = args["min_length"].get<size_t>();
  size_t max_length = args["max_length"].get<size_t>();
  int test_cases = conformance::get_test_cases();

  hegel::hegel(
      [=]() {
        auto gen =
            hegel::st::text({.min_size = min_length, .max_size = max_length});
        auto value = gen.generate();
        conformance::write_metrics({{"length", count_codepoints(value)}});
      },
      hegel::HegelOptions{.test_cases = test_cases});

  return 0;
}
