#pragma once

#include <CoreGraphics/CoreGraphics.h>

#include "../common/types.h"

namespace crosscap {

BitmapResult CaptureBitmap(const CaptureBitmapOptions& options);
CaptureResult Capture(const CaptureOptions& options);
CaptureResult CaptureRegion(const CaptureRegionOptions& options);

}  // namespace crosscap
