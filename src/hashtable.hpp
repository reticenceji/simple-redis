#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

struct HashNode {
  HashNode *next = nullptr;
  uint64_t hcode = 0;
};

// HashTable is a simple hash table implementation.
// The lookup function is a linear search.
class HashTable {
private:
  HashNode **tab_ = nullptr;
  size_t mask_ = 0;
  size_t size_ = 0;

public:
  HashTable() = default;
  HashTable(size_t n) {
    // Check if n is power of 2 and greater than 0
    if (n <= 0 || ((n - 1) & n) != 0) {
      throw std::invalid_argument("n must be power of 2 and greater than 0");
    }
    tab_ = new HashNode *[n](); // Use new and zero-initialize array
    mask_ = n - 1;
    size_ = 0;
  }
  ~HashTable() {
    if (tab_) {
      delete[] tab_;
    }
  }

  void insert(HashNode *node) {
    size_t pos = node->hcode & mask_;
    HashNode *next = tab_[pos];
    node->next = next;
    tab_[pos] = node;
    size_++;
  }

  HashNode **lookup(HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    size_t pos = key->hcode & mask_;

    for (HashNode **from = &tab_[pos]; *from != nullptr;
         from = &(*from)->next) {
      if ((*from)->hcode == key->hcode && eq(*from, key)) {
        return from;
      }
    }
    return nullptr;
  }

  HashNode *detach(HashNode **from) {
    HashNode *node = *from;
    *from = node->next;
    size_--;
    return node;
  }

  size_t mask_size() const { return mask_ + 1; }
  size_t size() const { return size_; }
  HashNode **head(size_t pos) const { return &tab_[pos]; }
  bool empty() const { return tab_ == nullptr; }
};

class HashMap {
private:
  HashTable newer_;
  HashTable older_;
  size_t migrate_pos_ = 0;
  const size_t k_max_load_factor = 8;
  const size_t k_rehashing_work = 128;

public:
  HashMap() : newer_(16) {}
  HashMap(size_t n) : newer_(n) {}
  HashNode *lookup(HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    migrate();
    HashNode **from = newer_.lookup(key, eq);
    if (!from) {
      from = older_.lookup(key, eq);
    }
    return from ? *from : nullptr;
  }
  HashNode *remove(HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    migrate();
    if (HashNode **from = newer_.lookup(key, eq)) {
      return newer_.detach(from);
    }
    if (HashNode **from = older_.lookup(key, eq)) {
      return older_.detach(from);
    }
    return nullptr;
  }

  void insert(HashNode *node) {
    newer_.insert(node);
    if (!older_.empty()) {
      size_t shreshold = newer_.mask_size() * k_max_load_factor;
      if (newer_.size() > shreshold) {
        rehash();
      }
    }
    migrate();
  }

  void rehash() {
    older_ = newer_;
    newer_ = HashTable(newer_.mask_size() * 2);
    migrate_pos_ = 0;
  }

  void migrate() {
    size_t nwork = 0;
    while (nwork < k_rehashing_work && older_.size() > 0) {
      HashNode **from = older_.head(migrate_pos_);
      if (!*from) {
        migrate_pos_++;
        continue;
      }
      newer_.insert(older_.detach(from));
      nwork++;
    }

    if (older_.size() == 0 && older_.empty()) {
      older_ = HashTable();
    }
  }
};