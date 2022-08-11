load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository', 'new_git_repository')

git_repository(
    name = "googletest",
    remote = "https://ghproxy.com/https://github.com/google/googletest",
    tag = "release-1.11.0",
)

new_git_repository(
    name = "nlohmann-json",
    remote = "https://github.com/nlohmann/json",
    tag = "v3.10.5",
    build_file = "//third_party:json.build",
)

new_git_repository(
    name = "rapid-json",
    remote = "https://github.com/Tencent/rapidjson",
    tag = "v1.1.0",
    build_file = "//third_party:rapidjson.build",
)

git_repository(
    name = "benchmark",
    remote = "https://github.com/google/benchmark",
    tag = "v1.7.0",
)

new_git_repository(
    name = "async_simple",
    remote = "https://github.com/alibaba/async_simple",
    commit = "04885f0a59f9a848a67d43d6be2bb9ad2e32d739",
    build_file = "//third_party:async_simple.build",
)