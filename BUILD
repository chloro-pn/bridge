package(default_visibility = ["//visibility:public"])

cc_library(
  name = "bridge",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/*.cc"]),
  includes = ["include"],
  linkopts = [
    "-pthread",
  ],
  deps = [
    "@async_simple//:async-simple",
  ],
  #defines = ["BRIDGE_USE_UNORDERED_MAP"],
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
  name = "scheduler",
  srcs = ["example/coroutine_parse.cc"],
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
    "@async_simple//:async-simple",
    "@rapid-json//:rapidjson",
  ],
  copts = [
    "-DASYNC_SIMPLE_HAS_NOT_AIO",
  ]
)