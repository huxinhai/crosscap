# CrossCap 技术设计文档

## 1. 目标

构建一个供 Electron 调用的原生截图模块，产物为 `.node`。

首期目标：

- 先实现 macOS。
- macOS 需要同时支持 Intel 和 Apple Silicon。
- 只支持“截取单张全屏图”。
- 需要先获取当前设备有几个屏幕。
- 用户可指定截取某个屏幕。
- 屏幕参数不是必传。
  - 未传时，默认截取主屏。
- 支持导出多种图片格式。
- 使用 `pnpm` 管理 JavaScript 侧依赖。

后续目标：

- 增加 Windows 实现。
- 保持 JavaScript API 不变，仅切换底层平台实现。

---

## 2. 需求拆解

### 2.1 功能需求

提供两个核心能力：

1. 枚举屏幕列表
2. 截取指定屏幕的单张截图

### 2.2 非功能需求

- Electron 可直接 `require/import` 使用。
- 原生模块导出为 `.node`。
- 对外接口稳定，便于后续补 Windows。
- macOS 优先使用最新官方 API。
- 同时兼容旧版本 macOS。
- 输出尽量减少额外内存拷贝。
- 错误信息可读，便于前端提示权限和版本问题。

---

## 3. 总体方案

采用三层结构：

1. JavaScript 封装层
2. Node-API 原生桥接层
3. 平台实现层

建议目录：

```text
crosscap/
  package.json
  pnpm-workspace.yaml
  tsconfig.json
  binding.gyp
  src/
    index.ts
    types.ts
    loader.ts
  native/
    common/
      errors.h
      result.h
      image_encoder.h
      image_encoder.mm
    mac/
      addon.mm
      screen_service.h
      screen_service.mm
      permissions.h
      permissions.mm
      converters.h
      converters.mm
  dist/
  prebuilds/
  docs/
    technical-design.md
```

职责划分：

- `src/`：提供 JS/TS 对外 API，处理参数默认值、类型定义、异常包装。
- `native/addon.mm`：Node-API 导出函数。
- `native/mac/`：macOS 平台逻辑。
- `native/common/`：图片编码、通用错误、结果对象。

---

## 4. macOS 实现策略

## 4.1 API 选型原则

macOS 先按“最新 API 优先，旧版本自动降级”实现：

- macOS 14+：
  - 优先使用 `ScreenCaptureKit` 的 `SCScreenshotManager`
- macOS 12.3 ~ 13.x：
  - 使用 `ScreenCaptureKit`
  - 通过 `SCShareableContent` 获取屏幕列表
  - 通过单帧采集方案拿到截图
- 更老版本：
  - 使用 CoreGraphics 回退
  - 枚举：`CGGetActiveDisplayList`
  - 截图：`CGDisplayCreateImage`

原因：

- `ScreenCaptureKit` 是 Apple 当前推荐的屏幕捕获方案。
- Apple 在较新的系统里明确收紧了旧捕获 API 的权限和提醒策略。
- `SCScreenshotManager` 更适合“一次性截图”场景，不必人为启动完整流。

## 4.2 版本分层

建议按以下运行时能力判断：

```text
if macOS >= 14 and SCScreenshotManager available
  使用最新截图 API
else if macOS >= 12.3 and ScreenCaptureKit available
  使用 ScreenCaptureKit 单帧方案
else
  使用 CoreGraphics 兜底
```

注意：

- 必须做运行时判断，不能只靠编译时判断。
- 构建时可基于较新的 macOS SDK，但运行时要兼容旧系统。

## 4.3 屏幕枚举

统一输出结构：

```ts
export interface DisplayInfo {
  id: string
  index: number
  isPrimary: boolean
  width: number
  height: number
  scaleFactor: number
  rotation: number
  originX: number
  originY: number
  name?: string
}
```

macOS 各实现来源：

- 新方案：
  - `SCShareableContent.current` 获取可共享显示器
- 旧方案：
  - `CGGetActiveDisplayList`
  - `CGDisplayBounds`
  - `CGDisplayPixelsWide`
  - `CGDisplayPixelsHigh`

`id` 设计：

- 对外统一转成字符串
- macOS 内部实际映射为 `CGDirectDisplayID`
- 示例：`"69732928"`

为什么不直接用 `index`：

