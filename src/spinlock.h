// Copyright 2019 Timothy Palpant

#ifndef MPROFILE_SRC_SPINLOCK_H_
#define MPROFILE_SRC_SPINLOCK_H_

#include <atomic>

// SpinLock is a simple adapter to use an std::atomic<bool>
// with a std::lock_guard.
//
// Adapted from: https://rigtorp.se/spinlock/
class SpinLock {
 public:
  SpinLock() {}
  // Not copyable or assignable.
  SpinLock(const SpinLock &) = delete;
  SpinLock &operator=(const SpinLock &) = delete;

  void lock() {
    while (true) {
      // Optimistically assume the lock is free on the first try.
      if (!flag_.exchange(true, std::memory_order_acquire)) {
        break;
      }

      // Wait for lock to be released without generating cache misses.
      while (flag_.load(std::memory_order_relaxed)) {
        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention
        // between hyper-threads.
        __builtin_ia32_pause();
      }
    }
  }

  void unlock() { flag_.store(false, std::memory_order_release); }

 private:
  std::atomic<bool> flag_ = {false};
};

#endif  // MPROFILE_SRC_SPINLOCK_H
