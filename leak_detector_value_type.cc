// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector_value_type.h"

#include <stdio.h>

namespace leak_detector {

const char* LeakDetectorValueType::GetTypeName() const {
  switch (type_) {
  case kSize:
    return "size";
  case kCallStack:
    return "call stack";
  default:
    return "(none)";
  }
}

const char* LeakDetectorValueType::ToString(size_t buffer_size,
                                            char* buffer) const {
  switch (type_) {
  case kSize:
    snprintf(buffer, buffer_size, "%u", size_);
    break;
  case kCallStack:
    snprintf(buffer, buffer_size, "%p", call_stack_);
    break;
  default:
    snprintf(buffer, buffer_size, "(none)");
    break;
  }

  return buffer;
}

bool LeakDetectorValueType::operator== (
    const LeakDetectorValueType& other) const {
  if (type_ != other.type_)
    return false;

  switch(type_) {
  case kSize:
    return size_ == other.size_;
  case kCallStack:
    return call_stack_ == other.call_stack_;
  default:
    return false;
  }
}

bool LeakDetectorValueType::operator< (
    const LeakDetectorValueType& other) const {
  if (type_ != other.type_)
    return type_ < other.type_;

  switch(type_) {
  case kSize:
    return size_ < other.size_;
  case kCallStack:
    return call_stack_ < other.call_stack_;
  default:
    return false;
  }
}

}  // leak_detector