- 多屏环境下 `index` 可能随插拔变化。
- `id` 更适合作为实际截图参数。
- `index` 仅用于 UI 展示。

## 4.4 截图能力

对外接口：

```ts
export type ImageFormat = 'png' | 'jpeg' | 'webp' | 'tiff' | 'bmp'

export interface CaptureOptions {
  displayId?: string
  format?: ImageFormat
  quality?: number
}

export interface CaptureResult {
  displayId: string
  format: ImageFormat
  width: number
  height: number
  scaleFactor: number
  byteLength: number
  data: Buffer
}
```

规则：

- `displayId` 不传：
  - 默认主屏
- `format` 不传：
  - 默认 `png`
- `quality`：
  - 仅对有损格式生效，如 `jpeg`、`webp`

补充说明：

- `data` 是可直接供 Electron、`sharp`、文件系统使用的二进制流。
- 当 `format` 为 `png/jpeg/tiff/webp/bmp` 时，`data` 表示该格式编码后的完整图片二进制内容。
- 这意味着 `capture()` 返回的是“编码后的图片 Buffer”，不是原始位图像素。

## 4.4.1 面向 sharp 的原始位图接口

由于后续 Electron 侧需要配合 `sharp` 做二次处理，建议额外提供一个 raw bitmap 接口：

```ts
export interface CaptureBitmapOptions {
  displayId?: string
}

export interface BitmapResult {
  displayId: string
  width: number
  height: number
  scaleFactor: number
  pixelFormat: 'bgra'
  channels: 4
  bitsPerChannel: 8
  bytesPerRow: number
  byteLength: number
  data: Buffer
}
```

说明：

- `captureBitmap()` 返回的是“原始像素二进制流”。
- 首版 macOS 建议统一输出 `BGRA 8-bit`。
- 这样更贴近系统底层图像内存布局，转换成本更低。
- Electron 和 `sharp` 都可以消费这类数据。

推荐对外 API：

```ts
export function listDisplays(): Promise<DisplayInfo[]>
export function capture(options?: CaptureOptions): Promise<CaptureResult>
export function captureBitmap(options?: CaptureBitmapOptions): Promise<BitmapResult>
```

`sharp` 使用示例：

```ts
const bitmap = await crosscap.captureBitmap({ displayId })

const output = await sharp(bitmap.data, {
  raw: {
    width: bitmap.width,
    height: bitmap.height,
    channels: bitmap.channels
  }
})
  .png()
  .toBuffer()
```

注意：

- `sharp` 的 `raw` 输入主要依赖 `width/height/channels`。
- 为了减少接口歧义，首版应固定输出四通道。
- 如未来需要支持 `RGBA`，再通过参数或返回值扩展。

## 4.5 macOS 14+：最新 API 路线

推荐实现：

1. 使用 `SCShareableContent` 获取 displays
2. 根据 `displayId` 找到目标显示器
3. 创建 `SCContentFilter`
4. 创建 `SCStreamConfiguration`
5. 调用 `SCScreenshotManager.captureImage(...)`
6. 得到 `CGImageRef`
7. 统一进入编码器导出目标格式

这一层只负责拿到原始图像，不直接耦合图片导出逻辑。

## 4.6 macOS 12.3 ~ 13.x：兼容方案

`SCScreenshotManager` 不可用时，仍优先使用 `ScreenCaptureKit`：

方案：

1. `SCShareableContent` 枚举显示器
2. 创建目标显示器的 `SCContentFilter`
3. 创建 `SCStream`
4. 只拉取首帧
5. 从 `CMSampleBuffer` / `CVPixelBuffer` 转成 `CGImageRef`
6. 立即停止流

说明：

- 这是兼容层，不是首选实现。
- 因为业务只需要“一张全屏图”，所以在首帧到达后立刻关闭流即可。
- 需要控制超时，避免权限未授权时一直等待。

## 4.7 老版本兜底方案

当系统不支持 `ScreenCaptureKit` 时：

- 枚举：
  - `CGGetActiveDisplayList`
- 截图：
  - `CGDisplayCreateImage(displayId)`

说明：

- 这是兼容兜底，不作为主路径。
- 新系统上尽量不要主动走这条路径。

---

## 5. 图片导出设计

## 5.1 支持格式

首批建议支持：

