#pragma once

#include <iostream>
#include <string>
#include "platform.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

using namespace std;

// ─── HTTP Server ──────────────────────────────────────────────────────────────
class HttpServer {
public:
    // port     : TCP port to listen on (default 8080)
    // threads  : number of worker threads in the pool (default 4)
    explicit HttpServer(int port = 8080, int threads = 4);
    ~HttpServer();

    // Attach a router whose routes will handle all incoming requests.
    HttpServer& use(Router& router);

    // Bind, listen, and enter the accept loop (blocking).
    void run();

private:
    int        port_;
    Router*    router_ = nullptr;
    ThreadPool pool_;
    socket_t   server_fd_ = INVALID_SOCK;

    // Read a full HTTP request from the client socket and dispatch it.
    void handle_client(socket_t client_fd) const;

    // Initialise the listening socket (Winsock on Windows).
    void init_socket();
};
