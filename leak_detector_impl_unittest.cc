// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector_impl.h"

#include <math.h>
#include <stdint.h>

#include <complex>
#include <new>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leak_detector {

namespace {

// Makes working with complex numbers easier.
using Complex = std::complex<double>;

// The mapping location in memory for a fictional executable.
const uintptr_t kMappingAddr = 0x800000;
const size_t kMappingSize = 0x200000;

// Some call stacks within the fictional executable.
// * - outside the mapping range, e.g. JIT code.
const uintptr_t kRawStack0[] = {
  0x800100,
  0x900000,
  0x880080,
  0x810000,
};
const uintptr_t kRawStack1[] = {
  0x940000,
  0x980000,
  0xdeadbeef,  // *
  0x9a0000,
};
const uintptr_t kRawStack2[] = {
  0x8f0d00,
  0x803abc,
  0x9100a0,
};
const uintptr_t kRawStack3[] = {
  0x90fcde,
  0x900df00d,  // *
  0x801000,
  0x880088,
  0xdeadcafe,  // *
  0x9f0000,
  0x8700a0,
  0x96037c,
};
const uintptr_t kRawStack4[] = {
  0x8c0000,
  0x85d00d,
  0x921337,
  0x780000,  // *
};
const uintptr_t kRawStack5[] = {
  0x990000,
  0x888888,
  0x830ac0,
  0x8e0000,
  0xc00000,  // *
};

// This struct makes it easier to pass call stack info to
// LeakDetectorImplTest::Alloc().
struct TestCallStack {
  size_t depth;
  const void* const* stack;
};

const TestCallStack kStack0 =
    { arraysize(kRawStack0), reinterpret_cast<const void* const*>(kRawStack0) };
const TestCallStack kStack1 =
    { arraysize(kRawStack1), reinterpret_cast<const void* const*>(kRawStack1) };
const TestCallStack kStack2 =
    { arraysize(kRawStack2), reinterpret_cast<const void* const*>(kRawStack2) };
const TestCallStack kStack3 =
    { arraysize(kRawStack3), reinterpret_cast<const void* const*>(kRawStack3) };
const TestCallStack kStack4 =
    { arraysize(kRawStack4), reinterpret_cast<const void* const*>(kRawStack4) };
const TestCallStack kStack5 =
    { arraysize(kRawStack5), reinterpret_cast<const void* const*>(kRawStack5) };

}  // namespace

class LeakDetectorImplTest : public ::testing::Test {
 public:
  LeakDetectorImplTest()
      : total_num_allocs_(0),
        total_num_frees_(0),
        total_alloced_size_(0),
        next_analysis_total_alloced_size_(kAllocedSizeAnalysisInterval) {}

  void SetUp() override {
    CustomAllocator::InitializeForUnitTest();

    const int kSizeSuspicionThreshold = 4;
    const int kCallStackSuspicionThreshold = 4;
    detector_.reset(
        new LeakDetectorImpl(kMappingAddr,
                             kMappingSize,
                             kSizeSuspicionThreshold,
                             kCallStackSuspicionThreshold,
                             true /* verbose */));
  }

  void TearDown() override {
    // Free any memory that was leaked by test cases. Do not use Free() because
    // that will try to modify |alloced_ptrs_|.
    for (void* ptr : alloced_ptrs_)
      delete [] reinterpret_cast<char*>(ptr);
    alloced_ptrs_.clear();

    // Must destroy all objects using CustomAllocator before shutting it down.
    detector_.reset();
    stored_reports_.clear();

    CustomAllocator::Shutdown();
  }

 protected:
  // Alloc and free functions that automatically pass allocation info to
  // |detector_|.
  void* Alloc(size_t size, const TestCallStack& stack) {
    void* ptr = new char[size];
    detector_->RecordAlloc(ptr, size, stack.depth, stack.stack);

    EXPECT_TRUE(alloced_ptrs_.find(ptr) == alloced_ptrs_.end());
    alloced_ptrs_.insert(ptr);

    ++total_num_allocs_;
    total_alloced_size_ += size;
    if (total_alloced_size_ >= next_analysis_total_alloced_size_) {
      InternalVector<InternalLeakReport> reports;
      detector_->TestForLeaks(false /* do_logging */, &reports);
      for (const InternalLeakReport& report : reports)
        stored_reports_.insert(report);

      // Determine when the next leak analysis should occur.
      while (total_alloced_size_ >= next_analysis_total_alloced_size_)
        next_analysis_total_alloced_size_ += kAllocedSizeAnalysisInterval;
    }
    return ptr;
  }

