#pragma once

#include <experimental/coroutine>
#include <sys/types.h>
#include <sys/socket.h>
#include <liburing.h>
#include <cstdio>

struct syscall_rv_base {
    std::experimental::coroutine_handle<> awaiting = {};
    int32_t rv = -1;
    bool done = false;
    void signal(int32_t value) {
        this->rv = value;
        if (awaiting) 
          std::move(awaiting).resume();
        else 
          done = true;
    }
};

template <typename T>
struct syscall_rv : public syscall_rv_base {
    syscall_rv(int32_t value) {
        signal(value);
    }
    syscall_rv(io_uring_sqe* s) {
        s->user_data = (uintptr_t)(syscall_rv_base*)this;
    }
    syscall_rv(syscall_rv<T>&& o) = delete; // ?
    const syscall_rv& operator=(syscall_rv<T>&& o) = delete; // ?
    ~syscall_rv() {
    }
    bool await_ready() {
        return done;
    }
    void await_suspend(std::experimental::coroutine_handle<> awaiting) {
        this->awaiting = awaiting;
    }
    auto await_resume() {
        return (T)std::move(rv);
    }
};

struct kernel_ring {
  kernel_ring() {
    if (io_uring_queue_init(8, &ring, 0) < 0) throw 42;
  }
  io_uring_sqe* get_sqe() {
    outstanding_requests++;
    return io_uring_get_sqe(&ring);
  }
  void run() {
    while (outstanding_requests) {
      io_uring_submit_and_wait(&ring, 1);
      io_uring_cqe* cqe;
      while (outstanding_requests && io_uring_peek_cqe(&ring, &cqe) == 0) {
        outstanding_requests--;
        syscall_rv_base* b = (syscall_rv_base*)cqe->user_data;
        b->signal(cqe->res);
        io_uring_cqe_seen(&ring,cqe);
      }
    }
  }
  ~kernel_ring() {
    io_uring_queue_exit(&ring);
  }
  void handle_cqe(io_uring_cqe* c) {
    syscall_rv_base* base = (syscall_rv_base*)c->user_data;
    base->signal(c->res);
  }
  struct io_uring ring;
  size_t outstanding_requests = 0;
};

inline kernel_ring& get_ring() {
  thread_local kernel_ring ring;
  return ring;
}

inline syscall_rv<ssize_t> async_readv(int fd, const struct iovec *iov, unsigned int iovcnt, off_t offset) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_readv(s, fd, iov, iovcnt, offset);
  return s;
}

inline syscall_rv<ssize_t> async_writev(int fd, const struct iovec *iov, unsigned int iovcnt, off_t offset) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_writev(s, fd, iov, iovcnt, offset);
  return s;
}

inline syscall_rv<ssize_t> async_read(int fd, void* buf, unsigned int bytes, off_t offset) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_read(s, fd, buf, bytes, offset);
  return s;
}

inline syscall_rv<ssize_t> async_write(int fd, void* buf, unsigned int bytes, off_t offset) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_write(s, fd, buf, bytes, offset);
  return s;
}

inline syscall_rv<int> async_fsync(int fd, int flags) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_fsync(s, fd, flags);
  return s;
}

inline syscall_rv<ssize_t> async_read_fixed(int fd, void *buf, size_t count, off_t offset, int buf_index) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_read_fixed(s, fd, buf, count, offset, buf_index);
  return s;
}

inline syscall_rv<ssize_t> async_write_fixed(int fd, const void *buf, size_t count, off_t offset, int buf_index) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_write_fixed(s, fd, buf, count, offset, buf_index);
  return s;
}

inline syscall_rv<ssize_t> async_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_sendmsg(s, sockfd, msg, flags);
  return s;
}

inline syscall_rv<ssize_t> async_recvmsg(int sockfd, struct msghdr *msg, int flags) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_recvmsg(s, sockfd, msg, flags);
  return s;
}

inline syscall_rv<int> async_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_accept(s, sockfd, addr, addrlen, flags);
  return s;
}

inline syscall_rv<int> async_connect(int sockfd, struct sockaddr* addr, socklen_t addrlen) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_connect(s, sockfd, addr, addrlen);
  return s;
}

inline syscall_rv<int> async_fallocate(int fd, int mode, off_t offset, off_t len) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_fallocate(s, fd, mode, offset, len);
  return s;
}

inline syscall_rv<int> async_openat(int dirfd, const char *pathname, int flags, mode_t mode) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_openat(s, dirfd, pathname, flags, mode);
  return s;
}

inline syscall_rv<int> async_close(int fd) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_close(s, fd);
  return s;
}

inline syscall_rv<int> async_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_statx(s, dirfd, pathname, flags, mask, statxbuf);
  return s;
}

inline syscall_rv<long> async_fadvise(int fs, loff_t offset, loff_t len, int advice) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_fadvise(s, fs, offset, len, advice);
  return s;
}

inline syscall_rv<int> async_madvise(void *addr, size_t length, int advice) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_madvise(s, addr, length, advice);
  return s;
}

inline syscall_rv<ssize_t> async_send(int sockfd, const void *buf, size_t len, int flags) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_send(s, sockfd, buf, len, flags);
  return s;
}

inline syscall_rv<ssize_t> async_recv(int sockfd, void *buf, size_t len, int flags) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_recv(s, sockfd, buf, len, flags);
  return s;
}

inline syscall_rv<int> async_openat2(int dfd, const char *pathname, struct open_how *how) {
  io_uring_sqe* s = get_ring().get_sqe();
  io_uring_prep_openat2(s, dfd, pathname, how);
  return s;
}

// POLL_ADD
// POLL_REMOVE
// SYNC_FILE_RANGE
// TIMEOUT
// TIMEOUT_REMOVE
// ASYNC_CANCEL
// LINK_TIMEOUT
// FILES_UPDATE
// ??? READ
// ??? WRITE
// EPOLL_CTL

inline void async_run() {
  get_ring().run();
}


