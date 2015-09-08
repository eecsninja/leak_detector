#ifndef COMPACT_ADDRESS_MAP_H_
#define COMPACT_ADDRESS_MAP_H_

#include <stdint.h>
#include <string.h>

class CompactAddressMap {
 public:
  typedef void* (*Allocator)(size_t size);
  typedef void  (*DeAllocator)(void* ptr);

  struct Stats {
    size_t heap_size;
    size_t num_clusters;
    size_t num_subclusters;
    size_t num_pages;
    size_t num_entries;
    size_t max_num_steps;
  };

  struct Entry {
    Entry* next;
    uint16_t size : 15;
    uint8_t has_call_stack : 1;
    uint16_t offset;
    uint32_t call_stack_hash;

    void Store(uint16_t offset, uint8_t size, const uint32_t* hash) {
      this->offset = offset;
      this->size = size;
      if (hash) {
        has_call_stack = true;
        call_stack_hash = *hash;
      } else {
        has_call_stack = false;
      }
    }
  };

  CompactAddressMap();
  ~CompactAddressMap();

  const Stats& stats() const {
    return stats_;
  }

  const size_t size() const {
    return num_entries_;
  }

  void Insert(const void* ptr, uint16_t size, const uint32_t* hash);
  bool FindAndRemove(const void *ptr, Entry* result);

 private:
  static const int kBlockSize = 256;

  static const int kNumBlocksPerPage = 16;
  static const int kPageSize = kNumBlocksPerPage * kBlockSize;
  struct Page {
    Entry* blocks[kNumBlocksPerPage];
  };

  static const int kNumPagesPerSubcluster = 16;
  static const int kSubclusterSize = kNumPagesPerSubcluster * kPageSize;
  struct Subcluster {
    Page* pages[kNumPagesPerSubcluster];
  };

  static const int kNumSubclustersPerCluster = 16;
  static const int kClusterSize = kNumSubclustersPerCluster * kSubclusterSize;
  struct Cluster {
    uint32_t id;
    Cluster* next;
    Subcluster* subclusters[kNumSubclustersPerCluster];
  };

  static const int kClusterHashTableSize = (1UL << 32) / kClusterSize;

  // Allocate this many free Entries at a time.
  static const int kEntryBulkAllocCount = 64;

  //--------------------------------------------------------------
  // Memory management -- we keep all objects we allocate linked
  // together in a singly linked list so we can get rid of them
  // when we are all done.  Furthermore, we allow the client to
  // pass in custom memory allocator/deallocator routines.
  //--------------------------------------------------------------
  struct Object {
    Object* next;
    Object* prev;
    int count;
    // The real data starts here
  };

  static void* Alloc(size_t size);
  static void Free(void* ptr);

  // Custom object allocator.
  template <class T>
  T* New(int count) {
    size_t size = sizeof(Object) + count * sizeof(T);
    void* ptr = Alloc(size);
    memset(ptr, 0, size);
    stats_.heap_size += size;

    Object* object = reinterpret_cast<Object*>(ptr);
    object->count = count;
    object->next = allocated_objects_;
    object->prev = NULL;
    if (allocated_objects_)
      allocated_objects_->prev = object;
    allocated_objects_ = object;
    return reinterpret_cast<T*>(reinterpret_cast<Object*>(ptr) + 1);
  }

  // Custom object deallocator
  template <class T>
  T* Delete(T* ptr) {
    Object* object = reinterpret_cast<Object*>(ptr) - 1;
    stats_.heap_size -= sizeof(Object) + sizeof(T) * object->count;

    if (object->prev)
      object->prev->next = object->next;
    else
      allocated_objects_ = object->next;

    if (object->next)
      object->next->prev = object->prev;

    Free(object);
  }

  Cluster* GetCluster(uintptr_t addr);
  Subcluster* GetSubcluster(Cluster* cluster, uintptr_t addr);
  Page* GetPage(uintptr_t addr);

  Cluster** cluster_hash_table_;
  Entry* free_entries_;
  Object* allocated_objects_;

  Stats stats_;

  size_t num_entries_;
};

#endif  // COMPACT_ADDRESS_MAP_H_
