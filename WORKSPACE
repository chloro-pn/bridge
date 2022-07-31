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