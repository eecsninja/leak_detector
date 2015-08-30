// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector.h"

#include <gperftools/custom_allocator.h>
#include <gperftools/malloc_extension.h>
#include <gperftools/malloc_hook.h>
#include <gperftools/spin_lock_wrapper.h>
#include <link.h>
#include <stdint.h>
#include <unistd.h>

#include <new>

#include "base/logging.h"
#include "components/metrics/leak_detector/leak_detector_impl.h"
#include "hooks.h"

namespace leak_detector {

namespace {

// We strip out different number of stack frames in debug mode
// because less inlining happens in that case
#ifdef NDEBUG
static const int kStripFrames = 2;
#else
static const int kStripFrames = 3;
#endif

// For storing the address range of the Chrome binary in memory.
struct MappingInfo {
  uintptr_t addr;
  size_t size;
} chrome_mapping;

// TODO(sque): This is a temporary solution for leak detector params. Eventually
// params should be passed in from elsewhere in Chromium, and this file should
// be deleted.

bool EnvToBool(const char* envname, const bool default_value) {
  return !getenv(envname)
      ? default_value
      : memchr("tTyY1\0", getenv(envname)[0], 6) != NULL;
}

int EnvToInt(const char* envname, const int default_value) {
  return !getenv(envname)
      ? default_value
      : strtol(getenv(envname), NULL, 10);
}

// Used for sampling allocs and frees. Randomly samples |g_sampling_factor|/256
// of the pointers being allocated and freed.
int g_sampling_factor = EnvToInt("LEAK_DETECTOR_SAMPLING_FACTOR", 1);

// The number of call stack levels to unwind when profiling allocations by call
// stack.
int g_stack_depth = EnvToInt("LEAK_DETECTOR_STACK_DEPTH", 4);

// Dump allocation stats and check for memory leaks after this many bytes have
// been allocated since the last dump/check. Does not get affected by sampling.
uint64_t g_dump_interval_bytes =
    EnvToInt("LEAK_DETECTOR_DUMP_INTERVAL_KB", 32768) * 1024;

// Enable verbose logging. Dump all leak analysis data, not just analysis
// summaries and suspected leak reports.
bool g_dump_leak_analysis = EnvToBool("LEAK_DETECTOR_VERBOSE", false);

// The number of times an allocation size must be suspected as a leak before it
// gets reported.
int g_size_suspicion_threshold =
    EnvToInt("LEAK_DETECTOR_SIZE_SUSPICION_THRESHOLD", 4);

// The number of times a call stack for a particular allocation size must be
// suspected as a leak before it gets reported.
int g_call_stack_suspicion_threshold =
    EnvToInt("LEAK_DETECTOR_CALL_STACK_SUSPICION_THRESHOLD", 4);

// Use a simple spinlock for locking. Don't use a mutex, which can call malloc
// and cause infinite recursion.
SpinLockWrapper* g_heap_lock = nullptr;

// Points to the active instance of the leak detector.
// Modify this only when locked.
LeakDetectorImpl* g_leak_detector = nullptr;

// Keep track of the total number of bytes allocated.
// Modify this only when locked.
uint64_t g_total_alloc_size = 0;

// Keep track of the total alloc size when the last dump occurred.
// Modify this only when locked.
uint64_t g_last_alloc_dump_size = 0;

// Dump allocation stats and check for leaks after |g_dump_interval_bytes| bytes
// have been allocated since the last time that was done. Should be called with
// a lock since it modifies the global variable |g_last_alloc_dump_size|.
inline void MaybeDumpStatsAndCheckForLeaks() {
  if (g_total_alloc_size > g_last_alloc_dump_size + g_dump_interval_bytes) {
    g_last_alloc_dump_size = g_total_alloc_size;

    InternalVector<InternalLeakReport> reports;
    g_leak_detector->TestForLeaks(true /* do_logging */, &reports);
  }
}

// Convert a pointer to a hash value. Returns only the upper eight bits.
inline uint64_t PointerToHash(const void* ptr) {
  // The input data is the pointer address, not the location in memory pointed
  // to by the pointer.
  // The multiplier is taken from Farmhash code:
  //   https://github.com/google/farmhash/blob/master/src/farmhash.cc
  const uint64_t kMultiplier = 0x9ddfea08eb382d69ULL;
  uint64_t value = reinterpret_cast<uint64_t>(ptr) * kMultiplier;
  return value >> 56;
}

// Uses PointerToHash() to pseudorandomly sample |ptr|.
inline bool ShouldSample(const void* ptr) {
  return PointerToHash(ptr) < static_cast<uint64_t>(g_sampling_factor);
}

// Allocation/deallocation hooks for MallocHook.
void NewHook(const void* ptr, size_t size) {
  {
    ScopedSpinLockHolder lock(g_heap_lock);
    g_total_alloc_size += size;
  }

  if (!ShouldSample(ptr) || !ptr || !g_leak_detector)
    return;

  // Take the stack trace outside the critical section.
  // |g_leak_detector->ShouldGetStackTraceForSize()| is const; there is no need
  // for a lock.
  void* stack[g_stack_depth];
  int depth = 0;
  if (g_leak_detector->ShouldGetStackTraceForSize(size)) {
    depth = MallocHook::GetCallerStackTrace(
        stack, g_stack_depth, kStripFrames + 1);
  }

  ScopedSpinLockHolder lock(g_heap_lock);
  g_leak_detector->RecordAlloc(ptr, size, depth, stack);
  MaybeDumpStatsAndCheckForLeaks();
}

void DeleteHook(const void* ptr) {
  if (!ShouldSample(ptr) || !ptr || !g_leak_detector)
    return;

  ScopedSpinLockHolder lock(g_heap_lock);
  g_leak_detector->RecordFree(ptr);
}

// Callback for dl_iterate_phdr() to find the Chrome binary mapping.
int IterateLoadedObjects(struct dl_phdr_info *shared_object,
                         size_t /* size */,
                         void *data) {
    if (g_dump_leak_analysis) {
      LOG(ERROR) << "name=" << shared_object->dlpi_name << ", "
                 << "addr=" << std::hex << shared_object->dlpi_phnum;
    }
    for (int i = 0; i < shared_object->dlpi_phnum; i++) {
      // Find the ELF segment header that contains the actual code of the Chrome
      // binary.
      const ElfW(Phdr)& segment_header = shared_object->dlpi_phdr[i];
      if (segment_header.p_type == SHT_PROGBITS &&
          segment_header.p_offset == 0 && data) {
        MappingInfo* mapping = static_cast<MappingInfo*>(data);

        // Make sure the fields in the ELF header and MappingInfo have the
        // same size.
        static_assert(sizeof(mapping->addr) == sizeof(shared_object->dlpi_addr),
                      "Integer size mismatch between MappingInfo::addr and "
                      "dl_phdr_info::dlpi_addr.");
        static_assert(sizeof(mapping->size) == sizeof(segment_header.p_offset),
                      "Integer size mismatch between MappingInfo::size and "
                      "ElfW(Phdr)::p_memsz.");

        mapping->addr = shared_object->dlpi_addr + segment_header.p_offset;
        mapping->size = segment_header.p_memsz;
        if (g_dump_leak_analysis) {
          LOG(ERROR) << "Chrome mapped from " << std::hex
                     << mapping->addr << " to "
                     << mapping->addr + mapping->size;
        }
        return 1;
      }
    }
    return 0;
}

}  // namespace

void Initialize() {
  // If the sampling factor is too low, don't bother enabling the leak detector.
  if (g_sampling_factor < 1) {
    LOG(ERROR) << "Not enabling leak detector because g_sampling_factor="
               << g_sampling_factor;
    return;
  }

  if (IsInitialized())
    return;

  // Locate the Chrome binary mapping before doing anything else.
  dl_iterate_phdr(IterateLoadedObjects, &chrome_mapping);

  // This should be done before the hooks are set up, since it should
  // call new, and we want that to be accounted for correctly.
  MallocExtension::Initialize();

  if (CustomAllocator::IsInitialized()) {
    LOG(ERROR) << "Custom allocator can only be initialized once!";
    return;
  }
  CustomAllocator::Initialize();

  g_heap_lock = new(CustomAllocator::Allocate(sizeof(SpinLockWrapper)))
      SpinLockWrapper;

  ScopedSpinLockHolder lock(g_heap_lock);
  if (g_leak_detector)
    return;

  LOG(ERROR) << "Starting leak detector. Sampling factor: "
             << g_sampling_factor;

  g_leak_detector = new(CustomAllocator::Allocate(sizeof(LeakDetectorImpl)))
      LeakDetectorImpl(chrome_mapping.addr,
                       chrome_mapping.size,
                       g_size_suspicion_threshold,
                       g_call_stack_suspicion_threshold,
                       g_dump_leak_analysis);

  // Now set the hooks that capture new/delete and malloc/free. Make sure
  // nothing is already set.
  CHECK(MallocHook::SetNewHook(&NewHook) == nullptr);
  CHECK(MallocHook::SetDeleteHook(&DeleteHook) == nullptr);
}

void Shutdown() {
  if (!IsInitialized())
    return;

  {
    ScopedSpinLockHolder lock(g_heap_lock);

    // Unset our new/delete hooks, checking they were previously set.
    CHECK_EQ(MallocHook::SetNewHook(nullptr), &NewHook);
    CHECK_EQ(MallocHook::SetDeleteHook(nullptr), &DeleteHook);

    g_leak_detector->~LeakDetectorImpl();
    CustomAllocator::Free(g_leak_detector, sizeof(LeakDetectorImpl));
    g_leak_detector = nullptr;
  }

  g_heap_lock->~SpinLockWrapper();
  CustomAllocator::Free(g_heap_lock, sizeof(*g_heap_lock));

  if (!CustomAllocator::Shutdown())
    LOG(ERROR) <<  "Memory leak in LeakDetector, allocated objects remain.";

  LOG(ERROR) << "Stopped leak detector.";
}

bool IsInitialized() {
  return g_leak_detector;
}

}  // namespace leak_detector
