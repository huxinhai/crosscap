import CoreGraphics
import Dispatch
import Foundation

#if canImport(ScreenCaptureKit)
import ScreenCaptureKit
#endif

#if canImport(ImageIO)
import ImageIO
#endif

#if canImport(UniformTypeIdentifiers)
import UniformTypeIdentifiers
#endif

func log(_ message: String) {
  FileHandle.standardError.write(Data("[sckit-repro] \(message)\n".utf8))
}

#if canImport(ScreenCaptureKit)
@available(macOS 14.0, *)
func savePNG(_ image: CGImage, to url: URL) throws {
  guard let destination = CGImageDestinationCreateWithURL(
    url as CFURL,
    UTType.png.identifier as CFString,
    1,
    nil
  ) else {
    throw NSError(domain: "crosscap.repro", code: 1, userInfo: [
      NSLocalizedDescriptionKey: "Failed to create PNG destination"
    ])
  }

  CGImageDestinationAddImage(destination, image, nil)
  if !CGImageDestinationFinalize(destination) {
    throw NSError(domain: "crosscap.repro", code: 2, userInfo: [
      NSLocalizedDescriptionKey: "Failed to finalize PNG file"
    ])
  }
}

@available(macOS 14.0, *)
func fetchShareableContent() async throws -> SCShareableContent {
  log("shareableContent:start")
  let capturedContent = try await SCShareableContent.current
  log("shareableContent:completion")

  log("shareableContent:success displays=\(capturedContent.displays.count)")
  return capturedContent
}

@available(macOS 14.0, *)
func captureImage(for displayID: CGDirectDisplayID) async throws -> CGImage {
  let content = try await fetchShareableContent()

  log("display:search:start displayID=\(displayID)")
  guard let display = content.displays.first(where: { $0.displayID == displayID }) else {
    throw NSError(domain: "crosscap.repro", code: 5, userInfo: [
      NSLocalizedDescriptionKey: "Display not found in SCShareableContent"
    ])
  }
  log("display:search:matched width=\(display.width) height=\(display.height)")

  let filter = SCContentFilter(
    display: display,
    excludingApplications: [],
    exceptingWindows: []
  )
  log("filter:created")

  let configuration = SCStreamConfiguration()
  configuration.width = display.width
  configuration.height = display.height
  configuration.showsCursor = false
  configuration.scalesToFit = false
  configuration.minimumFrameInterval = .zero
  log("configuration:created")

  let semaphore = DispatchSemaphore(value: 0)
  var capturedImage: CGImage?
  var capturedError: Error?

  log("capture:start")
  SCScreenshotManager.captureImage(
    contentFilter: filter,
    configuration: configuration
  ) { image, error in
    log("capture:completion")
    capturedImage = image
    capturedError = error
    semaphore.signal()
  }

  let timeout = DispatchTime.now() + .seconds(5)
  if semaphore.wait(timeout: timeout) == .timedOut {
    throw NSError(domain: "crosscap.repro", code: 6, userInfo: [
      NSLocalizedDescriptionKey: "Timed out while capturing image"
    ])
  }

  if let error = capturedError {
    throw error
  }

  guard let capturedImage else {
    throw NSError(domain: "crosscap.repro", code: 7, userInfo: [
      NSLocalizedDescriptionKey: "Captured image was nil"
    ])
  }

  log("capture:success width=\(capturedImage.width) height=\(capturedImage.height)")
  return capturedImage
}
#endif

@main
struct Main {
  static func main() async {
    #if canImport(ScreenCaptureKit)
    if #available(macOS 14.0, *) {
      let arguments = CommandLine.arguments
      let outputPath = arguments.count > 1 ? arguments[1] : nil
      let displayID = CGMainDisplayID()

      do {
        let image = try await captureImage(for: displayID)
        if let outputPath {
          try savePNG(image, to: URL(fileURLWithPath: outputPath))
          log("saved:\(outputPath)")
        }
      } catch {
        log("error:\(error.localizedDescription)")
        exit(1)
      }
      return
    }
    #endif

    log("ScreenCaptureKit reproduction requires macOS 14.0+")
    exit(1)
  }
}
