#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "udp_socket.h"

uint64_t read64be(uint8_t* p) {
  uint64_t v = 0;
  for (size_t n = 0; n < 8; n++) {
    v = (v << 8) | *p++;
  }
  return v;
}

uint32_t read32be(uint8_t* p) {
  uint32_t v = 0;
  for (size_t n = 0; n < 4; n++) {
    v = (v << 8) | *p++;
  }
  return v;
}

uint64_t parseReply(std::vector<uint8_t>& message, uint64_t sendTime, uint64_t currentTime) {
  if (message.size() < 48) return 0;
  uint64_t originT = read64be(message.data() + 24);
  if (originT != sendTime) return 0; // invalid message
  uint64_t recT = ((uint64_t)read32be(message.data() + 32) * 1000000) + (((uint64_t)read32be(message.data() + 36) * 1000000) >> 32) - 2208988800000000ULL;
  uint64_t sendT = ((uint64_t)read32be(message.data() + 40) * 1000000) + (((uint64_t)read32be(message.data() + 44) * 1000000) >> 32) - 2208988800000000ULL;
  return ((recT + sendT) - (sendTime + currentTime)) / 2;
}

std::vector<uint8_t> createSntpRequest(uint64_t currentTime) {
  std::vector<uint8_t> message;
  message.push_back(0x23); // SNTP v4, client request
  message.push_back(0); // stratum
  message.push_back(0); // poll interval
  message.push_back(0); // precision
  message.push_back(0); // root delay
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);

  message.push_back(0); // root dispersion
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);

  message.push_back(0); // Reference ID
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  
  message.push_back(0); // reference timestamp
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);

  message.push_back(0); // origin timestamp
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);

  message.push_back(0); // receive timestamp
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);
  message.push_back(0);

  message.push_back((currentTime >> 56) & 0xFF); // transmit timestamp
  message.push_back((currentTime >> 48) & 0xFF);
  message.push_back((currentTime >> 40) & 0xFF);
  message.push_back((currentTime >> 32) & 0xFF);
  message.push_back((currentTime >> 24) & 0xFF);
  message.push_back((currentTime >> 16) & 0xFF);
  message.push_back((currentTime >> 8) & 0xFF);
  message.push_back((currentTime) & 0xFF);
  return message;
}

static network_address defaultNtp("149.210.230.59:123");

future<uint64_t> currentTimeNtp(network_address ntpServer = defaultNtp) {
  udp_socket ntp_client = udp_socket();
  co_await ntp_client.sendmsg(ntpServer, createSntpRequest(42));
  while (true) {
    auto pair = co_await ntp_client.recvmsg(576);
    auto [target, message] = pair;
    if (to_string(target) == to_string(ntpServer)) {
      co_return parseReply(message, 42, 42);
    }
  }
}

