// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_LIST_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_LIST_H_

#include <gperftools/custom_allocator.h>
#include <stdint.h>

#include <list>

#include "base/macros.h"
#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "components/metrics/leak_detector/stl_allocator.h"

// RankedList lets you add entries and automatically sorts them internally, so
// they can be accessed in sorted order. The entries are stored as a vector
// array.

namespace leak_detector {

class RankedList {
 public:
  using ValueType = LeakDetectorValueType;

  // A single entry in the RankedList. The RankedList sorts entries by |count|
  // in descending order.
  struct Entry {
    ValueType value;
    int count;

    // Create a < comparator for reverse sorting.
    bool operator< (Entry& entry) const {
      return count > entry.count;
    }
  };

  using EntryList = std::list<Entry, STL_Allocator<Entry, CustomAllocator>>;
  using const_iterator = EntryList::const_iterator;

  explicit RankedList(size_t max_size) : max_size_(max_size) {}
  RankedList& operator= (RankedList&& other);  // Support std::move().
  ~RankedList() {}

  // Accessors for begin() and end() const iterators.
  const_iterator begin() const {
    return entries_.begin();
  }
  const_iterator end() const {
    return entries_.end();
  }

  size_t size() const {
    return entries_.size();
  }
  size_t max_size() const {
    return max_size_;
  }

  // Add a new value-count pair to the list. Does not check for existing entries
  // with the same value.
  void Add(const ValueType& value, int count);

 private:
  // Max and min counts. Returns 0 if the list is empty.
  const int max_count() const {
    return entries_.empty() ? 0 : entries_.begin()->count;
  }
  const int min_count() const {
    return entries_.empty() ? 0 : entries_.rbegin()->count;
  }

  // Max number of items that can be stored in the list.
  size_t max_size_;

  // Points to the array of entries.
  std::list<Entry, STL_Allocator<Entry, CustomAllocator>> entries_;

  DISALLOW_COPY_AND_ASSIGN(RankedList);
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_LIST_H_
