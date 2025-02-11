#include <asm-generic/errno-base.h>
#include <assert.h>
#include <cstddef>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "common.hpp"
#include "connection.hpp"

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

int main(int argc, char *argv[]) {
  AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    LOG(ERROR) << "socket error" << std::endl;
    return -1;
  }
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = ntohs(1234);
  server_addr.sin_addr.s_addr = ntohl(0);

  if (bind(listen_fd, (const struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    LOG(ERROR) << "bind error" << std::endl;
    return -1;
  }

  if (listen(listen_fd, 5) < 0) {
    LOG(ERROR) << "listen error" << std::endl;
    return -1;
  }
  fd_set_nb(listen_fd);

  std::vector<std::unique_ptr<Connection>> fd2conn;
  std::vector<struct pollfd> poll_args;
  while (true) {
    // poll every fd: first is listen socket, others are connections.
    poll_args.clear();
    struct pollfd pfd = {listen_fd, POLLIN, 0};
    poll_args.push_back(pfd);
    for (const auto &conn : fd2conn) {
      if (conn == nullptr) {
        continue;
      }
      struct pollfd pfd = {conn->fd(), POLLERR, 0};
      if (conn->state() == ConnectionState::STATE_REQ) {
        pfd.events |= POLLIN;
      }
      if (conn->state() == ConnectionState::STATE_RES) {
        pfd.events |= POLLOUT;
      }
      poll_args.push_back(pfd);
    }
    int rv = poll(poll_args.data(), poll_args.size(), -1);
    if (rv < 0) {
      if (errno == EINTR) {
        continue;
      }
      LOG(FATAL) << "poll error" << strerror(errno) << "\n";
      return -1;
    }

    // handle the listen socket
    if (poll_args[0].revents != 0) {
      if (auto conn = handle_accept(listen_fd)) {
        if (fd2conn.size() <= conn->fd()) {
          fd2conn.resize(conn->fd() + 1);
        }
        assert(fd2conn[conn->fd()] == nullptr);
        fd2conn[conn->fd()] = std::move(conn);
      }
    }
    // handle the connections
    for (size_t i = 1; i < poll_args.size(); i++) {
      uint32_t ready = poll_args[i].revents;
      if (ready != 0) {
        int fd = poll_args[i].fd;
        auto &conn = fd2conn[fd];

        if (ready & POLLIN) {
          conn->handle_read();
        }
        if (ready & POLLOUT) {
          conn->handle_write();
        }
        if (ready & POLLERR || conn->state() == ConnectionState::STATE_END) {
          fd2conn.erase(fd2conn.begin() + fd);
          LOG(INFO) << "connection closed" << std::endl;
        }
      }
    }
  }
  return 0;
}