// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_VALUE_TYPE_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_VALUE_TYPE_

#include <stddef.h>
#include <stdint.h>

namespace leak_detector {

// Used for tracking unique call stacks.
class CallStack;

class LeakDetectorValueType {
 public:
  // Supported types.
  enum Type {
    kNone,
    kSize,
    kCallStack,
  };

  LeakDetectorValueType()
      : type_(kNone),
        size_(0),
        call_stack_(nullptr) {}
  explicit LeakDetectorValueType(uint32_t size)
      : type_(kSize),
        size_(size),
        call_stack_(nullptr) {}
  explicit LeakDetectorValueType(const CallStack* call_stack)
      : type_(kCallStack),
        size_(0),
        call_stack_(call_stack) {}

  // Accessors.
  Type type() const {
    return type_;
  }
  uint32_t size() const {
    return size_;
  }
  const CallStack* call_stack() const {
    return call_stack_;
  }

  // Returns a string containing the word that describes the value type of the
  // current object. e.g. "size" or "call stack".
  const char* GetTypeName() const;

  // Writes the current value as a string to |buffer|. Will not write beyond the
  // buffer size given by |buffer_size|. Returns |buffer| as a const ptr.
  const char* ToString(size_t buffer_size, char* buffer) const;

  // Comparators.
  bool operator== (const LeakDetectorValueType& other) const;
  bool operator< (const LeakDetectorValueType& other) const;

 private:
  Type type_;

  uint32_t size_;
  const CallStack* call_stack_;
};

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_VALUE_TYPE_
