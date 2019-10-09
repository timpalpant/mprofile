// Copyright 2019 Timothy Palpant

#ifndef MPROFILE_SRC_SPINLOCK_H_
#define MPROFILE_SRC_SPINLOCK_H_

#include <atomic>

// Spinlock is a simple RAII-style wrapper to acquire an atomic flag
// in a given scope. The flag will be cleared by the destructor.
class Spinlock {
 public:
  explicit Spinlock(std::atomic_flag &flag) : flag_(flag) {
    while (flag_.test_and_set(std::memory_order_acquire))
      ;
  }

  ~Spinlock() { flag_.clear(std::memory_order_release); }

  // Not copyable or assignable.
  Spinlock(const Spinlock &) = delete;
  Spinlock &operator=(const Spinlock &) = delete;

 private:
  std::atomic_flag &flag_;
};

#endif  // MPROFILE_SRC_SPINLOCK_H
