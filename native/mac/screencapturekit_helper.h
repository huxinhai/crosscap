#pragma once

#include <CoreGraphics/CoreGraphics.h>

namespace crosscap {

bool ShouldPreferScreenCaptureKit();
CGImageRef TryCopyDisplayImageWithScreenCaptureKit(CGDirectDisplayID display_id);
CGImageRef TryCopyRectImageWithScreenCaptureKit(CGRect rect);

}  // namespace crosscap
