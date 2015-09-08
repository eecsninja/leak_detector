#include "compact_address_map.h"

#include <gperftools/custom_allocator.h>

CompactAddressMap::CompactAddressMap()
    : free_entries_(NULL),
      allocated_objects_(NULL),
      num_entries_(0) {
  stats_ = {0};
  cluster_hash_table_ = New<Cluster*>(kClusterHashTableSize);
}

CompactAddressMap::~CompactAddressMap() {
  // De-allocate all of the objects we allocated.
  for (Object* object = allocated_objects_; object != NULL; /**/) {
    Object* next = object->next;
    Free(object);
    object = next;
  }
}

// static
void* CompactAddressMap::Alloc(size_t size) {
  return CustomAllocator::Allocate(size);
}

// static
void CompactAddressMap::Free(void* ptr) {
  CustomAllocator::Free(ptr, 0);
}

CompactAddressMap::Cluster* CompactAddressMap::GetCluster(uintptr_t addr) {
  uintptr_t id = addr / kClusterSize;
  int index = id % kClusterHashTableSize;
  for (Cluster* c = cluster_hash_table_[index]; c != NULL; c = c->next) {
    if (c->id == id)
      return c;
  }

  Cluster* c = New<Cluster>(1);
  c->id = id;
  c->next = cluster_hash_table_[index];
  cluster_hash_table_[index] = c;
  ++stats_.num_clusters;
  return c;
}

CompactAddressMap::Subcluster* CompactAddressMap::GetSubcluster(
    Cluster* cluster, uintptr_t addr) {
  int index = (addr % kClusterSize) / kSubclusterSize;
  Subcluster* subcluster = cluster->subclusters[index];
  if (!subcluster) {
    subcluster = New<Subcluster>(1);
    cluster->subclusters[index] = subcluster;
    ++stats_.num_subclusters;
  }
  return subcluster;
}

CompactAddressMap::Page* CompactAddressMap::GetPage(uintptr_t addr) {
  Cluster* cluster = GetCluster(addr);
  Subcluster* subcluster = GetSubcluster(cluster, addr);
  int index = (addr % kSubclusterSize) / kPageSize;

  Page *page = subcluster->pages[index];
  if (!page) {
    page = New<Page>(1);
    subcluster->pages[index] = page;
    ++stats_.num_pages;
  }
  return page;
}

void CompactAddressMap::Insert(const void* ptr,
                               uint16_t size,
                               const uint32_t* hash) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  Page* page = GetPage(addr);

  // Look in linked-list for this block.
  const int block = (addr % kPageSize) / kBlockSize;
  const uint16_t block_offset = addr % kBlockSize;
  size_t num_steps = 0;
  for (Entry* entry = page->blocks[block]; entry != NULL; entry = entry->next) {
    ++num_steps;
    if (entry->offset == block_offset) {
      entry->Store(block_offset, size, hash);
      if (num_steps > stats_.max_num_steps)
        stats_.max_num_steps = num_steps;
      ++num_entries_;

      return;
    }
  }

  // Create entry.
  if (free_entries_ == NULL) {
    // Allocate a new batch of entries and add to free-list.
    Entry* array = New<Entry>(kEntryBulkAllocCount);
    stats_.num_entries += kEntryBulkAllocCount;
    for (int i = 0; i < kEntryBulkAllocCount - 1; i++)
      array[i].next = &array[i + 1];
    array[kEntryBulkAllocCount - 1].next = free_entries_;
    free_entries_ = &array[0];
  }

  // Detach a free entry for the current operation.
  Entry* entry = free_entries_;
  free_entries_ = entry->next;
  entry->Store(block_offset, size, hash);
  entry->next = page->blocks[block];
  page->blocks[block] = entry;
  ++num_entries_;

  if (num_steps > stats_.max_num_steps)
    stats_.max_num_steps = num_steps;
}

bool CompactAddressMap::FindAndRemove(const void *ptr, Entry* result) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  Page* page = GetPage(addr);

  // Look in linked-list for this block.
  const int block = (addr % kPageSize) / kBlockSize;
  const uint16_t block_offset = addr % kBlockSize;
  for (Entry** p = &page->blocks[block]; *p; p = &((*p)->next)) {
    Entry* entry = *p;
    if (entry->offset == block_offset) {
      *result = *entry;
      *p = entry->next;
      // Put the removed entry back into the free list.
      entry->next = free_entries_;
      free_entries_ = entry;
      --num_entries_;

      return true;
    }
  }
  return false;
}
