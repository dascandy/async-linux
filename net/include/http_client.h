#pragma once

#include "tcp_socket.h"
#include <vector>
#include <cstdint>
#include <map>
#include <string>
#include <span>

template <typename T>
struct http_client {
  uint8_t buffer[8192];
  T client;
  size_t currentOffset = 0, bufferSize = 0;
  enum {
    Free,
    InUse,
    Dead
  } state;
  static future<http_client> create(network_address addr) {
    co_return http_client(co_await T::create(addr));
  }
private:
  http_client(T&& client) 
  : client(std::move(client))
  , state(Free)
  {
  }
public:
  struct response {
    http_client& client;
    int errorCode = 0;
    std::map<std::string, std::string> headers;
    response(http_client& client, void* endptr) 
    : client(client)
    {
      uint8_t* p = client.buffer;
      while (p < endptr && *p != ' ') ++p;
      p++;
      while (p < endptr && *p >= '0' && *p <= '9') {
        errorCode = errorCode * 10 + *p - '0';
        p++;
      }
      while (p < endptr && (p[-1] != '\r' || p[0] != '\n')) ++p;
      ++p;
      while (p < endptr && *p != '\r') {
        while (*p == ' ') ++p;
        uint8_t* keyStart = p;
        while (p < endptr && *p != ':') ++p;
        uint8_t* keyEnd = p;
        ++p;
        while (*p == ' ') ++p;
        uint8_t* valueStart = p;
        while (p < endptr && *p != '\r') ++p;
        uint8_t* valueEnd = p;
        ++p;
        ++p;
        headers.insert({std::string(keyStart, keyEnd), std::string(valueStart, valueEnd)});
      }
    }
    ~response() {
      client.state = Free;
    }
    size_t contentLength() {
      if (auto it = headers.find("Content-Length"); it != headers.end()) {
        return std::stoul(it->second);
      } else {
        return std::numeric_limits<size_t>::max();
      }
    }
    future<std::vector<uint8_t>> readFullBody() {
      std::vector<uint8_t> buffer;
      buffer.resize(contentLength());
      co_await client.recvmsg(buffer.data(), buffer.size());
      co_return std::move(buffer);
    }
    future<void> readBodySection(std::span<uint8_t>& buffer);
  };
  future<size_t> recvmsg(uint8_t* data, size_t length) {
    size_t read = 0;
    if (currentOffset != bufferSize) {
      size_t toCopy = std::min(bufferSize - currentOffset, length);
      memcpy(data, buffer + currentOffset, toCopy);
      read = toCopy;
      currentOffset += toCopy;
    }
    while (read < length) {
      size_t bytesread = co_await client.recvmsg(data + read, length - read);
      if (bytesread <= 0) co_return read;
      read += bytesread;
    };
    co_return read;
  }
  struct request {
    std::string method;
    std::string target;
    std::map<std::string, std::string> headers = {};
    request(std::string method, std::string target)
    : method(method)
    , target(target)
    {
      headers["Connection"] = "keep-alive";
    }
  };
  future<response> sendRequest(request req) {
    state = InUse;
    std::string accum = req.method + " " + req.target + " HTTP/1.1\r\n";
    for (auto& [key, value] : req.headers) {
      accum += key + ": " + value + "\r\n";
    }
    accum += "\r\n";
    std::span<const uint8_t> send_buffer((const uint8_t*)accum.data(), (const uint8_t*)accum.data() + accum.size());
    co_await client.sendmsg(send_buffer);

    void* endptr = nullptr;
    do {
      size_t bytesread = co_await client.recvmsg(buffer + bufferSize, sizeof(buffer) - bufferSize);
      bufferSize += bytesread;
      endptr = memmem(buffer, bufferSize, "\r\n\r\n", 4);
    } while (endptr == nullptr);
    currentOffset = (const uint8_t*)endptr - buffer;
    co_return response(*this, endptr);
  }
};

using plain_http_client = http_client<tcp_socket>;
//using secure_http_client = http_client<tls<tcp_socket>>;
