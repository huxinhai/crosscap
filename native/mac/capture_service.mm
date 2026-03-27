#include "capture_service.h"

#include <CoreGraphics/CoreGraphics.h>
#include <CoreGraphics/CGWindow.h>

#include <stdexcept>

#include "../common/image_encoder.h"
#include "display_service.h"
#include "permission_service.h"
#include "screencapturekit_helper.h"

namespace crosscap {

namespace {

CGImageRef CopyDisplayImageFallback(CGDirectDisplayID display_id) {
  CGImageRef image = CGDisplayCreateImage(display_id);
  if (image == nullptr) {
    throw std::runtime_error("Failed to capture display image");
  }
  return image;
}

CGImageRef CopyRectImageFallback(CGRect rect) {
  CGImageRef image = CGWindowListCreateImage(
      rect, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
  if (image == nullptr) {
    throw std::runtime_error("Failed to capture region image");
  }
  return image;
}

CGImageRef CopyDisplayImage(CGDirectDisplayID display_id) {
  EnsureCapturePermission();

  if (CGImageRef image = TryCopyDisplayImageWithScreenCaptureKit(display_id)) {
    return image;
  }

  return CopyDisplayImageFallback(display_id);
}

CGImageRef CopyRectImage(CGRect rect) {
  EnsureCapturePermission();

  if (CGImageRef image = TryCopyRectImageWithScreenCaptureKit(rect)) {
    return image;
  }

  return CopyRectImageFallback(rect);
}

BitmapResult BitmapFromImage(CGImageRef image, CGDirectDisplayID display_id) {
  const size_t width = CGImageGetWidth(image);
  const size_t height = CGImageGetHeight(image);
  const size_t bytes_per_row = width * 4;

  if (width == 0 || height == 0) {
    throw std::runtime_error("Captured image is empty");
  }

  std::vector<uint8_t> buffer(bytes_per_row * height);
  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  if (color_space == nullptr) {
    throw std::runtime_error("Failed to create color space");
  }

  const CGBitmapInfo bitmap_info =
      static_cast<CGBitmapInfo>(
          static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) |
          static_cast<uint32_t>(kCGBitmapByteOrder32Little));
  CGContextRef context =
      CGBitmapContextCreate(buffer.data(), width, height, 8, bytes_per_row,
                            color_space, bitmap_info);
  CGColorSpaceRelease(color_space);

  if (context == nullptr) {
    throw std::runtime_error("Failed to create bitmap context");
  }

  CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
  CGContextRelease(context);

  const DisplayInfo display_info = GetDisplayInfoOrThrow(display_id);

  BitmapResult result;
  result.display_id = std::to_string(display_id);
  result.width = static_cast<uint32_t>(width);
  result.height = static_cast<uint32_t>(height);
  result.scale_factor = display_info.scale_factor;
  result.pixel_format = "bgra";
  result.channels = 4;
  result.bits_per_channel = 8;
  result.bytes_per_row = static_cast<uint32_t>(bytes_per_row);
  result.data = std::move(buffer);
  return result;
}

}  // namespace

BitmapResult CaptureBitmap(const CaptureBitmapOptions& options) {
  const CGDirectDisplayID display_id = ParseDisplayId(options.display_id);
  EnsureCapturePermission();
  CGImageRef image = CopyDisplayImageFallback(display_id);

  try {
    BitmapResult result = BitmapFromImage(image, display_id);
    CGImageRelease(image);
    return result;
  } catch (...) {
    CGImageRelease(image);
    throw;
  }
}

CaptureResult Capture(const CaptureOptions& options) {
  const CGDirectDisplayID display_id = ParseDisplayId(options.display_id);
  const std::string format = options.format.empty() ? "png" : options.format;
  CGImageRef image = CopyDisplayImage(display_id);

  try {
    const DisplayInfo display_info = GetDisplayInfoOrThrow(display_id);
    CaptureResult result;
    result.display_id = std::to_string(display_id);
    result.format = format;
    result.width = static_cast<uint32_t>(CGImageGetWidth(image));
    result.height = static_cast<uint32_t>(CGImageGetHeight(image));
    result.scale_factor = display_info.scale_factor;
    result.data = EncodeImage(image, format, options.quality);
    CGImageRelease(image);
    return result;
  } catch (...) {
    CGImageRelease(image);
    throw;
  }
}

CaptureResult CaptureRegion(const CaptureRegionOptions& options) {
  if (options.width <= 0 || options.height <= 0) {
    throw std::runtime_error("captureRegion width and height must be greater than 0");
  }

  const CGRect rect = CGRectMake(options.x, options.y, options.width, options.height);
  const std::string format = options.format.empty() ? "png" : options.format;
  CGImageRef image = CopyRectImage(rect);

  try {
    CaptureResult result;
    result.display_id = "region";
    result.format = format;
    result.width = static_cast<uint32_t>(CGImageGetWidth(image));
    result.height = static_cast<uint32_t>(CGImageGetHeight(image));
    result.scale_factor = 1.0;
    result.data = EncodeImage(image, format, options.quality);
    CGImageRelease(image);
    return result;
  } catch (...) {
    CGImageRelease(image);
    throw;
  }
}

}  // namespace crosscap
