#pragma once

#include <CoreGraphics/CoreGraphics.h>

#include <string>
#include <vector>

#include "../common/types.h"

namespace crosscap {

std::vector<DisplayInfo> ListDisplays();
DisplayInfo GetDisplayInfoOrThrow(CGDirectDisplayID display_id);
CGDirectDisplayID ParseDisplayId(const std::optional<std::string>& display_id);

}  // namespace crosscap
