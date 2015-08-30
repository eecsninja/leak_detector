#include "hooks.h"

#include <string.h>

#include <algorithm>

namespace MallocHook {

namespace {

NewHookType new_hook_ = NULL;
DeleteHookType delete_hook_ = NULL;
void* stack_trace_[32];
int depth_;

}  // namespace

NewHookType SetNewHook(NewHookType hook) {
  NewHookType old_hook = new_hook_;
  new_hook_ = hook;
  return old_hook;
}

DeleteHookType SetDeleteHook(DeleteHookType hook) {
  DeleteHookType old_hook = delete_hook_;
  delete_hook_ = hook;
  return old_hook;
}

void InvokeNewHook(const void* ptr, size_t size) {
  if (new_hook_)
    new_hook_(ptr, size);
}

void InvokeDeleteHook(const void* ptr) {
  if (delete_hook_)
    delete_hook_(ptr);
}

void SetCallerStackTrace(int depth, void* const stack[]) {
  memcpy(stack_trace_, stack, sizeof(*stack) * depth);
  depth_ = depth;
}

int GetCallerStackTrace(void* stack[], int depth, int /* skip */) {
  int actual_depth = std::min(depth, depth_);
  memcpy(stack, stack_trace_, sizeof(*stack) * actual_depth);
  return actual_depth;
}

}  // namespace MallocHook
