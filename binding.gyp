{
  "targets": [
    {
      "target_name": "terminal",
      "sources": [ 
        "src/terminal.cc",
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}