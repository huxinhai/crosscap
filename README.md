# CrossCap

CrossCap 是一个面向 Electron 的原生截图模块，产物为 `.node`。

当前阶段：

- 先支持 macOS
- 支持获取屏幕列表
- 支持按屏幕截图
- 支持返回编码后的图片二进制
- 支持返回 raw bitmap 二进制
- 使用 `pnpm` 管理依赖
- 使用 `Node-API + node-addon-api + Objective-C++`

## 当前能力

对外 API：

```ts
listDisplays(): Promise<DisplayInfo[]>
capture(options?: CaptureOptions): Promise<CaptureResult>
captureBitmap(options?: CaptureBitmapOptions): Promise<BitmapResult>
captureRegion(options: CaptureRegionOptions): Promise<CaptureResult>
getPermissionStatus(): Promise<PermissionStatus>
openSystemSettings(): Promise<void>
```

其中：

- `capture()` 返回编码后的图片 `Buffer`
- `captureBitmap()` 返回 raw `BGRA` 像素 `Buffer`
- `captureRegion()` 返回区域截图的图片 `Buffer`

## 工程结构

```text
src/
  core/
    index.ts
    native.ts
    types.ts
  index.ts

native/
  common/
    image_encoder.h
    image_encoder.mm
    types.h
  mac/
    addon.mm
    capture_service.h
    capture_service.mm
    display_service.h
    display_service.mm
    permission_service.h
    permission_service.mm
    screencapturekit_helper.h
    screencapturekit_helper.mm
    screen_service.h
    screen_service.mm

scripts/
  memory-smoke.mjs
  package-release.mjs
  smoke.mjs
```

职责划分：

- `src/core`：JS/TS API 与 native loader
- `native/common`：跨平台可复用的公共结构和图片编码
- `native/mac/display_service`：显示器枚举和 displayId 解析
- `native/mac/permission_service`：权限查询和系统设置跳转
- `native/mac/capture_service`：截图和 bitmap 输出
- `native/mac/screencapturekit_helper`：最新 API 分支
- `native/mac/addon`：Node-API 导出层

## 构建

安装依赖：

```bash
pnpm install
```

构建：

```bash
pnpm run build
```

清理：

```bash
pnpm run clean
```

## 测试

基础 smoke：

```bash
pnpm run test:smoke
```

轻量内存 smoke：

```bash
pnpm run test:memory
```

当前内存脚本会记录：

- `rss`
- `heapUsed`
- `external`

用于观察连续截图后的内存变化趋势。

## 发布打包

生成最小化发布目录：

```bash
pnpm run package:release
```

输出目录：

```text
release/package/
  dist/
  prebuilds/
    darwin-arm64/
      crosscap.node
  package.json
```

当前发布优化包括：

- JS 产物压缩
- 发布目录只保留运行时文件
- `.node` 执行 `strip`

## GitHub Actions

GitHub Actions 工作流在：

- [.github/workflows/build-macos.yml](/Users/mac/html/crosscap/.github/workflows/build-macos.yml)

当前会分别构建：

- `darwin-arm64`
- `darwin-x64`

并上传对应 artifact。

## Electron 接入

建议在主进程调用：

```ts
import * as crosscap from "crosscap";

const displays = await crosscap.listDisplays();
const image = await crosscap.capture({ format: "png" });
const bitmap = await crosscap.captureBitmap();
```

`captureBitmap()` 返回的数据可以直接给 `sharp`：

```ts
import sharp from "sharp";
import * as crosscap from "crosscap";

const bitmap = await crosscap.captureBitmap();

const output = await sharp(bitmap.data, {
  raw: {
    width: bitmap.width,
    height: bitmap.height,
    channels: bitmap.channels
  }
})
  .jpeg({ quality: 80 })
  .toBuffer();
```

## 权限

macOS 截图需要 Screen Recording 权限。

可以先查询：

```ts
await crosscap.getPermissionStatus();
```

未授权时可以引导用户打开系统设置：

```ts
await crosscap.openSystemSettings();
```

## 最新 API 策略

项目里已经单独拆出了 `ScreenCaptureKit` helper：

- [screencapturekit_helper.mm](/Users/mac/html/crosscap/native/mac/screencapturekit_helper.mm)

当前策略是：

- 全屏截图：
  - 默认优先使用 `SCScreenshotManager.captureImage(contentFilter:configuration:completionHandler:)`
- 区域截图：
  - `macOS 15.2+` 优先使用 `SCScreenshotManager.captureImageInRect`
- 失败回退：
  - 自动回退到稳定的 `CoreGraphics`

原因：

- 全屏截图需要按屏幕维度精确选择目标 display，`captureImage(...)` 更贴合这个语义
- 区域截图用 `captureImageInRect(...)` 更直接
- 对旧系统和异常情况仍保留稳定回退，降低发布风险

本轮排查还确认了一个关键 root cause：

- `captureImage(...)` 本身在这台机器上不是必崩
- 纯 Swift 最小复现可以稳定成功
- 之前 `.node` 内崩溃的根因在于 `SCShareableContent.displays` 的访问上下文处理不当
- 修复方式是：
  - 不再把 `SCShareableContent` 链路包进主线程 `dispatch_sync`
  - 在 `getShareableContentWithCompletionHandler` 的 completion 内部直接完成 display 查找

用于复现和比对的纯 Swift 调查脚本在：

- [scripts/sckit-filter-repro.swift](/Users/mac/html/crosscap/scripts/sckit-filter-repro.swift)

如果你想显式关闭最新 API 分支，可以设置：

```bash
CROSSCAP_PREFER_SCREEN_CAPTURE_KIT=0 pnpm run test:smoke
```

如果你想显式强制开启，也可以设置：

```bash
CROSSCAP_PREFER_SCREEN_CAPTURE_KIT=1 pnpm run test:smoke
```

注意：

- 旧系统仍会自动回退
- 如果要调试全屏 `captureImage(...)` 路径，可以设置：

```bash
CROSSCAP_SCKIT_FORCE_FILTER=1 pnpm run diagnose:sckit
```

## 区域截图预留

项目已经预留了区域截图接口：

```ts
captureRegion({
  x,
  y,
  width,
  height,
  format,
  quality
})
```

当前状态：

- 接口已实现
- `macOS 15.2+` 优先基于 `captureImageInRect(...)`
- 其他情况回退到 CoreGraphics 区域截图

## 当前状态说明

当前这版适合：

- 本地开发和 Electron 集成
- GitHub Actions 打包
- `arm64` 发布包生成
- 后续扩展 Intel / Windows

下一阶段重点：

1. 继续稳定 `ScreenCaptureKit` 最新 API 分支
2. 完成双架构发布验证
3. 增加 Windows 实现