  // See comment for Alloc().
  void Free(void* ptr) {
    auto find_ptr_iter = alloced_ptrs_.find(ptr);
    EXPECT_FALSE(find_ptr_iter == alloced_ptrs_.end());
    if (find_ptr_iter == alloced_ptrs_.end())
      return;

    alloced_ptrs_.erase(find_ptr_iter);
    detector_->RecordFree(ptr);
    ++total_num_frees_;
    delete [] reinterpret_cast<char*>(ptr);
  }

  // TEST CASE: Julia set fractal computation. Pass in has_leak=true to trigger
  // the memory leak.
  void JuliaSet(bool has_leak);

  // Instance of the class being tested.
  scoped_ptr<LeakDetectorImpl> detector_;

  // Number of pointers allocated and freed so far.
  size_t total_num_allocs_;
  size_t total_num_frees_;

  // The interval between consecutive analyses (LeakDetectorImpl::TestForLeaks),
  // in number of bytes allocated. e.g. if |kAllocedSizeAnalysisInterval| = 1024
  // then call TestForLeaks() every 1024 bytes of allocation that occur.
  static const size_t kAllocedSizeAnalysisInterval = 8192;

  // Keeps count of total size allocated by Alloc().
  size_t total_alloced_size_;

  // The cumulative allocation size at which to trigger the TestForLeaks() call.
  size_t next_analysis_total_alloced_size_;

  // Stores all pointers to memory allocated by by Alloc() so we can manually
  // free the leaked pointers at the end.
  std::set<void*> alloced_ptrs_;

  // Store leak reports here. Use a set so duplicate reports are not stored.
  std::set<InternalLeakReport> stored_reports_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LeakDetectorImplTest);
};

