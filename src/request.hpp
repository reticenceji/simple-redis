#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class ResponseStatus {
  OK = 0,
  NOT_FOUND = 1,
  BAD_REQUEST = 2,
};

class Response {
public:
  ResponseStatus status = ResponseStatus::OK;
  std::string data;
};

int parse_request(const uint8_t *data, size_t size,
                  std::vector<std::string> &out);
void do_request(std::vector<std::string> &&cmd, Response &out);
void make_response(const Response &resp, std::vector<uint8_t> &out);