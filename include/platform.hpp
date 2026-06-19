#pragma once

// ─── Cross-platform socket abstractions ──────────────────────────────────────
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>

using socket_t = SOCKET;
static const socket_t INVALID_SOCK = INVALID_SOCKET;
inline void close_sock(socket_t s) { closesocket(s); }

#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>

using socket_t = int;
static const socket_t INVALID_SOCK = -1;
inline void close_sock(socket_t s) { close(s); }

#endif