void LeakDetectorImplTest::JuliaSet(bool has_leak) {
  // The center region of the complex plane that is the basis for our Julia set
  // computations is a circle of radius kRadius.
  const static double kRadius = 2;

  // To track points in the complex plane, we will use a rectangular grid in the
  // range defined by [-kRadius, kRadius] along both axes.
  const static double kRangeMin = -kRadius;
  const static double kRangeMax = kRadius;

  // Divide each axis into intervals, each of which is associated with a point
  // on that axis at its center.
  const static double kIntervalInverse = 64;
  const static double kInterval = 1.0 / kIntervalInverse;
  const static int kNumPoints = (kRangeMax - kRangeMin) / kInterval + 1;

  // Contains some useful functions for converting between points on the complex
  // plane and in a gridlike data structure.
  struct ComplexPlane {
    static int GetXGridIndex(const Complex& value) {
      return (value.real() + kInterval / 2 - kRangeMin) / kInterval;
    }
    static int GetYGridIndex(const Complex& value) {
      return (value.imag() + kInterval / 2 - kRangeMin) / kInterval;
    }
    static Complex GetComplexForGridPoint(size_t x, size_t y) {
      return Complex(kRangeMin + x * kInterval, kRangeMin + y * kInterval);
    }
  };

  // Make sure the choice of interval doesn't result in any loss of precision.
  ASSERT_EQ(1.0, kInterval * kIntervalInverse);

  // Create a grid for part of the complex plane, with each axis within the
  // range [kRangeMin, kRangeMax].
  const size_t width = kNumPoints;
  const size_t height = kNumPoints;
  std::vector<Complex*> grid(width * height);

  // Initialize an object for each point within the inner circle |z| < kRadius.
  for (size_t i = 0; i < width; ++i) {
    for (size_t j = 0; j < height; ++j) {
      Complex point = ComplexPlane::GetComplexForGridPoint(i, j);
      // Do not store any values outside the inner circle.
      if (abs(point) <= kRadius) {
        grid[i + j * width] =
            new(Alloc(sizeof(Complex), kStack0)) Complex(point);
      }
    }
  }
  EXPECT_LE(alloced_ptrs_.size(), width * height);

  // Create a new grid for the result of the transformation.
  std::vector<Complex*> next_grid(width * height, nullptr);

  const int kNumIterations = 20;
  for (int n = 0; n < kNumIterations; ++n) {
    for (int i = 0; i < kNumPoints; ++i) {
      for (int j = 0; j < kNumPoints; ++j) {
        if (!grid[i + j * width])
          continue;

        // NOTE: The below code is NOT an efficient way to compute a Julia set.
        // This is only to test the leak detector with some nontrivial code.

        // A simple polynomial function for generating Julia sets is:
        //   f(z) = z^n + c

        // But in this algorithm, we need the inverse:
        //   fInv(z) = (z - c)^(1/n)

        // Here, let's use n=5 and c=0.544.
        const Complex c(0.544, 0);
        const Complex& z = *grid[i + j * width];

        // This is the principal root.
        Complex root = pow(z - c, 0.2);

        // Discard the result if it is too far out from the center of the plane.
        if (abs(root) > kRadius)
          continue;

        // The below code only allocates Complex objects of the same size. The
        // leak detector expects various sizes, so increase the allocation size
        // by a different amount at each call site.

        // Nth root produces N results.
        // Place all root results on |next_grid|.

        // First, place the principal root.
        int next_i = ComplexPlane::GetXGridIndex(root);
        int next_j = ComplexPlane::GetYGridIndex(root);
        if (!next_grid[next_i + next_j * width]) {
          next_grid[next_i + next_j * width] =
              new(Alloc(sizeof(Complex) + 24, kStack1)) Complex(root);
        }

        double magnitude = abs(root);
        double angle = arg(root);
        // To generate other roots, rotate the principal root by increments of
        // 1/N of a full circle.
        const double kAngleIncrement = M_PI * 2 / 5;

        // Second root.
        root = std::polar(magnitude, angle + kAngleIncrement);
        next_i = ComplexPlane::GetXGridIndex(root);
        next_j = ComplexPlane::GetYGridIndex(root);
        if (!next_grid[next_i + next_j * width]) {
          next_grid[next_i + next_j * width] =
              new(Alloc(sizeof(Complex) + 40, kStack2)) Complex(root);
        }

        // In some of the sections below, |has_leak| will trigger a memory leak
        // by overwriting the old Complex pointer value without freeing it.
        // Due to the nature of complex roots being confined to equal sections
        // of the complex plane, each new pointer will displace an old pointer
        // that was allocated from the same line of code.

        // Third root.
        root = std::polar(magnitude, angle + kAngleIncrement * 2);
        next_i = ComplexPlane::GetXGridIndex(root);
        next_j = ComplexPlane::GetYGridIndex(root);
        // *** LEAK ***
        if (has_leak || !next_grid[next_i + next_j * width]) {
          next_grid[next_i + next_j * width] =
              new(Alloc(sizeof(Complex) + 40, kStack3)) Complex(root);
        }

        // Fourth root.
        root = std::polar(magnitude, angle + kAngleIncrement * 3);
        next_i = ComplexPlane::GetXGridIndex(root);
        next_j = ComplexPlane::GetYGridIndex(root);
        // *** LEAK ***
        if (has_leak || !next_grid[next_i + next_j * width]) {
          next_grid[next_i + next_j * width] =
              new(Alloc(sizeof(Complex) + 52, kStack4)) Complex(root);
        }

        // Fifth root.
        root = std::polar(magnitude, angle + kAngleIncrement * 4);
        next_i = ComplexPlane::GetXGridIndex(root);
        next_j = ComplexPlane::GetYGridIndex(root);
        if (!next_grid[next_i + next_j * width]) {
          next_grid[next_i + next_j * width] =
              new(Alloc(sizeof(Complex) + 52, kStack5)) Complex(root);
        }
      }
    }

    // Clear the previously allocated points.
    for (Complex*& point : grid) {
      if (point) {
        Free(point);
        point = nullptr;
      }
    }

    // Now swap the two grids for the next iteration.
    grid.swap(next_grid);
  }

  // Clear the previously allocated points.
  for (Complex*& point : grid) {
    if (point) {
      Free(point);
      point = nullptr;
    }
  }
}

