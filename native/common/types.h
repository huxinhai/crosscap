#pragma once

#include <optional>
#include <string>
#include <vector>

namespace crosscap {

enum class PermissionState {
  kGranted,
  kDenied,
  kNotDetermined,
  kUnknown,
};

struct DisplayInfo {
  std::string id;
  uint32_t index;
  bool is_primary;
  uint32_t width;
  uint32_t height;
  double scale_factor;
  double rotation;
  double origin_x;
  double origin_y;
  std::string name;
};

struct CaptureBitmapOptions {
  std::optional<std::string> display_id;
};

struct CaptureRegionOptions {
  double x;
  double y;
  double width;
  double height;
  std::string format;
  std::optional<double> quality;
};

struct BitmapResult {
  std::string display_id;
  uint32_t width;
  uint32_t height;
  double scale_factor;
  std::string pixel_format;
  uint32_t channels;
  uint32_t bits_per_channel;
  uint32_t bytes_per_row;
  std::vector<uint8_t> data;
};

struct CaptureOptions {
  std::optional<std::string> display_id;
  std::string format;
  std::optional<double> quality;
};

struct CaptureResult {
  std::string display_id;
  std::string format;
  uint32_t width;
  uint32_t height;
  double scale_factor;
  std::vector<uint8_t> data;
};

}  // namespace crosscap
