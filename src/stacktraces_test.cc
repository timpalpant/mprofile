// Copyright 2019 Timothy Palpant
#include "stacktraces.h"

#include "gtest/gtest.h"
#include "scoped_object.h"

#if PY_MAJOR_VERSION >= 3
#define STRING_FROMSTRING PyUnicode_FromString
#else
#define STRING_FROMSTRING PyString_FromString
#endif

TEST(FuncLoc, Equal) {
  PyObjectRef filename(STRING_FROMSTRING("file.py"));
  PyObjectRef name(STRING_FROMSTRING("sleep"));
  FuncLoc f1(filename.get(), name.get(), 1, 3);

  FuncLoc f2(f1.filename(), f1.name(), 1, 4);
  FuncLoc f3 = f1;
  PyObjectRef filename2(STRING_FROMSTRING("file2.py"));
  FuncLoc f4(filename2.get(), f1.name(), f1.firstlineno(), f1.lineno());

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

  trace.push_back(FuncLoc());
  EXPECT_EQ(trace.size(), 1);
  trace.push_back(FuncLoc());
  EXPECT_EQ(trace.size(), 2);
}

TEST(CallTraceSet, Intern) {
  PyObjectRef filename1(STRING_FROMSTRING("file1.py"));
  PyObjectRef name1(STRING_FROMSTRING("do_stuff"));
  FuncLoc f1(filename1.get(), name1.get(), 3, 4);

  PyObjectRef filename2(STRING_FROMSTRING("file2.py"));
  PyObjectRef name2(STRING_FROMSTRING("sleep"));
  FuncLoc f2(filename2.get(), name2.get(), 7, 8);

  PyObjectRef filename3(STRING_FROMSTRING("file2.py"));
  PyObjectRef name3(STRING_FROMSTRING("main"));
  FuncLoc f3(filename3.get(), name3.get(), 11, 12);

  // f2 but reusing the (interned) filename string from f3.
  FuncLoc f2a(f3.filename(), f2.name(), f2.firstlineno(), f2.lineno());

  CallTrace trace1;
  trace1.push_back(f1);
  trace1.push_back(f2);
  trace1.push_back(f3);
  CallTrace trace2;
  trace2.push_back(f1);
  trace2.push_back(f2);
  trace2.push_back(f3);
  CallTrace trace3;
  trace3.push_back(f2);
  trace3.push_back(f3);
  CallTrace trace4;
  trace4.push_back(f1);
  trace4.push_back(f2);
  CallTrace trace5;
  trace5.push_back(f3);

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
