// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_table.h"

#include <gperftools/custom_allocator.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leak_detector {

namespace {

// Default threshold used for leak analysis.
const int kDefaultLeakThreshold = 5;

// Some test call stacks.
const void* kRawStack0[] = {
  reinterpret_cast<const void*>(0xaabbccdd),
  reinterpret_cast<const void*>(0x11223344),
  reinterpret_cast<const void*>(0x55667788),
  reinterpret_cast<const void*>(0x99887766),
};
const void* kRawStack1[] = {
  reinterpret_cast<const void*>(0xdeadbeef),
  reinterpret_cast<const void*>(0x900df00d),
  reinterpret_cast<const void*>(0xcafedeed),
  reinterpret_cast<const void*>(0xdeafbabe),
};
const void* kRawStack2[] = {
  reinterpret_cast<const void*>(0x12345678),
  reinterpret_cast<const void*>(0xabcdef01),
  reinterpret_cast<const void*>(0xfdecab98),
};
const void* kRawStack3[] = {
  reinterpret_cast<const void*>(0xdead0001),
  reinterpret_cast<const void*>(0xbeef0002),
  reinterpret_cast<const void*>(0x900d0003),
  reinterpret_cast<const void*>(0xf00d0004),
  reinterpret_cast<const void*>(0xcafe0005),
  reinterpret_cast<const void*>(0xdeed0006),
  reinterpret_cast<const void*>(0xdeaf0007),
  reinterpret_cast<const void*>(0xbabe0008),
};

// Generates a CallStack object from a raw call stack.
CallStack GenerateCallStack(uint32_t depth, const void** raw_call_stack) {
  CallStack new_stack;
  new_stack.depth = depth;
  new_stack.stack = raw_call_stack;
  new_stack.hash = CallStack::ComputeHash()(&new_stack);

  return new_stack;
}

// The unit tests require that call stack objects are placed in the proper
// sequence in memory. It is an important detail when checking the output of
// LeakAnalyzer's suspected leaks, which are ordered by the leak value.
// Instantiate the objects as part of an array to ensure their order in memory.
const CallStack kCallStacks[] = {
  GenerateCallStack(arraysize(kRawStack0), kRawStack0),
  GenerateCallStack(arraysize(kRawStack1), kRawStack1),
  GenerateCallStack(arraysize(kRawStack2), kRawStack2),
  GenerateCallStack(arraysize(kRawStack3), kRawStack3),
};

// Unit tests should directly reference these pointers to CallStack objects.
const CallStack* kStack0 = &kCallStacks[0];
const CallStack* kStack1 = &kCallStacks[1];
const CallStack* kStack2 = &kCallStacks[2];
const CallStack* kStack3 = &kCallStacks[3];

}  // namespace

class CallStackTableTest : public ::testing::Test {
 public:
  CallStackTableTest() {}

  void SetUp() override {
    CustomAllocator::InitializeForUnitTest();
  }
  void TearDown() override {
    CustomAllocator::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CallStackTableTest);
};

TEST_F(CallStackTableTest, Hash) {
  // Ensure increasing order of call stack placement in memory.
  EXPECT_LT(kStack0, kStack1);
  EXPECT_LT(kStack1, kStack2);
  EXPECT_LT(kStack2, kStack3);

  // Hash function should generate nonzero values.
  EXPECT_NE(0U, kStack0->hash);
  EXPECT_NE(0U, kStack1->hash);
  EXPECT_NE(0U, kStack2->hash);
  EXPECT_NE(0U, kStack3->hash);

  // Hash function should generate unique hashes for each call stack.
  EXPECT_NE(kStack0->hash, kStack1->hash);
  EXPECT_NE(kStack0->hash, kStack2->hash);
  EXPECT_NE(kStack0->hash, kStack3->hash);
  EXPECT_NE(kStack1->hash, kStack2->hash);
  EXPECT_NE(kStack1->hash, kStack3->hash);
  EXPECT_NE(kStack2->hash, kStack3->hash);
}