- `png`
- `jpeg`
- `webp`
- `tiff`
- `bmp`

建议分阶段：

- Phase 1：
  - `png`
  - `jpeg`
  - `tiff`
- Phase 2：
  - `webp`
  - `bmp`

原因：

- `png/jpeg/tiff` 通过系统框架更稳。
- `webp` 在不同系统版本上的支持情况需要单独验证。
- `bmp` 价值较低，但实现简单，可后补。

## 5.2 编码方式

统一以 `CGImageRef` 作为中间图像表示。

编码器职责：

- 接收 `CGImageRef`
- 根据 `format` 输出二进制 `NSData`
- 最终转成 Node `Buffer`

建议使用：

- `ImageIO`
- `UniformTypeIdentifiers`

格式映射：

- `png` -> `public.png`
- `jpeg` -> `public.jpeg`
- `tiff` -> `public.tiff`
- `webp` -> `org.webmproject.webp` 或系统可用类型
- `bmp` -> `com.microsoft.bmp`

质量参数：

- `jpeg/webp`：使用 `kCGImageDestinationLossyCompressionQuality`
- `png/tiff/bmp`：忽略 `quality`

## 5.3 是否直接落盘

首版建议不在原生层负责文件落盘。

原生层只返回 `Buffer`：

- 更灵活
- Electron 主进程或渲染进程都可自行保存
- 避免原生层承担路径权限和文件系统复杂度
- 更适合直接接入 `sharp` 做格式转换、裁剪、缩放、压缩

如后续需要，可再加：

```ts
saveCapture(options, filePath)
```

但这不是首期必需。

---

## 6. 权限与系统限制

macOS 截图涉及 Screen Recording 权限。

需要提供一个权限查询接口：

```ts
export interface PermissionStatus {
  screenRecording: 'granted' | 'denied' | 'not-determined' | 'unknown'
}
```

建议对外增加：

```ts
getPermissionStatus(): PermissionStatus
openSystemSettings(): void
```

实现建议：

- 检测当前是否具备屏幕录制权限
- 未授权时返回结构化错误
- 错误码例如：
  - `ERR_PERMISSION_DENIED`
  - `ERR_DISPLAY_NOT_FOUND`
  - `ERR_FORMAT_NOT_SUPPORTED`
  - `ERR_CAPTURE_TIMEOUT`
  - `ERR_MACOS_VERSION_UNSUPPORTED`

注意：

- 首次使用时，macOS 可能弹出系统授权。
- Electron 侧应在文案上明确引导用户去系统设置开启权限。

---

## 7. Node 原生模块设计

## 7.1 技术选型

建议使用：

- `Node-API`
- `node-addon-api`
- Objective-C++ (`.mm`)

原因：

- 对 Electron 更友好。
- 比 NAN / 直接 V8 绑定更稳定。
- 未来可在不改 JS API 的前提下扩展 Windows。

## 7.2 导出接口

原生层建议导出异步 API：

```ts
export function listDisplays(): Promise<DisplayInfo[]>
export function capture(options?: CaptureOptions): Promise<CaptureResult>
export function captureBitmap(options?: CaptureBitmapOptions): Promise<BitmapResult>
export function getPermissionStatus(): Promise<PermissionStatus>
export function openSystemSettings(): Promise<void>
```

为什么使用异步：

- `ScreenCaptureKit` 本身包含异步流程。
- 权限弹窗、首帧等待、图片编码都不适合同步阻塞 JS 主线程。

## 7.3 异步实现方式

建议：

- 原生侧使用 `Napi::Promise::Deferred`
- macOS 异步回调完成后 resolve / reject

注意：

- 需要确保跨线程回调时正确切回可安全操作 Node-API 的上下文。
- 若内部线程模型复杂，可使用 `Napi::AsyncWorker` 或 `ThreadSafeFunction`。

---

## 8. Electron 集成设计

## 8.1 推荐集成位置

推荐在 Electron 主进程调用原生模块：

- 权限判断更集中
- 文件保存更适合主进程
- 避免在渲染进程暴露过多原生能力

然后通过 `ipcMain` / `ipcRenderer` 或 `contextBridge` 暴露给前端。

示例：

