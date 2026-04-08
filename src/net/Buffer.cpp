#include <sys/uio.h>      // readv/writev
#include <unistd.h>       // read/write
#include <cstring>        // memcpy, memmem
#include <algorithm>      // std::copy
#include <cerrno>         // errno
#include <cassert>
#include "net/Buffer.h"

Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readIndex_(kCheapPrepend),
      writeIndex_(kCheapPrepend) {
    assert(readableBytes() == 0);
    assert(writableBytes() == initialSize);
    assert(prependableBytes() == kCheapPrepend);

}

void Buffer::retrieve(size_t len) {
    assert(len <= readableBytes());
    if (len < readableBytes()) {
        readIndex_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    readIndex_ = kCheapPrepend;
    writeIndex_ = kCheapPrepend;
}

std::string Buffer::retrieveAsString(size_t len) {
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
}

void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    writeIndex_ += len;
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.size());
}

void Buffer::prepend(const char* data, size_t len) {
    assert(len <= prependableBytes());
    readIndex_ -= len;
    std::copy(data, data + len, begin() + readIndex_);
}

void Buffer::prepend(const std::string& str) {
    prepend(str.data(), str.size());
}

size_t Buffer::findCRLF() const {
    const char* crlf = static_cast<const char*>(
        memmem(peek(), readableBytes(), "\r\n", 2));
    if (crlf) {
        return crlf - peek();
    }
    return std::string::npos;
}

size_t Buffer::findCRLF(size_t start) const {
    assert(start <= readableBytes());
    const char* startPos = peek() + start;
    const char* crlf = static_cast<const char*>(
        memmem(startPos, readableBytes() - start, "\r\n", 2));
    if (crlf) {
        return crlf - peek();
    }
    return std::string::npos;
}

ssize_t Buffer::readFd(int fd, int* savedErrno) {
    // 临时扩展缓冲区：若已有可写空间足够大，一次读入；否则用栈上辅助空间
    char extrabuf[65536];
    struct iovec vec[2];
    size_t writable = writableBytes();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;
        return -1;
    }
    if (static_cast<size_t>(n) <= writable) {
        writeIndex_ += n;
    } else {
        writeIndex_ = buffer_.size();
        // 超出部分在 extrabuf 中，需要追加
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno) {
    size_t readable = readableBytes();
    if (readable == 0) {
        return 0;
    }
    struct iovec vec;
    vec.iov_base = begin() + readIndex_;
    vec.iov_len = readable;
    ssize_t n = ::writev(fd, &vec, 1);
    if (n < 0) {
        *savedErrno = errno;
        return -1;
    }
    if (static_cast<size_t>(n) < readable) {
        // 只发送了一部分，移动 readIndex_ 丢弃已发送数据
        retrieve(n);
    } else {
        retrieveAll();
    }
    return n;
}

void Buffer::shrink(size_t reserve) {
    size_t readable = readableBytes();

    // 计算新缓冲区大小（刚好够用 + 一点预留）
    size_t newCapacity = kCheapPrepend + readable + reserve;
    if (newCapacity >= buffer_.size()) {
        // 当前容量已经足够小，无需收缩
        return;
    }
    
    std::vector<char> newBuffer(newCapacity);
    assert(newBuffer.size() == newCapacity);
    std::copy(begin() + readIndex_, begin() + writeIndex_,
              newBuffer.begin() + kCheapPrepend);
    buffer_.swap(newBuffer);
    readIndex_ = kCheapPrepend;
    writeIndex_ = readIndex_ + readable;
}

void Buffer::makeSpace(size_t len) {
    // 情况1：当前可写区域 + prepend 区域总和足够，则移动可读数据到前面
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        // 扩容
        buffer_.resize(writeIndex_ + len);
    } else {
        // 移动可读数据到缓冲区头部（紧跟在 kCheapPrepend 之后）
        size_t readable = readableBytes();
        std::copy(begin() + readIndex_, begin() + writeIndex_,
                  begin() + kCheapPrepend);
        readIndex_ = kCheapPrepend;
        writeIndex_ = readIndex_ + readable;
        assert(readable == readableBytes());
    }
}