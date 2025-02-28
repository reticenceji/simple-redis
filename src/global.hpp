#pragma once

#include "connection.hpp"
#include "hashtable.hpp"
#include "heap.hpp"
#include "utils.hpp"
#include <_types/_uint32_t.h>
#include <sys/_types/_size_t.h>

class GlobalState {
public:
  static const uint64_t k_idle_timeout_ms = 5 * 1000;
  static const size_t k_max_works = 2000;
  static HashMap &db() { return instance().db_; }
  static std::vector<std::unique_ptr<Connection>> &fd2conn() {
    return instance().fd2conn_;
  }
  static DList *timeout_dlist_header() { return &instance().idle_list_; }
  static Heap &ttl_heap() { return instance().ttl_heap_; }

public:
  GlobalState(const GlobalState &) = delete;
  GlobalState(GlobalState &&) = delete;
  GlobalState &operator=(const GlobalState &) = delete;
  GlobalState &operator=(GlobalState &&) = delete;

private:
  HashMap db_;
  std::vector<std::unique_ptr<Connection>> fd2conn_;
  DList idle_list_;
  Heap ttl_heap_;

private:
  GlobalState() : db_(HashMap(1024)) {
    idle_list_.prev = &idle_list_;
    idle_list_.next = &idle_list_;
  }
  ~GlobalState() = default;
  static GlobalState &instance() {
    static GlobalState gs;
    return gs;
  }
};

class Entry {
public:
  struct HashNode node; // Hashtable node
  size_t heap_idx = -1; // ttl heap index

  std::string key;
  std::string value;

  void set_ttl(uint32_t ttl_ms) {
    if (ttl_ms < 0 && heap_idx != (size_t)-1) {
      GlobalState::ttl_heap().remove(this->heap_idx);
      this->heap_idx = -1;
    } else if (ttl_ms >= 0) {
      uint64_t expire_at = ttl_ms + get_monotonic_msec();
      GlobalState::ttl_heap().insert(HeapItem{expire_at, &this->heap_idx});
    }
  }
};