// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gperftools/custom_allocator.h>

#include "base/low_level_alloc.h"

namespace {

LowLevelAlloc::Arena* g_arena = nullptr;

bool g_is_initalized_for_unit_test = false;

}  // namespace

// static
void CustomAllocator::Initialize() {
  g_arena = LowLevelAlloc::NewArena(0, LowLevelAlloc::DefaultArena());
}

// static
bool CustomAllocator::Shutdown() {
  if (!g_is_initalized_for_unit_test)
    return LowLevelAlloc::DeleteArena(g_arena);
  g_is_initalized_for_unit_test = false;
  return true;
}

// static
bool CustomAllocator::IsInitialized() {
  return g_arena || g_is_initalized_for_unit_test;
}

// static
void CustomAllocator::InitializeForUnitTest() {
  g_is_initalized_for_unit_test = true;
}

// static
void* CustomAllocator::Allocate(size_t size) {
  if (g_is_initalized_for_unit_test)
    return new char[size];

  if (!g_arena)
    return nullptr;

  return LowLevelAlloc::AllocWithArena(size, g_arena);
}

// static
void CustomAllocator::Free(void* ptr, size_t /* size */) {
  if (g_is_initalized_for_unit_test)
    delete [] reinterpret_cast<char*>(ptr);
  else
    LowLevelAlloc::Free(ptr);
}
