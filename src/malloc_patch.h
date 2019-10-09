// Adapted from heapprof.

#ifndef MPROFILE_SRC_MALLOC_PATCH_H_
#define MPROFILE_SRC_MALLOC_PATCH_H_

#include <Python.h>
#include <memory>

#include "heap.h"

// Attach a profiler to the malloc hooks and start profiling. This function
// takes ownership of the profiler state; it will be deleted when it is
// detached.
void AttachHeapProfiler(std::unique_ptr<HeapProfiler> profiler);

// Detach the profiler from the malloc hooks and stop profiling. It is not an
// error to call this if there is no active profiling.
void DetachHeapProfiler();

// Test if profiling is active.
bool IsHeapProfilerAttached();

// Get the current snapshot of all profiled heap allocations.
PyObject *GetHeapProfile();

// Get the current traceback limit for number of frames to save.
int GetMaxFrames();

// Get the current sampling rate, in bytes.
int GetSamplePeriod();

// Get the traceback where the given pointer was allocated.
PyObject *GetTrace(void *ptr);

// Clear all traced memory blocks from the current heap profiler.
void ResetHeapProfiler();

// Get an estimate of the memory used by the heap profiler.
std::size_t GetHeapProfilerMemUsage();

// Get the <current, peak> memory usage traced, in bytes.
std::pair<std::size_t, std::size_t> GetHeapProfilerTracedMemory();

#endif  // MPROFILE_MALLOC_PATCH_H__
