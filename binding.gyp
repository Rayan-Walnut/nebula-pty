{
  "targets": [{
    "target_name": "terminal",
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")",
      "src"
    ],
    "sources": [
      "src/terminal.cc",
      "src/win/conpty.cc"
    ],
    "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
    "libraries": [],
    "conditions": [
      ["OS=='win'", {
        "libraries": [
          "-lkernel32.lib"
        ]
      }]
    ],
    "msvs_settings": {
      "VCCLCompilerTool": {
        "ExceptionHandling": 1
      }
    }
  }]
}