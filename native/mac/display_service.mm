#include "display_service.h"

#include <AppKit/AppKit.h>

#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace crosscap {

namespace {

double GetScaleFactor(CGDirectDisplayID display_id) {
  const size_t pixel_width = CGDisplayPixelsWide(display_id);
  const CGRect bounds = CGDisplayBounds(display_id);
  if (bounds.size.width <= 0) {
    return 1.0;
  }

  return static_cast<double>(pixel_width) / bounds.size.width;
}

double GetRotation(CGDirectDisplayID display_id) {
  return CGDisplayRotation(display_id);
}

std::string GetDisplayName(CGDirectDisplayID display_id) {
  if (@available(macOS 10.15, *)) {
    NSNumber* screen_number = @(display_id);
    for (NSScreen* screen in [NSScreen screens]) {
      NSNumber* candidate = screen.deviceDescription[@"NSScreenNumber"];
      if (candidate != nil && [candidate isEqualToNumber:screen_number]) {
        NSString* localized_name = screen.localizedName;
        if (localized_name != nil) {
          return std::string([localized_name UTF8String]);
        }
      }
    }
  }

  return "";
}

}  // namespace

CGDirectDisplayID ParseDisplayId(const std::optional<std::string>& display_id) {
  if (!display_id.has_value() || display_id->empty()) {
    return CGMainDisplayID();
  }

  char* end_ptr = nullptr;
  const unsigned long parsed_value = std::strtoul(display_id->c_str(), &end_ptr, 10);
  if (end_ptr == display_id->c_str() || *end_ptr != '\0' ||
      parsed_value > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("Invalid displayId");
  }

  return static_cast<CGDirectDisplayID>(parsed_value);
}

std::vector<DisplayInfo> ListDisplays() {
  uint32_t count = 0;
  CGError error = CGGetActiveDisplayList(0, nullptr, &count);
  if (error != kCGErrorSuccess || count == 0) {
    return {};
  }

  std::vector<CGDirectDisplayID> display_ids(count);
  error = CGGetActiveDisplayList(count, display_ids.data(), &count);
  if (error != kCGErrorSuccess) {
    return {};
  }

  const CGDirectDisplayID main_display_id = CGMainDisplayID();
  std::vector<DisplayInfo> displays;
  displays.reserve(count);

  for (uint32_t index = 0; index < count; ++index) {
    const CGDirectDisplayID display_id = display_ids[index];
    const CGRect bounds = CGDisplayBounds(display_id);

    DisplayInfo info;
    info.id = std::to_string(display_id);
    info.index = index;
    info.is_primary = (display_id == main_display_id);
    info.width = static_cast<uint32_t>(CGDisplayPixelsWide(display_id));
    info.height = static_cast<uint32_t>(CGDisplayPixelsHigh(display_id));
    info.scale_factor = GetScaleFactor(display_id);
    info.rotation = GetRotation(display_id);
    info.origin_x = bounds.origin.x;
    info.origin_y = bounds.origin.y;
    info.name = GetDisplayName(display_id);

    displays.push_back(std::move(info));
  }

  return displays;
}

DisplayInfo GetDisplayInfoOrThrow(CGDirectDisplayID display_id) {
  const std::vector<DisplayInfo> displays = ListDisplays();
  for (const DisplayInfo& display : displays) {
    if (display.id == std::to_string(display_id)) {
      return display;
    }
  }

  throw std::runtime_error("Display not found");
}

}  // namespace crosscap
