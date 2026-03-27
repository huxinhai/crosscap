#include "display_service.h"

#include <ShellScalingApi.h>
#include <windows.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace crosscap {

namespace {

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return "";
  }

  const int size =
      WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                          nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    return "";
  }

  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

double GetScaleFactor(HMONITOR monitor) {
  UINT dpi_x = 96;
  UINT dpi_y = 96;
  const HRESULT result =
      GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
  if (FAILED(result) || dpi_x == 0) {
    return 1.0;
  }

  return static_cast<double>(dpi_x) / 96.0;
}

double GetRotation(const std::wstring& device_name) {
  DEVMODEW mode = {};
  mode.dmSize = sizeof(mode);
  if (!EnumDisplaySettingsExW(device_name.c_str(), ENUM_CURRENT_SETTINGS, &mode, 0)) {
    return 0.0;
  }

  switch (mode.dmDisplayOrientation) {
    case DMDO_90:
      return 90.0;
    case DMDO_180:
      return 180.0;
    case DMDO_270:
      return 270.0;
    case DMDO_DEFAULT:
    default:
      return 0.0;
  }
}

BOOL CALLBACK CollectDisplays(HMONITOR monitor,
                              HDC,
                              LPRECT,
                              LPARAM user_data) {
  auto* displays = reinterpret_cast<std::vector<DisplayRecord>*>(user_data);

  MONITORINFOEXW monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    return TRUE;
  }

  DISPLAY_DEVICEW display_device = {};
  display_device.cb = sizeof(display_device);
  EnumDisplayDevicesW(monitor_info.szDevice, 0, &display_device, 0);

  const RECT bounds = monitor_info.rcMonitor;
  DisplayRecord record;
  record.bounds = bounds;
  record.device_name = monitor_info.szDevice;
  record.info.id = WideToUtf8(monitor_info.szDevice);
  record.info.index = static_cast<uint32_t>(displays->size());
  record.info.is_primary = (monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0;
  record.info.width = static_cast<uint32_t>(bounds.right - bounds.left);
  record.info.height = static_cast<uint32_t>(bounds.bottom - bounds.top);
  record.info.scale_factor = GetScaleFactor(monitor);
  record.info.rotation = GetRotation(record.device_name);
  record.info.origin_x = bounds.left;
  record.info.origin_y = bounds.top;
  record.info.name = WideToUtf8(display_device.DeviceString);

  displays->push_back(std::move(record));
  return TRUE;
}

std::vector<DisplayRecord> ListDisplayRecords() {
  std::vector<DisplayRecord> displays;
  EnumDisplayMonitors(nullptr, nullptr, CollectDisplays,
                      reinterpret_cast<LPARAM>(&displays));
  return displays;
}

}  // namespace

std::vector<DisplayInfo> ListDisplays() {
  const std::vector<DisplayRecord> records = ListDisplayRecords();
  std::vector<DisplayInfo> displays;
  displays.reserve(records.size());

  for (const DisplayRecord& record : records) {
    displays.push_back(record.info);
  }

  return displays;
}

DisplayRecord GetDisplayRecordOrThrow(
    const std::optional<std::string>& display_id) {
  const std::vector<DisplayRecord> records = ListDisplayRecords();
  if (records.empty()) {
    throw std::runtime_error("No active displays found");
  }

  if (!display_id.has_value() || display_id->empty()) {
    for (const DisplayRecord& record : records) {
      if (record.info.is_primary) {
        return record;
      }
    }

    return records.front();
  }

  for (const DisplayRecord& record : records) {
    if (record.info.id == *display_id) {
      return record;
    }
  }

  throw std::runtime_error("Display not found");
}

}  // namespace crosscap
