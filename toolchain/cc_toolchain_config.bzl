# NEW
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
# NEW
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "tool_path",
)

all_compile_actions = [
    ACTION_NAMES.c_compile,
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.linkstamp_compile,
    ACTION_NAMES.assemble,
    ACTION_NAMES.preprocess_assemble,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.cpp_module_codegen,
]

all_link_actions = [ # NEW
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

def _impl(ctx):
  tool_paths = [
    tool_path(
      name = "gcc",
      path = "/home/linuxbrew/.linuxbrew/bin/clang++",
    ),
    tool_path(
      name = "ld",
      path = "/home/linuxbrew/.linuxbrew/bin/ld",
    ),
    tool_path(
      name = "ar",
      path = "/home/linuxbrew/.linuxbrew/bin/ar"
    ),
    tool_path(
        name = "cpp",
        path = "/bin/false",
    ),
    tool_path(
        name = "gcov",
        path = "/bin/false",
    ),
    tool_path(
        name = "nm",
        path = "/bin/false",
    ),
    tool_path(
        name = "objdump",
        path = "/bin/false",
    ),
    tool_path(
        name = "strip",
        path = "/bin/false",
    ),
  ]
  features = [ # NEW
    feature(
      name = "default_compiler_flags",
      enabled = True,
      flag_sets = [
        flag_set(
          actions = all_compile_actions,
          flag_groups = (
            [
              flag_group(
                flags = [
                  "-O2",
                  "-DNDEBUG",
                  "-Wall",
                  "-Wextra",
                  "-Wpedantic",
                  "-fPIC",
                ],
              ),
            ]
          ),
        ),
      ],
    ),
    feature(
        name = "default_linker_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = all_link_actions,
                flag_groups = ([
                    flag_group(
                        flags = [
                            "-lstdc++",
                        ],
                    ),
                ]),
            ),
        ],
    ),
  ]
  return cc_common.create_cc_toolchain_config_info(
    ctx = ctx,
    features = features,
    cxx_builtin_include_directories = [
      "/home/linuxbrew/.linuxbrew/include/c++/11",
      "/usr/include",
      "/usr/include/c++/11",
      "/usr/include/x86_64-linux-gnu/c++/11/bits",
      "/usr/include/c++/11/bits",
      "/usr/include/x86_64-linux-gnu/bits",
      "/usr/include/x86_64-linux-gnu/sys",
      "/usr/include/x86_64-linux-gnu/asm",
      "/home/linuxbrew/.linuxbrew/Cellar/llvm/14.0.6_1/lib/clang/14.0.6/include"
    ],
    toolchain_identifier = "x86-toolchain",
    host_system_name = "local",
    target_system_name = "local",
    target_cpu = "x86",
    target_libc = "unknown",
    compiler = "clang",
    abi_version = "unknown",
    abi_libc_version = "unknown",
    tool_paths = tool_paths,
  )

cc_toolchain_config = rule(
  implementation = _impl,
  attrs = {},
  provides = [CcToolchainConfigInfo],
)