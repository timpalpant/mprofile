// Portions of this file adapted from google-cloud-profiler:
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MPROFILE_SRC_STACKTRACES_H_
#define MPROFILE_SRC_STACKTRACES_H_

#include <Python.h>

#include <vector>

#include "third_party/greg7mdp/parallel-hashmap/phmap.h"

inline bool EqualPyString(PyObject *p1, PyObject *p2) {
  if (p1 == p2) {
    return true;
  }

#if PY_MAJOR_VERSION >= 3
  return PyUnicode_Compare(p1, p2) == 0;
#else
  return _PyString_Eq(p1, p2) == 1;
#endif
}

// FuncLoc captures the location of execution within a function.
// The info is extracted from the current Python stack frames.
// Struct is packed to reduce memory usage from 32 to 24 bytes.
struct FuncLoc {
  // Filename in which this call frame function is defined.
  PyObject *filename;
  // The function name of this call frame.
  PyObject *name;
  // The line number on which this call frame function is defined.
  // We keep this in addition to lineno for two reasons:
  //   1) It's basically free in memory since we pack the struct.
  //   2) Function names are not unique in a file, for instance there will
  //      be multiple __init__ for each class and this allows us to easily
  //      disambiguate them.
  int firstlineno __attribute__((packed));
  // The line number within the file which is currently executing.
  int lineno __attribute__((packed));
};

inline bool operator==(const FuncLoc &lhs, const FuncLoc &rhs) {
  return EqualPyString(lhs.filename, rhs.filename) &&
         EqualPyString(lhs.name, rhs.name) &&
         lhs.firstlineno == rhs.firstlineno && lhs.lineno == rhs.lineno;
}

inline bool operator!=(const FuncLoc &lhs, const FuncLoc &rhs) {
  return !(lhs == rhs);
}

// Maximum number of frames to store from the stack traces sampled.
const int kMaxFramesToCapture = 128;

// CallTrace represents a single stack trace.
// The first num_frames entries in frames are filled, with the 0th
// entry being the current stack frame and the num_frames-1 entry
// being the root of the stack. CallTrace is used to keep temporary
// traces on the stack when recording an allocation.
struct CallTrace {
  std::array<FuncLoc, kMaxFramesToCapture> frames;
  int num_frames;

  void push_back(FuncLoc loc) { frames[num_frames++] = loc; }
  int size() const { return num_frames; }
};

// Extract the current call stack trace for this Python thread.
// Populate the result in the first N frames of the provided CallTrace, up to
// max_frames.
void GetCurrentCallTrace(CallTrace *trace, int max_frames);

// CallTraceSet maintains an interned set of call traces, allowing
// for O(1) lookup while also minimizing memory usage.
//
// Internally, the call traces are stored as CallFrames with a pointer
// to any parent stack. Since we expect a large fraction of parent stacks
// from the root of the program to often be reused, this helps reduce
// memory usage to store stacks that differ only in the final leaf frames.
class CallTraceSet {
 private:
  struct CallFrame {
    // Pointer to parent frame in the call stack, which must be another
    // CallFrame interned within this CallTraceSet.
    // May be null if this is a root frame.
    const CallFrame *parent;
    // The location of this call frame.
    const FuncLoc loc;
  };

 public:
  CallTraceSet() {}
  ~CallTraceSet() {
    for (auto &o : string_table_) {
      Py_DECREF(o);
    }
  }

  // Not copyable or assignable.
  CallTraceSet(const CallTraceSet &) = delete;
  CallTraceSet &operator=(const CallTraceSet &) = delete;

  typedef const CallFrame *TraceHandle;

  // Intern the given CallTrace in the set, and return a handle that can be
  // used to retrieve the trace from the set later.
  const TraceHandle Intern(const CallTrace &trace);
  // Get the trace associated with the given handle.
  std::vector<FuncLoc> GetTrace(const TraceHandle h) const;

  // The number of distinct call stacks currently in the CallTraceSet.
  std::size_t size() const { return trace_leaves_.size(); }
  // Clear all traces and interned strings.
  void Reset();

 private:
  PyObject *InternString(PyObject *s);

  struct TraceEqual {
    bool operator()(const CallFrame &f1, const CallFrame &f2) const {
      return f1.parent == f2.parent && f1.loc == f2.loc;
    }
  };

  struct TraceHash {
    std::size_t operator()(const CallFrame &frame) const {
      const FuncLoc &loc = frame.loc;
      return phmap::HashState().combine(
          0, loc.filename, loc.name, loc.firstlineno, loc.lineno, frame.parent);
    }
  };

  // TraceHandle relies on reference stability, so we need to use node_hash_set
  // and can't use a flat_hash_set.
  phmap::node_hash_set<CallFrame, TraceHash, TraceEqual> trace_leaves_;

  struct PyObjectHash {
    std::size_t operator()(PyObject *p) const { return PyObject_Hash(p); }
  };

  struct PyObjectStringEqual {
    bool operator()(PyObject *p1, PyObject *p2) const {
      return EqualPyString(p1, p2);
    }
  };

  // Interned set of strings referenced by CallFrames in trace_leaves_.
  phmap::flat_hash_set<PyObject *, PyObjectHash, PyObjectStringEqual>
      string_table_;
};

inline PyObject *CallTraceSet::InternString(PyObject *s) {
  auto it = string_table_.insert(s);
  if (it.second) {  // s was added to string table.
    Py_INCREF(s);
  }
  return *it.first;
}

#endif  // MPROFILE_SRC_STACKTRACES_H_
