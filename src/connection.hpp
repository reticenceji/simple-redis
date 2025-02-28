#pragma once

#include <_types/_uint32_t.h>
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
#include "utils.hpp"

enum class ConnectionState {
  STATE_REQ = 0,
  STATE_RES = 1,
  STATE_END = 2,
};

class Connection {
public:
  Connection(int fd, DList *timeout_node_header)
      : fd_(fd), last_active_ms_(get_monotonic_msec()) {
    timeout_node_header->insert_before(&timeout_node);
  }
  ~Connection() {
    if (fd_ != -1) {
      close(fd_);
    }
    timeout_node.detach();
  }
  // getters
  int fd() const { return fd_; }
  ConnectionState state() const { return state_; }

  void handle_read();
  void handle_write();
  bool try_one_request();
  void update_timer(DList *timeout_node_header);
  static void conn_put(std::vector<Connection *> &fd2conn, Connection *conn);
  uint32_t get_last_activate_ms() { return last_active_ms_; }
  static Connection *container_of_timeout_node(DList *node) {
    return container_of(node, Connection, timeout_node);
  }

private:
  DList timeout_node;
  int fd_ = -1;
  uint8_t rbuf_[4096];
  uint8_t wbuf_[4096];
  uint32_t last_active_ms_;
  ConnectionState state_ = ConnectionState::STATE_REQ;

  // buffered input and output
  std::vector<uint8_t> incoming_;
  std::vector<uint8_t> outgoing_;
  const size_t k_max_msg = 1024;
};
