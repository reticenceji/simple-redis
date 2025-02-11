#pragma once

#include <asm-generic/errno-base.h>
#include <assert.h>
#include <cstddef>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "aixlog.hpp"

static bool read_all(int fd, char *buf, size_t len) {
  ssize_t n = 0;
  while (n < len) {
    ssize_t rv = read(fd, buf, len - n);
    if (rv <= 0) {
      if (rv == EINTR) {
        continue;
      } else {
        return false;
      }
    }
    n += rv;
    buf += rv;
  }
  assert(n == len);
  return true;
}

static bool write_all(int fd, const char *buf, size_t len) {
  ssize_t n = 0;
  while (n < len) {
    ssize_t rv = write(fd, buf, len - n);
    if (rv <= 0) {
      if (rv == EINTR) {
        continue;
      } else {
        return false;
      }
    }
    n += rv;
    buf += rv;
  }
  assert(n == len);
  return true;
}

/**
 * @brief set fd to non-blocking mode
 *
 * @param fd
 */
static void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno != 0) {
    LOG(FATAL) << "fcntl(F_GETFL) failed: " << strerror(errno) << "\n";
    abort();
  }
  errno = 0;
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (errno != 0) {
    LOG(FATAL) << "fcntl(F_SETFL) failed: " << strerror(errno) << "\n";
    abort();
  }
}

static bool read_u32(const uint8_t *&begin, const uint8_t *end, uint32_t &out) {
  if (end - begin < 4) {
    return false;
  }
  memcpy(&out, begin, 4);
  begin += 4;
  return true;
}
