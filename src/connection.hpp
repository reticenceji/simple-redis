#pragma once

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <vector>

#include "aixlog.hpp"
#include "common.hpp"

enum class ConnectionState {
  STATE_REQ = 0,
  STATE_RES = 1,
  STATE_END = 2,
};

class Connection {
public:
  Connection(int fd) : fd_(fd) {}
  ~Connection() {
    if (fd_ != -1) {
      close(fd_);
    }
  }
  // getters
  int fd() const { return fd_; }
  ConnectionState state() const { return state_; }

  void handle_read();
  void handle_write();
  bool try_one_request();
  static void conn_put(std::vector<Connection *> &fd2conn, Connection *conn);

private:
  int fd_ = -1;
  uint8_t rbuf_[4096];
  uint8_t wbuf_[4096];
  ConnectionState state_ = ConnectionState::STATE_REQ;

  // buffered input and output
  std::vector<uint8_t> incoming_;
  std::vector<uint8_t> outgoing_;
  const size_t k_max_msg = 1024;
};
