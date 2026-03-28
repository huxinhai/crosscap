#include "permission_service.h"

#include <AppKit/AppKit.h>
#include <CoreGraphics/CGWindow.h>

#include <stdexcept>

namespace crosscap {

PermissionState GetPermissionStatus() {
  if (@available(macOS 10.15, *)) {
    return CGPreflightScreenCaptureAccess() ? PermissionState::kGranted
                                            : PermissionState::kNotDetermined;
  }

  return PermissionState::kGranted;
}

PermissionState RequestPermission() {
  if (@available(macOS 10.15, *)) {
    if (CGPreflightScreenCaptureAccess()) {
      return PermissionState::kGranted;
    }

    return CGRequestScreenCaptureAccess() ? PermissionState::kGranted
                                          : PermissionState::kDenied;
  }

  return PermissionState::kGranted;
}

void EnsureCapturePermission() {
  const PermissionState state = GetPermissionStatus();
  if (state == PermissionState::kGranted) {
    return;
  }

  throw std::runtime_error(
      "Screen recording permission is required to capture the display");
}

void OpenSystemSettings() {
  NSURL* url = [NSURL
      URLWithString:
          @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture"];
  if (url == nil || ![[NSWorkspace sharedWorkspace] openURL:url]) {
    throw std::runtime_error("Failed to open Screen Recording settings");
  }
}

}  // namespace crosscap
