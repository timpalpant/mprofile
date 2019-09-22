// Copyright 2019 Timothy Palpant

#include "gtest/gtest.h"

#include <thread>
#include "reentrant_scope.h"

TEST(ReentrantScope, TopLevel) {
  ReentrantScope scope;
  EXPECT_TRUE(scope.is_top_level());

  ReentrantScope scope2;
  EXPECT_TRUE(scope.is_top_level());
  EXPECT_FALSE(scope2.is_top_level());
}

static void EnterReentrantScope() {
  ReentrantScope scope;
  EXPECT_TRUE(scope.is_top_level());
}

TEST(ReentrantScope, IsThreadLocal) {
  ReentrantScope scope;
  EXPECT_TRUE(scope.is_top_level());

  // Instantiation of ReentrantScope in other threads
  // should consider top-level scope to be thread-local.
  std::thread t(&EnterReentrantScope);
  t.join();

  EXPECT_TRUE(scope.is_top_level());
}
