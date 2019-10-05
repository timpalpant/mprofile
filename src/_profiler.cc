
#include <Python.h>

#include <memory>
#include "third_party/google/tcmalloc/sampler.h"

#include "heap.h"
#include "log.h"
#include "malloc_patch.h"
#include "scoped_object.h"

namespace {

#if PY_MAJOR_VERSION >= 3
#define INT_FROM_LONG PyLong_FromLong
#define INT_FROM_SIZE_T PyLong_FromSize_t
#else
#define INT_FROM_LONG PyInt_FromLong
#define INT_FROM_SIZE_T PyInt_FromSize_t
#endif

bool StartProfilerWithParams(uint64_t max_frames, uint64_t sample_rate) {
  if (IsHeapProfilerAttached()) {
    PyErr_SetString(PyExc_RuntimeError, "The profiler is already running.");
    return false;
  }

  if (max_frames < 0 || max_frames > kMaxFramesToCapture) {
    PyErr_SetString(PyExc_ValueError,
                    "the number of frames must be in range 0-128.");
    return false;
  }

  Sampler::SetSamplePeriod(sample_rate);
  AttachHeapProfiler(std::unique_ptr<HeapProfiler>(new HeapProfiler(max_frames)));
  return true;
}

PyObject *StartProfiler(PyObject *self, PyObject *args, PyObject *kwds) {
  static const char *kwlist[] = {"max_frames", "sample_rate", nullptr};
  uint64_t max_frames = kMaxFramesToCapture;
  uint64_t sample_rate = 0;
  // NB: PyArg_ParseTupleAndKeywords raises a Py exception on error.
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|LL",
                                   const_cast<char **>(kwlist), &max_frames,
                                   &sample_rate)) {
    return nullptr;
  }

  if (!StartProfilerWithParams(max_frames, sample_rate)) {
    return nullptr;
  }

  Py_RETURN_NONE;
}

PyObject *StopProfiler(PyObject *self, PyObject *args) {
  DetachHeapProfiler();
  Py_RETURN_NONE;
}

