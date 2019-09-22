// Copyright 2019 Timothy Palpant

#include "benchmark/benchmark.h"

#include "heap.h"

static void BM_HandleMalloc(benchmark::State &state) {
  auto gil_state = PyGILState_Ensure();
  Sampler::SetSamplePeriod(state.range(0));
  HeapProfiler profiler;
  for (auto _ : state) {
    void *fake_ptr = reinterpret_cast<void *>(1234);
    profiler.HandleMalloc(fake_ptr, 1024, false);
  }
  PyGILState_Release(gil_state);
}

static void BM_HandleRawMalloc(benchmark::State &state) {
  Sampler::SetSamplePeriod(state.range(0));
  HeapProfiler profiler;
  for (auto _ : state) {
    void *fake_ptr = reinterpret_cast<void *>(1234);
    profiler.HandleMalloc(fake_ptr, 1024, true);
  }
}

static void BM_HandleFree(benchmark::State &state) {
  Sampler::SetSamplePeriod(state.range(0));
  HeapProfiler profiler;
  int n = 100000;
  for (int i = 0; i < n; i++) {
    void *fake_ptr = reinterpret_cast<void *>((rand() % n) + 1);
    std::size_t sz = rand() % (4 * 1024);
    profiler.HandleMalloc(fake_ptr, sz, true);
  }

  std::size_t i = 0;
  for (auto _ : state) {
    void *fake_ptr = reinterpret_cast<void *>((i++ % n) + 1);
    profiler.HandleFree(fake_ptr);
  }
}

BENCHMARK(BM_HandleMalloc)
    ->Arg(0)
    ->Arg(128)
    ->Arg(1024)
    ->Arg(32 * 1024)
    ->Arg(128 * 1024)
    ->Arg(512 * 1024);
BENCHMARK(BM_HandleRawMalloc)->Arg(128 * 1024)->Threads(2);
BENCHMARK(BM_HandleFree)->Arg(0)->Arg(1024)->Arg(128 * 1024)->Arg(512 * 1024);
