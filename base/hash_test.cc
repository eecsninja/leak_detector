#include "base/hash.h"

#include <stdio.h>

#include <algorithm>
#include <string>

#include "gtest/gtest.h"
/*
#include <assert.h>
#define EXPECT_EQ(a,b) assert(a==b)
#define TEST(c,f) void c ## f()
*/
namespace {

const std::string kInput = "the quick brown fox jumps over the lazy dog";

}  // namespace

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

namespace base {

uint32_t Hash(const char *data, size_t len) {
    uint32_t hash = 0, tmp;
    int rem;

    if (len <= 0 || data == NULL)
        return hash;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

uint32_t HashStep(uint32_t hash, const char* data, size_t len) {
    int rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        int tmp = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    return hash;
}

uint32_t HashFinish(uint32_t hash) {
    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

}  // namespace base

TEST(HashTest, ZeroInput) {
  EXPECT_EQ(0U, base::Hash(nullptr, 0));
}

TEST(HashTest, StartFinish) {
  uint32_t hash = base::HashStep(0, kInput.c_str(), kInput.size());
  hash = base::HashFinish(hash);

  uint32_t expected = base::Hash(kInput.c_str(), kInput.size());
  EXPECT_EQ(expected, hash);
}

TEST(HashTest, Progressive) {
  size_t len = kInput.size();
  const char* s = kInput.c_str();

  uint32_t hash = 0;
  while (len) {
    int delta = std::min(sizeof(uint32_t), len);
    hash = base::HashStep(hash, s, delta);
    s += delta;
    len -= delta;
  }
  hash = base::HashFinish(hash);

  uint32_t expected = base::Hash(kInput.c_str(), kInput.size());
  EXPECT_EQ(expected, hash);
}
