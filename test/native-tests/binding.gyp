# This is a generated file, modify: generate/templates/binding.gyp.ejs.
{
  "targets": [
    {
      "target_name": "testmodule",

      "sources": [
        "module.cc",
        "error-common.cc",
        "../../grease_client.c"   # bring in just the client stub code
      ],

      "include_dirs": [
      "../..",
      "../../node_modules/nan"
      ],

      "cflags": [
        "-Wall",
        "-std=c++11",
        "-D_ERRCMN_ADD_CONSTS"
      ],

      "conditions": [
        [
          "OS=='linux'", {
          "configurations" : {
            "Release" : {
            },
            "Debug" : {
              "defines" : [ "ERRCMN_DEBUG_BUILD" ]
            }
          }
          }
        ],        
        [
          "OS=='mac'", {
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "WARNING_CFLAGS": [
                "-Wno-unused-variable",
              ],
            }
          }
        ]
      ]
    },
  ]
}
