#pragma once

#include <_types/_uint32_t.h>
#include <cstddef>
#include <string>
#include <vector>

enum ResponseType {
  NIL = 0,
  ERR = 1,
  STR = 2,
  INT = 3,
  DOUBLE = 4,
  ARRAY = 5,
};

enum ResponseErrorType {
  ERR_UNKNOWN = 1,
  ERR_TOO_BIG = 2,
};

class Response {
public:
  Response(std::vector<uint8_t> &buffer)
      : buffer_(buffer), begin_(buffer.size()) {
    push_back_u32(0); // reserve space to add length
  }

  void out_nil() { buffer_.push_back(ResponseType::NIL); }
  void out_str(const std::string &s) {
    buffer_.push_back(ResponseType::STR);
    push_back_u32(s.size());
    buffer_.insert(buffer_.end(), s.begin(), s.end());
  }
  void out_err(ResponseErrorType err, const std::string &msg) {
    buffer_.push_back(ResponseType::ERR);
    push_back_u32(err); // ????
    push_back_u32(msg.size());
    buffer_.insert(buffer_.end(), msg.begin(), msg.end());
  }
  void out_int(int64_t value) {
    buffer_.push_back(ResponseType::INT);
    push_back_i64(value);
  }
  void out_arrary(uint32_t n) {
    buffer_.push_back(ResponseType::ARRAY);
    push_back_u32(n);
  }
  void build() {
    size_t size = buffer_.size() - sizeof(uint32_t) - begin_;
    if (size > 100000) {
      buffer_.resize(begin_ + sizeof(uint32_t));
      out_err(ResponseErrorType::ERR_TOO_BIG, "response size too big");
      size = buffer_.size() - sizeof(uint32_t) - begin_;
    }
    // insert the while msg length
    std::copy(reinterpret_cast<uint8_t *>(&size),
              reinterpret_cast<uint8_t *>(&size) + sizeof(uint32_t),
              buffer_.begin() + begin_);
  }

private:
  std::vector<uint8_t> &buffer_;
  size_t begin_;
  void push_back_u32(uint32_t value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<uint8_t *>(&value),
                   reinterpret_cast<uint8_t *>(&value) + sizeof(uint32_t));
  }
  void push_back_i64(int64_t value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<uint8_t *>(&value),
                   reinterpret_cast<uint8_t *>(&value) + sizeof(int64_t));
  }
};

int parse_request(const uint8_t *data, size_t size,
                  std::vector<std::string> &out);
void do_request(std::vector<std::string> &&cmd, Response &out);
void make_response(const Response &resp, std::vector<uint8_t> &out);
