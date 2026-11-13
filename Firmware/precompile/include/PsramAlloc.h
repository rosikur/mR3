#pragma once
#include <esp_heap_caps.h>
#include <stddef.h>
#include <stdlib.h>

// STL allocator that prefers PSRAM and falls back to normal heap.
template <class T>
struct PsramAllocator {
  using value_type = T;
  PsramAllocator() = default;
  template <class U>
  PsramAllocator(const PsramAllocator<U>&) {}

  T* allocate(size_t n) {
    void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n * sizeof(T));
    if (!p) abort();
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t) { free(p); }
};

template <class T, class U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) { return false; }
