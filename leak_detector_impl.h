// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_IMPL_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_IMPL_H_

#include <gperftools/custom_allocator.h>
#include <stdint.h>
#include <string.h>  // For memset and memcpy.

#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "components/metrics/leak_detector/call_stack_table.h"
#include "components/metrics/leak_detector/leak_analyzer.h"

namespace leak_detector {

// Vector type that's safe to use within the memory leak detector. Uses
// CustomAllocator to avoid recursive malloc hook invocation.
template <typename T>
using InternalVector = std::vector<T, STL_Allocator<T, CustomAllocator>>;

struct InternalLeakReport {
  size_t alloc_size_bytes;

  // Unlike the CallStack struct, which consists of addresses, this call stack
  // will contain offsets in the executable binary.
  InternalVector<uintptr_t> call_stack;

  // TODO(sque): Add leak detector parameters.

  bool operator< (const InternalLeakReport& other) const;
};

// Class that contains the actual leak detection mechanism.
class LeakDetectorImpl {
 public:
  LeakDetectorImpl(uintptr_t mapping_addr,
                   size_t mapping_size,
                   int size_suspicion_threshold,
                   int call_stack_suspicion_threshold,
                   bool verbose);
  ~LeakDetectorImpl();

  // Indicates whether the given allocation size has an associated call stack
  // table, and thus requires a stack unwind.
  bool ShouldGetStackTraceForSize(size_t size) const;

  // Record allocs and frees.
  void RecordAlloc(const void* ptr,
                   size_t size,
                   int stack_depth,
                   const void* const call_stack[]);
  void RecordFree(const void* ptr);

  // Run check for possible leaks based on the current profiling data.
  void TestForLeaks(bool do_logging,
                    InternalVector<InternalLeakReport>* reports);

 private:
  // A record of allocations for a particular size.
  struct AllocSizeEntry {
    // Number of allocations and frees for this size.
    uint32_t num_allocs;
    uint32_t num_frees;

    // A stack table, if this size is being profiled for stack as well.
    CallStackTable* stack_table;
  };

  // Info for a single allocation.
  struct AllocInfo {
    AllocInfo() : call_stack(nullptr) {}

    // Number of bytes in this allocation.
    size_t size;

    // Points to a unique call stack.
    const CallStack* call_stack;
  };

  // Allocator class for allocation entry map. Maps allocated addresses to
  // AllocInfo objects.
  using AllocationEntryAllocator =
      STL_Allocator<std::pair<const void*, AllocInfo>, CustomAllocator>;

  // Allocator class for unique call stacks.
  using TableEntryAllocator = STL_Allocator<const CallStack*, CustomAllocator>;

  // Hash class for addresses.
  struct AddressHash {
    size_t operator() (uintptr_t addr) const;
  };

  // Comparator class for call stack objects.
  struct CallStackCompare {
    bool operator() (const CallStack* c1, const CallStack* c2) const {
      return c1->depth == c2->depth &&
             std::equal(c1->stack, c1->stack + c1->depth, c2->stack);
    }
  };

  // Returns a CallStack object for a given call stack. Each unique call stack
  // has its own CallStack object. If the given call stack has already been
  // created by a previous call to this function, return a pointer to that same
  // call stack object.
  CallStack* GetCallStack(int depth, const void* const stack[]);

  // Returns the offset of |ptr| within the current binary. If it is not in the
  // current binary, just return |ptr| as an integer.
  uintptr_t GetOffset(const void *ptr) const;

  // Dump current profiling statistics to log.
  void DumpStats() const;

  // Owns all unique call stack objects, which are allocated on the heap. Any
  // other class or function that references a call stack must get it from here,
  // but may not take ownership of the call stack object.
  std::unordered_set<CallStack*,
                     CallStack::ComputeHash,
                     CallStackCompare,
                     TableEntryAllocator> call_stacks_;

  // Allocation stats.
  uint64_t num_allocs_;
  uint64_t num_frees_;
  uint64_t alloc_size_;
  uint64_t free_size_;

  uint32_t num_allocs_with_call_stack_;
  uint32_t num_stack_tables_;

  // Stores all individual recorded allocations.
  std::unordered_map<uintptr_t,
                     AllocInfo,
                     AddressHash,
                     std::equal_to<uintptr_t>,
                     AllocationEntryAllocator> address_map_;

  // Used to analyze potential leak patterns in the allocation sizes.
  LeakAnalyzer size_leak_analyzer_;

  // Allocation stats for each size.
  InternalVector<AllocSizeEntry> size_entries_;

  // Address mapping info of the current binary.
  uintptr_t mapping_addr_;
  size_t mapping_size_;

  // Number of consecutive times an allocation size must trigger suspicion to be
  // considered a leak suspect.
  int size_suspicion_threshold_;

  // Number of consecutive times a call stack must trigger suspicion to be
  // considered a leak suspect.
  int call_stack_suspicion_threshold_;

  // Enable verbose dumping of much more leak analysis data.
  bool verbose_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorImpl);
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_IMPL_H_
