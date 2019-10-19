// Copyright 2019 Timothy Palpant
// Adapted from heapprof, Copyright 2019 Humu, Inc.

#ifndef MPROFILE_SRC_HEAP_H_
#define MPROFILE_SRC_HEAP_H_

#include <stdlib.h>
#include <mutex>
#include <vector>

#include "third_party/google/tcmalloc/addressmap.h"
#include "third_party/google/tcmalloc/sampler.h"
#include "third_party/greg7mdp/parallel-hashmap/phmap.h"

#include "spinlock.h"
#include "stacktraces.h"

class HeapProfiler {
 public:
  HeapProfiler() : HeapProfiler(kMaxFramesToCapture) {}
  explicit HeapProfiler(int max_frames)
      : max_frames_(max_frames),
        live_set_(malloc, free),
        total_mem_traced_(0),
        peak_mem_traced_(0) {}
  // Not copyable or assignable.
  HeapProfiler(const HeapProfiler &) = delete;
  HeapProfiler &operator=(const HeapProfiler &) = delete;

  // These each require that ptr (newptr) not be nullptr.
  void HandleMalloc(void *ptr, std::size_t size, bool is_raw);
  void HandleRealloc(void *oldptr, void *newptr, std::size_t size, bool is_raw);
  void HandleFree(void *ptr);

  std::vector<const void *> GetSnapshot();
  int GetMaxFrames() const { return max_frames_; }
  std::vector<FuncLoc> GetTrace(const void *ptr);
  std::size_t GetSize(const void *ptr);
  std::size_t TotalMemoryTraced();
  std::size_t PeakMemoryTraced();
  void Reset();

 private:
  void RecordMalloc(void *ptr, size_t size);

  // The information we store for a live pointer.
  struct LivePointer {
    // The trace at which it was allocated.
    // This is a reference to an element in traces_.
    CallTraceSet::TraceHandle trace_handle;
    // The size of the memory allocated.
    std::size_t size;
  };

  int max_frames_;
  // Guards access to live_set_.
  SpinLock mu_;

  // Map of live pointer -> trace + size of that pointer (if it was sampled).
  // Protected by mu_.
  AddressMap<LivePointer> live_set_;
  std::size_t total_mem_traced_;
  std::size_t peak_mem_traced_;

  // Interned set of referenced stack traces.
  // Protected by the GIL.
  CallTraceSet traces_;
};

inline void HeapProfiler::HandleMalloc(void *ptr, std::size_t size,
                                       bool is_raw) {
  // NOTE: Only constant expressions are safe to use as thread_local
  // initializers in a dynamic library. This is why the sample rate is
  // set as a static variable on the Sampler class.
  thread_local Sampler sampler;
  if (LIKELY(sampler.RecordAllocation(size))) {
    return;
  }

  if (ptr == nullptr) {
    return;
  }

  PyGILState_STATE gil_state;
  if (is_raw) {
    gil_state = PyGILState_Ensure();
  }

  RecordMalloc(ptr, size);

  if (is_raw) {
    PyGILState_Release(gil_state);
  }
}

inline void HeapProfiler::HandleRealloc(void *oldptr, void *newptr,
                                        std::size_t size, bool is_raw) {
  assert(newptr != nullptr);
  if (oldptr != nullptr) {
    HandleFree(oldptr);
  }

  HandleMalloc(newptr, size, is_raw);
}

inline void HeapProfiler::HandleFree(void *ptr) {
  // We could use a reader-writer lock and only take the write lock
  // if the pointer is found in the live set. In practice this is a little
  // bit slower and likely not beneficial since Python is mostly
  // single-threaded anyway. The GIL cannot be held in HandleFree because
  // it would introduce a deadlock in PyThreadState_DeleteCurrent().
  std::lock_guard<SpinLock> lock(mu_);
  LivePointer removed;
  if (UNLIKELY(live_set_.FindAndRemove(ptr, &removed))) {
    total_mem_traced_ -= removed.size;
  }
}

#endif  // MPROFILE_SRC_HEAP_H_
