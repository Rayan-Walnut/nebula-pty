{
  "targets": [
    {
      "target_name": "terminal",
      "sources": [
        "src/terminal.cc",
        "src/Logger/logger.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "src"
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}