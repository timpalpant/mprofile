# mprofile

A low-overhead sampling memory profiler for Python, derived from [heapprof](https://github.com/humu/heapprof), with an interface similar to [tracemalloc](https://pytracemalloc.readthedocs.io).
mprofile attempts to give results comparable to tracemalloc, but uses statistical sampling to lower memory and CPU overhead. The sampling algorithm is the one used by [tcmalloc](https://github.com/gperftools/gperftools) and Golang heap profilers.

## Installation & usage

1.  Install the profiler package using PyPI:

    ```shell
    pip3 install mprofile
    ```

2.  Enable the profiler in your application, get a snapshot of (sampled) memory usage:

    ```python
    import mprofile

    mprofile.start(sample_rate=128 * 1024)
    snap = mprofile.take_snapshot()
    ```

See the [tracemalloc](https://docs.python.org/3/library/tracemalloc.html) for API documentation. The API and objects returned by mprofile are compatible.

## Compatibility

mprofile is compatible with Python >= 3.4.
It can also be used with earlier versions of Python, but you must build CPython from source and apply the [pytracemalloc patches](https://pytracemalloc.readthedocs.io/install.html#manual-installation).

## Benchmarks

We are primarily interested in profiling the memory usage of webservers, so used the `tornado_http` benchmark from pyperformance to estimate overhead.
mprofile has similar performance to tracemalloc when comprehensively tracing all allocations, but when statistical sampling is used, the overhead is significantly reduced.
In addition, mprofile interns call stacks in a tree data structure that reduces memory overhead of storing the traces.

With the recommended setting of `sample_rate=128kB`, we observe ~5% slow down in the `tornado_http` benchmark.

TODO: Run the full [pyperformance](https://pyperformance.readthedocs.io) suite of benchmarks.

### Baseline
```
Python 2.7.16, no profiling:
tornado_http: Mean +- std dev: 664 ms +- 30 ms
Maximum resident set size (kbytes): 39176
```

### tracemalloc
```
Python 2.7.16, tracemallocframes=128:
tornado_http: Mean +- std dev: 1.74 sec +- 0.04 sec
Maximum resident set size (kbytes): 43752

# Saving only one frame in each stack trace rather than full call stacks.
Python 2.7.16, tracemallocframes=1:
tornado_http: Mean +- std dev: 960 ms +- 30 ms
Maximum resident set size (kbytes): 40000
```

### mprofile
```
Python 2.7.16, mprofileframes=128, mprofilerate=1 (i.e. tracemalloc):
tornado_http: Mean +- std dev: 1.78 sec +- 0.05 sec
Maximum resident set size (kbytes): 40588

Python 2.7.16, mprofileframes=128, mprofilerate=1024:
tornado_http: Mean +- std dev: 888 ms +- 28 ms
Maximum resident set size (kbytes): 39752

Python 2.7.16, mprofileframes=128, mprofilerate=128 * 1024:
tornado_http: Mean +- std dev: 700 ms +- 26 ms
Maximum resident set size (kbytes): 39388

# Saving only one frame in each stack trace rather than full call stacks.
Python 2.7.16, mprofileframes=1, mprofilerate=1 (i.e. tracemalloc):
tornado_http: Mean +- std dev: 890 ms +- 19 ms
Maximum resident set size (kbytes): 40152

Python 2.7.16, mprofileframes=1, mprofilerate=1024:
tornado_http: Mean +- std dev: 738 ms +- 24 ms
Maximum resident set size (kbytes): 39568

Python 2.7.16, mprofileframes=1, mprofilerate=128 * 1024:
tornado_http: Mean +- std dev: 678 ms +- 22 ms
Maximum resident set size (kbytes): 39328
```

## Developer notes

Run the unit tests:
```
bazel test --test_output=streamed //src:profiler_test
```

Run the benchmarks:
```
bazel test -c opt --test_output=streamed //src:profiler_bench
```

Run the end-to-end (Python) tests:
```
bazel test --config asan --test_output=streamed //test:*
```

Run tests with ASAN and UBSAN:
```
bazel test --config asan --test_output=streamed //src:* //test:*
```

# Contributing

Pull requests and issues are welcomed!

# License

mprofile is released under the [MIT License](https://opensource.org/licenses/MIT) and incorporates code from [heapprof](https://github.com/humu/heapprof), which is also released under the MIT license.
