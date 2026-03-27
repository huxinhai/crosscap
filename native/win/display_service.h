#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

#include "../common/types.h"

namespace crosscap {

struct DisplayRecord {
  DisplayInfo info;
  RECT bounds;
  std::wstring device_name;
};

std::vector<DisplayInfo> ListDisplays();
DisplayRecord GetDisplayRecordOrThrow(
    const std::optional<std::string>& display_id);

}  // namespace crosscap
