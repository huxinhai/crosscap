#pragma once

#include <CoreGraphics/CoreGraphics.h>

#include <optional>
#include <string>
#include <vector>

namespace crosscap {

std::vector<uint8_t> EncodeImage(CGImageRef image,
                                 const std::string& format,
                                 const std::optional<double>& quality);

}  // namespace crosscap
