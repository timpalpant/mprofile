// Copyright 2019 Timothy Palpant
// Adapted from heapprof, Copyright 2019 Humu, Inc.

#include "malloc_patch.h"

#include <Python.h>

#include "scoped_object.h"

namespace {

// Our global profiler state.
static std::unique_ptr<HeapProfiler> g_profiler;

// The underlying allocators that we're going to wrap. This gets filled in with
// meaningful content during AttachProfiler.
//
// Protected by the GIL, which must be held to call AttachProfiler.
struct {
  PyMemAllocatorEx raw;
  PyMemAllocatorEx mem;
  PyMemAllocatorEx obj;
} g_base_allocators;

// The various Python allocators (raw, mem, obj) sometimes delegate to
// each other, which can result in re-entrant calls to our heap tracer.
// For example, the PyObject allocator delegates to PyMem allocator to
// allocate arenas of memory. We only want to trace the outermost call.
// ReentrantScope is a simple RAII-style scope guard to do this.
class ReentrantScope {
 public:
  ReentrantScope() : is_outer_scope_(!is_active_) {
    if (is_outer_scope_) {
      is_active_ = true;
    }
  }

  ~ReentrantScope() {
    if (is_outer_scope_) {
      is_active_ = false;
    }
  }

  bool is_outer_scope() { return is_outer_scope_; }

 private:
  const bool is_outer_scope_;
  static thread_local bool is_active_;
};

thread_local bool ReentrantScope::is_active_ = false;

// The wrapped methods with which we will replace the standard malloc, etc. In
// each case, ctx will be a pointer to the appropriate base allocator.

void *WrappedMalloc(void *ctx, size_t size) {
  ReentrantScope scope;
  PyMemAllocatorEx *alloc = reinterpret_cast<PyMemAllocatorEx *>(ctx);
  void *ptr = alloc->malloc(alloc->ctx, size);
  if (scope.is_outer_scope()) {
    bool is_raw = alloc == &g_base_allocators.raw;
    g_profiler->HandleMalloc(ptr, size, is_raw);
  }
  return ptr;
}

void *WrappedCalloc(void *ctx, size_t nelem, size_t elsize) {
  ReentrantScope scope;
  PyMemAllocatorEx *alloc = reinterpret_cast<PyMemAllocatorEx *>(ctx);
  void *ptr = alloc->calloc(alloc->ctx, nelem, elsize);
  if (scope.is_outer_scope()) {
    bool is_raw = alloc == &g_base_allocators.raw;
    g_profiler->HandleMalloc(ptr, nelem * elsize, is_raw);
  }
  return ptr;
}

void *WrappedRealloc(void *ctx, void *ptr, size_t new_size) {
  ReentrantScope scope;
  PyMemAllocatorEx *alloc = reinterpret_cast<PyMemAllocatorEx *>(ctx);
  void *ptr2 = alloc->realloc(alloc->ctx, ptr, new_size);
  if (scope.is_outer_scope()) {
    bool is_raw = alloc == &g_base_allocators.raw;
    g_profiler->HandleRealloc(ptr, ptr2, new_size, is_raw);
  }
  return ptr2;
}

void WrappedFree(void *ctx, void *ptr) {
  ReentrantScope scope;
  PyMemAllocatorEx *alloc = reinterpret_cast<PyMemAllocatorEx *>(ctx);
  // Remove from traced set before delegating to actual free to prevent possible
  // race if memory address is reused.
  if (scope.is_outer_scope()) {
    g_profiler->HandleFree(ptr);
  }
  alloc->free(alloc->ctx, ptr);
}

PyObjectRef NewPyTrace(const std::vector<FuncLoc> &trace) {
  // Build the key as a Python tuple of tuples of frames:
  // ((func_name, filename, start_line, line_num), ...).
  PyObjectRef py_frames(PyTuple_New(trace.size()));
  if (py_frames == nullptr) {
    return nullptr;
  }

  for (std::size_t i = 0; i < trace.size(); i++) {
    const FuncLoc &loc = trace[i];
    PyObject *py_frame = Py_BuildValue("(OOii)", loc.name, loc.filename,
                                       loc.firstlineno, loc.lineno);
    if (py_frame == nullptr) {
      return nullptr;
    }

    // PyTuple_SET_ITEM is like PyTuple_SetItem(), but does no error checking.
    // Error checking is unnecessary here as we are filling precreated brand
    // new tuple. Note that PyTuple_SET_ITEM does NOT increase the reference
    // count for the inserted item. We are no longer responsible for
    // decreasing the reference count of py_frame. It'll be decreased when
    // py_frames is deallocated.
    PyTuple_SET_ITEM(py_frames.get(), i, py_frame);
  }

  return py_frames;
}

PyObjectRef NewPyTraces(const std::vector<const void *> &snap) {
  // Asserts that GIL is held in debug mode.
  assert(PyGILState_Check());

  PyObjectRef py_traces(PyTuple_New(snap.size()));
  if (py_traces == nullptr) {
    return nullptr;
  }

  // Temporary used to dedupe and intern identical tracebacks.
  PyObjectRef py_tracebacks(PyDict_New());
  if (py_tracebacks == nullptr) {
    return nullptr;
  }

  std::size_t i = 0;
  for (const void *ptr : snap) {
    // Build the Trace value as a Python tuple (size, traceback).
    auto trace = g_profiler->GetTrace(ptr);
    PyObjectRef unknown_filename;
    PyObjectRef unknown_name;
    if (trace.size() == 0) {
      unknown_filename.reset(PyUnicode_InternFromString("<unknown>"));
      unknown_name.reset(PyUnicode_InternFromString("[Unknown - No Python thread state]"));
      trace.push_back({
          .filename = unknown_filename.get(),
          .name = unknown_name.get(),
      });
    }

    PyObjectRef py_frames(NewPyTrace(trace));

    // Dedupe traceback tuples to reduce memory usage.
    PyObject *py_traceback =
        PyDict_GetItem(py_tracebacks.get(), py_frames.get());
    if (py_traceback != nullptr) {
      // PyDict_GetItem returns a borrowed reference to the interned traceback.
      // Discard our duplicate copy and take a new reference to the one in the
      // interned set.
      Py_INCREF(py_traceback);
      py_frames.reset(py_traceback);
    } else if (PyDict_SetItem(py_tracebacks.get(), py_frames.get(),
                              py_frames.get()) < 0) {
      // PyDict_SetItem increases the reference count for both key and item. We
      // are responsible for decreasing the reference count for py_frames.
      return nullptr;
    }

    std::size_t size = g_profiler->GetSize(ptr);
    PyObject *py_trace = Py_BuildValue("(iO)", size, py_frames.get());
    if (py_trace == nullptr) {
      return nullptr;
    }

    PyTuple_SET_ITEM(py_traces.get(), i, py_trace);
    i++;
  }

  return py_traces;
}

}  // namespace

