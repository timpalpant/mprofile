#include "heap.h"

// Records the given pointer and size in the current set of live ptrs,
// associated with the current stack trace.
void HeapProfiler::RecordMalloc(void *ptr, size_t size) {
  // The RAW allocator may be called without the GIL held.
  // We may increase references to the filename fields in the
  // code object when we save the trace, so we need to first ensure that
  // the GIL is held.
  CallTrace trace;
  GetCurrentCallTrace(&trace, max_frames_);
  auto trace_handle = traces_.Intern(trace);

  LivePointer lp = {trace_handle, size};
  live_set_.Insert(ptr, lp);
  total_mem_traced_ += size;
  if (total_mem_traced_ > peak_mem_traced_) {
    peak_mem_traced_ = total_mem_traced_;
  }
}

// Callback used to extract all traces from AddressMap into a std::vector.
template <class Value>
void AppendToVector(const void *ptr, Value lp, std::vector<const void *> &v) {
  v.push_back(ptr);
}

std::vector<const void *> HeapProfiler::GetSnapshot() {
  std::vector<const void *> snap;
  live_set_.Iterate<std::vector<const void *> &>(&AppendToVector, snap);
  return snap;
}

std::vector<FuncLoc> HeapProfiler::GetTrace(const void *ptr) {
  const LivePointer *lp = live_set_.Find(ptr);
  if (lp == nullptr) {
    return {};
  }
  return traces_.GetTrace(lp->trace_handle);
}

std::size_t HeapProfiler::GetSize(const void *ptr) {
  const LivePointer *lp = live_set_.Find(ptr);
  if (lp == nullptr) {
    return 0;
  }

  return lp->size;
}

void HeapProfiler::Reset() {
  live_set_.Reset();
  total_mem_traced_ = 0;
  peak_mem_traced_ = 0;  // Matches tracemalloc behavior.
  traces_.Reset();
}

std::size_t HeapProfiler::TotalMemoryTraced() {
  return total_mem_traced_;
}

std::size_t HeapProfiler::PeakMemoryTraced() {
  return peak_mem_traced_;
}
