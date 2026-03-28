#include "permission_service.h"

namespace crosscap {

PermissionState GetPermissionStatus() {
  return PermissionState::kGranted;
}

PermissionState RequestPermission() {
  return PermissionState::kGranted;
}

void EnsureCapturePermission() {}

void OpenSystemSettings() {}

}  // namespace crosscap
