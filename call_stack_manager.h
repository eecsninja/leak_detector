// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_

#include <gperftools/custom_allocator.h>
#include <stdint.h>

#include <unordered_set>

#include "base/macros.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace leak_detector {

// Struct to represent a call stack.
struct CallStack {
  uint32_t depth;                        // Depth of current call stack.
  const void** stack;                    // Call stack as an array of addrs.

  size_t hash;                           // Hash of call stack.
};

// Maintains and owns all unique call stack objects.
class CallStackManager {
 public:
  CallStackManager();
  ~CallStackManager();

  // Returns a CallStack object for a given call stack. Each unique call stack
  // has its own CallStack object. If the given call stack has already been
  // created by a previous call to this function, return a pointer to that same
  // call stack object.
  //
  // Returns the call stacks as const pointers because no caller should take
  // ownership of them and modify or delete them.
  const CallStack* GetCallStack(int depth, const void* const stack[]);

  size_t size() const {
    return call_stacks_.size();
  }

 private:
  // Allocator class for unique call stacks.
  using CallStackPointerAllocator = STL_Allocator<CallStack*, CustomAllocator>;

  // Used to compute hashes from call stack objects. Takes a pointer to a call
  // stack object as an argument.
  struct CallStackPointerHash {
    size_t operator() (const CallStack* call_stack) const;
  };

  // Equality comparator for call stack objects. Takes pointers to call stack
  // objects as arguments.
  struct CallStackPointerEqual {
    bool operator() (const CallStack* c1, const CallStack* c2) const;
  };

  // Holds all call stack objects.
  std::unordered_set<CallStack*,
                     CallStackPointerHash,
                     CallStackPointerEqual,
                     CallStackPointerAllocator> call_stacks_;

  DISALLOW_COPY_AND_ASSIGN(CallStackManager);
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_
