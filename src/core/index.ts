import binding from "./native";

export type {
  BitmapResult,
  CaptureBitmapOptions,
  CaptureOptions,
  CaptureRegionOptions,
  CaptureResult,
  DisplayInfo,
  ImageFormat,
  PermissionStatus,
} from "./types";

export function listDisplays() {
  return binding.listDisplays();
}

export function capture(
  options: import("./types").CaptureOptions = {},
) {
  return binding.capture(options);
}

export function captureBitmap(
  options: import("./types").CaptureBitmapOptions = {},
) {
  return binding.captureBitmap(options);
}

export function captureRegion(
  options: import("./types").CaptureRegionOptions,
) {
  return binding.captureRegion(options);
}

export function getPermissionStatus() {
  return binding.getPermissionStatus();
}

export function requestPermission() {
  return binding.requestPermission();
}

export function openSystemSettings() {
  return binding.openSystemSettings();
}
