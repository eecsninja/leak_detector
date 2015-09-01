// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_manager.h"

#include <gperftools/custom_allocator.h>
#include <stdint.h>

#include <new>

#include "base/hash.h"
#include "base/logging.h"

namespace leak_detector {

CallStackManager::CallStackManager() : num_call_stacks_(0) {}

CallStackManager::~CallStackManager() {
  FreeNode(&call_stack_tree_root_node_);
}

CallStack* CallStackManager::GetCallStack(
    int depth, const void* const stack[]) {
  CallStackNode* node = &call_stack_tree_root_node_;

  // Iterate through the call stack tree.
  for (int i = depth - 1; i >= 0; --i) {
    const void* ptr = stack[i];

    auto node_iter = node->children.find(ptr);
    if (node_iter != node->children.end()) {
      // Found existing node, proceed.
      node = node_iter->second;
      continue;
    }

    // Create and insert new node.
    CallStackNode* new_node =
        new(CustomAllocator::Allocate(sizeof(CallStackNode))) CallStackNode();
    uintptr_t ptr_value = reinterpret_cast<uintptr_t>(ptr);
    new_node->hash = base::HashStep(node->hash, &ptr_value, sizeof(ptr_value));
    new_node->call_stack = nullptr;
    node->children[ptr] = new_node;
    //node->children.insert(new_node);

    // Continue on...
    node = new_node;
  }

  // |node| now points to the node in the call stack tree corresponding to the
  // end of the current call stack.

  if (node->call_stack)
    return node->call_stack;

  // Create a new call stack object if there isn't already one.
  CallStack* call_stack =
      new(CustomAllocator::Allocate(sizeof(CallStack))) CallStack;
  ++num_call_stacks_;

  node->call_stack = call_stack;

  call_stack->depth = depth;
  call_stack->hash = base::HashFinish(node->hash);
  call_stack->stack = reinterpret_cast<const void**>(
      CustomAllocator::Allocate(sizeof(*stack) * depth));
  std::copy(stack, stack + depth, call_stack->stack);

  return call_stack;
}

// static
void CallStackManager::FreeNode(CallStackManager::CallStackNode* node) {
  if (node->call_stack) {
    CallStack* call_stack = node->call_stack;
    CustomAllocator::Free(call_stack->stack,
                          sizeof(*call_stack->stack) * call_stack->depth);
    CustomAllocator::Free(call_stack, sizeof(*call_stack));
    node->call_stack = nullptr;
  }

  for (auto child_pair : node->children) {
    CallStackNode* child_node = child_pair.second;
    FreeNode(child_node);
    CustomAllocator::Free(child_node, sizeof(*child_node));
  }
  node->children.clear();
}

}  // namespace leak_detector
