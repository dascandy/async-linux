#pragma once

#include <cstdio>
#include "async_syscall.h"
#include "future.h"
#include <algorithm>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>

struct network_address {
  static constexpr size_t buffersize = 32;
  static void parse_ipv4(sockaddr_in* in, const char* str);
  static void parse_ipv6(sockaddr_in6* in, const char* str);
  static void parse_ipv6_noport(sockaddr_in6* in, const char*& str);
  static void parse_ipv6_wrapped_ipv4(sockaddr_in6* in, const char* str);

  network_address() noexcept
  : namelen(0)
  {}
  network_address(const std::string& str) noexcept
  {
    // TODO: check if this is an OK check to do like this
    if (str.find("::ffff:") != std::string::npos) {
      // ipv6-wrapped ipv4 address
      namelen = sizeof(struct sockaddr_in6);
      struct sockaddr_in6* a = (struct sockaddr_in6*)sockaddr();
      parse_ipv6_wrapped_ipv4(a, str.c_str());
    } else if (str.find(".") != std::string::npos) {
      namelen = sizeof(struct sockaddr_in);
      struct sockaddr_in* a = (struct sockaddr_in*)sockaddr();
      parse_ipv4(a, str.c_str());
    } else {
      namelen = sizeof(struct sockaddr_in6);
      struct sockaddr_in6* a = (struct sockaddr_in6*)sockaddr();
      parse_ipv6(a, str.c_str());
    }
  }
  network_address(const struct sockaddr* addr, socklen_t length) noexcept
  : namelen(length)
  {
    if (namelen > buffersize) {
      address.remote = reinterpret_cast<char*>(malloc(namelen));
    } else {
      memcpy(address.local, addr, namelen);
    }
  }
  ~network_address() {
    if (namelen > buffersize) 
      free(address.remote);
  }
  network_address(const network_address& addr) noexcept
  : network_address(addr.sockaddr(), addr.length())
  { }
  network_address(network_address&& addr) noexcept
  : namelen(addr.namelen)
  {
    if (namelen > buffersize) {
      address.remote = addr.address.remote;
      addr.namelen = 0;
    } else {
      memcpy(address.local, addr.address.local, namelen);
    }
  }
  void resize(size_t size) {
    if (size > namelen) {
      if (namelen > buffersize) {
        address.remote = reinterpret_cast<char*>(realloc(address.remote, size));
      } else {
        address.remote = reinterpret_cast<char*>(malloc(size));
      }
    } else if (size <= buffersize) {
      free(address.remote);
    }
    namelen = size;
  }
  const network_address& operator=(const network_address& rhs) noexcept {
    resize(rhs.namelen);
    if (namelen > buffersize) {
      memcpy(address.remote, rhs.address.remote, namelen);
    } else {
      memcpy(address.local, rhs.address.local, namelen);
    }
    return *this;
  }
  const network_address& operator=(network_address&& rhs) noexcept {
    if (namelen > buffersize) {
      free(address.remote);
      address.remote = rhs.address.remote;
      namelen = rhs.namelen;
    } else {
      namelen = rhs.namelen;
      memcpy(address.local, rhs.address.local, namelen);
    }
    return *this;
  }

  union {
    char local[buffersize];
    char* remote;
  } address;
  socklen_t namelen;
  const struct sockaddr* sockaddr() const noexcept { 
    return reinterpret_cast<const struct sockaddr*>(buffer());
  }
  struct sockaddr* sockaddr() noexcept { 
    return reinterpret_cast<struct sockaddr*>(buffer());
  }
  socklen_t& length() noexcept {
    return namelen;
  }
  socklen_t length() const noexcept {
    return namelen;
  }
  char* buffer() noexcept {
    if (namelen > buffersize) 
      return address.remote; 
    return address.local; 
  };
  const char* buffer() const noexcept {
    if (namelen > buffersize) 
      return address.remote; 
    return address.local; 
  };
  friend std::string to_string(const network_address& addr) {
    static char hextab[] = "0123456789abcdef";
    char buffer[64];
    size_t pos = 63;
    buffer[pos] = 0;
    switch(addr.sockaddr()->sa_family) {
      case AF_INET:
      {
        struct sockaddr_in* a = (struct sockaddr_in*)addr.sockaddr();
        uint32_t addr = ntohl(a->sin_addr.s_addr);
        uint16_t port = ntohs(a->sin_port);
        if (port) {
          do {
            buffer[--pos] = hextab[port % 10];
            port /= 10;
          } while (port);
          buffer[--pos] = ':';
        }
        for (size_t n = 0; n < 4; n++) {
          uint8_t val = (addr >> (n * 8)) & 0xFF;
          do {
            buffer[--pos] = hextab[val % 10];
            val /= 10;
          } while (val);
          if (n != 3) buffer[--pos] = '.';
        }
        return buffer + pos;
      }
      case AF_INET6:
      {
        struct sockaddr_in6* a = (struct sockaddr_in6*)addr.sockaddr();
        uint16_t port = ntohs(a->sin6_port);
        if (port) {
          do {
            buffer[--pos] = hextab[port % 10];
            port /= 10;
          } while (port);
          buffer[--pos] = ':';
          buffer[--pos] = ']';
          port = 1;
        }

        enum {before, during, after} dc = before;
        bool is_4in6 = true;
        for (size_t index = 0; index < 12; index++) {
          if (index < 10 && a->sin6_addr.s6_addr[index] != 0) is_4in6 = false;
          else if ((index == 10 || index == 11) && a->sin6_addr.s6_addr[index] != 0xFF) is_4in6 = false;
        }
        if (is_4in6) {
          for (size_t n = 0; n < 4; n++) {
            uint8_t val = a->sin6_addr.s6_addr[15-n];
            do {
              buffer[--pos] = hextab[val % 10];
              val /= 10;
            } while (val);
            if (n != 3) buffer[--pos] = '.';
          }
          buffer[--pos] = ':';
          buffer[--pos] = 'f';
          buffer[--pos] = 'f';
          buffer[--pos] = 'f';
          buffer[--pos] = 'f';
          buffer[--pos] = ':';
          buffer[--pos] = ':';

        } else {
          for (size_t index = 8; index --> 0;) {
            uint16_t value = (((a->sin6_addr.s6_addr[index*2]) << 8) + (a->sin6_addr.s6_addr[index*2+1]));
            if (value == 0 && dc == before) {
              dc = during;
              buffer[--pos] = ':';
              if (index == 7) buffer[--pos] = ':';
            } 
            if (value != 0 && dc == during) {
              dc = after;
            }
            if (dc == before || dc == after) {
              do {
                buffer[--pos] = hextab[value & 0xF];
                value >>= 4;
              } while (value);
              buffer[--pos] = ':';
            }
          }
          if (dc != during)
            pos++;
        }
        if (port) {
          buffer[--pos] = '[';
        }
        return buffer + pos;
      }
      default:
        return "Unknown sockaddr type";
    }
  }
};