/* Our API */

void AttachHeapProfiler(std::unique_ptr<HeapProfiler> profiler) {
  g_profiler = std::move(profiler);

  PyMemAllocatorEx alloc;
  alloc.malloc = WrappedMalloc;
  alloc.calloc = WrappedCalloc;
  alloc.realloc = WrappedRealloc;
  alloc.free = WrappedFree;

  // Grab the base allocators
  PyMem_GetAllocator(PYMEM_DOMAIN_RAW, &g_base_allocators.raw);
  PyMem_GetAllocator(PYMEM_DOMAIN_MEM, &g_base_allocators.mem);
  PyMem_GetAllocator(PYMEM_DOMAIN_OBJ, &g_base_allocators.obj);

  // And repoint allocation at our wrapped methods!
  alloc.ctx = &g_base_allocators.raw;
  PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &alloc);

  alloc.ctx = &g_base_allocators.mem;
  PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &alloc);

  alloc.ctx = &g_base_allocators.obj;
  PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &alloc);
}

void DetachHeapProfiler() {
  if (IsHeapProfilerAttached()) {
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &g_base_allocators.raw);
    PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &g_base_allocators.mem);
    PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &g_base_allocators.obj);

    g_profiler.reset(nullptr);
  }
}

bool IsHeapProfilerAttached() { return g_profiler != nullptr; }

// Returns a new reference.
PyObject *GetHeapProfile() {
  if (!IsHeapProfilerAttached()) {
    return nullptr;
  }

  auto snap = g_profiler->GetSnapshot();
  auto py_snap = NewPyTraces(snap);
  return py_snap.release();
}

int GetMaxFrames() {
  if (!IsHeapProfilerAttached()) {
    return -1;
  }

  return g_profiler->GetMaxFrames();
}

PyObject *GetTrace(void *ptr) {
  if (!IsHeapProfilerAttached()) {
    return nullptr;
  }

  auto trace = g_profiler->GetTrace(ptr);
  auto py_trace = NewPyTrace(trace);
  return py_trace.release();
}

void ResetHeapProfiler() {
  if (!IsHeapProfilerAttached()) {
    return;
  }

  // Asserts that GIL is held in debug mode.
  assert(PyGILState_Check());

  g_profiler->Reset();
}

std::size_t GetHeapProfilerMemUsage() {
  if (!IsHeapProfilerAttached()) {
    return 0;
  }

  return 0;  // Not yet implemented.
}

std::pair<std::size_t, std::size_t> GetHeapProfilerTracedMemory() {
  if (!IsHeapProfilerAttached()) {
    return {0, 0};
  }

  std::size_t current = g_profiler->TotalMemoryTraced();
  std::size_t peak = g_profiler->PeakMemoryTraced();
  return {current, peak};
}
