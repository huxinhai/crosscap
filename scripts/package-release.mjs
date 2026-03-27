import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";

const root = process.cwd();
const releaseRoot = path.join(root, "release");
const packageRoot = path.join(releaseRoot, "package");
const prebuildRoot = path.join(
  packageRoot,
  "prebuilds",
  `${process.platform}-${process.arch}`,
);

fs.rmSync(releaseRoot, { recursive: true, force: true });
fs.mkdirSync(path.join(packageRoot, "dist"), { recursive: true });
fs.mkdirSync(prebuildRoot, { recursive: true });

for (const file of ["index.js", "index.d.ts"]) {
  fs.copyFileSync(
    path.join(root, "dist", file),
    path.join(packageRoot, "dist", file),
  );
}

const nativeSource = path.join(root, "build", "Release", "crosscap.node");
const nativeTarget = path.join(prebuildRoot, "crosscap.node");
fs.copyFileSync(nativeSource, nativeTarget);

const pdbSource = path.join(root, "build", "Release", "crosscap.pdb");
if (process.platform === "win32" && fs.existsSync(pdbSource)) {
  console.log("[crosscap] skip pdb for release package");
}

if (process.platform === "darwin") {
  try {
    execFileSync("strip", ["-x", "-S", nativeTarget], { stdio: "inherit" });
  } catch (error) {
    console.warn("[crosscap] strip skipped:", error.message);
  }
}

const sourcePkg = JSON.parse(
  fs.readFileSync(path.join(root, "package.json"), "utf8"),
);

const publishPkg = {
  name: sourcePkg.name,
  version: sourcePkg.version,
  description: sourcePkg.description,
  main: "dist/index.js",
  types: "dist/index.d.ts",
  exports: {
    ".": {
      types: "./dist/index.d.ts",
      require: "./dist/index.js",
      default: "./dist/index.js",
    },
  },
  dependencies: sourcePkg.dependencies,
  os: [process.platform],
  cpu: [process.arch],
  files: ["dist", "prebuilds"],
};

fs.writeFileSync(
  path.join(packageRoot, "package.json"),
  `${JSON.stringify(publishPkg, null, 2)}\n`,
);

console.log(`[crosscap] release package prepared at ${packageRoot}`);
