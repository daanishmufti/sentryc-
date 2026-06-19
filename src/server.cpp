#include "server.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

#include "request.hpp"
#include "response.hpp"

// ─── Constructor / Destructor ─────────────────────────────────────────────────

HttpServer::HttpServer(int port, int threads)
    : port_(port), pool_(threads) {
    init_socket();
}

HttpServer::~HttpServer() {
    if (server_fd_ != INVALID_SOCK) {
        close_sock(server_fd_);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

// ─── Use ──────────────────────────────────────────────────────────────────────

HttpServer& HttpServer::use(Router& router) {
    router_ = &router;
    return *this;
}

// ─── Init ─────────────────────────────────────────────────────────────────────

void HttpServer::init_socket() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == INVALID_SOCK) {
        throw std::runtime_error("Failed to create socket");
    }

    // Allow port reuse so we can restart quickly during development.
    int opt = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("bind() failed — is port " +
                                 std::to_string(port_) + " already in use?");
    }

    if (::listen(server_fd_, SOMAXCONN) < 0) {
        throw std::runtime_error("listen() failed");
    }
}

// ─── Accept loop ─────────────────────────────────────────────────────────────

void HttpServer::run() {
    std::cout << "┌─────────────────────────────────────────┐\n";
    std::cout << "│  C++ HTTP Server listening on :" << port_ << "      │\n";
    std::cout << "│  Press Ctrl+C to stop                   │\n";
    std::cout << "└─────────────────────────────────────────┘\n";

    while (true) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif
        socket_t client_fd = ::accept(
            server_fd_,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len);

        if (client_fd == INVALID_SOCK) {
            std::cerr << "[warn] accept() returned invalid socket\n";
            continue;
        }

        // Hand off to the thread pool — capture client_fd by value.
        pool_.enqueue([this, client_fd] {
            handle_client(client_fd);
        });
    }
}

// ─── Handle client ────────────────────────────────────────────────────────────

void HttpServer::handle_client(socket_t client_fd) const {
    std::string raw;
    std::array<char, 4096> buf{};

    // Read until we have the full headers (and possibly body).
    while (true) {
        int n = ::recv(client_fd, buf.data(), static_cast<int>(buf.size()), 0);
        if (n <= 0) break;

        raw.append(buf.data(), static_cast<std::size_t>(n));

        // We have at least the full headers.
        auto header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos) continue;

        // Check if Content-Length says there's more body to read.
        auto cl_pos = raw.find("Content-Length: ");
        if (cl_pos != std::string::npos) {
            std::size_t cl_start = cl_pos + 16;
            std::size_t cl_end   = raw.find("\r\n", cl_start);
            if (cl_end != std::string::npos) {
                std::size_t content_length = std::stoul(
                    raw.substr(cl_start, cl_end - cl_start));
                std::size_t body_received =
                    raw.size() - (header_end + 4);
                if (body_received < content_length) continue;  // need more
            }
        }
        break;
    }

    if (raw.empty()) {
        close_sock(client_fd);
        return;
    }

    HttpRequest  req = parse_request(raw);
    HttpResponse res;

    std::cout << "[" << req.method << "] " << req.path << "\n";

    if (router_) {
        res = router_->dispatch(req);
    } else {
        res = HttpResponse::not_found();
    }

    res.send(client_fd);
    close_sock(client_fd);
}
