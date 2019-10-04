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


