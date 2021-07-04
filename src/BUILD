COPTS = [
    "-std=c++11",
    "-Wall",
    "-Wshadow",
    "-fstrict-aliasing",
    "-fno-omit-frame-pointer",
]

cc_library(
    name = "profiler",
    deps = [
        "@local_config_python//:python_headers",
        "//third_party/greg7mdp/parallel-hashmap",
        "//third_party/google/tcmalloc:sampler",
    ],
    hdrs = glob(["*.h"]),
    srcs = glob(["*.cc"], exclude=["*_test.cc", "*_bench.cc"]),
    copts = COPTS,
    visibility = ["//:__subpackages__"],
)

cc_test(
    name = "profiler_test",
    srcs = glob(["*_test.cc"]),
    copts = COPTS,
    linkopts = select({
        "@local_config_python//:python2": ["-ldl", "-lpthread", "-lpython2.7", "-lrt"],
        "@local_config_python//:python3": ["-ldl", "-lpthread", "-lpython3.9", "-lrt"],
        "//conditions:default": ["not-existing.headers"],
    }),
    deps = [
        ":profiler",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "profiler_bench",
    srcs = glob(["*_bench.cc"]),
    copts = COPTS + ["-O3"],
    linkopts = select({
        "@local_config_python//:python2": ["-ldl", "-lpthread", "-lpython2.7", "-lrt"],
        "@local_config_python//:python3": ["-ldl", "-lpthread", "-lpython3.9", "-lrt"],
        "//conditions:default": ["not-existing.headers"],
    }),
    deps = [
        ":profiler",
        "@com_github_google_benchmark//:benchmark_main",
    ],
)
