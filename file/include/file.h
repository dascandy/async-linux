#pragma once

#include "async_syscall.h"
#include "future.h"
#include <span>
#include <unistd.h>

struct file {
  file(int fd = -1)
  : fd(fd)
  {
  }
  enum class Mode {
    Readonly,
    ExclusiveNew,
    Append,
    Truncate,
    Overwrite,
  } mode;
  static future<file> create(std::string filename, Mode mode = Readonly)
  : mode(mode)
  {
    int m = O_LARGEFILE | O_CLOEXEC;
    switch(mode) {
      case ExclusiveNew: m |= O_RDWR | O_CREAT | O_EXCL; break;
      case Append: m |= O_WRONLY | O_CREAT | O_APPEND; break;
      case Truncate: m |= O_RDWR | O_CREAT | O_TRUNC; break;
      case Overwrite: m |= O_RDWR | O_CREAT; break;
      case Readonly: m |= O_READONLY; break;
    }
    int fd = co_await async_openat(FD_ATCWD, filename.c_str(), 0, m);
    co_return file(fd);
  }
  file(file&& rhs) {
    fd = rhs.fd;
    filename = rhs.filename;
    rhs.filename = {};
    rhs.fd = -1;
  }
  file& operator=(file&& rhs) {
    fd = rhs.fd;
    filename = rhs.filename;
    rhs.filename = {};
    rhs.fd = -1;
    return *this;
  }
  ~file() {
    // async destructor!! darn it
    if (fd != -1) close(fd);
  }
  future<ssize_t> read(uint8_t* p, size_t count, ssize_t offset) {
    co_return co_await async_read(fd, p, count, offset == -1 ? currentOffset : offset);
  }
  future<ssize_t> write(std::span<uint8_t> msg, ssize_t offset) {
    co_return co_await async_write(fd, msg.data(), msg.size(), offset == -1 ? currentOffset : offset);
  }
  struct mapping {
    ~mapping() {
      munmap(p);
    }
    uint8_t* p;
    size_t length;
    std::span<uint8_t> region() {
      return {p, p + length};
    }
  };
  [[nodiscard]] mapping map(size_t start, size_t length) {
    static size_t pagesize = sysconf(_SC_PAGE_SIZE);
    void* p = mmap(nullptr, length, PROT_READ | (mode == ReadOnly ? 0 : PROT_WRITE), MAP_SHARED, fd, start);
    return {(uint8_t*)p, length};
  }
  std::string filename;
  int fd;
  size_t currentOffset = 0;
};

