// Copyright 2019 Timothy Palpant

#include "benchmark/benchmark.h"

#include "sampler.h"

static void BM_RecordAllocation(benchmark::State& state) {
    Sampler s;
    for (auto _ : state) {
        s.RecordAllocation(10);
    }
}

static void BM_TryRecordAllocationFast(benchmark::State& state) {
    Sampler s;
    for (auto _ : state) {
        if(!s.TryRecordAllocationFast(10)) {
            s.RecordAllocation(10);
        }
    }
}

BENCHMARK(BM_RecordAllocation);
BENCHMARK(BM_TryRecordAllocationFast);
