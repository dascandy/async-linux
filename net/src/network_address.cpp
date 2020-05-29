#include "network_address.h"

void network_address::parse_ipv4(sockaddr_in* in, const char* str) {
  uint32_t value = 0;
  uint32_t ipaddr = 0;
  int index = 0;
  while (*str) {
    if (*str == '.' || *str == ':') {
      ipaddr |= (value << ((3-index) * 8));
      value = 0;
      index++;
    } else {
      value = value * 10 + (*str) - '0';
    }
    str++;
  }
  if (index == 4)
    in->sin_port = htons(value);
  else {
    ipaddr |= (value << ((3-index) * 8));
    in->sin_port = 0;
  }
  in->sin_family = AF_INET;
  in->sin_addr.s_addr = htonl(ipaddr);
}

void network_address::parse_ipv6_noport(sockaddr_in6* in, const char*& str) {
  uint16_t front[8];
  uint16_t back[8];
  int f_i = 0, b_i = 0;
  uint16_t accum = 0;
  bool atBack = false;
  while (*str != ']' && *str != '\0') {
    switch(*str) {
      case ':': 
        if (atBack) {
          back[b_i] = accum;
          b_i++;
        } else {
          front[f_i] = accum; 
          f_i++;
        }
        if (str[1] == ':') atBack = true;
        accum = 0;
        break;
      default:
        accum = (accum << 4) + *str - '0';
        if (*str >= 'A') accum -= 7;
        if (*str >= 'a') accum -= 32;
        break;
    }
    ++str;
  }
  if (atBack) {
    back[b_i] = accum;
    b_i++;
  } else {
    front[f_i] = accum; 
    f_i++;
  }
  int index = 0;
  for (; index < f_i; ++index) {
    in->sin6_addr.s6_addr[index*2] = (front[index] & 0xFF00) >> 8;
    in->sin6_addr.s6_addr[index*2+1] = (front[index] & 0xFF);
  }
  for (; index < (8 - b_i); ++index) {
    in->sin6_addr.s6_addr[index*2] = 0;
    in->sin6_addr.s6_addr[index*2+1] = 0;
  }
  for (; index < 8; ++index) {
    in->sin6_addr.s6_addr[index*2] = (back[index + b_i - 8] & 0xFF00) >> 8;
    in->sin6_addr.s6_addr[index*2+1] = (back[index + b_i - 8] & 0xFF);
  }
}

void network_address::parse_ipv6(sockaddr_in6* in, const char* str) {
  if (*str == '[') {
    str++;
    parse_ipv6_noport(in, str);
    str++;
    str++;
    uint16_t port = 0;
    while (*str) {
      port = (port * 10) + *str - '0';
      ++str;
    }
    in->sin6_port = htons(port);
  } else {
    parse_ipv6_noport(in, str);
    in->sin6_port = 0;
  }
  in->sin6_family = AF_INET6;
}

void network_address::parse_ipv6_wrapped_ipv4(sockaddr_in6* in, const char* str) {
  if (*str == '[') str++;
  str += 7;
  uint32_t value = 0;
  uint32_t ipaddr = 0;
  int index = 0;
  while (*str && *str != ']') {
    if (*str == '.') {
      ipaddr |= (value << ((3-index) * 8));
      value = 0;
      index++;
    } else {
      value = value * 10 + (*str) - '0';
    }
    str++;
  }
  ipaddr |= (value << ((3-index) * 8));
  if (*str == ']') {
    str++;
    str++;
    uint16_t port = 0;
    while (*str) {
      port = (port * 10) + *str - '0';
      ++str;
    }
    in->sin6_port = htons(port);
  } else {
    in->sin6_port = 0;
  }
  for (size_t n = 0; n < 16; n++) {
    if (n < 12) {
      in->sin6_addr.s6_addr[n] = (n < 10 ? 0 : 255);
    } else {
      in->sin6_addr.s6_addr[n] = (ipaddr >> ((15-n) * 8)) & 0xFF;
    }
  }
  in->sin6_family = AF_INET6;
}


