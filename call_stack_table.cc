// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_table.h"

#include <utility>

#include "components/metrics/leak_detector/call_stack_manager.h"

namespace leak_detector {

namespace {

using ValueType = LeakDetectorValueType;

// Get the top |kRankedListSize| entries.
const int kRankedListSize = 16;

// Initial number of hash table buckets.
const int kInitialHashTableSize = 1999;

}  // namespace

size_t CallStackTable::StoredHash::operator() (
    const CallStack* call_stack) const {
  // The call stack object should already have a hash computed when it was
  // created.
  //
  // This is NOT the actual hash computation function for a new call stack.
  return call_stack->hash;
}

CallStackTable::CallStackTable(int call_stack_suspicion_threshold)
    : num_allocs_(0),
      num_frees_(0),
      entry_map_(kInitialHashTableSize),
      leak_analyzer_(kRankedListSize, call_stack_suspicion_threshold) {
}

CallStackTable::~CallStackTable() {}

void CallStackTable::Add(const CallStack* call_stack) {
  auto iter = entry_map_.find(call_stack);
  Entry* entry = nullptr;
  if (iter == entry_map_.end()) {
    entry = &entry_map_[call_stack];
  } else {
    entry = &iter->second;
  }

  ++entry->net_num_allocs;
  ++num_allocs_;
}

void CallStackTable::Remove(const CallStack* call_stack) {
  auto iter = entry_map_.find(call_stack);
  if (iter == entry_map_.end())
    return;
  Entry* entry = &iter->second;
  --entry->net_num_allocs;
  ++num_frees_;

  // Delete zero-alloc entries to free up space.
  if (entry->net_num_allocs == 0)
    entry_map_.erase(iter);
}

size_t CallStackTable::Dump(const size_t buffer_size, char* buffer) const {
  size_t size_left = buffer_size;

  if (entry_map_.empty())
    return size_left;

  int attempted_size =
      snprintf(buffer, size_left,
               "Total number of allocations: %u\n"
                   "Total number of frees: %u\n"
                   "Net number of allocations: %u\n"
                   "Total number of distinct stack traces: %zu\n",
               num_allocs_, num_frees_, num_allocs_ - num_frees_,
               entry_map_.size());
  size_left -= attempted_size;
  buffer += attempted_size;

  if (size_left > 1) {
    int attempted_size = leak_analyzer_.Dump(size_left, buffer);
    size_left -= attempted_size;
    buffer += attempted_size;
  }

  return buffer_size - size_left;
}

void CallStackTable::TestForLeaks() {
  // Add all entries to the ranked list.
  RankedList ranked_list(kRankedListSize);

  for (const auto& entry_pair : entry_map_) {
    const Entry& entry = entry_pair.second;
    if (entry.net_num_allocs > 0) {
      LeakDetectorValueType call_stack_value(entry_pair.first);
      ranked_list.Add(call_stack_value, entry.net_num_allocs);
    }
  }
  leak_analyzer_.AddSample(std::move(ranked_list));
}

}  // namespace leak_detector
