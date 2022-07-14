package(default_visibility = ["//visibility:public"])

cc_library(
  name = "bridge",
  hdrs = glob(["include/**/*.h"]),
  includes = ["include"]
)

cc_binary(
  name = "example",
  srcs = ["example/main.cc"],
  deps = [
    ":bridge",
  ]
)

cc_test(
  name = "bridge_test",
  srcs = glob(["test/*.cc"]),
  deps = [
    ":bridge",
    "@googletest//:gtest",
    "@googletest//:gtest_main",
  ]
)