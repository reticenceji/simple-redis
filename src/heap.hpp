#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/_types/_size_t.h>
#include <vector>

struct HeapItem {
  uint64_t val = 0;
  size_t *ref = nullptr;
};

class Heap {
public:
  void remove(size_t pos);
  void upsert(size_t pos, HeapItem t);
  void insert(HeapItem t);
  bool is_empty() { return heap.empty(); }
  HeapItem &top() { return heap[0]; }

private:
  std::vector<HeapItem> heap;
};