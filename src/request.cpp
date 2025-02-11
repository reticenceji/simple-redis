#include "request.hpp"
#include "common.hpp"
#include <cstdint>
#include <vector>

int parse_request(const uint8_t *data, size_t size,
                  std::vector<std::string> &out) {
  const uint8_t *end = data + size;
  uint32_t nstr = 0;
  if (!read_u32(data, end, nstr)) {
    return -1;
  }
  while (out.size() < nstr) {
    uint32_t len = 0;
    if (!read_u32(data, end, len)) {
      return -1;
    }
    if (data + len > end) {
      return -1;
    }
    out.push_back(std::string(data, data + len));
    data += len;
  }
  if (data != end) {
    return -1;
  }
  return 0;
}

static std::map<std::string, std::string> g_data;

void do_request(const std::vector<std::string> &cmd, Response &out) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    auto it = g_data.find(cmd[1]);
    if (it == g_data.end()) {
      out.status = ResponseStatus::NOT_FOUND;
      return;
    }
    const std::string &value = it->second;
    out.data.assign(value.begin(), value.end());
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    g_data[cmd[1]] = cmd[2];
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    g_data.erase(cmd[1]);
  } else {
    out.status = ResponseStatus::BAD_REQUEST;
  }
}

void make_response(const Response &resp, std::vector<uint8_t> &out) {
  uint32_t resp_len = 4 + (uint32_t)resp.data.size();
  out.insert(out.end(), reinterpret_cast<uint8_t *>(&resp_len),
             reinterpret_cast<uint8_t *>(&resp_len) + 4);
  out.insert(out.end(), reinterpret_cast<const uint8_t *>(&resp.status),
             reinterpret_cast<const uint8_t *>(&resp.status) + 4);
  out.insert(out.end(), resp.data.begin(), resp.data.end());
}