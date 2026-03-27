#include "capture_service.h"

#include <windows.h>

#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "display_service.h"
#include "image_encoder.h"
#include "permission_service.h"

namespace crosscap {

namespace {

class ScopedDc {
 public:
  explicit ScopedDc(HDC dc) : dc_(dc) {}
  ~ScopedDc() {
    if (dc_ != nullptr) {
      DeleteDC(dc_);
    }
  }

  HDC Get() const { return dc_; }

 private:
  HDC dc_;
};

class ScopedBitmap {
 public:
  explicit ScopedBitmap(HBITMAP bitmap) : bitmap_(bitmap) {}
  ~ScopedBitmap() {
    if (bitmap_ != nullptr) {
      DeleteObject(bitmap_);
    }
  }

  HBITMAP Get() const { return bitmap_; }

 private:
  HBITMAP bitmap_;
};

BitmapResult CaptureBitmapForRect(const RECT& rect,
                                  const std::string& display_id,
                                  double scale_factor) {
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("Capture region is empty");
  }

  HDC screen_dc = GetDC(nullptr);
  if (screen_dc == nullptr) {
    throw std::runtime_error("Failed to create screen device context");
  }

  HDC memory_dc = CreateCompatibleDC(screen_dc);
  if (memory_dc == nullptr) {
    ReleaseDC(nullptr, screen_dc);
    throw std::runtime_error("Failed to create memory device context");
  }
  ScopedDc scoped_memory_dc(memory_dc);

  BITMAPINFO bitmap_info = {};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bitmap == nullptr || bits == nullptr) {
    ReleaseDC(nullptr, screen_dc);
    throw std::runtime_error("Failed to create bitmap section");
  }
  ScopedBitmap scoped_bitmap(bitmap);

  HGDIOBJ previous = SelectObject(memory_dc, bitmap);
  if (previous == nullptr) {
    ReleaseDC(nullptr, screen_dc);
    throw std::runtime_error("Failed to select capture bitmap");
  }

  if (!BitBlt(memory_dc, 0, 0, width, height, screen_dc, rect.left, rect.top,
              SRCCOPY | CAPTUREBLT)) {
    SelectObject(memory_dc, previous);
    ReleaseDC(nullptr, screen_dc);
    throw std::runtime_error("Failed to capture screen pixels");
  }

  SelectObject(memory_dc, previous);
  ReleaseDC(nullptr, screen_dc);

  const uint32_t bytes_per_row = static_cast<uint32_t>(width * 4);
  const size_t byte_length = static_cast<size_t>(bytes_per_row) *
                             static_cast<size_t>(height);
  std::vector<uint8_t> data(byte_length);
  std::memcpy(data.data(), bits, byte_length);

  BitmapResult result;
  result.display_id = display_id;
  result.width = static_cast<uint32_t>(width);
  result.height = static_cast<uint32_t>(height);
  result.scale_factor = scale_factor;
  result.pixel_format = "bgra";
  result.channels = 4;
  result.bits_per_channel = 8;
  result.bytes_per_row = bytes_per_row;
  result.data = std::move(data);
  return result;
}

CaptureResult CaptureEncodedFromBitmap(const BitmapResult& bitmap,
                                       const std::string& format,
                                       const std::optional<double>& quality) {
  CaptureResult result;
  result.display_id = bitmap.display_id;
  result.format = format;
  result.width = bitmap.width;
  result.height = bitmap.height;
  result.scale_factor = bitmap.scale_factor;
  result.data = EncodeBitmapToImage(bitmap.data.data(), bitmap.width, bitmap.height,
                                    bitmap.bytes_per_row, format, quality);
  return result;
}

}  // namespace

BitmapResult CaptureBitmap(const CaptureBitmapOptions& options) {
  EnsureCapturePermission();
  const DisplayRecord display = GetDisplayRecordOrThrow(options.display_id);
  return CaptureBitmapForRect(display.bounds, display.info.id,
                              display.info.scale_factor);
}

CaptureResult Capture(const CaptureOptions& options) {
  EnsureCapturePermission();
  const DisplayRecord display = GetDisplayRecordOrThrow(options.display_id);
  const BitmapResult bitmap =
      CaptureBitmapForRect(display.bounds, display.info.id, display.info.scale_factor);
  const std::string format = options.format.empty() ? "png" : options.format;
  return CaptureEncodedFromBitmap(bitmap, format, options.quality);
}

CaptureResult CaptureRegion(const CaptureRegionOptions& options) {
  EnsureCapturePermission();
  if (options.width <= 0 || options.height <= 0) {
    throw std::runtime_error("captureRegion width and height must be greater than 0");
  }

  RECT rect = {
      static_cast<LONG>(options.x),
      static_cast<LONG>(options.y),
      static_cast<LONG>(options.x + options.width),
      static_cast<LONG>(options.y + options.height),
  };
  const BitmapResult bitmap = CaptureBitmapForRect(rect, "region", 1.0);
  const std::string format = options.format.empty() ? "png" : options.format;
  return CaptureEncodedFromBitmap(bitmap, format, options.quality);
}

}  // namespace crosscap
