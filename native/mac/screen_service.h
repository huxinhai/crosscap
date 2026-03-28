#pragma once

#include "../common/types.h"

namespace crosscap {

std::vector<DisplayInfo> ListDisplays();
BitmapResult CaptureBitmap(const CaptureBitmapOptions& options);
CaptureResult Capture(const CaptureOptions& options);
CaptureResult CaptureRegion(const CaptureRegionOptions& options);
PermissionState GetPermissionStatus();
PermissionState RequestPermission();
void OpenSystemSettings();

}  // namespace crosscap
