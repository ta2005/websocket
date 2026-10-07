#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <tuple>
#include <unistd.h>

class RingBuffer {
    uint8_t *buffer      = nullptr;
    size_t   buffer_size = 0;
    int      fd          = -1;
    size_t   head        = 0;
    size_t   tail        = 0;

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

    RingBuffer(const RingBuffer &)            = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    RingBuffer(RingBuffer &&other) noexcept
        : buffer(other.buffer), buffer_size(other.buffer_size), fd(other.fd),
          head(other.head), tail(other.tail) {
        other.buffer = nullptr;
        other.fd     = -1;
    }

    RingBuffer &operator=(RingBuffer &&other) noexcept {
        if (this != &other) {
            this->~RingBuffer();
            buffer       = other.buffer;
            buffer_size  = other.buffer_size;
            fd           = other.fd;
            head         = other.head;
            tail         = other.tail;
            other.buffer = nullptr;
            other.fd     = -1;
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
    auto create(size_t size) { return RingBuffer(size); }
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
    size_t   writable_bytes() const { return buffer_size - (tail - head); }
    void     advance_tail(size_t size) { tail += size; }

    uint8_t *read_ptr() { return &buffer[head]; }
    size_t   readable_bytes() const { return tail - head; }
    void     advance_head(size_t size) {
        head += size;
        if (head >=
            buffer_size) { // wrap around once head completes a full cycle
            head -= buffer_size;
            tail -= buffer_size;
        }
    }
};

#include <cstdint>
#include <span>
#include <memory>
// Forward declare EventLoop, or include it.
// If we include it, we need to make sure there's no circular dependency.
// But SocketCtx needs to be known, so let's forward declare it instead.
    struct SocketCtx;


class EventLoop;
struct ReadAwaitable;
struct WaitWritable;
struct AcceptAwaitable;

class TcpSocket {
  private:
    int                                m_fd;
    EventLoop                         &m_loop;
    std::unique_ptr<SocketCtx>         m_ctx;
    RingBuffer             m_read_buffer;

  public:
    TcpSocket(int fd, EventLoop &loop);
    ~TcpSocket();

    // Disable copy
    TcpSocket(const TcpSocket &)            = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    // Enable move
    TcpSocket(TcpSocket &&other) noexcept;
    TcpSocket &operator=(TcpSocket &&other) noexcept;

    auto get_fd() const { return m_fd; }
    SocketCtx *get_ctx() const { return m_ctx.get(); }
    RingBuffer &get_buffer() { return m_read_buffer; }
    EventLoop &get_loop() { return m_loop; }

    void close();

    ReadAwaitable read(size_t n_bytes);
    WaitWritable  write(std::span<const uint8_t> meta,
                        std::span<const uint8_t> payload);
    AcceptAwaitable accept();
};

#include <coroutine>
#include <expected>
#include <sys/epoll.h>
#include <vector>
using Handle = std::coroutine_handle<>;

struct SocketCtx {
    int fd;
    Handle read_handle;
    Handle write_handle;
};

class EventLoop {
  public:
    EventLoop(EventLoop &)            = delete;
    EventLoop &operator=(EventLoop &) = delete;
    EventLoop(EventLoop &&);
    ~EventLoop();
    EventLoop                             &operator=(EventLoop &&);
    EventLoop create();

    void run();
    auto get_fd() { return m_epollfd; }

    void register_socket(int fd, SocketCtx *ctx);
    void                       unregister_socket(int fd);

  private:
    EventLoop(int fd);
    int m_epollfd;
};


#include <cerrno>
#include <cstdint>
#include <errno.h>
#include <expected>
#include <span>
#include <sys/epoll.h>
#include <unistd.h>

struct ReadAwaitable {
    TcpSocket &socket;
    size_t     n_bytes;
    bool       eof = false;
    bool       error = false;

    bool try_read_socket() {
        auto &rb = socket.get_buffer();
        while (rb.writable_bytes() > 0) {
            ssize_t n = ::read(socket.get_fd(), rb.write_ptr(), rb.writable_bytes());
            if (n > 0) {
                rb.advance_tail(n);
            } else if (n == 0) {
                eof = true;
                return true;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return false; // Hit EAGAIN, need epoll
                }
                error = true;
                return true;
            }
        }
        // Buffer is full. We didn't hit EAGAIN.
        // We will stop reading for now to exert backpressure.
        return true; 
    }

    bool await_ready() {
        auto &rb = socket.get_buffer();
        if (rb.readable_bytes() > 0) {
            return true; // Fast path: data already in buffer
        }
        try_read_socket();
        return rb.readable_bytes() > 0 || eof || error;
    }

    void await_suspend(Handle hd) { 
        socket.get_ctx()->read_handle = hd; 
    }

    std::span<uint8_t> await_resume() {
        auto &rb = socket.get_buffer();
        
        if (rb.readable_bytes() == 0 && !eof && !error) {
            // Woken up by epoll, try reading again
            try_read_socket();
        }


        size_t to_return = std::min(n_bytes, rb.readable_bytes());
        std::span<uint8_t> data(rb.read_ptr(), to_return);
        rb.advance_head(to_return); // Mark as consumed
        
        return data;
    }
};


#include <array>
#include <cerrno>
#include <sys/epoll.h>
#include <sys/uio.h>


// A lightweight awaitable that simply puts the coroutine to sleep
// until Epoll says the socket is writable again.
struct WaitWritable {
    TcpSocket               &socket;
    EventLoop               &loop;
    std::span<const uint8_t> meta;
    std::span<const uint8_t> payload;

    std::array<iovec, 2> io = {{{.iov_base = const_cast<void *>(
                                     static_cast<const void *>(meta.data())),
                                 .iov_len = meta.size()},
                                {.iov_base = const_cast<void *>(
                                     static_cast<const void *>(payload.data())),
                                 .iov_len = payload.size()}}};
    ssize_t              bytes_sent = 0;

    bool await_ready() {
        bytes_sent = ::writev(socket.get_fd(), io.data(), io.size());
        if (bytes_sent > 0 || bytes_sent == 0)
            return true; // Got data or EOF, don't suspend!
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false; // Need data, suspend!
        return true;      // Error occurred
    }

    void await_suspend(Handle hd) {
        socket.get_ctx()->write_handle = hd; 
    }

    size_t await_resume() {
        if (bytes_sent > 0 || bytes_sent == 0)
            return bytes_sent;
        // Epoll woke us up, read now!
        bytes_sent = ::writev(socket.get_fd(), io.data(), io.size());
        if (bytes_sent < 0) {
            return -1;
        }
        return bytes_sent;
    }
};
