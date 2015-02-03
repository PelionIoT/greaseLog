{
  "targets": [
    {
      "target_name": "greaseLog",


      "conditions": [
        [
          "OS=='mac'", {
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "WARNING_CFLAGS": [
                "-Wno-unused-variable",
              ],
            }
          }
        ],

        [
          "OS=='linux'", {
      "sources": [
        "logger.cc",
        "grease.cc",
        "error-common.cc",
        "deps/twlib/tw_alloc.cpp",
        "deps/twlib/tw_utils.cpp"
      ],

      "include_dirs": [
         "deps/twlib/include"
      ],


    'configurations': {
      'Debug': {
        'defines': [ 'ERRCMN_DEBUG_BUILD' ], # , 'LOGGER_HEAVY_DEBUG'
        "cflags": [
          "-Wall",
          "-std=c++11",
          "-D_POSIX_C_SOURCE=200809L",
          "-DERRCMN_DEBUG_BUILD=1"
          ],
##        'conditions': [
##          ['target_arch=="x64"', {
##            'msvs_configuration_platform': 'x64',
##          }],
##        ]
      },
      'Release': {
        "cflags": [
          "-Wall",
          "-std=c++11",
          "-D_POSIX_C_SOURCE=200809L",
          "-DNO_ERROR_CMN_OUTPUT=1",
          ],
      }

    },



          }
        ],  # end Linux




      ]
    },
  ],
}
