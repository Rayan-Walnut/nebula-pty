{
<<<<<<< HEAD
  "target_defaults": {
    "dependencies": [
      "<!(node -p \"require('node-addon-api').targets\"):node_addon_api_except"
    ],
    "conditions": [
      ["OS==\"win\"", {
        "msvs_configuration_attributes": {
          "SpectreMitigation": "Spectre"
        },
        "msvs_settings": {
          "VCCLCompilerTool": {
            "AdditionalOptions": [
              "/guard:cf",
              "/w34244",
              "/we4267",
              "/ZH:SHA_256"
            ]
          },
          "VCLinkerTool": {
            "AdditionalOptions": [
              "/guard:cf"
            ]
          }
        }
      }]
    ]
  },
  "targets": [
    {
      "target_name": "terminal",
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src"
      ],
      "sources": [
        "src/terminal.cc",
        "src/win/conpty.cc",
        "src/win/path_util.cc"
      ],
      "libraries": [
        "shlwapi.lib"
      ],
      "defines": [
        "UNICODE",
        "_UNICODE"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      }
=======
  "targets": [
    {
      "target_name": "terminal",
      "sources": [
        "src/terminal.cc",
        "src/Logger/logger.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "<!(node -p \"require('node-addon-api').include\")",
        "src"
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
>>>>>>> 5b0892aac6276a1b3194bb950640099db31811b9
    }
  ]
}