TEST_F(CallStackTableTest, HashWithReducedDepth) {
  ASSERT_GT(kStack3->depth, 4U);

  // Hash function should only operate on the first |CallStack::depth| elements
  // of CallStack::stack. To test this, reduce the depth value of one of the
  // stacks and make sure the hash changes.
  EXPECT_NE(kStack3->hash,
            GenerateCallStack(kStack3->depth - 1, kStack3->stack).hash);
  EXPECT_NE(kStack3->hash,
            GenerateCallStack(kStack3->depth - 2, kStack3->stack).hash);
  EXPECT_NE(kStack3->hash,
            GenerateCallStack(kStack3->depth - 3, kStack3->stack).hash);
  EXPECT_NE(kStack3->hash,
            GenerateCallStack(kStack3->depth - 4, kStack3->stack).hash);
}

TEST_F(CallStackTableTest, EmptyTable) {
  CallStackTable table(kDefaultLeakThreshold);
  EXPECT_TRUE(table.empty());

  EXPECT_EQ(0U, table.num_allocs());
  EXPECT_EQ(0U, table.num_frees());

  // The table should be able to gracefully handle an attempt to remove a call
  // stack entry when none exists.
  table.Remove(kStack0);
  table.Remove(kStack1);
  table.Remove(kStack2);
  table.Remove(kStack3);

  EXPECT_EQ(0U, table.num_allocs());
  EXPECT_EQ(0U, table.num_frees());
}

TEST_F(CallStackTableTest, InsertionAndRemoval) {
  CallStackTable table(kDefaultLeakThreshold);

  table.Add(kStack0);
  EXPECT_EQ(1U, table.size());
  EXPECT_EQ(1U, table.num_allocs());
  table.Add(kStack1);
  EXPECT_EQ(2U, table.size());
  EXPECT_EQ(2U, table.num_allocs());
  table.Add(kStack2);
  EXPECT_EQ(3U, table.size());
  EXPECT_EQ(3U, table.num_allocs());
  table.Add(kStack3);
  EXPECT_EQ(4U, table.size());
  EXPECT_EQ(4U, table.num_allocs());

  // Add some call stacks that have already been added. There should be no
  // change in the number of entries, as they are aggregated by call stack.
  table.Add(kStack2);
  EXPECT_EQ(4U, table.size());
  EXPECT_EQ(5U, table.num_allocs());
  table.Add(kStack3);
  EXPECT_EQ(4U, table.size());
  EXPECT_EQ(6U, table.num_allocs());

  // Start removing entries.
  EXPECT_EQ(0U, table.num_frees());

  table.Remove(kStack0);
  EXPECT_EQ(3U, table.size());
  EXPECT_EQ(1U, table.num_frees());
  table.Remove(kStack1);
  EXPECT_EQ(2U, table.size());
  EXPECT_EQ(2U, table.num_frees());

  // Removing call stacks with multiple counts will not reduce the overall
  // number of table entries, until the count reaches 0.
  table.Remove(kStack2);
  EXPECT_EQ(2U, table.size());
  EXPECT_EQ(3U, table.num_frees());
  table.Remove(kStack3);
  EXPECT_EQ(2U, table.size());
  EXPECT_EQ(4U, table.num_frees());

  table.Remove(kStack2);
  EXPECT_EQ(1U, table.size());
  EXPECT_EQ(5U, table.num_frees());
  table.Remove(kStack3);
  EXPECT_EQ(0U, table.size());
  EXPECT_EQ(6U, table.num_frees());

  // Now the table should be empty, but attempt to remove some more and make
  // sure nothing breaks.
  table.Remove(kStack0);
  table.Remove(kStack1);
  table.Remove(kStack2);
  table.Remove(kStack3);

  EXPECT_TRUE(table.empty());
  EXPECT_EQ(6U, table.num_allocs());
  EXPECT_EQ(6U, table.num_frees());
}

