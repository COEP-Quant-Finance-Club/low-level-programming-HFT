#include "networking/TcpSocket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace nkm {

TcpSocket::TcpSocket()
    : fd_(::socket(AF_INET, SOCK_STREAM, 0))
{
    if (fd_ == kInvalidFd)
    {
        // Capture errno immediately after the failing call, before any
        // other library code (including the std::string construction
        // below) gets a chance to run and clobber it.
        const int err = errno;
        throw std::runtime_error(
            std::string("TcpSocket: ::socket() failed: ") + std::strerror(err));
    }
}

TcpSocket::~TcpSocket()
{
    close();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept
    : fd_(other.fd_)
{
    other.fd_ = kInvalidFd;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept
{
    if (this != &other)
    {
        close();                 // release whatever this object currently owns
        fd_ = other.fd_;
        other.fd_ = kInvalidFd;  // leave the source in a valid, closed state
    }
    return *this;
}

bool TcpSocket::connect(std::string_view host, std::uint16_t port) noexcept
{
    if (!is_open())
    {
        return false;
    }

    // V1 supports numeric IPv4 addresses only ("127.0.0.1", not
    // "exchange.example.com"). Hostname resolution needs getaddrinfo()
    // (netdb.h), which performs its own blocking DNS I/O and internally
    // heap-allocates the returned addrinfo list — different enough in
    // cost and failure mode from the rest of this class that it belongs
    // in its own component (e.g. a future HostResolver) rather than
    // folded in here.
    //
    // inet_pton() requires a null-terminated C string, but std::string_view
    // is not guaranteed to be null-terminated. Rather than construct a
    // std::string (a heap allocation) we copy into a fixed stack buffer
    // sized for the longest possible IPv4 text form: "255.255.255.255" is
    // 15 characters + 1 for the NUL = 16, exactly INET_ADDRSTRLEN.
    if (host.size() >= INET_ADDRSTRLEN)
    {
        return false;
    }

    char host_buf[INET_ADDRSTRLEN];
    std::memcpy(host_buf, host.data(), host.size());
    host_buf[host.size()] = '\0';

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (::inet_pton(AF_INET, host_buf, &addr.sin_addr) != 1)
    {
        // Not a valid dotted-decimal IPv4 address.
        return false;
    }

    // ::connect — explicitly global-qualified. Inside a member function
    // named connect(), an unqualified call to connect(...) would resolve
    // to this member function itself (member names hide enclosing/global
    // names of the same spelling), not the POSIX syscall. The same
    // qualification issue applies to send(), recv() (called as receive()
    // here, so no clash), and close() below.
    return ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

ssize_t TcpSocket::send(std::span<const std::byte> data) noexcept
{
    if (!is_open())
    {
        return -1;
    }

    // MSG_NOSIGNAL: if the peer has closed the connection, a plain
    // send(2) raises SIGPIPE, which terminates the process by default.
    // We want that failure to surface as an ordinary -1 return + EPIPE
    // errno the caller can check, not an asynchronous signal — consistent
    // with "no hidden control flow" for a routine, expected-to-fail
    // operation.
    return ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
}

ssize_t TcpSocket::receive(std::span<std::byte> buffer) noexcept
{
    if (!is_open())
    {
        return -1;
    }

    return ::recv(fd_, buffer.data(), buffer.size(), 0);
}

void TcpSocket::close() noexcept
{
    if (fd_ != kInvalidFd)
    {
        ::close(fd_);
        fd_ = kInvalidFd;
    }
}

bool TcpSocket::is_open() const noexcept
{
    return fd_ != kInvalidFd;
}

} // namespace nkm