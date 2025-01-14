{
  "targets": [{
    "target_name": "terminal",
    "sources": [ 
      "./src/terminal.cc",
      "./src/win/conpty.cc"
    ],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")",
      "src"
    ],
    "libraries": [
      "kernel32.lib"
    ],
    "defines": [
      "UNICODE",
      "_UNICODE"
    ],
    "msvs_settings": {
      "VCCLCompilerTool": {
        "ExceptionHandling": 1,
        "AdditionalOptions": [
          "/std:c++17"
        ]
      }
    },
    "cflags!": [ "-fno-exceptions" ],
    "cflags_cc!": [ "-fno-exceptions" ]
  }]
}