TEST_F(CallStackTableTest, MassiveInsertionAndRemoval) {
  CallStackTable table(kDefaultLeakThreshold);

  for (int i = 0; i < 100; ++i)
    table.Add(kStack3);
  EXPECT_EQ(1U, table.size());
  EXPECT_EQ(100U, table.num_allocs());

  for (int i = 0; i < 100; ++i)
    table.Add(kStack2);
  EXPECT_EQ(2U, table.size());
  EXPECT_EQ(200U, table.num_allocs());

  for (int i = 0; i < 100; ++i)
    table.Add(kStack1);
  EXPECT_EQ(3U, table.size());
  EXPECT_EQ(300U, table.num_allocs());

  for (int i = 0; i < 100; ++i)
    table.Add(kStack0);
  EXPECT_EQ(4U, table.size());
  EXPECT_EQ(400U, table.num_allocs());

  // Remove them in a different order, by removing one of each stack during one
  // iteration. The size should not decrease until the last iteration.
  EXPECT_EQ(0U, table.num_frees());

  for (int i = 0; i < 100; ++i) {
    table.Remove(kStack0);
    EXPECT_EQ(4U * i + 1, table.num_frees());

    table.Remove(kStack1);
    EXPECT_EQ(4U * i + 2, table.num_frees());

    table.Remove(kStack2);
    EXPECT_EQ(4U * i + 3, table.num_frees());

    table.Remove(kStack3);
    EXPECT_EQ(4U * i + 4, table.num_frees());
  }
  EXPECT_EQ(400U, table.num_frees());
  EXPECT_TRUE(table.empty());

  // Try to remove some more from an empty table and make sure nothing breaks.
  table.Remove(kStack0);
  table.Remove(kStack1);
  table.Remove(kStack2);
  table.Remove(kStack3);

  EXPECT_TRUE(table.empty());
  EXPECT_EQ(400U, table.num_allocs());
  EXPECT_EQ(400U, table.num_frees());
}

TEST_F(CallStackTableTest, DetectLeak) {
  CallStackTable table(kDefaultLeakThreshold);

  // Add some base number of entries.
  for (int i = 0; i < 60; ++i)
      table.Add(kStack0);
  for (int i = 0; i < 50; ++i)
      table.Add(kStack1);
  for (int i = 0; i < 64; ++i)
      table.Add(kStack2);
  for (int i = 0; i < 72; ++i)
      table.Add(kStack3);

  table.TestForLeaks();
  EXPECT_TRUE(table.leak_analyzer().suspected_leaks().empty());

  // Use the following scheme:
  // - kStack0: increase by 4 each time -- leak suspect
  // - kStack1: increase by 3 each time -- leak suspect
  // - kStack2: increase by 1 each time -- not a suspect
  // - kStack3: alternate between increasing and decreasing - not a suspect
  bool increase_kstack3 = true;
  for (int i = 0; i < kDefaultLeakThreshold; ++i) {
    EXPECT_TRUE(table.leak_analyzer().suspected_leaks().empty());

    for (int j = 0; j < 4; ++j)
      table.Add(kStack0);

    for (int j = 0; j < 3; ++j)
      table.Add(kStack1);

    table.Add(kStack2);

    // Alternate between adding and removing.
    if (increase_kstack3)
      table.Add(kStack3);
    else
      table.Remove(kStack3);
    increase_kstack3 = !increase_kstack3;

    table.TestForLeaks();
  }

  // Check that the correct leak values have been detected.
  const auto& leaks = table.leak_analyzer().suspected_leaks();
  ASSERT_EQ(2U, leaks.size());
  // Suspected leaks are reported in increasing leak value -- in this case, the
  // CallStack object's address.
  EXPECT_EQ(kStack0, leaks[0].call_stack());
  EXPECT_EQ(kStack1, leaks[1].call_stack());
}

}  // namespace leak_detector