PyObject *IsTracing(PyObject *self, PyObject *args) {
  if (IsHeapProfilerAttached()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
}

PyObject *ClearTraces(PyObject *self, PyObject *args) {
  if (!IsHeapProfilerAttached()) {
    PyErr_SetString(PyExc_RuntimeError, "The heap profiler is not started.");
    return nullptr;
  }

  ResetHeapProfiler();
  Py_RETURN_NONE;
}

PyObject *GetTracemallocMemory(PyObject *self, PyObject *args) {
  if (!IsHeapProfilerAttached()) {
    return INT_FROM_LONG(0);
  }

  std::size_t mem_usage = GetHeapProfilerMemUsage();
  return INT_FROM_SIZE_T(mem_usage);
}

PyObject *GetTracedMemory(PyObject *self, PyObject *args) {
  std::pair<std::size_t, std::size_t> mem_usage = GetHeapProfilerTracedMemory();
  PyObject *size_obj = INT_FROM_SIZE_T(mem_usage.first);
  PyObject *peak_size_obj = INT_FROM_SIZE_T(mem_usage.second);
  return Py_BuildValue("NN", size_obj, peak_size_obj);
}

PyObject *TakeSnapshot(PyObject *self, PyObject *args) {
  if (!IsHeapProfilerAttached()) {
    return PyList_New(0);
  }

  return GetHeapProfile();
}

PyObject *GetSampleRate(PyObject *self, PyObject *args) {
  if (!IsHeapProfilerAttached()) {
    PyErr_SetString(PyExc_RuntimeError, "The heap profiler is not started.");
    return nullptr;
  }

  return INT_FROM_LONG(Sampler::GetSamplePeriod());
}

PyObject *GetTracebackLimit(PyObject *self, PyObject *args) {
  int max_frames = 1;  // Match behavior of tracemalloc.
  if (IsHeapProfilerAttached()) {
    max_frames = GetMaxFrames();
  }

  return INT_FROM_LONG(max_frames);
}

PyObject *GetObjectTraceback(PyObject *self, PyObject *args) {
  if (!IsHeapProfilerAttached()) {
    PyErr_SetString(PyExc_RuntimeError, "The heap profiler is not started.");
    return nullptr;
  }

  PyObject *o;
  if (!PyArg_ParseTuple(args, "O", &o)) {
    return nullptr;
  }

  void *ptr = reinterpret_cast<void *>(o);
  return GetTrace(ptr);
}

int GetEnvFrames() {
  char *p = Py_GETENV("MPROFILEFRAMES");
  if (p == NULL || *p == '\0') {
    return kMaxFramesToCapture;
  }

  char *endptr = p;
  int max_frames = strtol(p, &endptr, 10);
  if (*endptr != '\0' || max_frames < 0) {
    Py_FatalError("MPROFILEFRAMES: invalid number of frames");
  }

  return max_frames;
}

PyObject *MProfileAtexit(PyObject *self) {
  DetachHeapProfiler();
  Py_RETURN_NONE;
}

// Adapted from pytracemalloc.
int MProfileAtexitRegister(PyObject *module) {
  PyObjectRef method(PyObject_GetAttrString(module, "_atexit"));
  if (method == nullptr) {
    return -1;
  }

  PyObjectRef atexit(PyImport_ImportModule("atexit"));
  if (atexit == nullptr) {
    if (!PyErr_Warn(PyExc_ImportWarning,
                    "atexit module is missing: "
                    "cannot automatically disable mprofile at exit")) {
      PyErr_Clear();
      return 0;
    }

    return -1;
  }

  PyObjectRef func(PyObject_GetAttrString(atexit.get(), "register"));
  if (func == nullptr) {
    return -1;
  }

  PyObjectRef result(
      PyObject_CallFunctionObjArgs(func.get(), method.get(), nullptr));
  if (result == nullptr) {
    return -1;
  }

  return 0;
}

bool MProfileInit(PyObject *self) {
  if (MProfileAtexitRegister(self) < 0) {
    LogWarning("mprofile: Failed to install atexit handler");
  }

  char *p = Py_GETENV("MPROFILERATE");
  if (p == NULL || *p == '\0') {
    return true;
  }

  char *endptr = p;
  int sample_rate = strtol(p, &endptr, 10);
  if (*endptr != '\0' || sample_rate < 0) {
    Py_FatalError("MPROFILERATE: invalid sample rate");
  }

  if (!StartProfilerWithParams(GetEnvFrames(), sample_rate)) {
    return false;
  }

  return true;
}

PyMethodDef ProfilerMethods[] = {
    {"start", (PyCFunction)StartProfiler, METH_VARARGS | METH_KEYWORDS,
     "Start memory profiling."},
    {"stop", StopProfiler, METH_VARARGS, "Stop memory profiling."},
    {"is_tracing", IsTracing, METH_VARARGS,
     "True/False if memory profiler is active."},
    {"clear_traces", ClearTraces, METH_VARARGS,
     "Clear all current traces to reclaim memory."},
    {"_get_traces", TakeSnapshot, METH_VARARGS,
     "Get snapshot of live heap allocations."},
    {"get_sample_rate", GetSampleRate, METH_VARARGS,
     "Get the current sample rate for allocations."},
    {"get_traceback_limit", GetTracebackLimit, METH_VARARGS,
     "Get the max number of frames that will be stored in a traceback."},
    {"get_tracemalloc_memory", GetTracemallocMemory, METH_VARARGS,
     "Get the estimated memory used by mprofile module (in bytes)."},
    {"get_traced_memory", GetTracedMemory, METH_VARARGS,
     "Get the total memory traced by mprofile module (in bytes)."},
    {"_get_object_traceback", GetObjectTraceback, METH_VARARGS,
     "Get the traceback where a particular object was allocated."},

    // Private, used as an atexit handler to disable heap profiler.
    {"_atexit", (PyCFunction)MProfileAtexit, METH_NOARGS},

    {nullptr, nullptr, 0, nullptr} /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT, "_profiler", /* name of module */
    "mprofile C++ extension module",    /* module documentation */
    -1, ProfilerMethods};
}  // namespace

PyMODINIT_FUNC PyInit__profiler(void) {
  PyObject *m = PyModule_Create(&moduledef);
  if (m == nullptr) {
    return nullptr;
  }

  if (!MProfileInit(m)) {
    return nullptr;
  }

  return m;
}
#else
}  // namespace

PyMODINIT_FUNC init_profiler(void) {
  PyObject *m = Py_InitModule("_profiler", ProfilerMethods);
  if (m == nullptr) {
    return;
  }

  MProfileInit(m);
}
#endif
