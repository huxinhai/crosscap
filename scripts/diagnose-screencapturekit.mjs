import { spawnSync } from "node:child_process";

const baseEnv = {
  ...process.env,
  CROSSCAP_DEBUG_NATIVE: "1",
};

const cases = [
  {
    name: "default-path",
    env: {
      ...baseEnv,
      CROSSCAP_PREFER_SCREEN_CAPTURE_KIT: "1",
    },
    script: `
      const crosscap = require('./dist');
      (async () => {
        const image = await crosscap.capture({ format: 'png' });
        console.log(JSON.stringify({
          mode: 'default',
          width: image.width,
          height: image.height,
          byteLength: image.byteLength
        }));
      })();
    `,
  },
  {
    name: "raw-bitmap",
    env: {
      ...baseEnv,
      CROSSCAP_PREFER_SCREEN_CAPTURE_KIT: "1",
    },
    script: `
      const crosscap = require('./dist');
      (async () => {
        const bitmap = await crosscap.captureBitmap();
        console.log(JSON.stringify({
          mode: 'bitmap',
          width: bitmap.width,
          height: bitmap.height,
          byteLength: bitmap.byteLength
        }));
      })();
    `,
  },
  {
    name: "legacy-filter-path",
    env: {
      ...baseEnv,
      CROSSCAP_PREFER_SCREEN_CAPTURE_KIT: "1",
      CROSSCAP_SCKIT_FORCE_FILTER: "1",
    },
    script: `
      const crosscap = require('./dist');
      (async () => {
        const image = await crosscap.capture({ format: 'png' });
        console.log(JSON.stringify({
          mode: 'legacy-filter',
          width: image.width,
          height: image.height,
          byteLength: image.byteLength
        }));
      })();
    `,
  },
];

for (const item of cases) {
  console.log(`\\n=== ${item.name} ===`);
  const result = spawnSync(process.execPath, ["-e", item.script], {
    cwd: process.cwd(),
    env: item.env,
    encoding: "utf8",
  });

  if (result.stdout) {
    process.stdout.write(result.stdout);
  }
  if (result.stderr) {
    process.stderr.write(result.stderr);
  }

  console.log(`exitCode=${result.status} signal=${result.signal ?? "none"}`);
}
