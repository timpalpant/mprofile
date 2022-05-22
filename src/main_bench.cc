// Copyright 2019 Timothy Palpant

#include <Python.h>

#include "benchmark/benchmark.h"

int main(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  PyThreadState* py_tstate = NULL;
  Py_Initialize();
#if PY_VERSION_HEX < 0x030900B1
  PyEval_InitThreads();
#endif

  py_tstate = PyGILState_GetThisThreadState();
  PyEval_ReleaseThread(py_tstate);

  ::benchmark::RunSpecifiedBenchmarks();

  PyGILState_Ensure();
  Py_Finalize();

  return 0;
}
