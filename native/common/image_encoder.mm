#include "image_encoder.h"

#include <Foundation/Foundation.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <stdexcept>

namespace crosscap {

namespace {

CFStringRef FormatToUti(const std::string& format) {
  if (format == "png") {
    return CFSTR("public.png");
  }
  if (format == "jpeg") {
    return CFSTR("public.jpeg");
  }
  if (format == "tiff") {
    return CFSTR("public.tiff");
  }
  if (format == "bmp") {
    return CFSTR("com.microsoft.bmp");
  }
  if (format == "webp") {
    return CFSTR("org.webmproject.webp");
  }

  throw std::runtime_error("Unsupported image format");
}

}  // namespace

std::vector<uint8_t> EncodeImage(CGImageRef image,
                                 const std::string& format,
                                 const std::optional<double>& quality) {
  CFStringRef uti = FormatToUti(format);
  NSMutableData* data = [NSMutableData data];
  if (data == nil) {
    throw std::runtime_error("Failed to allocate image buffer");
  }

  CGImageDestinationRef destination =
      CGImageDestinationCreateWithData((__bridge CFMutableDataRef)data, uti, 1, nullptr);
  if (destination == nullptr) {
    throw std::runtime_error("Failed to create image destination");
  }

  CFMutableDictionaryRef properties =
      CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  if (properties == nullptr) {
    CFRelease(destination);
    throw std::runtime_error("Failed to create image properties");
  }

  if (quality.has_value() && (format == "jpeg" || format == "webp")) {
    const double clamped_quality =
        std::max(0.0, std::min(1.0, quality.value()));
    CFNumberRef quality_value =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &clamped_quality);
    if (quality_value != nullptr) {
      CFDictionarySetValue(properties, kCGImageDestinationLossyCompressionQuality,
                           quality_value);
      CFRelease(quality_value);
    }
  }

  CGImageDestinationAddImage(destination, image, properties);
  const bool finalized = CGImageDestinationFinalize(destination);
  CFRelease(properties);
  CFRelease(destination);

  if (!finalized) {
    throw std::runtime_error("Failed to encode image");
  }

  const uint8_t* bytes = static_cast<const uint8_t*>([data bytes]);
  return std::vector<uint8_t>(bytes, bytes + [data length]);
}

}  // namespace crosscap
