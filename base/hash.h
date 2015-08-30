// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_H_
#define BASE_HASH_H_

#include <stddef.h>
#include <stdint.h>

namespace base {

// Perform a full hash.
uint32_t Hash(const void*, size_t);

// Perform a hash in stages.
uint32_t HashStep(uint32_t, const void*, size_t);
uint32_t HashFinish(uint32_t);

}  // namespace base

#endif  // BASE_HASH_H_
