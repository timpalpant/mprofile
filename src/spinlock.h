// Copyright 2019 Timothy Palpant

#ifndef MPROFILE_SRC_SPINLOCK_H_
#define MPROFILE_SRC_SPINLOCK_H_

#include <atomic>

// SpinLock is a simple adapter to use an std::atomic_flag
// with a std::lock_guard.
class SpinLock {
 public:
  SpinLock() {}
  // Not copyable or assignable.
  SpinLock(const SpinLock &) = delete;
  SpinLock &operator=(const SpinLock &) = delete;

  void lock() {
    while (flag_.test_and_set(std::memory_order_acquire))
      ;
  }

  void unlock() { flag_.clear(std::memory_order_release); }

 private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

#endif  // MPROFILE_SRC_SPINLOCK_H
