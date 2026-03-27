export type ImageFormat = "png" | "jpeg" | "webp" | "tiff" | "bmp";

export interface DisplayInfo {
  id: string;
  index: number;
  isPrimary: boolean;
  width: number;
  height: number;
  scaleFactor: number;
  rotation: number;
  originX: number;
  originY: number;
  name?: string;
}

export interface CaptureOptions {
  displayId?: string;
  format?: ImageFormat;
  quality?: number;
}

export interface CaptureResult {
  displayId: string;
  format: ImageFormat;
  width: number;
  height: number;
  scaleFactor: number;
  byteLength: number;
  data: Buffer;
}

export interface CaptureBitmapOptions {
  displayId?: string;
}

export interface CaptureRegionOptions {
  x: number;
  y: number;
  width: number;
  height: number;
  format?: ImageFormat;
  quality?: number;
}

export interface BitmapResult {
  displayId: string;
  width: number;
  height: number;
  scaleFactor: number;
  pixelFormat: "bgra";
  channels: 4;
  bitsPerChannel: 8;
  bytesPerRow: number;
  byteLength: number;
  data: Buffer;
}

export interface PermissionStatus {
  screenRecording: "granted" | "denied" | "not-determined" | "unknown";
}
