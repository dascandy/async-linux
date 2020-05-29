#pragma once

#include "async_syscall.h"
#include "future.h"
#include "network_address.h"
#include <vector>
#include <span>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

struct udp_socket {
  udp_socket(uint16_t port = 0) {
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (port) {
      struct sockaddr_in in;
      in.sin_family = AF_INET;
      in.sin_port = htons(port);
      in.sin_addr.s_addr = htonl(INADDR_ANY);
      bind(fd, reinterpret_cast<const struct sockaddr*>(&in), sizeof(in));
    }
  }
  udp_socket(udp_socket&& rhs) {
    fd = rhs.fd;
    rhs.fd = -1;
  }
  udp_socket& operator=(udp_socket&& rhs) {
    fd = rhs.fd;
    rhs.fd = -1;
    return *this;
  }
  ~udp_socket() {
    if (fd != -1)
      close(fd);
  }
  future<std::pair<network_address, std::vector<uint8_t>>> recvmsg(size_t maxSize) {
    char namebuf[128];
    std::vector<unsigned char> msgbuf;
    msgbuf.resize(maxSize);
    struct iovec iov = { msgbuf.data(), msgbuf.size() };
    struct msghdr hdr;
    hdr.msg_name = namebuf;
    hdr.msg_namelen = sizeof(namebuf);
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
    ssize_t recvres = co_await async_recvmsg(fd, &hdr, 0);
    msgbuf.resize(recvres);
    // TODO: workaround for Clang9 bug, temporary network_address storage
    network_address addr((const struct sockaddr*)hdr.msg_name, (socklen_t)hdr.msg_namelen);
    co_return {std::move(addr), std::move(msgbuf)};
  }
  future<void> sendmsg(network_address target, std::span<const uint8_t> msg) {
    struct iovec iov = { (void*)msg.data(), msg.size() };
    struct msghdr hdr;
    hdr.msg_name = target.sockaddr();
    hdr.msg_namelen = target.length();
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
    co_await async_sendmsg(fd, &hdr, 0);
    co_return;
  }
  int fd;
};

