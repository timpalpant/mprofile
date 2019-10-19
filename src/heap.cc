#include "heap.h"

// Records the given pointer and size in the current set of live ptrs,
// associated with the current stack trace. The GIL must be held.
void HeapProfiler::RecordMalloc(void *ptr, size_t size) {
  CallTrace trace;
  GetCurrentCallTrace(&trace, max_frames_);
  auto trace_handle = traces_.Intern(trace);

  std::lock_guard<SpinLock> lock(mu_);
  LivePointer lp = {trace_handle, size};
  live_set_.Insert(ptr, lp);
  total_mem_traced_ += size;
  if (total_mem_traced_ > peak_mem_traced_) {
    peak_mem_traced_ = total_mem_traced_;
  }
}

// Callback used to extract all pointers from AddressMap into a std::vector.
template <class Value>
void AppendToVector(const void *ptr, Value lp, std::vector<const void *> &v) {
  v.push_back(ptr);
}

std::vector<const void *> HeapProfiler::GetSnapshot() {
  std::lock_guard<SpinLock> lock(mu_);
  std::vector<const void *> snap;
  live_set_.Iterate<std::vector<const void *> &>(&AppendToVector, snap);
  return snap;
}

std::vector<FuncLoc> HeapProfiler::GetTrace(const void *ptr) {
  std::lock_guard<SpinLock> lock(mu_);
  const LivePointer *lp = live_set_.Find(ptr);
  if (lp == nullptr) {
    return {};
  }
  return traces_.GetTrace(lp->trace_handle);
}

std::size_t HeapProfiler::GetSize(const void *ptr) {
  std::lock_guard<SpinLock> lock(mu_);
  const LivePointer *lp = live_set_.Find(ptr);
  if (lp == nullptr) {
    return 0;
  }

  return lp->size;
}

void HeapProfiler::Reset() {
  std::lock_guard<SpinLock> lock(mu_);
  live_set_.Reset();
  total_mem_traced_ = 0;
  peak_mem_traced_ = 0;  // Matches tracemalloc behavior.
  traces_.Reset();
}

std::size_t HeapProfiler::TotalMemoryTraced() {
  std::lock_guard<SpinLock> lock(mu_);
  return total_mem_traced_;
}

std::size_t HeapProfiler::PeakMemoryTraced() {
  std::lock_guard<SpinLock> lock(mu_);
  return peak_mem_traced_;
}
