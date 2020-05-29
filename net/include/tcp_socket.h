#pragma once

#include "async_syscall.h"
#include "future.h"
#include "network_address.h"
#include <span>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

struct tcp_socket {
  friend struct tcp_listen_socket;
  tcp_socket()
  : fd(-1)
  {}
  tcp_socket(network_address target, int fd) 
  : target(target)
  , fd(fd)
  {
    printf("tcp open %d\n", fd);
  }
  static future<tcp_socket> create(network_address target)
  {
    co_return tcp_socket(target);
  }
  tcp_socket(network_address target) 
  : target(target)
  {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    connect(fd, target.sockaddr(), target.length());
  }
  tcp_socket(tcp_socket&& rhs) {
    fd = rhs.fd;
    target = rhs.target;
    rhs.target = {};
    rhs.fd = -1;
  }
  tcp_socket& operator=(tcp_socket&& rhs) {
    fd = rhs.fd;
    target = rhs.target;
    rhs.target = {};
    rhs.fd = -1;
    return *this;
  }
  ~tcp_socket() {
    close(fd);
  }
  future<size_t> recvmsg(uint8_t* p, size_t count) {
    struct iovec iov = { p, count };
    struct msghdr hdr;
    hdr.msg_name = nullptr;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
    ssize_t recvres = co_await async_recvmsg(fd, &hdr, 0);
    if (recvres == -1) {
      exit(-1);
    } else {
      co_return recvres;
    }
  }
  future<void> sendmsg(std::span<const uint8_t> msg) {
    printf("tcp send %d\n", fd);
    struct iovec iov = { (void*)msg.data(), msg.size() };
    struct msghdr hdr;
    hdr.msg_name = nullptr;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
    ssize_t res = co_await async_sendmsg(fd, &hdr, 0);
    if (res == -1) {
      printf("async_sendmsg error");
      exit(-1);
    }
    co_return;
  }
  network_address target;
  int fd;
};

struct tcp_listen_socket {
  tcp_listen_socket(network_address listen_address, std::function<void(tcp_socket)> onConnect) {
    // TODO: handle errors
    fd = socket(AF_INET, SOCK_STREAM, 0);
    bind(fd, listen_address.sockaddr(), listen_address.length());
    listen(fd, 5);
    acceptLoopF = acceptLoop(std::move(onConnect));
  }
  future<void> acceptLoop(std::function<void(tcp_socket)> onConnect) {
    while (not done) {
      network_address addr;
      addr.resize(sizeof(sockaddr_in6));
      int newFd = co_await async_accept(fd, addr.sockaddr(), &addr.length(), 0);
//      if (error)  TODO
      onConnect(tcp_socket(addr, newFd));
    }
  }
  ~tcp_listen_socket() {
    done = true;
    // how to interrupt loop?
    acceptLoopF.get_value();
    close(fd);
  }
  std::atomic<bool> done{false};
  int fd;
  future<void> acceptLoopF;
};

