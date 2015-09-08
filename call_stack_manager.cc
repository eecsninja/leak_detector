// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_manager.h"

#include <gperftools/custom_allocator.h>
#include <string.h>   // For memset.

#include <algorithm>  // For std::copy.
#include <new>

#include "base/hash.h"

namespace leak_detector {

CallStackManager::CallStackManager() {}

CallStackManager::~CallStackManager() {
  for (CallStack* call_stack : call_stacks_) {
    CustomAllocator::Free(call_stack->stack,
                          call_stack->depth * sizeof(*call_stack->stack));
    CustomAllocator::Free(call_stack, sizeof(CallStack));
  }
  call_stacks_.clear();
}

const CallStack* CallStackManager::GetCallStack(
    int depth, const void* const stack[]) {
  // Temporarily create a call stack object for lookup in |call_stacks_|.
  CallStack temp;
  temp.depth = depth;
  temp.stack = const_cast<const void**>(stack);
  // This is the only place where the call stack's hash is computed. This value
  // can be reused in the created object to avoid further hash computation.
  temp.hash =
      base::Hash(reinterpret_cast<const char*>(stack), sizeof(*stack) * depth);

  auto iter = call_stacks_.find(&temp);
  if (iter != call_stacks_.end())
    return *iter;

  // Since |call_stacks_| stores CallStack pointers rather than actual objects,
  // create new call objects manually here.
  CallStack* call_stack =
      new(CustomAllocator::Allocate(sizeof(CallStack))) CallStack;
  memset(call_stack, 0, sizeof(*call_stack));
  call_stack->depth = depth;
  call_stack->hash = temp.hash;  // Don't run the hash function again.
  call_stack->stack =
      reinterpret_cast<const void**>(
          CustomAllocator::Allocate(sizeof(*stack) * depth));
  std::copy(stack, stack + depth, call_stack->stack);

  call_stacks_.insert(call_stack);
  return call_stack;
}

bool CallStackManager::CallStackPointerEqual::operator() (
    const CallStack* c1, const CallStack* c2) const {
  return c1->depth == c2->depth &&
         std::equal(c1->stack, c1->stack + c1->depth, c2->stack);
}

}  // namespace leak_detector