```ts
// main process
import * as crosscap from '@crosscap/core'

ipcMain.handle('crosscap:list-displays', () => crosscap.listDisplays())
ipcMain.handle('crosscap:capture', (_, options) => crosscap.capture(options))
ipcMain.handle('crosscap:capture-bitmap', (_, options) => crosscap.captureBitmap(options))
```

## 8.2 包结构建议

建议最终包名结构：

```text
packages/
  core/
    src/
    package.json
    binding.gyp
    native/
```

也可以先单包实现，后续再拆 workspace。

---

## 9. 构建与发布方案

## 9.0 CPU 架构支持策略

macOS 首版不仅要区分系统版本，还要区分 CPU 架构：

- `arm64`
  - Apple Silicon，例如 M1 / M2 / M3
- `x64`
  - Intel Mac

你当前开发机是 M1，因此本地开发阶段默认会先产出：

- `darwin-arm64`

但最终设计需要支持：

- `darwin-arm64`
- `darwin-x64`

建议目标：

1. 开发期
   - 在当前 M1 机器先完成 `arm64` 编译和调试
2. 发布期
   - 通过 GitHub Actions 产出两套预编译包，分别对应 `arm64` 和 `x64`
   - 或进一步合成 `universal` 二进制

说明：

- `ScreenCaptureKit`、`CoreGraphics`、`ImageIO` 这些系统框架本身支持 Intel 和 Apple Silicon。
- 我们主要需要保证 `.node` 原生模块的编译产物架构正确。
- API 层不需要为 Intel 和 Apple Silicon 分叉。

## 9.1 pnpm 依赖管理

建议依赖：

```json
{
  "name": "crosscap",
  "private": true,
  "type": "module",
  "scripts": {
    "build": "pnpm run build:ts && pnpm run build:native",
    "build:ts": "tsup src/index.ts --format esm,cjs --dts",
    "build:native": "node-gyp rebuild",
    "dev": "pnpm run build",
    "rebuild:electron": "electron-rebuild -f -w crosscap",
    "prepack": "pnpm run build"
  },
  "dependencies": {
    "node-addon-api": "^8.0.0",
    "node-gyp-build": "^4.8.0"
  },
  "devDependencies": {
    "@types/node": "^24.0.0",
    "electron": "^latest",
    "@electron/rebuild": "^latest",
    "node-gyp": "^11.0.0",
    "tsup": "^8.0.0",
    "typescript": "^5.0.0"
  }
}
```

安装：

```bash
pnpm add node-addon-api node-gyp-build
pnpm add -D typescript tsup node-gyp @types/node @electron/rebuild electron
```

## 9.2 为什么选 node-gyp

首版建议直接使用 `node-gyp`：

- Node 原生模块标准路径，资料多。
- 与 `node-addon-api` 配合成熟。
- macOS 单平台起步，复杂度最低。

后续如要分发预编译包，可增加：

- `prebuildify`
- `node-gyp-build`

## 9.2.1 macOS 多架构构建建议

首版开发阶段：

- 在 M1 机器本地直接构建 `arm64`

示例：

```bash
pnpm install
pnpm run build
```

如果要显式指定架构，可在构建脚本中增加：

```bash
npm_config_arch=arm64 pnpm run build:native
```

Intel 构建策略建议：

1. GitHub Actions 分架构构建
   - 在 `macos-13` / `macos-14` 之类 runner 上分别构建 `x64`、`arm64`
2. 本地不要求交叉编译
   - 本地只负责开发和调试当前机器架构

更稳妥的发布方式：

- 分发两套 `.node`
- 运行时按 `process.platform + process.arch` 自动加载

例如：

```text
prebuilds/
  darwin-arm64/
    crosscap.node
  darwin-x64/
    crosscap.node
```

JS loader 按架构选择：

```ts
const key = `${process.platform}-${process.arch}`
```

## 9.2.2 是否做 universal 二进制

可以支持，但不建议作为第一阶段必须项。

优先级建议：

1. 先稳定产出 `darwin-arm64` 和 `darwin-x64`
2. 后续如果有统一分发需求，再考虑 `universal`

原因：

- 两套预编译产物更直接，排障更简单。
- Electron 项目里按架构加载也很常见。
- `universal` 更像发布优化，不是能力前提。

## 9.2.3 GitHub 打包与发布

既然后续准备直接在 GitHub 上打包，建议正式采用 CI 产物模式。

