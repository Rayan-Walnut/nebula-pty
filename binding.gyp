{
  "targets": [
    {
      "target_name": "test_addon",
      "sources": [ "nebula-pty.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<!(node -p \"require('node-addon-api').include_dir\")",
        "deps/winpty/include",
        "deps/winpty/src/include"
      ],
      "defines": [ "NODE_ADDON_API_CPP_EXCEPTIONS" ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
        "OTHER_LDFLAGS": [ "-framework CoreFoundation" ]
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 }
      },
      "conditions": [
        ["OS==\"win\"", {
          "defines": [
            "_UNICODE",
            "UNICODE"
          ],
          "include_dirs": [
            "deps/winpty/include",
            "deps/winpty/src/include"
          ],
          "libraries": [
            "../deps/winpty/build/winpty.lib",
            "kernel32.lib",
            "user32.lib",
            "gdi32.lib",
            "winspool.lib",
            "comdlg32.lib",
            "advapi32.lib",
            "shell32.lib",
            "ole32.lib",
            "oleaut32.lib",
            "uuid.lib",
            "odbc32.lib",
            "odbccp32.lib"
          ]
        }],
        ["OS==\"linux\"", {
          "libraries": [
            "-lutil"
          ]
        }],
        ["OS==\"mac\"", {
          "libraries": [
            "-lutil"
          ]
        }]
      ]
    }
  ]
}