TEST_F(LeakDetectorImplTest, CheckTestFramework) {
  EXPECT_EQ(0U, total_num_allocs_);
  EXPECT_EQ(0U, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());

  // Allocate some memory.
  void* ptr0 = Alloc(12, kStack0);
  void* ptr1 = Alloc(16, kStack0);
  void* ptr2 = Alloc(24, kStack0);
  EXPECT_EQ(3U, total_num_allocs_);
  EXPECT_EQ(0U, total_num_frees_);
  EXPECT_EQ(3U, alloced_ptrs_.size());

  // Free one of the pointers.
  Free(ptr1);
  EXPECT_EQ(3U, total_num_allocs_);
  EXPECT_EQ(1U, total_num_frees_);
  EXPECT_EQ(2U, alloced_ptrs_.size());

  // Allocate some more memory.
  void* ptr3 = Alloc(72, kStack1);
  void* ptr4 = Alloc(104, kStack1);
  void* ptr5 = Alloc(96, kStack1);
  void* ptr6 = Alloc(24, kStack1);
  EXPECT_EQ(7U, total_num_allocs_);
  EXPECT_EQ(1U, total_num_frees_);
  EXPECT_EQ(6U, alloced_ptrs_.size());

  // Free more pointers.
  Free(ptr2);
  Free(ptr4);
  Free(ptr6);
  EXPECT_EQ(7U, total_num_allocs_);
  EXPECT_EQ(4U, total_num_frees_);
  EXPECT_EQ(3U, alloced_ptrs_.size());

  // Free remaining memory.
  Free(ptr0);
  Free(ptr3);
  Free(ptr5);
  EXPECT_EQ(7U, total_num_allocs_);
  EXPECT_EQ(7U, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());
}

TEST_F(LeakDetectorImplTest, JuliaSetNoLeak) {
  JuliaSet(false);

  EXPECT_EQ(total_num_allocs_, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());
  ASSERT_EQ(0U, stored_reports_.size());
}

TEST_F(LeakDetectorImplTest, JuliaSetWithLeak) {
  JuliaSet(true);

  EXPECT_GT(total_num_allocs_, total_num_frees_);
  EXPECT_GT(alloced_ptrs_.size(), 0U);
  ASSERT_EQ(2U, stored_reports_.size());

  // The reports should be stored in order of size.
  const InternalLeakReport& report1 = *stored_reports_.begin();
  EXPECT_EQ(sizeof(Complex) + 40, report1.alloc_size_bytes);
  EXPECT_EQ(kStack3.depth, report1.call_stack.size());
  for (size_t i = 0; i < kStack3.depth && i < report1.call_stack.size(); ++i) {
    // The call stack's addresses may not fall within the mapping address.
    // Those that don't will not be converted to mapping offsets.
    if (kRawStack3[i] >= kMappingAddr &&
        kRawStack3[i] <= kMappingAddr + kMappingSize) {
      EXPECT_EQ(kRawStack3[i] - kMappingAddr, report1.call_stack[i]);
    } else {
      EXPECT_EQ(kRawStack3[i], report1.call_stack[i]);
    }
  }

  const InternalLeakReport& report2 = *(++stored_reports_.begin());
  EXPECT_EQ(sizeof(Complex) + 52, report2.alloc_size_bytes);
  EXPECT_EQ(kStack4.depth, report2.call_stack.size());
  for (size_t i = 0; i < kStack4.depth && i < report2.call_stack.size(); ++i) {
    // The call stack's addresses may not fall within the mapping address.
    // Those that don't will not be converted to mapping offsets.
    if (kRawStack4[i] >= kMappingAddr &&
        kRawStack4[i] <= kMappingAddr + kMappingSize) {
      EXPECT_EQ(kRawStack4[i] - kMappingAddr, report2.call_stack[i]);
    } else {
      EXPECT_EQ(kRawStack4[i], report2.call_stack[i]);
    }
  }
}

}  // namespace leak_detector
