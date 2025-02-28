#include "heap.hpp"
#include <sys/_types/_size_t.h>

static inline size_t parent(size_t i) { return (i + 1) / 2 - 1; }
static size_t left(size_t i) { return i * 2 + 1; }
static size_t right(size_t i) { return i * 2 + 2; }
static void heap_up(HeapItem *root, size_t pos) {
  HeapItem t = root[pos];
  while (pos > 0 && root[parent(pos)].val > t.val) {
    root[pos] = root[parent(pos)];
    *root[pos].ref = pos;
    pos = parent(pos);
  }
  root[pos] = t;
  *root[pos].ref = pos;
}

static void heap_down(HeapItem *root, size_t pos, size_t len) {
  HeapItem t = root[pos];
  while (true) {
    // find the smallest one among the parent and their kids
    size_t l = left(pos);
    size_t r = right(pos);
    size_t min_pos = pos;
    uint64_t min_val = t.val;
    if (l < len && root[l].val < min_val) {
      min_pos = l;
      min_val = root[l].val;
    }
    if (r < len && root[r].val < min_val) {
      min_pos = r;
    }
    if (min_pos == pos) {
      break;
    }
    // swap with the kid
    root[pos] = root[min_pos];
    *root[pos].ref = pos;
    pos = min_pos;
  }
  root[pos] = t;
  *root[pos].ref = pos;
}
static void heap_update(HeapItem *root, size_t pos, size_t len) {
  if (pos > 0 && root[parent(pos)].val > root[pos].val) {
    heap_up(root, pos);
  } else {
    heap_down(root, pos, len);
  }
}

void Heap::remove(size_t pos) {
  heap[pos] = heap.back();
  heap.pop_back();
  if (pos < heap.size()) {
    heap_update(heap.data(), pos, heap.size());
  }
}

void Heap::upsert(size_t pos, HeapItem t) {
  if (pos < heap.size()) {
    heap[pos] = t; // update
  } else {
    pos = heap.size();
    heap.push_back(t);
  }
  heap_update(heap.data(), pos, heap.size());
}

void Heap::insert(HeapItem t) {
  size_t pos = heap.size();
  *t.ref = pos;
  heap.push_back(t);

  heap_update(heap.data(), pos, heap.size());
}