// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_STL_ALLOCATOR_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_STL_ALLOCATOR_H_

#include <stddef.h>

#include <limits>

#include "base/logging.h"

// Generic allocator class for STL objects
// that uses a given type-less allocator Alloc, which must provide:
//   static void* Alloc::Allocate(size_t size);
//   static void Alloc::Free(void* ptr, size_t size);
//
// STL_Allocator<T, MyAlloc> provides the same thread-safety
// guarantees as MyAlloc.
//
// Usage example:
//   set<T, less<T>, STL_Allocator<T, MyAlloc> > my_set;
// CAVEAT: Parts of the code below are probably specific
//         to the STL version(s) we are using.
//         The code is simply lifted from what std::allocator<> provides.
template <typename T, class Alloc>
class STL_Allocator : public std::allocator<T> { 
 public:
  typedef size_t     size_type;
  typedef ptrdiff_t  difference_type;
  typedef T*         pointer;
  typedef const T*   const_pointer;
  typedef T&         reference;
  typedef const T&   const_reference;
  typedef T          value_type;

  template <class T1> struct rebind {
    typedef STL_Allocator<T1, Alloc> other;
  };

  STL_Allocator() {}
  explicit STL_Allocator(const STL_Allocator&) {}
  template <class T1> STL_Allocator(const STL_Allocator<T1, Alloc>&) {}
  ~STL_Allocator() {}

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  pointer allocate(size_type n, const void* = 0) {
    // Make sure the computation of the total allocation size does not cause an
    // integer overflow.
    RAW_CHECK(n < max_size());
    return static_cast<T*>(Alloc::Allocate(n * sizeof(T)));
  }
  void deallocate(pointer p, size_type n) { Alloc::Free(p, n * sizeof(T)); }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }

  void construct(pointer p, const T& val) { ::new(p) T(val); }
  void construct(pointer p) { ::new(p) T(); }
  void destroy(pointer p) { p->~T(); }

  // There's no state, so these allocators always return the same value.
  bool operator==(const STL_Allocator&) const { return true; }
  bool operator!=(const STL_Allocator&) const { return false; }
};

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_STL_ALLOCATOR_H_
