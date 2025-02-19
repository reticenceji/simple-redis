#include "request.hpp"
#include "common.hpp"
#include "hashtable.hpp"
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

static struct {
  HashMap db = HashMap(1024);
} g_data;

struct Entry {
  struct HashNode node;
  std::string key;
  std::string value;
};

static bool entry_eq(HashNode *a, HashNode *b) {
  struct Entry *ea = container_of(a, struct Entry, node);
  struct Entry *eb = container_of(b, struct Entry, node);
  return ea->key == eb->key;
}

uint64_t hash(const std::string value) {
  uint32_t h = 0x811c9dc5;
  for (char c : value) {
    h = (h + c) * 0x01000193;
  }
  return h;
}

void do_get(const std::vector<std::string> &&cmd, Response &out) {
  // a dummy `Entry` just for the lookup
  Entry entry;
  entry.key = std::move(cmd[1]);
  entry.node.hcode = hash(entry.key);
  // hashtable lookup
  HashNode *node = g_data.db.lookup(&entry.node, entry_eq);
  if (!node) {
    out.status = ResponseStatus::NOT_FOUND;
    return;
  }
  {
    const std::string &value = container_of(node, Entry, node)->value;
    out.data.assign(value.begin(), value.end());
  }
}

void do_set(std::vector<std::string> &&cmd, Response &out) {
  Entry entry;
  entry.key = std::move(cmd[1]);
  entry.node.hcode = hash(entry.key);
  HashNode *node = g_data.db.lookup(&entry.node, entry_eq);
  if (node != nullptr) {
    container_of(node, Entry, node)->value = std::move(cmd[2]);
  } else {
    // We should use `new` to allocate the memory for `Entry`
    Entry *ent = new Entry;
    ent->node.hcode = std::move(entry.node.hcode);
    ent->key = std::move(entry.key);
    ent->value = std::move(cmd[2]);
    g_data.db.insert(&ent->node);
  }
}

void do_del(const std::vector<std::string> &&cmd, Response &out) {
  Entry entry;
  entry.key = cmd[1];
  entry.node.hcode = hash(entry.key);
  HashNode *node = g_data.db.remove(&entry.node, entry_eq);
  if (node) {
    delete container_of(node, Entry, node);
  }
}

void do_request(std::vector<std::string> &&cmd, Response &out) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    do_get(std::move(cmd), out);
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    do_set(std::move(cmd), out);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    do_del(std::move(cmd), out);
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