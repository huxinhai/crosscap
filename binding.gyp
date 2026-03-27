{
  "targets": [
    {
      "target_name": "crosscap",
      "sources": [
        "native/mac/addon.mm",
        "native/mac/screen_service.mm",
        "native/common/image_encoder.mm",
        "native/mac/capture_service.mm",
        "native/mac/display_service.mm",
        "native/mac/permission_service.mm",
        "native/mac/screencapturekit_helper.mm"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "xcode_settings": {
        "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
        "CLANG_CXX_LIBRARY": "libc++",
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "OTHER_LDFLAGS": [
          "-framework",
          "AppKit",
          "-framework",
          "CoreGraphics",
          "-framework",
          "Foundation",
          "-framework",
          "ImageIO",
          "-weak_framework",
          "ScreenCaptureKit"
        ],
        "MACOSX_DEPLOYMENT_TARGET": "11.0"
      }
    }
  ]
}
