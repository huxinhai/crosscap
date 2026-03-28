#include <napi.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "screen_service.h"

namespace {

Napi::Object ToDisplayObject(Napi::Env env,
                             const crosscap::DisplayInfo& display) {
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("id", Napi::String::New(env, display.id));
  obj.Set("index", Napi::Number::New(env, display.index));
  obj.Set("isPrimary", Napi::Boolean::New(env, display.is_primary));
  obj.Set("width", Napi::Number::New(env, display.width));
  obj.Set("height", Napi::Number::New(env, display.height));
  obj.Set("scaleFactor", Napi::Number::New(env, display.scale_factor));
  obj.Set("rotation", Napi::Number::New(env, display.rotation));
  obj.Set("originX", Napi::Number::New(env, display.origin_x));
  obj.Set("originY", Napi::Number::New(env, display.origin_y));
  if (!display.name.empty()) {
    obj.Set("name", Napi::String::New(env, display.name));
  }
  return obj;
}

Napi::Object ToBitmapObject(Napi::Env env,
                            const crosscap::BitmapResult& bitmap) {
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("displayId", Napi::String::New(env, bitmap.display_id));
  obj.Set("width", Napi::Number::New(env, bitmap.width));
  obj.Set("height", Napi::Number::New(env, bitmap.height));
  obj.Set("scaleFactor", Napi::Number::New(env, bitmap.scale_factor));
  obj.Set("pixelFormat", Napi::String::New(env, bitmap.pixel_format));
  obj.Set("channels", Napi::Number::New(env, bitmap.channels));
  obj.Set("bitsPerChannel", Napi::Number::New(env, bitmap.bits_per_channel));
  obj.Set("bytesPerRow", Napi::Number::New(env, bitmap.bytes_per_row));
  obj.Set("byteLength", Napi::Number::New(env, bitmap.data.size()));
  obj.Set("data", Napi::Buffer<char>::Copy(
                      env,
                      reinterpret_cast<const char*>(bitmap.data.data()),
                      bitmap.data.size()));
  return obj;
}

Napi::Object ToCaptureObject(Napi::Env env,
                             const crosscap::CaptureResult& capture) {
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("displayId", Napi::String::New(env, capture.display_id));
  obj.Set("format", Napi::String::New(env, capture.format));
  obj.Set("width", Napi::Number::New(env, capture.width));
  obj.Set("height", Napi::Number::New(env, capture.height));
  obj.Set("scaleFactor", Napi::Number::New(env, capture.scale_factor));
  obj.Set("byteLength", Napi::Number::New(env, capture.data.size()));
  obj.Set("data",
          Napi::Buffer<uint8_t>::Copy(env, capture.data.data(), capture.data.size()));
  return obj;
}

std::optional<std::string> ReadOptionalDisplayId(const Napi::Value& value) {
  if (!value.IsObject()) {
    return std::nullopt;
  }

  Napi::Object object = value.As<Napi::Object>();
  if (!object.Has("displayId")) {
    return std::nullopt;
  }

  Napi::Value display_id = object.Get("displayId");
  if (display_id.IsUndefined() || display_id.IsNull()) {
    return std::nullopt;
  }
  if (!display_id.IsString()) {
    throw std::runtime_error("displayId must be a string");
  }

  return display_id.As<Napi::String>().Utf8Value();
}

crosscap::CaptureOptions ReadCaptureOptions(const Napi::CallbackInfo& info) {
  crosscap::CaptureOptions options;
  options.format = "png";

  if (info.Length() == 0 || info[0].IsUndefined() || info[0].IsNull()) {
    return options;
  }

  if (!info[0].IsObject()) {
    throw std::runtime_error("capture options must be an object");
  }

  Napi::Object object = info[0].As<Napi::Object>();
  options.display_id = ReadOptionalDisplayId(object);

  if (object.Has("format")) {
    Napi::Value format = object.Get("format");
    if (!format.IsUndefined() && !format.IsNull()) {
      if (!format.IsString()) {
        throw std::runtime_error("format must be a string");
      }
      options.format = format.As<Napi::String>().Utf8Value();
    }
  }

  if (object.Has("quality")) {
    Napi::Value quality = object.Get("quality");
    if (!quality.IsUndefined() && !quality.IsNull()) {
      if (!quality.IsNumber()) {
        throw std::runtime_error("quality must be a number");
      }
      options.quality = quality.As<Napi::Number>().DoubleValue();
    }
  }

  return options;
}

crosscap::CaptureBitmapOptions ReadCaptureBitmapOptions(
    const Napi::CallbackInfo& info) {
  crosscap::CaptureBitmapOptions options;

  if (info.Length() == 0 || info[0].IsUndefined() || info[0].IsNull()) {
    return options;
  }

  if (!info[0].IsObject()) {
    throw std::runtime_error("captureBitmap options must be an object");
  }

  Napi::Object object = info[0].As<Napi::Object>();
  options.display_id = ReadOptionalDisplayId(object);
  return options;
}

crosscap::CaptureRegionOptions ReadCaptureRegionOptions(
    const Napi::CallbackInfo& info) {
  if (info.Length() == 0 || info[0].IsUndefined() || info[0].IsNull() ||
      !info[0].IsObject()) {
    throw std::runtime_error("captureRegion options must be an object");
  }

  Napi::Object object = info[0].As<Napi::Object>();
  const auto read_number = [&](const char* key) -> double {
    if (!object.Has(key) || !object.Get(key).IsNumber()) {
      throw std::runtime_error(std::string(key) + " must be a number");
    }
    return object.Get(key).As<Napi::Number>().DoubleValue();
  };

  crosscap::CaptureRegionOptions options;
  options.x = read_number("x");
  options.y = read_number("y");
  options.width = read_number("width");
  options.height = read_number("height");
  options.format = "png";

  if (object.Has("format")) {
    Napi::Value format = object.Get("format");
    if (!format.IsUndefined() && !format.IsNull()) {
      if (!format.IsString()) {
        throw std::runtime_error("format must be a string");
      }
      options.format = format.As<Napi::String>().Utf8Value();
    }
  }

  if (object.Has("quality")) {
    Napi::Value quality = object.Get("quality");
    if (!quality.IsUndefined() && !quality.IsNull()) {
      if (!quality.IsNumber()) {
        throw std::runtime_error("quality must be a number");
      }
      options.quality = quality.As<Napi::Number>().DoubleValue();
    }
  }

  return options;
}

class ListDisplaysWorker : public Napi::AsyncWorker {
 public:
  explicit ListDisplaysWorker(const Napi::Env& env)
      : Napi::AsyncWorker(env), deferred_(Napi::Promise::Deferred::New(env)) {}

