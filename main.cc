#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>

#include "base/logging.h"
#include "hooks.h"
#include "leak_detector.h"

const uint32_t kAllocCode = 0xdeadbeef;
const uint32_t kFreeCode = 0xcafebabe;

struct AllocEntry {
  uint32_t code;
  const void* ptr;
  uint32_t size;
  uint32_t depth;
  const void* const* stack;
};

struct FreeEntry {
  uint32_t code;
  const void* ptr;
};

static bool DEBUG = getenv("DEBUG");

namespace leak_detector {

extern uint64_t default_chrome_addr;
extern uint64_t default_chrome_size;

}  // namespace leak_detector

using leak_detector::default_chrome_addr;
using leak_detector::default_chrome_size;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Need to provide an input file:\n");
    printf("  %s [FILE].\n", argv[0]);
    return 0;
  }

  FILE* fp = fopen(argv[1], "rb");
  CHECK(fp);
  CHECK(!feof(fp));
  CHECK_EQ(1, fread(&default_chrome_addr, sizeof(default_chrome_addr), 1, fp));
  CHECK_EQ(1, fread(&default_chrome_size, sizeof(default_chrome_size), 1, fp));

  leak_detector::Initialize();

  while (!feof(fp)) {
    union {
      uint32_t code;
      AllocEntry alloc;
      FreeEntry free;
    };
    int entry_offset = ftell(fp);
    fread(&code, sizeof(code), 1, fp);
    if (code == kAllocCode) {
      fread(&code + 1, sizeof(alloc) - sizeof(code) - sizeof(alloc.stack), 1, fp);
      if (DEBUG) {
        printf("%x: ALLOC %p\t%u\t%u\n", entry_offset, alloc.ptr, alloc.size,
               alloc.depth);
      }
      std::unique_ptr<void*[]> stack;
      if (alloc.depth > 0) {
        stack.reset(new void*[alloc.depth]);
        fread(stack.get(), sizeof(void*), alloc.depth, fp);
      }
      MallocHook::SetCallerStackTrace(alloc.depth, stack.get());
      if (alloc.ptr && alloc.size)
        MallocHook::InvokeNewHook(alloc.ptr, alloc.size);
    } else if (code == kFreeCode) {
      if (DEBUG)
        printf("%x: FREE %p\n", entry_offset, free.ptr);
      fread(&code + 1, sizeof(free) - sizeof(code), 1, fp);
      MallocHook::InvokeDeleteHook(free.ptr);
    } else {
      printf("Unknown code at offset %lx, quitting: %x\n",
             ftell(fp) - sizeof(code), code);
      break;
    }
  }

  printf("Finished with %lu bytes read\n", ftell(fp));
  fclose(fp);

  leak_detector::Shutdown();

  return 0;
}
