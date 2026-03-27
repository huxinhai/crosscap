{
  "targets": [
    {
      "target_name": "crosscap",
      "sources": [],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "cflags_cc": [
        "-fvisibility=hidden",
        "-fvisibility-inlines-hidden"
      ],
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "conditions": [
        ["OS==\"mac\"", {
          "sources": [
            "native/mac/addon.mm",
            "native/mac/screen_service.mm",
            "native/common/image_encoder.mm",
            "native/mac/capture_service.mm",
            "native/mac/display_service.mm",
            "native/mac/permission_service.mm",
            "native/mac/screencapturekit_helper.mm"
          ],
          "xcode_settings": {
            "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
            "CLANG_CXX_LIBRARY": "libc++",
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
            "OTHER_LDFLAGS": [
              "-Wl,-dead_strip",
              "-Wl,-exported_symbols_list,native/common/crosscap.exports",
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
        }],
        ["OS==\"win\"", {
          "sources": [
            "native/win/addon.cpp",
            "native/win/screen_service.cpp",
            "native/win/capture_service.cpp",
            "native/win/display_service.cpp",
            "native/win/permission_service.cpp",
            "native/win/image_encoder.cpp"
          ],
          "defines": [
            "UNICODE",
            "_UNICODE"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "FavorSizeOrSpeed": 1,
              "FunctionLevelLinking": "true",
              "InlineFunctionExpansion": 2,
              "Optimization": 3,
              "AdditionalOptions": [
                "/std:c++20",
                "/Gw",
                "/Gy",
                "/Zc:inline"
              ]
            },
            "VCLinkerTool": {
              "AdditionalOptions": [
                "/OPT:REF",
                "/OPT:ICF",
                "/DEBUG:NONE"
              ],
              "GenerateDebugInformation": "false",
              "ModuleDefinitionFile": "..\\native\\common\\crosscap.def"
            }
          },
          "msvs_disabled_warnings": [
            4251
          ],
          "libraries": [
            "windowscodecs.lib",
            "gdi32.lib",
            "user32.lib",
            "ole32.lib",
            "shcore.lib"
          ]
        }]
      ]
    }
  ]
}
