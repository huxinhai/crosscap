#pragma once

#include <optional>
#include <string>
#include <vector>

namespace crosscap {

std::vector<uint8_t> EncodeBitmapToImage(
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t bytes_per_row,
    const std::string& format,
    const std::optional<double>& quality);

}  // namespace crosscap