  void Execute() override {
    try {
      displays_ = crosscap::ListDisplays();
    } catch (const std::exception& error) {
      SetError(error.what());
    }
  }

  void OnOK() override {
    Napi::HandleScope scope(Env());
    Napi::Array result = Napi::Array::New(Env(), displays_.size());
    for (size_t index = 0; index < displays_.size(); ++index) {
      result.Set(index, ToDisplayObject(Env(), displays_[index]));
    }
    deferred_.Resolve(result);
  }

  void OnError(const Napi::Error& error) override {
    deferred_.Reject(error.Value());
  }

  Napi::Promise Promise() const { return deferred_.Promise(); }

 private:
  Napi::Promise::Deferred deferred_;
  std::vector<crosscap::DisplayInfo> displays_;
};

Napi::Value ListDisplaysWrapped(const Napi::CallbackInfo& info) {
  auto* worker = new ListDisplaysWorker(info.Env());
  Napi::Promise promise = worker->Promise();
  worker->Queue();
  return promise;
}

Napi::Value CaptureWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  try {
    crosscap::CaptureOptions options = ReadCaptureOptions(info);
    crosscap::CaptureResult result = crosscap::Capture(options);
    deferred.Resolve(ToCaptureObject(env, result));
  } catch (const std::exception& error) {
    deferred.Reject(Napi::Error::New(env, error.what()).Value());
  }

  return deferred.Promise();
}

Napi::Value CaptureBitmapWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  try {
    crosscap::CaptureBitmapOptions options = ReadCaptureBitmapOptions(info);
    crosscap::BitmapResult result = crosscap::CaptureBitmap(options);
    deferred.Resolve(ToBitmapObject(env, result));
  } catch (const std::exception& error) {
    deferred.Reject(Napi::Error::New(env, error.what()).Value());
  }

  return deferred.Promise();
}

Napi::Value CaptureRegionWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  try {
    crosscap::CaptureRegionOptions options = ReadCaptureRegionOptions(info);
    crosscap::CaptureResult result = crosscap::CaptureRegion(options);
    deferred.Resolve(ToCaptureObject(env, result));
  } catch (const std::exception& error) {
    deferred.Reject(Napi::Error::New(env, error.what()).Value());
  }

  return deferred.Promise();
}

Napi::Value GetPermissionStatusWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  const crosscap::PermissionState state = crosscap::GetPermissionStatus();
  Napi::Object result = Napi::Object::New(env);

  switch (state) {
    case crosscap::PermissionState::kGranted:
      result.Set("screenRecording", "granted");
      break;
    case crosscap::PermissionState::kDenied:
      result.Set("screenRecording", "denied");
      break;
    case crosscap::PermissionState::kNotDetermined:
      result.Set("screenRecording", "not-determined");
      break;
    case crosscap::PermissionState::kUnknown:
    default:
      result.Set("screenRecording", "unknown");
      break;
  }

  deferred.Resolve(result);
  return deferred.Promise();
}

Napi::Value RequestPermissionWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  const crosscap::PermissionState state = crosscap::RequestPermission();
  Napi::Object result = Napi::Object::New(env);

  switch (state) {
    case crosscap::PermissionState::kGranted:
      result.Set("screenRecording", "granted");
      break;
    case crosscap::PermissionState::kDenied:
      result.Set("screenRecording", "denied");
      break;
    case crosscap::PermissionState::kNotDetermined:
      result.Set("screenRecording", "not-determined");
      break;
    case crosscap::PermissionState::kUnknown:
    default:
      result.Set("screenRecording", "unknown");
      break;
  }

  deferred.Resolve(result);
  return deferred.Promise();
}

Napi::Value OpenSystemSettingsWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  try {
    crosscap::OpenSystemSettings();
    deferred.Resolve(env.Undefined());
  } catch (const std::exception& error) {
    deferred.Reject(Napi::Error::New(env, error.what()).Value());
  }

  return deferred.Promise();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("listDisplays", Napi::Function::New(env, ListDisplaysWrapped));
  exports.Set("capture", Napi::Function::New(env, CaptureWrapped));
  exports.Set("captureBitmap",
              Napi::Function::New(env, CaptureBitmapWrapped));
  exports.Set("captureRegion",
              Napi::Function::New(env, CaptureRegionWrapped));
  exports.Set("getPermissionStatus",
              Napi::Function::New(env, GetPermissionStatusWrapped));
  exports.Set("requestPermission",
              Napi::Function::New(env, RequestPermissionWrapped));
  exports.Set("openSystemSettings",
              Napi::Function::New(env, OpenSystemSettingsWrapped));
  return exports;
}

}  // namespace

NODE_API_MODULE(crosscap, Init)
