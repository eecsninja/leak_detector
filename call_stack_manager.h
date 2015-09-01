// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_

#include <gperftools/custom_allocator.h>
#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace leak_detector {

// Struct to represent a call stack.
struct CallStack {
  uint32_t depth;                        // Depth of current call stack.
  const void** stack;                    // Call stack as an array of addrs.

  size_t hash;                           // Hash of call stack.
};

// Contains a hash table where the key is the call stack and the value is the
// number of allocations from that call stack.
class CallStackManager {
 public:
  CallStackManager();
  ~CallStackManager();

  // Returns a CallStack object for a given call stack. Each unique call stack
  // has its own CallStack object. If the given call stack has already been
  // created by a previous call to this function, return a pointer to that same
  // call stack object.
  CallStack* GetCallStack(int depth, const void* const stack[]);

  size_t size() const {
    return num_call_stacks_;
  }

 private:
  struct CallStackNode {
    uint32_t hash;
    CallStack* call_stack;
    // TODO(sque): Consider unordered_map.
    std::map<const void*,
             CallStackNode*,
             std::less<const void*>,
             STL_Allocator<std::pair<const void*, CallStackNode*>,
                           CustomAllocator>> children;

    CallStackNode() : hash(0), call_stack(nullptr) {}
  };

  // Recursively frees all dynamically allocated memory within |node|. Does not
  // free |node| itself.
  static void FreeNode(CallStackManager::CallStackNode* node);

  CallStackNode call_stack_tree_root_node_;

  size_t num_call_stacks_;

  DISALLOW_COPY_AND_ASSIGN(CallStackManager);
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_
