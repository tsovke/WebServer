#include "buffer.h"

Buffer::Buffer(int initBuffSize)
    : buffer_(initBuffSize), readPos(0), writePos(0) {}

size_t Buffer::WritableBytes() const { return buffer_.size() - writePos; }

size_t Buffer::ReadableBytes() const { return writePos - readPos; }

size_t Buffer::PrependableBytes() const { return readPos; }

const char *Buffer::Peek() const { return &buffer_[readPos]; }

void Buffer::EnsureWritable(size_t len) {
  if (len > WritableBytes())
    MakeSpace_(len);
  assert(len <= WritableBytes());
}

void Buffer::HasWritten(size_t len) { writePos += len; }

void Buffer::Retrieve(size_t len) { readPos += len; }

void Buffer::RetrieveUntil(const char *end) {
  assert(Peek() <= end);
  Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
  std::memset(reinterpret_cast<void *>(&buffer_), '\0', buffer_.size());
  readPos = writePos = 0;
}

std::string Buffer::RetrieveAllToStr() {
  std::string str(Peek(), ReadableBytes());
  RetrieveAll();
  return str;
}

const char *Buffer::BeginWriteConst() const { return &buffer_[writePos]; }

char *Buffer::BeginWrite() { return &buffer_[writePos]; }

void Buffer::Append(const char *str, size_t len) {
  assert(str);
  EnsureWritable(len);
  std::copy(str, str + len, BeginWrite());
  HasWritten(len);
}
void Buffer::Append(const std::string &str) { Append(str.c_str(), str.size()); }

void Buffer::Append(const void *data, size_t len) {
  Append(static_cast<const char *>(data), len);
}

void Buffer::Append(const Buffer &buff) {
  Append(buff.Peek(), buff.ReadableBytes());
}

// 将fd的内容读到缓冲区
ssize_t Buffer::ReadFd(int fd, int *Errno) {
  char buff[65535];
  struct iovec iov[2];
  size_t writable = WritableBytes();
  // 分散读
  iov[0].iov_base = BeginWrite();
  iov[0].iov_len = writable;
  iov[1].iov_base = buff;
  iov[1].iov_len = sizeof(buff);
  ssize_t len = readv(fd, iov, 2);
  if (len < 0) {
    *Errno = errno;
  } else if (static_cast<size_t>(len) <= writable) {
    writePos += len;
  } else {
    writePos = buffer_.size();
    Append(buff, static_cast<size_t>(len - writable));
  }
  return len;
}

ssize_t Buffer::WriteFd(int fd, int *Errno) {
  ssize_t len = write(fd, Peek(), ReadableBytes());
  if (len < 0) {
    *Errno = errno;
    return len;
  }
  Retrieve(len);
  return len;
}

char *Buffer::BeginPtr_() { return &buffer_[0]; }

const char *Buffer::BeginPtr_() const { return &buffer_[0]; }

void Buffer::MakeSpace_(size_t len) {
  if (WritableBytes() + PrependableBytes() < len) {
    buffer_.resize(writePos + len + 1);
  } else {
    size_t readable = ReadableBytes();
    std::copy(BeginPtr_() + readPos, BeginPtr_() + writePos, BeginPtr_());
    readPos = 0;
    writePos = readable;
    assert(readable == ReadableBytes());
  }
}
