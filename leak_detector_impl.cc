// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leak_detector_impl.h"

#include <inttypes.h>
#include <stddef.h>
#include <unistd.h>

#include <algorithm>
#include <new>
#include <utility>

#include "base/hash.h"
#include "components/metrics/leak_detector/call_stack_table.h"
#include "components/metrics/leak_detector/ranked_list.h"

namespace leak_detector {

namespace {

// Look for leaks in the the top N entries in each tier, where N is this value.
const int kRankedListSize = 16;

// Initial hash table size for |LeakDetectorImpl::address_map_|.
const int kAddressMapNumBuckets = 100003;

// Number of entries in the alloc size table. As sizes are aligned to 32-bits
// the max supported allocation size is (kNumSizeEntries * 4 - 1). Any larger
// sizes are ignored. This value is chosen high enough that such large sizes
// are rare if not nonexistent.
const int kNumSizeEntries = 2048;

using ValueType = LeakDetectorValueType;

// Print the contents of |str| prefixed with the current pid.
void PrintWithPid(const char* str) {
  char line[1024];
  snprintf(line, sizeof(line), "%d: %s\n", getpid(), str);
  RAW_LOG(ERROR, line);
}

// Prints the input string buffer using RAW_LOG, pre-fixing each line with the
// process id. Will modify |str| temporarily but restore it at the end.
void PrintWithPidOnEachLine(char* str) {
  char* current_line = str;
  // Attempt to find a newline that will indicate the end of the first line
  // and the start of the second line.
  while (char *newline_ptr = strchr(current_line, '\n')) {
    // Terminate the current line so it can be printed as a separate string.
    // Restore the original string when done.
    *newline_ptr = '\0';
    PrintWithPid(current_line);
    *newline_ptr = '\n';

    // Point |current_line| to the next line.
    current_line = newline_ptr + 1;
  }
  // There may be an extra line at the end of the input string that is not
  // newline-terminated. e.g. if the input was only one line, or the last line
  // did not end with a newline.
  if (current_line[0] != '\0')
    PrintWithPid(current_line);
}

// Functions to convert an allocation size to/from the array index used for
// |LeakDetectorImpl::size_entries_|.
int SizeToIndex(const size_t size) {
  int result = static_cast<int>(size / sizeof(uint32_t));
  if (result < kNumSizeEntries)
    return result;
  return 0;
}

size_t IndexToSize(int index){
  return sizeof(uint32_t) * index;
}

}  // namespace

bool InternalLeakReport::operator< (const InternalLeakReport& other) const {
  if (alloc_size_bytes != other.alloc_size_bytes)
    return alloc_size_bytes < other.alloc_size_bytes;
  for (size_t i = 0;
       i < call_stack.size() && i < other.call_stack.size();
       ++i) {
    if (call_stack[i] != other.call_stack[i])
      return call_stack[i] < other.call_stack[i];
  }
  return call_stack.size() < other.call_stack.size();
}

LeakDetectorImpl::LeakDetectorImpl(uintptr_t mapping_addr,
                                   size_t mapping_size,
                                   int size_suspicion_threshold,
                                   int call_stack_suspicion_threshold,
                                   bool verbose)
    : num_stack_tables_(0),
      address_map_(kAddressMapNumBuckets),
      size_leak_analyzer_(kRankedListSize, size_suspicion_threshold),
      size_entries_(kNumSizeEntries, {0}),
      mapping_addr_(mapping_addr),
      mapping_size_(mapping_size),
      call_stack_suspicion_threshold_(call_stack_suspicion_threshold),
      verbose_(verbose) {
}

LeakDetectorImpl::~LeakDetectorImpl() {
  for (CallStack* call_stack : call_stacks_)
    CustomAllocator::Free(call_stack, sizeof(CallStack));
  call_stacks_.clear();

  // Free any call stack tables.
  for (AllocSizeEntry& entry : size_entries_) {
    CallStackTable* table = entry.stack_table;
    if (!table)
      continue;
    table->~CallStackTable();
    CustomAllocator::Free(table, sizeof(CallStackTable));
  }
  size_entries_.clear();
}

bool LeakDetectorImpl::ShouldGetStackTraceForSize(size_t size) const {
  return size_entries_[SizeToIndex(size)].stack_table != nullptr;
}

void LeakDetectorImpl::RecordAlloc(
    const void* ptr, size_t size,
    int stack_depth, const void* const stack[]) {
  AllocInfo alloc_info;
  alloc_info.size = size;

  alloc_size_ += alloc_info.size;
  ++num_allocs_;

  AllocSizeEntry* entry = &size_entries_[SizeToIndex(size)];
  ++entry->num_allocs;

  if (entry->stack_table && stack_depth > 0) {
    alloc_info.call_stack = GetCallStack(stack_depth, stack);
    entry->stack_table->Add(alloc_info.call_stack);

    ++num_allocs_with_call_stack_;
  }

  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  address_map_.insert(std::pair<uintptr_t, AllocInfo>(addr, alloc_info));
}

void LeakDetectorImpl::RecordFree(const void* ptr) {
  // Look up address.
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  auto iter = address_map_.find(addr);
  if (iter == address_map_.end())
    return;

  const AllocInfo& alloc_info = iter->second;

  AllocSizeEntry* entry = &size_entries_[SizeToIndex(alloc_info.size)];
  ++entry->num_frees;

  const CallStack* call_stack = alloc_info.call_stack;
  if (call_stack) {
    if (entry->stack_table)
      entry->stack_table->Remove(call_stack);
  }
  ++num_frees_;
  free_size_ += alloc_info.size;

  address_map_.erase(iter);
}

void LeakDetectorImpl::TestForLeaks(
    bool do_logging,
    InternalVector<InternalLeakReport>* reports) {
  if (do_logging)
    DumpStats();

  // Add net alloc counts for each size to a ranked list.
  RankedList size_ranked_list(kRankedListSize);
  for (size_t i = 0; i < size_entries_.size(); ++i) {
    const AllocSizeEntry& entry = size_entries_[i];
    ValueType size_value(IndexToSize(i));
    size_ranked_list.Add(size_value, entry.num_allocs - entry.num_frees);
  }
  size_leak_analyzer_.AddSample(std::move(size_ranked_list));

  // Dump out the top entries.
  char buf[0x4000];
  if (do_logging && verbose_) {
    if (size_leak_analyzer_.Dump(sizeof(buf), buf) < sizeof(buf))
      PrintWithPidOnEachLine(buf);
  }

  // Get suspected leaks by size.
  for (const ValueType& size_value : size_leak_analyzer_.suspected_leaks()) {
    uint32_t size = size_value.size();
    AllocSizeEntry* entry = &size_entries_[SizeToIndex(size)];
    if (entry->stack_table)
      continue;
    if (do_logging) {
      snprintf(buf, sizeof(buf), "Adding stack table for size %u\n", size);
      PrintWithPidOnEachLine(buf);
    }
    entry->stack_table = new(CustomAllocator::Allocate(sizeof(CallStackTable)))
        CallStackTable(call_stack_suspicion_threshold_);
    ++num_stack_tables_;
  }

  // Check for leaks in each CallStackTable. It makes sense to this before
  // checking the size allocations, because that could potentially create new
  // CallStackTable. However, the overhead to check a new CallStackTable is
  // small since this function is run very rarely. So handle the leak checks of
  // Tier 2 here.
  reports->clear();
  for (size_t i = 0; i < size_entries_.size(); ++i) {
    const AllocSizeEntry& entry = size_entries_[i];
    CallStackTable* stack_table = entry.stack_table;
    if (!stack_table || stack_table->empty())
      continue;

    size_t size = IndexToSize(i);
    if (do_logging && verbose_) {
      // Dump table info.
      snprintf(buf, sizeof(buf), "Stack table for size %zu:\n", size);
      PrintWithPidOnEachLine(buf);

      if (stack_table->Dump(sizeof(buf), buf) < sizeof(buf))
        PrintWithPidOnEachLine(buf);
    }

    // Get suspected leaks by call stack.
    stack_table->TestForLeaks();
    const LeakAnalyzer& leak_analyzer = stack_table->leak_analyzer();
    for (const ValueType& call_stack_value : leak_analyzer.suspected_leaks()) {
      const CallStack* call_stack = call_stack_value.call_stack();

      // Return reports by storing in |*reports|.
      reports->resize(reports->size() + 1);
      InternalLeakReport* report = &reports->back();
      report->alloc_size_bytes = size;
      report->call_stack.resize(call_stack->depth);
      for (size_t j = 0; j < call_stack->depth; ++j) {
        report->call_stack[j] = GetOffset(call_stack->stack[j]);
      }

      if (do_logging) {
        int offset = snprintf(buf, sizeof(buf),
                              "Suspected call stack for size %zu, %p:\n",
                              size, call_stack);
        for (size_t j = 0; j < call_stack->depth; ++j) {
          offset += snprintf(buf + offset, sizeof(buf) - offset,
                             "\t%" PRIxPTR "\n",
                             GetOffset(call_stack->stack[j]));
        }
        PrintWithPidOnEachLine(buf);
      }
    }
  }
}

size_t LeakDetectorImpl::AddressHash::operator() (uintptr_t addr) const {
  return base::Hash(reinterpret_cast<const char*>(&addr), sizeof(addr));
}

CallStack* LeakDetectorImpl::GetCallStack(
    int depth, const void* const stack[]) {
  // Temporarily create a call stack object for lookup in |call_stacks_|.
  CallStack temp;
  temp.depth = depth;
  temp.stack = const_cast<const void**>(stack);

  auto iter = call_stacks_.find(&temp);
  if (iter != call_stacks_.end())
    return *iter;

  // Since |call_stacks_| stores CallStack pointers rather than actual objects,
  // create new call objects manually here.
  CallStack* new_call_stack =
      new(CustomAllocator::Allocate(sizeof(CallStack))) CallStack;
  memset(new_call_stack, 0, sizeof(*new_call_stack));
  new_call_stack->depth = depth;
  new_call_stack->hash = call_stacks_.hash_function()(&temp);
  new_call_stack->stack =
      reinterpret_cast<const void**>(
          CustomAllocator::Allocate(sizeof(*stack) * depth));
  std::copy(stack, stack + depth, new_call_stack->stack);

  call_stacks_.insert(new_call_stack);
  return new_call_stack;
}

uintptr_t LeakDetectorImpl::GetOffset(const void *ptr) const {
  uintptr_t ptr_value = reinterpret_cast<uintptr_t>(ptr);
  if (ptr_value >= mapping_addr_ && ptr_value < mapping_addr_ + mapping_size_)
    return ptr_value - mapping_addr_;
  return ptr_value;
}

void LeakDetectorImpl::DumpStats() const {
  char buf[1024];
  snprintf(buf, sizeof(buf),
           "Alloc size: %" PRIu64"\n"
           "Free size: %" PRIu64 "\n"
           "Net alloc size: %" PRIu64 "\n"
           "Number of stack tables: %u\n"
           "Percentage of allocs with stack traces: %.2f%%\n"
           "Number of call stack buckets: %zu\n",
           alloc_size_, free_size_, alloc_size_ - free_size_, num_stack_tables_,
           num_allocs_ ? 100.0f * num_allocs_with_call_stack_ / num_allocs_ : 0,
           call_stacks_.bucket_count());
  PrintWithPidOnEachLine(buf);
}

}  // namespace leak_detector
