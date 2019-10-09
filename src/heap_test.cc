// Copyright 2019 Timothy Palpant
//
#include "gtest/gtest.h"

#include <thread>
#include "heap.h"

TEST(HeapProfiler, HandleMalloc) {
  HeapProfiler p;

  void *fake_ptr = reinterpret_cast<void *>(123);
  void *fake_ptr2 = reinterpret_cast<void *>(456);
  void *fake_ptr3 = reinterpret_cast<void *>(789);

  // GIL may be held but not required to be held.
  Py_BEGIN_ALLOW_THREADS;
  p.HandleMalloc(fake_ptr, 12, true);
  EXPECT_EQ(p.TotalMemoryTraced(), 12);
  EXPECT_EQ(p.PeakMemoryTraced(), 12);
  Py_END_ALLOW_THREADS;
  p.HandleMalloc(fake_ptr2, 6, false);
  EXPECT_EQ(p.TotalMemoryTraced(), 12 + 6);
  EXPECT_EQ(p.PeakMemoryTraced(), 12 + 6);
  p.HandleMalloc(fake_ptr3, 36, false);
  EXPECT_EQ(p.TotalMemoryTraced(), 12 + 6 + 36);
  EXPECT_EQ(p.PeakMemoryTraced(), 12 + 6 + 36);

  auto snap = p.GetSnapshot();
  EXPECT_EQ(snap.size(), 3);
  std::size_t total = 0;
  for (const void *ptr : snap) {
    total += p.GetSize(ptr);
  }
  EXPECT_EQ(total, 12 + 6 + 36);

  p.Reset();
  EXPECT_EQ(p.GetSnapshot().size(), 0);
}

TEST(HeapProfiler, HandleFree) {
  HeapProfiler p;

  void *fake_ptr = reinterpret_cast<void *>(123);
  void *fake_ptr2 = reinterpret_cast<void *>(456);
  void *fake_ptr3 = reinterpret_cast<void *>(789);

  p.HandleMalloc(fake_ptr, 12, false);
  p.HandleMalloc(fake_ptr2, 6, false);
  p.HandleMalloc(fake_ptr3, 36, false);
  p.HandleFree(fake_ptr2);

  EXPECT_EQ(p.TotalMemoryTraced(), 12 + 36);
  EXPECT_EQ(p.PeakMemoryTraced(), 12 + 6 + 36);

  auto snap = p.GetSnapshot();
  EXPECT_EQ(snap.size(), 2);
  std::size_t total = 0;
  for (const void *ptr : snap) {
    total += p.GetSize(ptr);
  }
  EXPECT_EQ(total, 12 + 36);
}

TEST(HeapProfiler, GetTrace) {
  HeapProfiler p;

  void *fake_ptr = reinterpret_cast<void *>(123);

  p.HandleMalloc(fake_ptr, 12, false);
  auto trace = p.GetTrace(fake_ptr);
  // No Python thread state.
  EXPECT_EQ(trace.size(), 0);

  // Not in traced set.
  void *invalid_ptr = reinterpret_cast<void *>(456);
  trace = p.GetTrace(invalid_ptr);
  EXPECT_EQ(trace.size(), 0);
}
