// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_TABLE_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_TABLE_H_

#include <gperftools/custom_allocator.h>
#include <stdint.h>

#include <functional>
#include <unordered_map>

#include "components/metrics/leak_detector/leak_analyzer.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace leak_detector {

// Struct to represent a call stack.
struct CallStack {
  uint32_t depth;                        // Depth of current call stack.
  const void** stack;                    // Call stack as an array of addrs.

  size_t hash;                           // Hash of call stack.

  // Generate hash from call stack.
  struct ComputeHash {
    size_t operator() (const CallStack* call_stack) const;
  };
};

// Contains a hash table where the key is the call stack and the value is the
// number of allocations from that call stack.
class CallStackTable {
 public:
  struct StoredHash {
    size_t operator() (const CallStack* call_stack) const {
      // The call stack object should already have a hash computed when it was
      // created.
      //
      // This is NOT the actual hash computation function for a new call stack.
      return call_stack->hash;
    }
  };

  explicit CallStackTable(int call_stack_suspicion_threshold);
  ~CallStackTable();

  // Add/Remove an allocation for the given call stack.
  // Note that this class does NOT own the CallStack objects. Instead, it
  // identifies different CallStacks by their hashes.
  void Add(const CallStack* call_stack);
  void Remove(const CallStack* call_stack);

  // Dump contents to log buffer |buffer| of size |size|. Returns the number of
  // bytes remaining in the buffer after writing to it. The number of bytes
  // remaining includes the zero terminator that was just written, so this will
  // always return at least 1, unless |size| == 0.
  size_t Dump(const size_t buffer_size, char* buffer) const;

  // Check for leak patterns in the allocation data.
  void TestForLeaks();

  const LeakAnalyzer& leak_analyzer() const {
    return leak_analyzer_;
  }

  size_t size() const {
    return entry_map_.size();
  }
  bool empty() const {
    return entry_map_.empty();
  }

  uint32_t num_allocs() const {
    return num_allocs_;
  }
  uint32_t num_frees() const {
    return num_frees_;
  }

 private:
  // Hash table entry used to track number of allocs and frees for a call stack.
  struct Entry {
    // Number of allocs minus number of frees for a given call stack.
    uint32_t net_num_allocs;
  };

  // Total number of allocs and frees in this table.
  uint32_t num_allocs_;
  uint32_t num_frees_;

  // Hash table containing entries.
  using TableEntryAllocator =
      STL_Allocator<std::pair<const CallStack*, Entry>, CustomAllocator>;
  std::unordered_map<const CallStack*,
                     Entry,
                     StoredHash,
                     std::equal_to<const CallStack*>,
                     TableEntryAllocator> entry_map_;

  // For detecting leak patterns in incoming allocations.
  LeakAnalyzer leak_analyzer_;

  DISALLOW_COPY_AND_ASSIGN(CallStackTable);
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_TABLE_H_
