#pragma once

#include <assert.h>
#include <cstddef>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "aixlog.hpp"

bool read_all(int fd, char *buf, size_t len);
bool write_all(int fd, const char *buf, size_t len);

/**
 * @brief set fd to non-blocking mode
 *
 * @param fd
 */
void fd_set_nb(int fd);

bool read_u32(const uint8_t *&begin, const uint8_t *end, uint32_t &out);

uint64_t get_monotonic_msec();

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

class DList {
public:
  DList() {
    this->prev = this;
    this->next = this;
  }
  inline void detach() {
    DList *next = this->next;
    DList *prev = this->prev;
    next->prev = prev;
    prev->next = next;
  }

  inline void insert_before(DList *rookie) {
    DList *prev = this->prev;
    prev->next = rookie;
    rookie->next = this;

    this->prev = rookie;
    rookie->prev = prev;
  }

  inline bool is_empty() { return prev == next; }

  DList *prev;
  DList *next;
};
