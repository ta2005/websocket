#ifndef RING_BUFFER_CPP
#define RING_BUFFER_CPP

#include "common/error.hpp"
#include <expected>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <tuple>
#include <unistd.h>

namespace ws::detail {
class RingBuffer {
    uint8_t *buffer = nullptr;
    size_t   buffer_size = 0;
    int      fd = -1;
    size_t   head = 0;
    size_t   tail = 0;

  public:
    RingBuffer() = default;
    ~RingBuffer() {
        if (buffer) {
            munmap(buffer, buffer_size);
            munmap(buffer + buffer_size, buffer_size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    RingBuffer(RingBuffer&& other) noexcept 
        : buffer(other.buffer), buffer_size(other.buffer_size), fd(other.fd), head(other.head), tail(other.tail) {
        other.buffer = nullptr;
        other.fd = -1;
    }

    RingBuffer& operator=(RingBuffer&& other) noexcept {
        if (this != &other) {
            this->~RingBuffer();
            buffer = other.buffer;
            buffer_size = other.buffer_size;
            fd = other.fd;
            head = other.head;
            tail = other.tail;
            other.buffer = nullptr;
            other.fd = -1;
        }
        return *this;
    }

  private:
    RingBuffer(size_t size) {
        fd = memfd_create("queue_buffer", 0);
        ftruncate(fd, size);
        buffer_size = size;
        // we ask for two virtual buffers
        buffer = (uint8_t *)mmap(NULL, size * 2, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        // we set both for buffer and buffer + size to
        // point to the same physical struct
        std::ignore = mmap(buffer, size, PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_FIXED, fd, 0);
        std::ignore = mmap(buffer + size, size, PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_FIXED, fd, 0);
    }

  public:
    static std::expected<RingBuffer, Error> create(size_t size) {
        if (size % getpagesize() != 0) {
            return std::unexpected(Error::InvalidSize);
        }
        return RingBuffer(size);
    }
    bool put(uint8_t *data, size_t size) {
        if (writable_bytes() < size) {
            return false;
        }
        memcpy(&buffer[tail], data, size);
        tail += size;
        return true;
    }

    bool get(uint8_t *data, size_t size) {
        if (readable_bytes() < size) {
            return false;
        }
        memcpy(data, &buffer[head], size);
        advance_head(size);
        return true;
    }

    // Direct memory access for zero-copy I/O
    uint8_t *write_ptr() { return &buffer[tail]; }
    size_t writable_bytes() const { return buffer_size - (tail - head); }
    void advance_tail(size_t size) { tail += size; }

    uint8_t *read_ptr() { return &buffer[head]; }
    size_t readable_bytes() const { return tail - head; }
    void advance_head(size_t size) {
        head += size;
        if (head >= buffer_size) { // wrap around once head completes a full cycle
            head -= buffer_size;
            tail -= buffer_size;
        }
    }
};
} // namespace ws::detail
#endif
