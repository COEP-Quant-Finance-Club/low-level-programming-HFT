#pragma once

// -----------------------------------------------------------------------------
// TcpSocket.hpp — Nakamoto Core networking, V1.
//
// A minimal RAII wrapper around a POSIX blocking TCP socket. V1 supports:
//   - IPv4 only, connect-by-numeric-address only (no DNS resolution)
//   - blocking send/receive
//   - RAII lifetime management (fd opened on construction, closed on
//     destruction)
//
// V1 explicitly does NOT implement TLS, WebSockets, epoll/async I/O,
// non-blocking mode, or reconnect logic. Bolting any of those onto this
// class would turn a ~100-line primitive into something trying to be three
// things at once — they belong in separate components layered on top.
// -----------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <sys/types.h>   // ssize_t

namespace nkm {

class TcpSocket
{
public:
    // Creates the underlying socket file descriptor immediately. This is
    // the one operation in this class that can fail for reasons entirely
    // outside the caller's control (fd exhaustion, kernel resource limits)
    // rather than anything about a specific remote host — that distinction
    // is exactly why this throws while connect()/send()/receive() do not.
    TcpSocket();

    // Closes the socket if still open. noexcept by necessity: a throwing
    // destructor is close to unusable, and a failure from ::close() here
    // (e.g. EINTR) isn't something a destructor can meaningfully react to.
    ~TcpSocket();

    // Copying a socket has no sensible meaning — two TcpSocket objects
    // would share one kernel fd, and whichever destructs first would close
    // it out from under the other. Deleted, so misuse is a compile error
    // rather than a runtime surprise.
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Moving transfers fd ownership. The moved-from object becomes a
    // valid, "closed" TcpSocket (is_open() == false), not a half-alive
    // object — destroying or reassigning it afterward is safe.
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    // Connects to host:port. `host` must be a numeric IPv4 dotted-decimal
    // address (e.g. "127.0.0.1") in V1 — see the .cpp for why DNS
    // resolution is deferred. Returns false on any failure (malformed
    // address text, refused/unreachable connection, socket already
    // closed); never throws. This is a routine, expected-to-sometimes-fail
    // operation, not an initialization error.
    [[nodiscard]] bool connect(std::string_view host, std::uint16_t port) noexcept;

    // Blocking send/receive. Return the number of bytes transferred
    // (standard POSIX send(2)/recv(2) semantics) or -1 on error — callers
    // inspect errno themselves for the specific cause, same as they would
    // with the raw syscalls. Neither function allocates: both operate
    // directly on the caller-provided span.
    [[nodiscard]] ssize_t send(std::span<const std::byte> data) noexcept;
    [[nodiscard]] ssize_t receive(std::span<std::byte> buffer) noexcept;

    // Closes the socket early. Safe to call multiple times and safe to
    // call on an already-closed or moved-from socket (both are no-ops).
    void close() noexcept;

    // True if the underlying file descriptor is valid — i.e. socket()
    // succeeded and close() hasn't since been called. This reflects fd
    // lifecycle, not TCP connection state: a freshly constructed,
    // not-yet-connected socket is "open" by this definition. Check
    // connect()'s return value to know whether a connection actually
    // succeeded.
    [[nodiscard]] bool is_open() const noexcept;

private:
    int fd_;

    static constexpr int kInvalidFd = -1;
};

} // namespace nkm 