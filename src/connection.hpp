
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
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

  void handle_read() {
    ssize_t rv = read(fd_, rbuf_, sizeof(rbuf_));
    // handle error
    if (rv < 0) {
      // EAGAIN means "there is no data available right now, try again later".
      if (errno != EAGAIN) {
        LOG(ERROR) << "read failed: " << strerror(errno) << "\n";
        state_ = ConnectionState::STATE_END;
      }
      return;
    } else if (rv == 0) { // handle EOF
      if (incoming_.size() == 0) {
        LOG(INFO) << "client closed"
                  << "\n";
      } else {
        LOG(INFO) << "unexpected EOF"
                  << "\n";
      }
      state_ = ConnectionState::STATE_END;
      return;
    } else {
      incoming_.insert(incoming_.end(), rbuf_, rbuf_ + rv);
      while (try_one_request()) {
      }
      if (outgoing_.size() > 0) {
        state_ = ConnectionState::STATE_RES;
        return handle_write();
      }
    }
  }
  void handle_write() {
    ssize_t rv = write(fd_, outgoing_.data(), outgoing_.size());
    if (rv < 0) {
      if (errno != EAGAIN) {
        LOG(ERROR) << "write failed: " << strerror(errno) << "\n";
        state_ = ConnectionState::STATE_END;
      }
      return;
    }
    outgoing_.erase(outgoing_.begin(), outgoing_.begin() + rv);

    if (outgoing_.size() == 0) {
      state_ = ConnectionState::STATE_REQ;
    }
  }

  bool try_one_request() {
    if (incoming_.size() < 4) {
      return false; // need read more
    }
    uint32_t len = 0;
    memcpy(&len, incoming_.data(), 4);
    if (len > k_max_msg) {
      LOG(ERROR) << "message too long: " << len << "\n";
      state_ = ConnectionState::STATE_END;
      return false;
    }
    if (incoming_.size() < len + 4) {
      return false;
    }
    incoming_.erase(incoming_.begin(), incoming_.begin() + 4);
    std::string msg(incoming_.begin(), incoming_.begin() + len);
    incoming_.erase(incoming_.begin(), incoming_.begin() + len);
    LOG(DEBUG) << "Received message: " << msg << "\n";

    // TODO handle message, now simply echo back
    outgoing_.insert(outgoing_.end(), reinterpret_cast<uint8_t *>(&len),
                     reinterpret_cast<uint8_t *>(&len) + 4);
    outgoing_.insert(outgoing_.end(), msg.begin(), msg.end());

    return true;
  }

  static void conn_put(std::vector<Connection *> &fd2conn, Connection *conn) {
    if (fd2conn.size() <= conn->fd_) {
      fd2conn.resize(conn->fd_ + 1);
    }
    fd2conn[conn->fd_] = conn;
  }

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

static std::unique_ptr<Connection> handle_accept(int fd) {
  struct sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
  if (connfd < 0) {
    LOG(ERROR) << "accept failed: " << strerror(errno) << "\n";
    return nullptr;
  }
  uint32_t ip = client_addr.sin_addr.s_addr;
  LOG(INFO) << "Accept connection from " << inet_ntoa({ip}) << ":"
            << ntohs(client_addr.sin_port) << std::endl;

  fd_set_nb(connfd);
  auto conn = std::make_unique<Connection>(connfd);

  return conn;
}
