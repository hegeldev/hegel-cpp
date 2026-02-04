#include <hegel/base64.h>

namespace hegel::detail {

    constexpr char base64_alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string base64_encode(const std::vector<uint8_t>& input) {
        std::string result;
        result.reserve((input.size() + 2) / 3 * 4);

        for (size_t i = 0; i < input.size(); i += 3) {
            // 3 bytes (3x8=24 bits) -> 4 base64 chars (4x6=24 bits)
            uint8_t b0 = input[i];
            uint8_t b1 = (i + 1 < input.size()) ? input[i + 1] : 0;
            uint8_t b2 = (i + 2 < input.size()) ? input[i + 2] : 0;

            result.push_back(base64_alphabet[b0 >> 2]);
            result.push_back(base64_alphabet[((b0 & 0x03) << 4) | (b1 >> 4)]);

            if (i + 1 < input.size()) {
                result.push_back(base64_alphabet[((b1 & 0x0F) << 2) | (b2 >> 6)]);
            } else {
                result.push_back('=');
            }

            if (i + 2 < input.size()) {
                result.push_back(base64_alphabet[b2 & 0x3F]);
            } else {
                result.push_back('=');
            }
        }

        return result;
    }

    inline int8_t base64_char_value(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    std::vector<uint8_t> base64_decode(const std::string& input) {
        std::vector<uint8_t> result;
        result.reserve((input.size() * 3) / 4);

        for (size_t i = 0; i + 4 <= input.size(); i += 4) {
            int8_t a = base64_char_value(input[i]);
            int8_t b = base64_char_value(input[i + 1]);
            int8_t c = base64_char_value(input[i + 2]);
            int8_t d = base64_char_value(input[i + 3]);

            // 4 base64 chars (4x6=24 bits) -> 3 bytes (3x8=24 bits)
            result.push_back((a << 2) | (b >> 4));
            if (input[i + 2] != '=') {
                result.push_back(((b & 0x0F) << 4) | (c >> 2));
            }
            if (input[i + 3] != '=') {
                result.push_back(((c & 0x03) << 6) | d);
            }
        }

        return result;
    }

}  // namespace hegel::detail
