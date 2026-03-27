#include "screencapturekit_helper.h"

#include <dispatch/dispatch.h>

#include <cstdlib>
#include <string>
#include <stdexcept>

#include "../common/debug_log.h"

#if __has_include(<ScreenCaptureKit/ScreenCaptureKit.h>)
#include <ScreenCaptureKit/ScreenCaptureKit.h>
#define CROSSCAP_HAS_SCREEN_CAPTURE_KIT 1
#else
#define CROSSCAP_HAS_SCREEN_CAPTURE_KIT 0
#endif

namespace crosscap {

bool ShouldPreferScreenCaptureKit() {
  const char* value = std::getenv("CROSSCAP_PREFER_SCREEN_CAPTURE_KIT");
  if (value == nullptr) {
    return true;
  }

  return std::string(value) == "1";
}

bool ShouldForceLegacyFilterPath() {
  const char* value = std::getenv("CROSSCAP_SCKIT_FORCE_FILTER");
  return value != nullptr && std::string(value) == "1";
}

#if CROSSCAP_HAS_SCREEN_CAPTURE_KIT
API_AVAILABLE(macos(15.2))
static CGImageRef CopyDisplayImageInRectWithScreenCaptureKit(CGRect rect) {
  DebugLog("sckit", "captureImageInRect:start");
  __block CGImageRef captured_image = nullptr;
  __block NSError* capture_error = nil;
  dispatch_semaphore_t capture_semaphore = dispatch_semaphore_create(0);

  [SCScreenshotManager captureImageInRect:rect
                        completionHandler:^(CGImageRef _Nullable image,
                                            NSError* _Nullable error) {
    DebugLog("sckit", "captureImageInRect:completion");
    if (image != nullptr) {
      captured_image = CGImageRetain(image);
    }
    capture_error = error;
    dispatch_semaphore_signal(capture_semaphore);
  }];

  const dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW,
                                                static_cast<int64_t>(5 * NSEC_PER_SEC));
  if (dispatch_semaphore_wait(capture_semaphore, timeout) != 0) {
    DebugLog("sckit", "captureImageInRect:timeout");
    throw std::runtime_error("Timed out while capturing display image");
  }

  if (capture_error != nil) {
    DebugLog("sckit", "captureImageInRect:error");
    throw std::runtime_error([[capture_error localizedDescription] UTF8String]);
  }

  DebugLog("sckit", "captureImageInRect:success");
  return captured_image;
}

