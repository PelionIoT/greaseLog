# This is a generated file, modify: generate/templates/binding.gyp.ejs.
{
  "targets": [
    {
      "target_name": "testmodule",

      "sources": [
        "module.cc",
        "error-common.cc"
      ],

#      "include_dirs": [
#
#      ],

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
              "defines" : [ "ERRCMN_DEBUG_BUILD", "PSEUDOFS_DEBUG_BUILD" ]
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
