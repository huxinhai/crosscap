#include "image_encoder.h"

#include <objidl.h>
#include <wincodec.h>
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace crosscap {

namespace {

class ComScope {
 public:
  ComScope() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

  ~ComScope() {
    if (SUCCEEDED(result_)) {
      CoUninitialize();
    }
  }

 private:
  HRESULT result_;
};

template <typename T>
class ComPtr {
 public:
  ComPtr() : ptr_(nullptr) {}
  ~ComPtr() {
    if (ptr_ != nullptr) {
      ptr_->Release();
    }
  }

  T** operator&() { return &ptr_; }
  T* Get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  void Attach(T* value) {
    if (ptr_ != nullptr) {
      ptr_->Release();
    }
    ptr_ = value;
  }

 private:
  T* ptr_;
};

const GUID& FormatToContainerGuid(const std::string& format) {
  if (format == "png") {
    return GUID_ContainerFormatPng;
  }
  if (format == "jpeg") {
    return GUID_ContainerFormatJpeg;
  }
  if (format == "tiff") {
    return GUID_ContainerFormatTiff;
  }
  if (format == "bmp") {
    return GUID_ContainerFormatBmp;
  }
#ifdef GUID_ContainerFormatWebp
  if (format == "webp") {
    return GUID_ContainerFormatWebp;
  }
#endif

  throw std::runtime_error("Unsupported image format on Windows");
}

void CheckHr(HRESULT hr, const char* message) {
  if (FAILED(hr)) {
    throw std::runtime_error(message);
  }
}

}  // namespace

std::vector<uint8_t> EncodeBitmapToImage(
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t bytes_per_row,
    const std::string& format,
    const std::optional<double>& quality) {
  ComScope com_scope;
  static_cast<void>(com_scope);

  ComPtr<IWICImagingFactory> factory;
  CheckHr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)),
          "Failed to create WIC imaging factory");

  IStream* stream = nullptr;
  CheckHr(CreateStreamOnHGlobal(nullptr, TRUE, &stream),
          "Failed to create output stream");
  ComPtr<IStream> stream_ptr;
  stream_ptr.Attach(stream);

  ComPtr<IWICBitmapEncoder> encoder;
  const GUID& container_guid = FormatToContainerGuid(format);
  CheckHr(factory->CreateEncoder(container_guid, nullptr, &encoder),
          "Failed to create bitmap encoder");
  CheckHr(encoder->Initialize(stream_ptr.Get(), WICBitmapEncoderNoCache),
          "Failed to initialize bitmap encoder");

  ComPtr<IWICBitmapFrameEncode> frame;
  ComPtr<IPropertyBag2> properties;
  CheckHr(encoder->CreateNewFrame(&frame, &properties),
          "Failed to create bitmap frame");

  if (properties.Get() != nullptr && quality.has_value() && format == "jpeg") {
    PROPBAG2 property = {};
    property.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_R4;
    value.fltVal = static_cast<float>(
        std::max(0.0, std::min(1.0, quality.value())));
    properties->Write(1, &property, &value);
    VariantClear(&value);
  }

  CheckHr(frame->Initialize(properties.Get()),
          "Failed to initialize bitmap frame");
  CheckHr(frame->SetSize(width, height), "Failed to set frame size");

  WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
  CheckHr(frame->SetPixelFormat(&pixel_format), "Failed to set pixel format");

  CheckHr(frame->WritePixels(height, bytes_per_row,
                             bytes_per_row * height,
                             const_cast<BYTE*>(pixels)),
          "Failed to write pixels");
  CheckHr(frame->Commit(), "Failed to commit bitmap frame");
  CheckHr(encoder->Commit(), "Failed to commit bitmap encoder");

  HGLOBAL global_handle = nullptr;
  CheckHr(GetHGlobalFromStream(stream_ptr.Get(), &global_handle),
          "Failed to read encoded output");

  const SIZE_T size = GlobalSize(global_handle);
  if (size == 0) {
    return {};
  }

  void* locked_memory = GlobalLock(global_handle);
  if (locked_memory == nullptr) {
    throw std::runtime_error("Failed to lock encoded image buffer");
  }

  std::vector<uint8_t> data(
      static_cast<uint8_t*>(locked_memory),
      static_cast<uint8_t*>(locked_memory) + size);
  GlobalUnlock(global_handle);
  return data;
}

}  // namespace crosscap
