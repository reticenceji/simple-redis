#include <_types/_uint32_t.h>
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

#include "connection.hpp"
#include "global.hpp"
#include "hashtable.hpp"
#include "heap.hpp"
#include "utils.hpp"

static int64_t next_timer_ms() {
  DList *header = GlobalState::timeout_dlist_header();
  uint64_t next_ms = (uint64_t)-1;
  uint64_t now_ms = get_monotonic_msec();

  if (!header->is_empty()) {
    Connection *conn = Connection::container_of_timeout_node(header->next);
    next_ms = conn->get_last_activate_ms() + GlobalState::k_idle_timeout_ms;
  }
  if (!GlobalState::ttl_heap().is_empty()) {
    auto next_ms_ttl = GlobalState::ttl_heap().top().val;
    if (next_ms > next_ms_ttl) {
      next_ms = next_ms_ttl;
    }
  }

  if (next_ms == (uint64_t)-1) {
    return -1;
  } else if (next_ms <= now_ms) {
    return 0;
  } else {
    return (int64_t)(next_ms - now_ms);
  }
}

static void process_timers() {
  uint64_t now_ms = get_monotonic_msec();
  DList *header = GlobalState::timeout_dlist_header();
  while (!header->is_empty()) {
    Connection *conn = Connection::container_of_timeout_node(header->next);
    uint32_t next_ms =
        conn->get_last_activate_ms() + GlobalState::k_idle_timeout_ms;
    if (next_ms >= now_ms) {
      break; // not expired
    }
    auto &fd2conn = GlobalState::fd2conn();
    int fd = conn->fd();
    LOG(INFO) << "remove idle connection" << fd << "\n";
    fd2conn.erase(fd2conn.begin() + fd);
  }

  auto &heap = GlobalState::ttl_heap();
  for (int i = 0; i < GlobalState::k_max_works; i++) {
    if (heap.is_empty() || heap.top().val > now_ms) {
      break;
    }
    Entry *ent = container_of(heap.top().ref, Entry, heap_idx);
    HashNode *node = GlobalState::db().remove(
        &ent->node, [](HashNode *a, HashNode *b) { return a == b; });
    assert(node == &ent->node);
    heap.remove(ent->heap_idx);
    LOG(INFO) << "remove expired entry" << ent->key << "\n";

    delete ent;
  }
}

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
  auto conn =
      std::make_unique<Connection>(connfd, GlobalState::timeout_dlist_header());

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
    int32_t timeout_ms = next_timer_ms();
    int rv = poll(poll_args.data(), poll_args.size(), timeout_ms);
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
        conn->update_timer(GlobalState::timeout_dlist_header());
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
    process_timers();
  }
  return 0;
}