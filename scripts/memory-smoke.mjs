import { capture, captureBitmap } from "../dist/index.js";

function snapshot(label) {
  const usage = process.memoryUsage();
  console.log(
    JSON.stringify({
      label,
      rss: usage.rss,
      heapUsed: usage.heapUsed,
      external: usage.external,
    }),
  );
}

async function run() {
  snapshot("start");

  for (let index = 0; index < 10; index += 1) {
    const image = await capture({ format: "png" });
    if (!image.data.length) {
      throw new Error("capture returned empty data");
    }
  }

  snapshot("after_capture_png_10");

  for (let index = 0; index < 10; index += 1) {
    const bitmap = await captureBitmap();
    if (!bitmap.data.length) {
      throw new Error("captureBitmap returned empty data");
    }
  }

  snapshot("after_capture_bitmap_10");
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
