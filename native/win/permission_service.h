#pragma once

#include "../common/types.h"

namespace crosscap {

PermissionState GetPermissionStatus();
PermissionState RequestPermission();
void EnsureCapturePermission();
void OpenSystemSettings();

}  // namespace crosscap
