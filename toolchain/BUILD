load(":cc_toolchain_config.bzl", "cc_toolchain_config")

package(default_visibility = ["//visibility:public"])


cc_toolchain_suite(
  name = "clang_suite",
  toolchains = {
    "k8" : ":x86_toolchain"
  }
)

filegroup(
  name = "empty",
)

cc_toolchain_config(
  name = "x86_toolchain_config",
)

cc_toolchain(
  name = "x86_toolchain",
  toolchain_identifier = "x86-toolchain",
  toolchain_config = ":x86_toolchain_config",
  all_files = ":empty",
  compiler_files = ":empty",
  dwp_files = ":empty",
  linker_files = ":empty",
  objcopy_files = ":empty",
  strip_files = ":empty",
  supports_param_files = 0,
)