推荐流程：

1. 本地开发
   - 在当前机器上做功能开发与调试
   - 例如你当前 M1 上只要求本地跑通 `darwin-arm64`
2. GitHub Actions
   - 安装依赖
   - 编译 `.node`
   - 执行自动化测试
   - 上传构建产物
3. Release
   - 发布 `darwin-arm64` 与 `darwin-x64` 预编译包
   - Electron 项目运行时按架构加载

建议的 CI Job：

- `build-macos-arm64`
- `build-macos-x64`
- `test-js`
- `test-native`
- `test-memory`
- `package-prebuilds`

建议产物结构：

```text
artifacts/
  crosscap-darwin-arm64.tar.gz
  crosscap-darwin-x64.tar.gz
```

如果后续要跟 npm 包一起分发，可以整理为：

```text
prebuilds/
  darwin-arm64/
    crosscap.node
  darwin-x64/
    crosscap.node
```

## 9.3 `.node` 产物

构建完成后会生成类似：

```text
build/Release/crosscap.node
```

JS 层通过 loader 加载：

```ts
import load from 'node-gyp-build'
import { dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = dirname(fileURLToPath(import.meta.url))
const binding = load(__dirname)

export default binding
```

如果后续采用预编译多架构分发，loader 需要能按架构定位到正确产物。

## 9.4 Electron 兼容

虽然使用 `Node-API` 可以显著降低 Electron ABI 适配压力，但仍建议在 Electron 项目里保留：

```bash
pnpm exec electron-rebuild
```

这样本地开发体验更稳，尤其是在不同 Electron 版本切换时。

补充：

- 在 Apple Silicon 的 Electron 上，应优先加载 `arm64` 原生模块。
- 在 Intel 的 Electron 上，应加载 `x64` 原生模块。
- 不建议在生产环境依赖 Rosetta 混跑作为正式方案。

---

## 10. macOS 原生实现细节

## 10.1 Node-API 到平台层的边界

建议平台层暴露 C++ 接口：

```cpp
struct CaptureRequest {
  std::optional<std::string> displayId;
  std::string format;
  std::optional<double> quality;
};

struct CaptureResponse {
  std::string displayId;
  std::string format;
  uint32_t width;
  uint32_t height;
  double scaleFactor;
  std::vector<uint8_t> bytes;
};
```

同时补充 raw bitmap 响应：

```cpp
struct CaptureBitmapRequest {
  std::optional<std::string> displayId;
};

struct CaptureBitmapResponse {
  std::string displayId;
  uint32_t width;
  uint32_t height;
  double scaleFactor;
  uint32_t channels;
  uint32_t bitsPerChannel;
  uint32_t bytesPerRow;
  std::string pixelFormat;
  std::vector<uint8_t> bytes;
};
```

Node-API 层只做：

- 参数解析
- Promise 包装
- 错误对象转换

平台层负责：

- 枚举屏幕
- 版本判断
- 权限检查
- 截图
- 图片编码

## 10.2 主屏默认策略

未传 `displayId` 时：

- 优先使用系统主屏
- 不使用“第一个枚举到的屏幕”作为默认值

macOS 主屏可通过：

- `CGMainDisplayID()`

## 10.3 显示器热插拔

首版不做事件订阅。

即：

- 每次 `listDisplays()` 都实时查询
- `capture()` 也实时校验 `displayId`

这样最简单，也足够满足当前需求。

如后续需要再加：

- `onDisplaysChanged`

---

## 11. Windows 预留设计

虽然当前先不实现，但接口现在就要定好。

Windows 侧未来建议：

- 枚举：
  - DXGI / EnumDisplayMonitors
- 截图：
  - Windows Graphics Capture 为优先
  - 旧系统回退 GDI / DXGI Desktop Duplication

关键原则：

- JS API 不变
- `displayId` 仍为字符串
- 输出结构完全一致

平台目录预留：

```text
native/
  mac/
  win/
  common/
```

---

## 12. 建议的首期里程碑

## Milestone 1：项目骨架

- 初始化 `pnpm`
- 建立 TypeScript 包结构
- 接入 `node-gyp`
- 输出最小可加载 `.node`

验收：

- Electron 中可成功加载模块

## Milestone 2：mac 枚举屏幕

- 实现 `listDisplays()`
- 返回主屏和坐标信息

