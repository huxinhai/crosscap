import assert from "node:assert/strict";
import {
  capture,
  captureBitmap,
  captureRegion,
  getPermissionStatus,
  listDisplays,
} from "../dist/index.js";

async function run() {
  const displays = await listDisplays();
  assert.ok(Array.isArray(displays));
  assert.ok(displays.length >= 1);

  for (const display of displays) {
    assert.equal(typeof display.id, "string");
    assert.equal(typeof display.index, "number");
    assert.equal(typeof display.isPrimary, "boolean");
    assert.ok(display.width > 0);
    assert.ok(display.height > 0);
  }

  const status = await getPermissionStatus();
  assert.ok(
    ["granted", "denied", "not-determined", "unknown"].includes(
      status.screenRecording,
    ),
  );

  if (status.screenRecording === "granted") {
    const image = await capture({ format: "png" });
    assert.equal(image.format, "png");
    assert.ok(image.byteLength > 0);
    assert.equal(image.byteLength, image.data.length);

    const bitmap = await captureBitmap();
    assert.equal(bitmap.pixelFormat, "bgra");
    assert.equal(bitmap.channels, 4);
    assert.equal(bitmap.bitsPerChannel, 8);
    assert.ok(bitmap.byteLength > 0);
    assert.equal(bitmap.byteLength, bitmap.data.length);
    assert.ok(bitmap.bytesPerRow >= bitmap.width * 4);

    const region = await captureRegion({
      x: 0,
      y: 0,
      width: Math.min(200, image.width),
      height: Math.min(120, image.height),
      format: "png",
    });
    assert.equal(region.format, "png");
    assert.ok(region.byteLength > 0);
    assert.equal(region.byteLength, region.data.length);
  }

  console.log("[crosscap] smoke checks passed");
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