API_AVAILABLE(macos(14.0))
static CGImageRef CopyDisplayImageWithScreenCaptureKit(CGDirectDisplayID display_id) {
  DebugLog("sckit", "filterCapture:start");
  __block SCShareableContent* shareable_content = nil;
  __block NSError* content_error = nil;
  __block SCDisplay* target_display = nil;
  dispatch_semaphore_t content_semaphore = dispatch_semaphore_create(0);

  [SCShareableContent getShareableContentWithCompletionHandler:^(
                          SCShareableContent* _Nullable content,
                          NSError* _Nullable error) {
    DebugLog("sckit", "filterCapture:shareableContent:completion");
    shareable_content = content;
    content_error = error;

    if (content != nil && error == nil) {
      DebugLog("sckit", "filterCapture:display:search:start");
      NSArray<SCDisplay*>* displays = [content.displays copy];
      for (SCDisplay* display in displays) {
        DebugLog("sckit", "filterCapture:display:iter");
        if (display.displayID == display_id) {
          target_display = display;
          DebugLog("sckit", "filterCapture:display:matched");
          break;
        }
      }
    }

    dispatch_semaphore_signal(content_semaphore);
  }];

  const dispatch_time_t content_timeout =
      dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(5 * NSEC_PER_SEC));
  if (dispatch_semaphore_wait(content_semaphore, content_timeout) != 0) {
    DebugLog("sckit", "filterCapture:shareableContent:timeout");
    throw std::runtime_error("Timed out while fetching shareable content");
  }

  if (content_error != nil) {
    DebugLog("sckit", "filterCapture:shareableContent:error");
    throw std::runtime_error([[content_error localizedDescription] UTF8String]);
  }

  if (shareable_content == nil) {
    DebugLog("sckit", "filterCapture:shareableContent:nil");
    throw std::runtime_error("Shareable content was nil");
  }

  DebugLog("sckit", "filterCapture:shareableContent:success");
  if (target_display == nil) {
    DebugLog("sckit", "filterCapture:display:not_found");
    throw std::runtime_error("Display not found");
  }

  DebugLog("sckit", "filterCapture:display:found");
  SCContentFilter* filter =
      [[SCContentFilter alloc] initWithDisplay:target_display
                           excludingApplications:@[]
                               exceptingWindows:@[]];

  DebugLog("sckit", "filterCapture:filter:created");
  SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
  configuration.width = CGDisplayPixelsWide(display_id);
  configuration.height = CGDisplayPixelsHigh(display_id);
  configuration.showsCursor = NO;
  configuration.scalesToFit = NO;
  configuration.minimumFrameInterval = kCMTimeZero;

  __block CGImageRef captured_image = nullptr;
  __block NSError* capture_error = nil;
  dispatch_semaphore_t capture_semaphore = dispatch_semaphore_create(0);
  const dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW,
                                                static_cast<int64_t>(5 * NSEC_PER_SEC));

  DebugLog("sckit", "filterCapture:capture:request");
  [SCScreenshotManager captureImageWithFilter:filter
                                configuration:configuration
                            completionHandler:^(CGImageRef _Nullable image,
                                                NSError* _Nullable error) {
    DebugLog("sckit", "filterCapture:capture:completion");
    if (image != nullptr) {
      captured_image = CGImageRetain(image);
    }
    capture_error = error;
    dispatch_semaphore_signal(capture_semaphore);
  }];

  if (dispatch_semaphore_wait(capture_semaphore, timeout) != 0) {
    DebugLog("sckit", "filterCapture:capture:timeout");
    throw std::runtime_error("Timed out while capturing display image");
  }

  if (capture_error != nil) {
    DebugLog("sckit", "filterCapture:capture:error");
    throw std::runtime_error([[capture_error localizedDescription] UTF8String]);
  }

  DebugLog("sckit", "filterCapture:success");
  return captured_image;
}
#endif

CGImageRef TryCopyDisplayImageWithScreenCaptureKit(CGDirectDisplayID display_id) {
#if CROSSCAP_HAS_SCREEN_CAPTURE_KIT
  if (!ShouldPreferScreenCaptureKit()) {
    DebugLog("sckit", "disabled");
    return nullptr;
  }

  if (@available(macOS 14.0, *)) {
    try {
      DebugLog("sckit", "trying:captureImageWithFilter");
      return CopyDisplayImageWithScreenCaptureKit(display_id);
    } catch (const std::exception&) {
      DebugLog("sckit", "captureImageWithFilter:fallback");
    }
  }

  if (!ShouldForceLegacyFilterPath()) {
    if (@available(macOS 15.2, *)) {
      try {
        DebugLog("sckit", "trying:captureImageInRect");
        return CopyDisplayImageInRectWithScreenCaptureKit(CGDisplayBounds(display_id));
      } catch (const std::exception&) {
        DebugLog("sckit", "captureImageInRect:fallback");
      }
    }
  }
#endif

  static_cast<void>(display_id);
  return nullptr;
}

CGImageRef TryCopyRectImageWithScreenCaptureKit(CGRect rect) {
#if CROSSCAP_HAS_SCREEN_CAPTURE_KIT
  if (!ShouldPreferScreenCaptureKit()) {
    DebugLog("sckit", "rect:disabled");
    return nullptr;
  }

  if (@available(macOS 15.2, *)) {
    try {
      DebugLog("sckit", "rect:trying:captureImageInRect");
      return CopyDisplayImageInRectWithScreenCaptureKit(rect);
    } catch (const std::exception&) {
      DebugLog("sckit", "rect:captureImageInRect:fallback");
      return nullptr;
    }
  }
#endif

  static_cast<void>(rect);
  return nullptr;
}

}  // namespace crosscap