验收：

- 多显示器下返回数量正确

## Milestone 3：mac 最新 API 截图

- macOS 14+ 实现 `SCScreenshotManager`
- 默认主屏截图
- 支持指定 `displayId`

验收：

- 能返回 `png` Buffer

## Milestone 4：图片编码扩展

- 增加 `jpeg`
- 增加 `tiff`
- 验证 `quality`

验收：

- 不同格式输出可正常写入文件并打开

## Milestone 5：旧版本兼容

- 加入 `ScreenCaptureKit` 单帧兼容层
- 加入 CoreGraphics 兜底

验收：

- 旧 macOS 环境可正常工作

## Milestone 6：权限与错误治理

- 明确权限错误码
- 增加 `openSystemSettings()`
- 完善超时和日志

---

## 13. 风险与应对

### 风险 1：系统权限导致截图失败

应对：

- 在原生层先做权限检查
- 返回结构化错误码
- Electron 侧补引导文案

### 风险 2：旧版 macOS API 行为差异

应对：

- 将“最新 API 路线”和“兼容路线”彻底分开
- 每条路径都单独测试

### 风险 3：多格式导出兼容性不一致

应对：

- 首期先把 `png/jpeg/tiff` 做稳
- `webp/bmp` 作为扩展格式，单独验证

### 风险 4：Electron 本地构建失败

应对：

- 使用 `Node-API`
- 提供 `electron-rebuild`
- 后续可补预编译产物

### 风险 5：不同 CPU 架构的 `.node` 无法互用

应对：

- 明确区分 `darwin-arm64` 与 `darwin-x64`
- loader 按架构加载
- 发布阶段补充双架构构建流程

### 风险 6：本地只在 M1 测试，导致 GitHub 打出来的 Intel 包有问题

应对：

- Intel 包必须在 GitHub CI 独立编译和验证
- 每个架构都执行基础功能测试
- 发布前校验产物可被 Electron 成功加载

---

## 14. 测试与验证方案

截图原生模块不能只做功能测试，还要做稳定性测试。

建议分为六类：

1. 接口功能测试
2. Electron 集成测试
3. `sharp` 联动测试
4. 内存占用测试
5. 内存泄漏测试
6. 压力与性能测试

## 14.1 功能测试

覆盖点：

- `listDisplays()` 返回屏幕数量正确
- 主屏标记正确
- `displayId` 指定有效时可截图成功
- `displayId` 不传时默认截图主屏
- 无效 `displayId` 返回预期错误
- `capture()` 支持 `png/jpeg/tiff`
- `captureBitmap()` 返回 raw buffer 和正确元信息

断言项：

- 返回 `width/height` 大于 0
- `data.length` 大于 0
- `byteLength === data.length`
- `captureBitmap()` 的 `bytesPerRow >= width * 4`

## 14.2 Electron 集成测试

覆盖点：

- Electron 主进程成功加载 `.node`
- `ipcMain.handle` 可正常调用
- 返回的 `Buffer` 能在主进程写入文件
- M1 和 Intel 两个架构都能成功加载各自产物

验证方式：

- 启动最小 Electron 示例应用
- 在 CI 中调用 `listDisplays()` 与 `capture()`
- 验证生成文件存在且可读取

## 14.3 sharp 联动测试

覆盖点：

- `capture()` 返回的编码 Buffer 可直接输入 `sharp`
- `captureBitmap()` 返回的 raw Buffer 可直接输入 `sharp`
- `sharp` 可完成：
  - 缩放
  - 格式转换
  - 压缩
  - 裁剪

示例验证链路：

```ts
const bitmap = await crosscap.captureBitmap()

const output = await sharp(bitmap.data, {
  raw: {
    width: bitmap.width,
    height: bitmap.height,
    channels: bitmap.channels
  }
})
  .resize(800)
  .jpeg({ quality: 80 })
  .toBuffer()
```

断言：

- 输出 buffer 非空
- 输出图片可被 `sharp.metadata()` 正常识别

## 14.4 内存占用测试

重点关注：

- 单次截图峰值内存
- 多次连续截图后的 RSS 增长
- raw bitmap 与编码图片两条路径的内存差异

建议测试场景：

