#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hegel::impl {

std::string base64_encode(const std::vector<uint8_t>& input);

std::vector<uint8_t> base64_decode(const std::string& input);

} // namespace hegel::impl
