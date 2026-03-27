#pragma once

#include "../common/types.h"

namespace crosscap {

PermissionState GetPermissionStatus();
void EnsureCapturePermission();
void OpenSystemSettings();

}  // namespace crosscap