1. 单次截图
2. 连续 10 次截图
3. 连续 100 次截图
4. `captureBitmap()` 后立刻交给 `sharp`
5. 大分辨率多屏环境下截图

观测指标：

- `process.memoryUsage().rss`
- `process.memoryUsage().heapUsed`
- `process.memoryUsage().external`

判定方式：

- 连续截图后内存应有波动，但不能持续线性增长
- 原生内存增长应在若干次 GC 后回落到可接受区间

## 14.5 内存泄漏测试

这是原生模块必须单独验证的重点。

需要重点检查：

- `CGImageRef` 是否正确释放
- `CFTypeRef` / `NSObject` 生命周期是否正确
- `CVPixelBuffer` / `CMSampleBuffer` 是否释放
- `NSData` 转 `Buffer` 时是否重复持有
- Promise reject 路径是否遗漏释放

推荐方法：

1. Instruments
   - Leaks
   - Allocations
2. 循环调用压测
   - 连续调用 500 次或 1000 次截图
3. 失败路径测试
   - 无权限
   - 无效 displayId
   - 超时

重点标准：

- 调用结束后对象数量不应持续累积
- Leaks 工具不应报告稳定复现的泄漏
- 成功路径和异常路径都要测

## 14.6 性能测试

关注：

- 首次截图耗时
- 连续截图平均耗时
- 编码不同格式的耗时差异
- `captureBitmap()` 与 `capture()` 的耗时差异

建议记录：

- `listDisplays()` 耗时
- `capture()` 总耗时
- 原始图像获取耗时
- 图片编码耗时

这样后续更容易判断瓶颈在：

- 系统截图阶段
- 权限阶段
- 编码阶段

## 14.7 CI 测试分层

GitHub Actions 中建议分层执行：

1. 轻量测试
   - TS 类型检查
   - JS 单元测试
   - 参数校验测试
2. 原生集成测试
   - 构建 `.node`
   - 调用 `listDisplays()` / `capture()`
3. 稳定性测试
   - 循环截图
   - 记录 RSS 和 external memory
4. 发布前验证
   - Electron 加载测试
   - `sharp` 联动测试

说明：

- Instruments 这类更深的泄漏检查，不一定适合每次 CI 都跑。
- 更适合作为发布前检查或定期专项检查。

## 14.8 发布门槛

建议设定最低发布门槛：

- M1 上功能测试通过
- GitHub 上 `darwin-arm64` / `darwin-x64` 构建通过
- Electron 加载测试通过
- `sharp` 联动测试通过
- 连续截图压测无明显内存泄漏
- 失败路径无明显资源泄漏

---

## 15. 最终建议

建议首版实现顺序：

1. `pnpm + TypeScript + node-gyp + node-addon-api` 项目骨架
2. macOS `listDisplays()`
3. macOS 14+ `SCScreenshotManager` 单屏截图
4. `captureBitmap()` raw 输出
5. `png/jpeg/tiff` 编码导出
6. 旧版本兼容层
7. Windows 实现

当前最合理的技术路线是：

- 对外只暴露统一 JS API
- 底层使用 `.node`
- macOS 14+ 优先 `SCScreenshotManager`
- macOS 12.3 ~ 13.x 使用 `ScreenCaptureKit` 单帧截图
- 更老版本使用 `CGDisplayCreateImage` 兜底
- 输出同时覆盖：
  - 编码后的图片二进制流
  - 供 `sharp` 消费的 raw bitmap 二进制流
- 构建产物同时覆盖：
  - `darwin-arm64`
  - `darwin-x64`

这样可以同时满足：

- 最新 API 优先
- 旧系统兼容
- Electron 易接入
- `sharp` 工作流友好
- 兼容 Intel 和 Apple Silicon
- 后续 Windows 易扩展

---

## 16. 参考依据

以下是本方案使用的官方依据：

- Apple WWDC23 对 `ScreenCaptureKit` 新增截图 API 的说明，提到 `SCScreenshotManager` 用于一次性截图。
- Apple WWDC22 对 `ScreenCaptureKit` 的基础能力说明，确认其作为现代屏幕捕获框架。
- Apple Developer Forums 中 Apple DTS 的建议：
  - 新系统对旧捕获 API 会有更严格提醒
  - 建议迁移到 `ScreenCaptureKit`
  - 一次性截图可查看 `SCScreenshotManager`
