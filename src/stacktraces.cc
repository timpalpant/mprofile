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

bool SkipFrame(PyFrameObject *pyframe) {
  // If the filename begins with a <, skip it.
  // These are typically frames from the importer machinery, and for
  // import-time allocations make the stacks 3x as large.
#if PY_MAJOR_VERSION >= 3
  const Py_UCS4 first_char =
      PyUnicode_READ_CHAR(pyframe->f_code->co_filename, 0);
  return (first_char == 0x3c);
#else
  const char *first_char = PyString_AsString(pyframe->f_code->co_filename);
  return (first_char != nullptr && *first_char == 0x3c);
#endif
}

}  // namespace

void GetCurrentCallTrace(CallTrace *trace, int max_frames) {
  trace->num_frames = 0;
  if (max_frames > kMaxFramesToCapture) {
    max_frames = kMaxFramesToCapture;
  }

  PyThreadState *ts = PyGILState_GetThisThreadState();
  if (ts != nullptr) {
    PyFrameObject *pyframe = ts->frame;
    while (pyframe != nullptr && trace->size() < max_frames) {
      if (SkipFrame(pyframe)) {
        pyframe = pyframe->f_back;
        continue;
      }

      const PyCodeObject *code = pyframe->f_code;
      trace->push_back(FuncLoc{
          .filename = code->co_filename,
          .name = code->co_name,
          .firstlineno = code->co_firstlineno,
          .lineno = PyFrame_GetLineNumber(pyframe),
      });

      pyframe = pyframe->f_back;
    }
  }
}

const CallTraceSet::TraceHandle CallTraceSet::Intern(const CallTrace &trace) {
  std::size_t num_to_intern = trace.size();
  const CallFrame *parent = nullptr;
  // This is a slightly faster path where we try to find each frame, starting
  // from the root, in the interned trace set, but without performing any
  // string interning. This can have false negatives (same string different
  // pointer) but not false positives. If we have a false negative it's no
  // harm because we just proceed a little bit early to the slower path below.
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
