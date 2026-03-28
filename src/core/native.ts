import path from "node:path";
import fs from "node:fs";

const load = require("node-gyp-build");

export type NativeBinding = {
  listDisplays: () => Promise<import("./types").DisplayInfo[]>;
  capture: (
    options?: import("./types").CaptureOptions,
  ) => Promise<import("./types").CaptureResult>;
  captureBitmap: (
    options?: import("./types").CaptureBitmapOptions,
  ) => Promise<import("./types").BitmapResult>;
  captureRegion: (
    options: import("./types").CaptureRegionOptions,
  ) => Promise<import("./types").CaptureResult>;
  getPermissionStatus: () => Promise<import("./types").PermissionStatus>;
  requestPermission: () => Promise<import("./types").PermissionStatus>;
  openSystemSettings: () => Promise<void>;
};

function loadNativeBinding(): NativeBinding {
  const packageRoot = path.resolve(__dirname, "..");
  const localCandidates = [
    path.resolve(packageRoot, "..", "build", "Release", "crosscap.node"),
    path.resolve(
      packageRoot,
      "..",
      "prebuilds",
      `${process.platform}-${process.arch}`,
      "crosscap.node",
    ),
  ];

  for (const candidate of localCandidates) {
    if (fs.existsSync(candidate)) {
      return require(candidate) as NativeBinding;
    }
  }

  return load(packageRoot) as NativeBinding;
}

const binding = loadNativeBinding();

export default binding;
