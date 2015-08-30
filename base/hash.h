// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_H_
#define BASE_HASH_H_

#include <stddef.h>
#include <stdint.h>

namespace base {

uint32_t SuperFastHash(const char*, size_t);

inline uint32_t Hash(const char* data, size_t len) {
  return SuperFastHash(data, len);
}

}  // namespace base

#endif  // BASE_HASH_H_
