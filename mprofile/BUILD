py_library(
    name = "mprofile",
    srcs = glob(["*.py"]),
    data = [
        ":_profiler.so",
    ],
    imports = ['.'],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "_profiler.so",
    deps = ["//src:profiler"],
    linkshared = True,
    visibility = ["//mprofile:__subpackages__"],
)
