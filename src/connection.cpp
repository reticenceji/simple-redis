#include "connection.hpp"
#include "request.hpp"

void Connection::handle_read() {
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

void Connection::handle_write() {
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

bool Connection::try_one_request() {
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
  std::vector<std::string> cmd;
  if (parse_request(incoming_.data(), len, cmd) < 0) {
    state_ = ConnectionState::STATE_END;
    return false;
  }
  Response resp(outgoing_);
  do_request(std::move(cmd), resp);
  resp.build();

  incoming_.erase(incoming_.begin(), incoming_.begin() + len);
  return true;
}

void Connection::conn_put(std::vector<Connection *> &fd2conn,
                          Connection *conn) {
  if (fd2conn.size() <= conn->fd_) {
    fd2conn.resize(conn->fd_ + 1);
  }
  fd2conn[conn->fd_] = conn;
}
