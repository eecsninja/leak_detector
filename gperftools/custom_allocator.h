// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CUSTOM_ALLOCATOR_H_
#define CUSTOM_ALLOCATOR_H_

#include <stddef.h>

// PERFTOOLS_DLL_DECL is unnecessary, as it is Windows specific.

// A container class for using LowLevelAlloc with STL_Allocator. The functions
// Allocate() and Free() must match the specificiations in STL_Allocator.
class CustomAllocator {
 public:
  // This is a stateless class, but there is static data within the module that
  // needs to be created and deleted.
  static void Initialize();
  static bool Shutdown();
  static bool IsInitialized();

  // Special initialization for unit testing. Uses new/delete for allocations.
  static void InitializeForUnitTest();

  static void* Allocate(size_t size);
  static void Free(void* ptr, size_t size);
};

#endif  // CUSTOM_ALLOCATOR_H_
