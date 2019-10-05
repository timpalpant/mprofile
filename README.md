# mprofile

A low-overhead sampling memory profiler for Python, derived from [heapprof](https://github.com/humu/heapprof), with an interface similar to [tracemalloc](https://pytracemalloc.readthedocs.io).

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
