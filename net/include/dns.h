#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "udp_socket.h"

void add_u32_be(std::vector<uint8_t>& vec, uint32_t value) {
  vec.push_back((value >> 24) & 0xFF);
  vec.push_back((value >> 16) & 0xFF);
  vec.push_back((value >> 8) & 0xFF);
  vec.push_back(value & 0xFF);
}

void add_u16_be(std::vector<uint8_t>& vec, uint16_t value) {
  vec.push_back((value >> 8) & 0xFF);
  vec.push_back(value & 0xFF);
}

void add_u8_string(std::vector<uint8_t>& vec, std::string name) {
  vec.push_back(name.size());
  for (auto& c : name) vec.push_back(c);
}

void add_dns_string(std::vector<uint8_t>& vec, std::string name) {
  size_t dot = name.find_first_of(".");
  while (dot != std::string::npos) {
    add_u8_string(vec, name.substr(0, dot));
    name = name.substr(dot+1);
    dot = name.find_first_of(".");
  }
  if (!name.empty()) add_u8_string(vec, name);
  vec.push_back(0);
}

uint32_t get_u32_be(const std::vector<uint8_t>& vec, size_t& offset) {
  uint32_t value = ((uint32_t)vec[offset] << 24) + (vec[offset+1] << 16) + (vec[offset+2] << 8) + (vec[offset+3]);
  offset += 4;
  return value;
}

uint16_t get_u16_be(const std::vector<uint8_t>& vec, size_t& offset) {
  uint16_t value = (uint16_t)((vec[offset] << 8) + (vec[offset+1]));
  offset += 2;
  return value;
}

std::string get_dns_string(std::vector<uint8_t>& vec, size_t& offset) {
  std::string rv;
  uint8_t val = vec[offset];

  while (val != 0) {
    if (val >= 0xC0) {
      size_t curOffs = (vec[offset] << 8) + (vec[offset+1]) - 0xC000;
      offset += 2;
      return rv + get_dns_string(vec, curOffs);
    }
    offset++;
    rv += std::string(vec.data() + offset, vec.data() + offset + val) + ".";
    offset += val;
    val = vec[offset];
  }
  offset++;
  rv.pop_back();
  return rv;
}

enum class DnsRecordType : uint16_t {
  A = 0x01,
  AAAA = 0x1C,
};

std::vector<uint8_t> createDnsRequest(std::string name, uint16_t requestId, DnsRecordType recordType) {
  std::vector<uint8_t> req;
  add_u16_be(req, requestId);
  add_u16_be(req, 0x0100); // rd=1, rest dunno
  add_u16_be(req, 1);
  add_u16_be(req, 0);
  add_u16_be(req, 0);
  add_u16_be(req, 1);

  // Query
  add_dns_string(req, name);
  add_u16_be(req, (uint16_t)recordType); // IPv6 AAAA record
  add_u16_be(req, 1);

  // Additional RR (OPT)
  add_dns_string(req, "");
  add_u16_be(req, 41);
  add_u16_be(req, 0x0200);
  add_u32_be(req, 0);
  add_u16_be(req, 0);

  return req;
}

inline void parseReply(std::vector<uint8_t>& message, uint16_t port, std::vector<network_address>& addresses) {
  uint8_t answers = message[7];
  if (!answers) return;
  size_t offset = 12;
  get_dns_string(message, offset);
  get_u16_be(message, offset);
  get_u16_be(message, offset);
  get_dns_string(message, offset);
  DnsRecordType type = (DnsRecordType)get_u16_be(message, offset);

  get_u16_be(message, offset);
  get_u32_be(message, offset);
  size_t dataLength = get_u16_be(message, offset);
  if (type == DnsRecordType::A && dataLength == 4) {
    struct sockaddr_in addr;
    memcpy(&addr.sin_addr.s_addr, message.data() + offset, dataLength);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addresses.push_back(network_address((struct sockaddr*)&addr, sizeof(addr)));
  } else if (type == DnsRecordType::AAAA && dataLength == 16) {
    struct sockaddr_in6 addr;
    memcpy(&addr.sin6_addr.s6_addr, message.data() + offset, dataLength);
    addr.sin6_port = htons(port);
    addr.sin6_family = AF_INET6;
    addresses.push_back(network_address((struct sockaddr*)&addr, sizeof(addr)));
  } else {
    fprintf(stderr, "found unknown record\n");
  }
}

inline future<std::vector<network_address>> resolve(std::string name, uint16_t port, udp_socket dns_client = udp_socket()) {
  static network_address dns_server("8.8.8.8:53");
  std::vector<network_address> addresses;
  auto v4req = createDnsRequest(name, 0x4241, DnsRecordType::A);
  auto v6req = createDnsRequest(name, 0x4242, DnsRecordType::AAAA);
  auto v4 = dns_client.sendmsg(dns_server, v4req);
  co_await dns_client.sendmsg(dns_server, v6req);
  co_await v4;
  int answers = 0;
  while (answers < 2) {
    auto pair = co_await dns_client.recvmsg(576);
    auto [target, message] = pair;
    if (to_string(target) == to_string(dns_server)) {
      parseReply(message, port, addresses);
      answers++;
    }
  }
  co_return addresses;
}

