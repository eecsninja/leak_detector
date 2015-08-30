// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_ANALYZER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_ANALYZER_H_

#include <gperftools/custom_allocator.h>

#include <map>
#include <vector>

#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "components/metrics/leak_detector/ranked_list.h"

// This class looks for possible leak patterns in allocation data over time.

namespace leak_detector {

class LeakAnalyzer {
 public:
  using ValueType = LeakDetectorValueType;

  template <typename Type>
  using Allocator = STL_Allocator<Type, CustomAllocator>;

  LeakAnalyzer(uint32_t ranking_size, uint32_t num_suspicions_threshold)
      : ranking_size_(ranking_size),
        score_threshold_(num_suspicions_threshold),
        ranked_entries_(ranking_size),
        prev_ranked_entries_(ranking_size) {
    suspected_leaks_.reserve(ranking_size);
  }

  ~LeakAnalyzer() {}

  // Take in a RankedList of allocations, sorted by count. Removes the contents
  // of |ranked_list|.
  void AddSample(RankedList&& ranked_list);

  // Used to report suspected leaks. Reported leaks are sorted by ValueType.
  const std::vector<ValueType, Allocator<ValueType>>& suspected_leaks() const {
    return suspected_leaks_;
  }

  // Log the leak detector's top sizes and suspected sizes. Writes output to log
  // buffer |buffer| of size |size|. Returns the number of bytes remaining in
  // the buffer after writing to it. The number of bytes remaining includes the
  // zero terminator that was just written, so this will always return at least
  // 1, unless |size| == 0.
  size_t Dump(const size_t buffer_size, char* buffer) const;

 private:
  // Analyze a list of allocation count deltas from the previous iteration. If
  // anything looks like a possible leak, update the suspicion scores.
  void AnalyzeDeltas(const RankedList& ranked_deltas);

  // Returns the count for the given value from the previous analysis in
  // |count|. Returns true if the given value was present in the previous
  // analysis, or false if not.
  bool GetPreviousCountForValue(const ValueType& value, uint32_t* count) const;

  // Look for the top |ranking_size_| entries when analyzing leaks.
  const uint32_t ranking_size_;

  // Report suspected leaks when the suspicion score reaches this value.
  const uint32_t score_threshold_;

  // A mapping of allocation values to suspicion score. All allocations in this
  // container are suspected leaks. The score can increase or decrease over
  // time. Once the score  reaches |score_threshold_|, the entry is reported as
  // a suspected leak in |suspected_leaks_|.
  std::map<ValueType,
           uint32_t,
           std::less<ValueType>,
           Allocator<std::pair<ValueType, uint32_t>>> suspected_histogram_;

  // Array of allocated values that passed the suspicion threshold and are being
  // reported.
  std::vector<ValueType, Allocator<ValueType>> suspected_leaks_;

  // The most recent allocation entries, since the last call to AddSample().
  RankedList ranked_entries_;
  // The previous allocation entries, from before the last call to AddSample().
  RankedList prev_ranked_entries_;

  DISALLOW_COPY_AND_ASSIGN(LeakAnalyzer);
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_ANALYZER_H_
