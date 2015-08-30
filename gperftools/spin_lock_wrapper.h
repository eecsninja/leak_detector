// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SPINLOCK_WRAPPER_H_
#define SPINLOCK_WRAPPER_H_

class SpinLock;

// A container class for using tcmalloc's SpinLock class.
// This allows users of this class to avoid including base/spinlock.h, which
// causes #include path conflicts.

// SpinLockWrapper depends on CustomAllocator, which must be initialized before
// any instance of this class is created.
class SpinLockWrapper {
 public:
  SpinLockWrapper();
  ~SpinLockWrapper();

  void Lock();
  void Unlock();

 private:
  SpinLock* lock_;
};

// Corresponding locker object that arranges to acquire a spinlock for the
// duration of a C++ scope.
class ScopedSpinLockHolder {
 public:
  explicit ScopedSpinLockHolder(SpinLockWrapper* lock) : lock_(lock) {
    lock_->Lock();
  }

  ~ScopedSpinLockHolder() {
    lock_->Unlock();
  }

 private:
  SpinLockWrapper* lock_;
};

#endif  // SPINLOCK_WRAPPER_H_
