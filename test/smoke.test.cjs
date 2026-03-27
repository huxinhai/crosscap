const test = require("node:test");
const assert = require("node:assert/strict");

const crosscap = require("../dist");

test("listDisplays returns at least one display", async () => {
  const displays = await crosscap.listDisplays();

  assert.ok(Array.isArray(displays));
  assert.ok(displays.length >= 1);

  for (const display of displays) {
    assert.equal(typeof display.id, "string");
    assert.equal(typeof display.index, "number");
    assert.equal(typeof display.isPrimary, "boolean");
    assert.equal(typeof display.width, "number");
    assert.equal(typeof display.height, "number");
    assert.equal(typeof display.scaleFactor, "number");
    assert.equal(typeof display.rotation, "number");
    assert.equal(typeof display.originX, "number");
    assert.equal(typeof display.originY, "number");
    assert.ok(display.width > 0);
    assert.ok(display.height > 0);
  }
});

test("permission status shape is stable", async () => {
  const status = await crosscap.getPermissionStatus();
  assert.ok(
    ["granted", "denied", "not-determined", "unknown"].includes(
      status.screenRecording,
    ),
  );
});

test("capture returns an encoded image when permission is granted", async (t) => {
  const status = await crosscap.getPermissionStatus();
  if (status.screenRecording !== "granted") {
    t.skip("screen recording permission is not granted in this environment");
    return;
  }

  const image = await crosscap.capture({ format: "png" });
  assert.equal(typeof image.displayId, "string");
  assert.equal(image.format, "png");
  assert.ok(image.width > 0);
  assert.ok(image.height > 0);
  assert.ok(Buffer.isBuffer(image.data));
  assert.equal(image.byteLength, image.data.length);
});

test("captureBitmap returns raw BGRA data when permission is granted", async (t) => {
  const status = await crosscap.getPermissionStatus();
  if (status.screenRecording !== "granted") {
    t.skip("screen recording permission is not granted in this environment");
    return;
  }

  const bitmap = await crosscap.captureBitmap();
  assert.equal(typeof bitmap.displayId, "string");
  assert.equal(bitmap.pixelFormat, "bgra");
  assert.equal(bitmap.channels, 4);
  assert.equal(bitmap.bitsPerChannel, 8);
  assert.ok(bitmap.width > 0);
  assert.ok(bitmap.height > 0);
  assert.ok(Buffer.isBuffer(bitmap.data));
  assert.equal(bitmap.byteLength, bitmap.data.length);
  assert.ok(bitmap.bytesPerRow >= bitmap.width * 4);
});
