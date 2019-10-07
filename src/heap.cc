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

  Spinlock lock(flag_);
  LivePointer lp = {trace_handle, size};
  live_set_.emplace(ptr, lp);
  total_mem_traced_ += size;
  if (total_mem_traced_ > peak_mem_traced_) {
    peak_mem_traced_ = total_mem_traced_;
  }
}

std::vector<void *> HeapProfiler::GetSnapshot() {
  Spinlock lock(flag_);
  std::vector<void *> snap;
  snap.reserve(live_set_.size());

  for (const auto &item : live_set_) {
    snap.push_back(item.first);
  }

  return snap;
}

std::vector<FuncLoc> HeapProfiler::GetTrace(const void *ptr) {
  Spinlock lock(flag_);
  auto it = live_set_.find(ptr);
  if (it == live_set_.end()) {
    return {};
  }

  const LivePointer &lp = it->second;
  return traces_.GetTrace(lp.trace_handle);
}

std::size_t HeapProfiler::GetSize(const void *ptr) {
  Spinlock lock(flag_);
  auto it = live_set_.find(ptr);
  if (it == live_set_.end()) {
    return {};
  }

  const LivePointer &lp = it->second;
  return lp.size;
}

void HeapProfiler::Reset() {
  Spinlock lock(flag_);

  phmap::flat_hash_map<void *, const LivePointer> empty_live_set;
  std::swap(empty_live_set, live_set_);
  total_mem_traced_ = 0;
  peak_mem_traced_ = 0;  // Matches tracemalloc behavior.
  traces_.Reset();
}

std::size_t HeapProfiler::TotalMemoryTraced() {
  Spinlock lock(flag_);
  return total_mem_traced_;
}

std::size_t HeapProfiler::PeakMemoryTraced() {
  Spinlock lock(flag_);
  return peak_mem_traced_;
}
