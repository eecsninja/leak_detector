#include "compact_address_map.h"

#include <gperftools/custom_allocator.h>

#include <map>

#include "base/macros.h"
#include "gtest/gtest.h"

struct AllocInfo {
  uint16_t size;
  uint32_t hash;
};

class CompactAddressMapTest : public ::testing::Test {
 public:
  CompactAddressMapTest() {}

  void SetUp() override {
    CustomAllocator::InitializeForUnitTest();
  }
  void TearDown() override {
    CustomAllocator::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactAddressMapTest);
};

TEST_F(CompactAddressMapTest, Test) {
  std::map<int*, AllocInfo> map;
  CompactAddressMap cam;
  EXPECT_EQ(0, cam.size());

  for (size_t n = 0; n < 1000; ++n) {
    uint8_t size = rand() & UINT8_MAX;
    int* ptr = new int[size];
    uint32_t hash = ~reinterpret_cast<uint64_t>(ptr);
    map[ptr] = { size, hash };
    cam.Insert(ptr, size, &hash);
    EXPECT_EQ(n + 1, cam.size());
  }

  for (auto it = map.begin(); it != map.end(); ++it) {
    int* ptr = it->first;
    const AllocInfo info = it->second;

    CompactAddressMap::Entry entry = {};
    EXPECT_TRUE(cam.FindAndRemove(ptr, &entry));

    EXPECT_EQ(info.size, entry.size);

    EXPECT_TRUE(entry.has_call_stack);
    EXPECT_EQ(info.hash, entry.call_stack_hash);

    delete [] ptr;
  }
}
