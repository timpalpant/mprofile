// Copyright 2019 Timothy Palpant

#include "benchmark/benchmark.h"

#include "reentrant_scope.h"

static void BM_ReentrantScope(benchmark::State &state) {
  for (auto _ : state) {
    ReentrantScope scope;
    if (!scope.is_top_level()) {
      state.SkipWithError("scope is top-level!");
    }
  }
}

BENCHMARK(BM_ReentrantScope)->Threads(2);
