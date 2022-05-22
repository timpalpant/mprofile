workspace(name = "pypprof")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party/py:python_configure.bzl", "python_configure")

# Dynamically configure Python headers based on python in PATH.
# This is adapted from tensorflow/grpc.
python_configure(name = "local_config_python")

# Python and pip rules for Bazel.
http_archive(
    name = "rules_python",
    url = "https://github.com/bazelbuild/rules_python/archive/9d68f24659e8ce8b736590ba1e4418af06ec2552.zip",
    sha256 = "f7402f11691d657161f871e11968a984e5b48b023321935f5a55d7e56cf4758a",
    strip_prefix = "rules_python-9d68f24659e8ce8b736590ba1e4418af06ec2552",
)

load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

# C++ rules for Bazel.
http_archive(
    name = "rules_cc",
    sha256 = "67412176974bfce3f4cf8bdaff39784a72ed709fc58def599d1f68710b58d68b",
    strip_prefix = "rules_cc-b7fe9697c0c76ab2fd431a891dbb9a6a32ed7c3e",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_cc/archive/b7fe9697c0c76ab2fd431a891dbb9a6a32ed7c3e.zip",
        "https://github.com/bazelbuild/rules_cc/archive/b7fe9697c0c76ab2fd431a891dbb9a6a32ed7c3e.zip",
    ],
)

# Proto rules for Bazel.
http_archive(
    name = "rules_proto",
    sha256 = "602e7161d9195e50246177e7c55b2f39950a9cf7366f74ed5f22fd45750cd208",
    strip_prefix = "rules_proto-97d8af4dc474595af3900dd85cb3a29ad28cc313",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()

# GoogleTest/GoogleMock framework. Used by most unit-tests.
http_archive(
    name = "com_github_google_googletest",
    urls = ["https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.zip"],  # 2019-01-07
    strip_prefix = "googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb",
    sha256 = "ff7a82736e158c077e76188232eac77913a15dac0b22508c390ab3f88e6d6d86",
)

# Google benchmark.
http_archive(
    name = "com_github_google_benchmark",
    urls = ["https://github.com/google/benchmark/archive/refs/tags/v1.6.1.tar.gz"],
    strip_prefix = "benchmark-1.6.1",
    sha256 = "6132883bc8c9b0df5375b16ab520fac1a85dc9e4cf5be59480448ece74b278d4",
)
