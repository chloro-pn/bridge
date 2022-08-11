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

cc_binary(
  name = "string_map",
  srcs = ["example/string_map.cc"],
  deps = [
    ":bridge",
    "@nlohmann-json//:json",
  ]
)

cc_binary(
  name = "adaptor",
  srcs = ["example/adaptor.cc"],
  deps = [
    ":bridge",
  ]
)

cc_binary(
  name = "bench_mark",
  srcs = ["benchmark/bench_mark.cc"],
  deps = [
    ":bridge",
    "@rapid-json//:rapidjson",
    "@benchmark//:benchmark",
    "@benchmark//:benchmark_main",
  ],
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