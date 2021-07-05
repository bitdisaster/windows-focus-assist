{
  "targets": [
    {
      "target_name": "focusassist",
      "sources": [ ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "conditions": [
        ['OS=="win"', {
          "sources": [ "lib/focus-assist.cc", "lib/quiethours.idl", "lib/quiethours_h.h", "lib/quiethours_i.c" ],
        }]
      ]
    }
  ]
}