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

#include "stacktraces.h"

#include <Python.h>
#include <frameobject.h>

#include "third_party/greg7mdp/parallel-hashmap/phmap_utils.h"

namespace {

bool SkipFrame(PyCodeObject *f_code) {
  if (f_code == nullptr) {
    return true;
  }

  // If the filename begins with a <, skip it.
  // These are typically frames from the importer machinery, and for
  // import-time allocations make the stacks 3x as large.
  const Py_UCS4 first_char =
      PyUnicode_READ_CHAR(f_code->co_filename, 0);
  return (first_char == 0x3c);
}

}  // namespace

void GetCurrentCallTrace(CallTrace *trace, int max_frames) {
  trace->num_frames = 0;
  if (max_frames > kMaxFramesToCapture) {
    max_frames = kMaxFramesToCapture;
  }

  PyThreadState *ts = PyGILState_GetThisThreadState();
  if (ts == nullptr) {
    return;
  }

#if PY_VERSION_HEX >= 0x030900B1
  PyFrameObject *pyframe = PyThreadState_GetFrame(ts);
#else
  PyFrameObject *pyframe = ts->frame;
#endif

  while (pyframe != nullptr && trace->size() < max_frames) {
#if PY_VERSION_HEX >= 0x030900B1
    PyCodeObject *f_code = PyFrame_GetCode(pyframe);
#else
    PyCodeObject *f_code = pyframe->f_code;
#endif

    if (!SkipFrame(f_code)) {
      Py_XINCREF(f_code->co_filename);
      Py_XINCREF(f_code->co_name);
      trace->push_back(FuncLoc{
        .filename = f_code->co_filename,
        .name = f_code->co_name,
        .firstlineno = f_code->co_firstlineno,
        .lineno = PyFrame_GetLineNumber(pyframe)
      });
    }

#if PY_VERSION_HEX >= 0x030900B1
    PyFrameObject *prev_frame = pyframe;
    pyframe = PyFrame_GetBack(pyframe);
    Py_XDECREF(f_code);
    Py_XDECREF(prev_frame);
#else
    pyframe = pyframe->f_back;
#endif
  }

#if PY_VERSION_HEX >= 0x030900B1
  Py_XDECREF(pyframe);
#endif
}

void FreeCallTrace(const CallTrace &trace) {
  for (int i = 0; i < trace.size(); i++) {
    const FuncLoc& loc = trace.frames[i];
    Py_XDECREF(loc.filename);
    Py_XDECREF(loc.name);
  }
}

const CallTraceSet::TraceHandle CallTraceSet::Intern(const CallTrace &trace) {
  std::size_t num_to_intern = trace.size();
  const CallFrame *parent = nullptr;
  // This is a slightly faster path where we try to find each frame, starting
  // from the root, in the interned trace set, but without performing any
  // string interning. This is the common case since much of the stack trace
  // will likely already be interned. Once we fail to find a frame already
  // in the set, we proceed to add that frame and all descendants below.
  for (int i = trace.size() - 1; i >= 0; i--) {
    CallFrame frame{parent, trace.frames[i]};
    auto it = trace_leaves_.find(frame);
    if (it == trace_leaves_.end()) {
      break;
    }

    // Stack down to this frame is already interned in the set.
    num_to_intern--;
    parent = &(*it);
  }

  // Now start at the first frame we need to intern, and walk down the
  // stack to the leaf, interning and updating parent pointer at each step.
  // In the case where num_to_intern == 0, then this is skipped and we
  // already have in parent a pointer to the interned leaf.
  for (int i = num_to_intern - 1; i >= 0; i--) {
    FuncLoc loc = trace.frames[i];
    loc.filename = InternString(loc.filename);
    loc.name = InternString(loc.name);

    CallFrame frame{parent, loc};
    auto it = trace_leaves_.emplace(frame);
    parent = &(*it.first);
  }

  return parent;
}

std::vector<FuncLoc> CallTraceSet::GetTrace(
    const CallTraceSet::TraceHandle h) const {
  std::vector<FuncLoc> result;
  if (h == nullptr) {
    return result;
  }

  std::vector<FuncLoc>::size_type num_frames = 1;
  for (const CallFrame *p = h->parent; p != nullptr; p = p->parent) {
    num_frames++;
  }

  result.reserve(num_frames);
  result.push_back(h->loc);
  for (const CallFrame *p = h->parent; p != nullptr; p = p->parent) {
    result.push_back(p->loc);
  }

  return result;
}

void CallTraceSet::Reset() {
  for (auto &o : string_table_) {
    Py_DECREF(o);
  }

  phmap::flat_hash_set<PyObject *, PyObjectHash, PyObjectStringEqual>
      empty_string_table;
  std::swap(string_table_, empty_string_table);
  phmap::node_hash_set<CallFrame, TraceHash, TraceEqual> empty_trace_leaves;
  std::swap(trace_leaves_, empty_trace_leaves);
}
