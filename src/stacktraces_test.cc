// Copyright 2019 Timothy Palpant
#include "stacktraces.h"

#include "gtest/gtest.h"
#include "scoped_object.h"

TEST(FuncLoc, Equal) {
  PyObjectRef filename(PyUnicode_FromString("file.py"));
  PyObjectRef name(PyUnicode_FromString("sleep"));
  FuncLoc f1 = {
      .filename = filename.get(),
      .name = name.get(),
      .firstlineno = 1,
      .lineno = 3,
  };

  FuncLoc f2 = f1;
  f2.lineno = 4;
  FuncLoc f3 = f1;
  FuncLoc f4 = f1;
  PyObjectRef filename2(PyUnicode_FromString("file2.py"));
  f4.filename = filename2.get();

  EXPECT_FALSE(f1 == f2);
  EXPECT_TRUE(f1 != f2);
  EXPECT_TRUE(f1 == f3);
  EXPECT_FALSE(f1 != f3);
  EXPECT_FALSE(f1 == f4);
  EXPECT_TRUE(f1 != f4);
}

TEST(CallTrace, PushBack) {
  CallTrace trace{};
  EXPECT_EQ(trace.size(), 0);

  trace.push_back(FuncLoc{});
  EXPECT_EQ(trace.size(), 1);
  trace.push_back(FuncLoc{});
  EXPECT_EQ(trace.size(), 2);
}

TEST(CallTraceSet, Intern) {
  PyObjectRef filename1(PyUnicode_FromString("file1.py"));
  PyObjectRef name1(PyUnicode_FromString("do_stuff"));
  FuncLoc f1 = {
      .filename = filename1.get(),
      .name = name1.get(),
      .firstlineno = 3,
      .lineno = 4,
  };

  PyObjectRef filename2(PyUnicode_FromString("file2.py"));
  PyObjectRef name2(PyUnicode_FromString("sleep"));
  FuncLoc f2 = {
      .filename = filename2.get(),
      .name = name2.get(),
      .firstlineno = 7,
      .lineno = 8,
  };

  PyObjectRef filename3(PyUnicode_FromString("file2.py"));
  PyObjectRef name3(PyUnicode_FromString("main"));
  FuncLoc f3 = {
      .filename = filename3.get(),
      .name = name3.get(),
      .firstlineno = 11,
      .lineno = 12,
  };

  // f2 but reusing the (interned) filename string from f3.
  FuncLoc f2a = f2;
  f2a.filename = f3.filename;

  CallTrace trace1 = {{f1, f2, f3}, 3};
  CallTrace trace2 = {{f1, f2, f3}, 3};
  CallTrace trace3 = {{f2, f3}, 2};
  CallTrace trace4 = {{f1, f2}, 2};
  CallTrace trace5 = {{f3}, 1};

  CallTraceSet cts;
  auto handle1 = cts.Intern(trace1);
  cts.Intern(trace2);  // Should be no-op.
  auto handle3 = cts.Intern(trace3);
  auto handle4 = cts.Intern(trace4);
  auto handle5 = cts.Intern(trace5);
  // trace1 and trace2 are identical, contribute 3.
  // trace3 is a subset of trace1/2, contributes 0.
  // trace4 is not a subset (due to different root), contributes 2.
  // trace5 is a subset of trace 3, constributes 0.
  EXPECT_EQ(cts.size(), 5);

  auto result1 = cts.GetTrace(handle1);
  std::vector<FuncLoc> expected = {f1, f2a, f3};
  EXPECT_EQ(result1.size(), expected.size());
  for (std::size_t i = 0; i < result1.size(); i++) {
    EXPECT_EQ(result1[i], expected[i]) << "frame " << i << " not equal";
  }

  auto result3 = cts.GetTrace(handle3);
  expected = {f2a, f3};
  EXPECT_EQ(result3.size(), trace3.size());
  for (std::size_t i = 0; i < result3.size(); i++) {
    EXPECT_EQ(result3[i], expected[i]) << "frame " << i << " not equal";
  }

  auto result4 = cts.GetTrace(handle4);
  expected = {f1, f2a};
  EXPECT_EQ(result4.size(), expected.size());
  for (std::size_t i = 0; i < result4.size(); i++) {
    EXPECT_EQ(result4[i], expected[i]) << "frame " << i << " not equal";
  }

  auto result5 = cts.GetTrace(handle5);
  expected = {f3};
  EXPECT_EQ(result5.size(), expected.size());
  EXPECT_EQ(result5[0], expected[0]);

  cts.Reset();
  EXPECT_EQ(cts.size(), 0);
}
