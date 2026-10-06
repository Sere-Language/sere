/// @file sere_socket.c
/// Cross-platform native socket handles for the low-level Sere socket module.
#include "sere_rt.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SereNativeSocket;
#define SERE_INVALID_SOCKET INVALID_SOCKET
#define sere_close_socket closesocket
static INIT_ONCE sere_winsock_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK sere_start_winsock(PINIT_ONCE once, PVOID parameter, PVOID* context) {
  WSADATA data;
  (void)once;
  (void)parameter;
  (void)context;
  return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}
static int sere_ensure_winsock(void) {
  return InitOnceExecuteOnce(&sere_winsock_once, sere_start_winsock, NULL, NULL) != 0;
}
#define SERE_THREAD_LOCAL __declspec(thread)
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int SereNativeSocket;
#define SERE_INVALID_SOCKET (-1)
#define sere_close_socket close
#define SERE_THREAD_LOCAL _Thread_local
static int sere_ensure_winsock(void) { return 1; }
#endif

#define SERE_SOCKET_HOST_CAP 1025

typedef struct {
  SereNativeSocket native;
  int32_t family;
  int32_t type;
  int32_t proto;
  int32_t blocking;
  double timeout;
  char peer_host[SERE_SOCKET_HOST_CAP];
  int32_t peer_port;
} SereSocket;

static SERE_THREAD_LOCAL char sere_socket_error[512];

static void sere_socket_clear_error(void) { sere_socket_error[0] = '\0'; }

static void sere_socket_set_error(const char* operation, int code) {
  (void)snprintf(sere_socket_error, sizeof(sere_socket_error),
                 "%s failed (native socket error %d)", operation, code);
}

static int sere_socket_last_native_error(void) {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

static int sere_native_family(int32_t family) {
  if (family == 2) return AF_INET;
  if (family == 10) return AF_INET6;
#ifdef AF_UNIX
  if (family == 1) return AF_UNIX;
#endif
  if (family == 0) return AF_UNSPEC;
  return family;
}

static int sere_logical_family(int family) {
  if (family == AF_INET) return 2;
  if (family == AF_INET6) return 10;
#ifdef AF_UNIX
  if (family == AF_UNIX) return 1;
#endif
  return family;
}

static int sere_native_type(int32_t type) {
  if (type == 1) return SOCK_STREAM;
  if (type == 2) return SOCK_DGRAM;
#ifdef SOCK_RAW
  if (type == 3) return SOCK_RAW;
#endif
#ifdef SOCK_SEQPACKET
  if (type == 5) return SOCK_SEQPACKET;
#endif
  return type;
}

static int sere_native_level(int32_t level) {
  if (level == 1) return SOL_SOCKET;
  if (level == 6) return IPPROTO_TCP;
  if (level == 17) return IPPROTO_UDP;
  if (level == 41) return IPPROTO_IPV6;
  return level;
}

static int sere_native_option(int32_t level, int32_t option) {
  if (level == 1) {
    switch (option) {
    case 2: return SO_REUSEADDR;
    case 3: return SO_TYPE;
    case 4: return SO_ERROR;
    case 6: return SO_BROADCAST;
    case 7: return SO_SNDBUF;
    case 8: return SO_RCVBUF;
    case 9: return SO_KEEPALIVE;
#ifdef SO_LINGER
    case 13: return SO_LINGER;
#endif
#ifdef SO_REUSEPORT
    case 15: return SO_REUSEPORT;
#endif
#ifdef SO_RCVTIMEO
    case 20: return SO_RCVTIMEO;
#endif
#ifdef SO_SNDTIMEO
    case 21: return SO_SNDTIMEO;
#endif
    default: break;
    }
  }
  if (level == 6 && option == 1) return TCP_NODELAY;
#ifdef IPV6_V6ONLY
  if (level == 41 && option == 26) return IPV6_V6ONLY;
#endif
  return option;
}

static SereSocket* sere_socket_get(void* handle, const char* operation) {
  SereSocket* socket = (SereSocket*)handle;
  if (socket == NULL || socket->native == SERE_INVALID_SOCKET) {
    sere_socket_set_error(operation, -1);
    return NULL;
  }
  return socket;
}

static void sere_socket_store_peer(SereSocket* socket, const struct sockaddr* address,
                                  socklen_t address_size) {
  char service[16] = "0";
  socket->peer_host[0] = '\0';
  socket->peer_port = 0;
  if (address == NULL || address_size == 0) return;
  if (getnameinfo(address, address_size, socket->peer_host,
                  (socklen_t)sizeof(socket->peer_host), service, (socklen_t)sizeof(service),
                  NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    socket->peer_port = atoi(service);
  }
}

static void sere_clear_timeout(SereSocket* socket) {
#ifdef _WIN32
  DWORD zero_timeout = 0;
  (void)setsockopt(socket->native, SOL_SOCKET, SO_RCVTIMEO,
                   (const char*)&zero_timeout, (int)sizeof(zero_timeout));
  (void)setsockopt(socket->native, SOL_SOCKET, SO_SNDTIMEO,
                   (const char*)&zero_timeout, (int)sizeof(zero_timeout));
#else
  struct timeval zero_timeout = {0, 0};
  (void)setsockopt(socket->native, SOL_SOCKET, SO_RCVTIMEO,
                   &zero_timeout, (socklen_t)sizeof(zero_timeout));
  (void)setsockopt(socket->native, SOL_SOCKET, SO_SNDTIMEO,
                   &zero_timeout, (socklen_t)sizeof(zero_timeout));
#endif
}

static int sere_set_nonblocking(SereSocket* socket, int enabled) {
#ifdef _WIN32
  u_long mode = enabled ? 1UL : 0UL;
  return ioctlsocket(socket->native, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(socket->native, F_GETFL, 0);
  if (flags < 0) return 0;
  int next = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  return fcntl(socket->native, F_SETFL, next) == 0;
#endif
}

static int sere_apply_timeout(SereSocket* socket, double seconds) {
  if (seconds <= 0.0) return 1;
#ifdef _WIN32
  DWORD milliseconds = (DWORD)(seconds * 1000.0 + 0.5);
  if (milliseconds == 0) milliseconds = 1;
  return setsockopt(socket->native, SOL_SOCKET, SO_RCVTIMEO,
                    (const char*)&milliseconds, (int)sizeof(milliseconds)) == 0 &&
         setsockopt(socket->native, SOL_SOCKET, SO_SNDTIMEO,
                    (const char*)&milliseconds, (int)sizeof(milliseconds)) == 0;
#else
  struct timeval timeout;
  timeout.tv_sec = (time_t)seconds;
  timeout.tv_usec = (suseconds_t)((seconds - (double)timeout.tv_sec) * 1000000.0);
  if (timeout.tv_sec == 0 && timeout.tv_usec == 0) timeout.tv_usec = 1;
  return setsockopt(socket->native, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
         setsockopt(socket->native, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

void* sere_socket_new(int32_t family, int32_t type, int32_t proto) {
  sere_socket_clear_error();
  if (!sere_ensure_winsock()) {
    sere_socket_set_error("Winsock initialization", sere_socket_last_native_error());
    return NULL;
  }
  const int native_family = sere_native_family(family);
  const int native_type = sere_native_type(type);
  SereNativeSocket native = socket(native_family, native_type, proto);
  if (native == SERE_INVALID_SOCKET) {
    (void)snprintf(sere_socket_error, sizeof(sere_socket_error),
                   "socket(family=%d, type=%d, proto=%d) failed (native socket error %d)",
                   family, type, proto, sere_socket_last_native_error());
    return NULL;
  }
#ifndef _WIN32
  int fd_flags = fcntl(native, F_GETFD, 0);
  if (fd_flags >= 0) (void)fcntl(native, F_SETFD, fd_flags | FD_CLOEXEC);
#endif
  SereSocket* result = (SereSocket*)calloc(1, sizeof(*result));
  if (result == NULL) {
    sere_close_socket(native);
    sere_socket_set_error("socket allocation", ENOMEM);
    return NULL;
  }
  result->native = native;
  result->family = family;
  result->type = type;
  result->proto = proto;
  result->blocking = 1;
  result->timeout = -1.0;
  return result;
}

void sere_socket_close(void* handle) {
  SereSocket* socket = (SereSocket*)handle;
  if (socket == NULL) return;
  if (socket->native != SERE_INVALID_SOCKET) {
    sere_close_socket(socket->native);
    socket->native = SERE_INVALID_SOCKET;
  }
  free(socket);
}

int32_t sere_socket_bind(void* handle, const char* host, int64_t host_len, int32_t port) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "bind");
  if (socket == NULL) return -1;
  if (host_len < 0 || host_len > 1024 || port < 0 || port > 65535 ||
      (host_len > 0 && host == NULL)) {
    sere_socket_set_error("bind address validation", EINVAL);
    return -1;
  }
  char name[1025];
  if (host_len > 0) memcpy(name, host, (size_t)host_len);
  name[host_len] = '\0';
  if ((int64_t)strlen(name) != host_len) {
    sere_socket_set_error("bind address validation", EINVAL);
    return -1;
  }
  char service[16];
  (void)snprintf(service, sizeof(service), "%d", port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = sere_native_family(socket->family);
  hints.ai_socktype = sere_native_type(socket->type);
  hints.ai_protocol = socket->proto;
  hints.ai_flags = AI_PASSIVE;
  struct addrinfo* addresses = NULL;
  int result = getaddrinfo(name[0] == '\0' ? NULL : name, service, &hints, &addresses);
  if (result != 0) {
    sere_socket_set_error("bind address resolution", result);
    return -1;
  }
  int bound = 0;
  int last = 0;
  for (struct addrinfo* address = addresses; address != NULL; address = address->ai_next) {
#ifdef _WIN32
    if (bind(socket->native, address->ai_addr, (int)address->ai_addrlen) == 0) {
#else
    if (bind(socket->native, address->ai_addr, (socklen_t)address->ai_addrlen) == 0) {
#endif
      bound = 1;
      break;
    }
    last = sere_socket_last_native_error();
  }
  freeaddrinfo(addresses);
  if (!bound) {
    sere_socket_set_error("bind", last);
    return -1;
  }
  return 0;
}

int32_t sere_socket_listen(void* handle, int32_t backlog) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "listen");
  if (socket == NULL) return -1;
  if (backlog < 0) backlog = SOMAXCONN;
  if (listen(socket->native, backlog) != 0) {
    sere_socket_set_error("listen", sere_socket_last_native_error());
    return -1;
  }
  return 0;
}

void* sere_socket_accept(void* handle) {
  sere_socket_clear_error();
  SereSocket* server = sere_socket_get(handle, "accept");
  if (server == NULL) return NULL;
  struct sockaddr_storage peer;
#ifdef _WIN32
  int peer_size = (int)sizeof(peer);
  SereNativeSocket native = accept(server->native, (struct sockaddr*)&peer, &peer_size);
#else
  socklen_t peer_size = (socklen_t)sizeof(peer);
  SereNativeSocket native = accept(server->native, (struct sockaddr*)&peer, &peer_size);
#endif
  if (native == SERE_INVALID_SOCKET) {
    sere_socket_set_error("accept", sere_socket_last_native_error());
    return NULL;
  }
#ifndef _WIN32
  int fd_flags = fcntl(native, F_GETFD, 0);
  if (fd_flags >= 0) (void)fcntl(native, F_SETFD, fd_flags | FD_CLOEXEC);
#endif
  SereSocket* result = (SereSocket*)calloc(1, sizeof(*result));
  if (result == NULL) {
    sere_close_socket(native);
    sere_socket_set_error("accept allocation", ENOMEM);
    return NULL;
  }
  result->native = native;
  result->family = server->family;
  result->type = server->type;
  result->proto = server->proto;
  result->blocking = server->blocking;
  result->timeout = server->timeout;
  sere_socket_store_peer(result, (struct sockaddr*)&peer, peer_size);
  if (!sere_set_nonblocking(result, !result->blocking)) {
    sere_close_socket(native);
    free(result);
    sere_socket_set_error("accepted socket mode", sere_socket_last_native_error());
    return NULL;
  }
  if (result->timeout > 0.0 && !sere_apply_timeout(result, result->timeout)) {
    sere_close_socket(native);
    free(result);
    sere_socket_set_error("accepted socket timeout", sere_socket_last_native_error());
    return NULL;
  }
  return result;
}

int32_t sere_socket_connect(void* handle, const char* host, int64_t host_len, int32_t port) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "connect");
  if (socket == NULL) return -1;
  if (host_len < 0 || host_len > 1024 || port < 0 || port > 65535 ||
      (host_len > 0 && host == NULL)) {
    sere_socket_set_error("connect address validation", EINVAL);
    return -1;
  }
  char name[1025];
  if (host_len > 0) memcpy(name, host, (size_t)host_len);
  name[host_len] = '\0';
  if ((int64_t)strlen(name) != host_len) {
    sere_socket_set_error("connect address validation", EINVAL);
    return -1;
  }
  char service[16];
  (void)snprintf(service, sizeof(service), "%d", port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = sere_native_family(socket->family);
  hints.ai_socktype = sere_native_type(socket->type);
  hints.ai_protocol = socket->proto;
  struct addrinfo* addresses = NULL;
  int result = getaddrinfo(name, service, &hints, &addresses);
  if (result != 0) {
    sere_socket_set_error("connect address resolution", result);
    return -1;
  }
  int connected = 0;
  int last = 0;
  for (struct addrinfo* address = addresses; address != NULL; address = address->ai_next) {
#ifdef _WIN32
    if (connect(socket->native, address->ai_addr, (int)address->ai_addrlen) == 0) {
#else
    if (connect(socket->native, address->ai_addr, (socklen_t)address->ai_addrlen) == 0) {
#endif
      sere_socket_store_peer(socket, address->ai_addr, (socklen_t)address->ai_addrlen);
      connected = 1;
      break;
    }
    last = sere_socket_last_native_error();
  }
  freeaddrinfo(addresses);
  if (!connected) {
    sere_socket_set_error("connect", last);
    return -1;
  }
  return 0;
}

int64_t sere_socket_send(void* handle, const uint8_t* data, int64_t size) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "send");
  if (socket == NULL) return -1;
  if (size < 0 || (size > 0 && data == NULL) || size > INT_MAX) {
    sere_socket_set_error("send buffer validation", EINVAL);
    return -1;
  }
  if (size == 0) return 0;
#ifdef _WIN32
  int sent = send(socket->native, (const char*)data, (int)size, 0);
#else
  ssize_t sent;
  do {
#ifdef MSG_NOSIGNAL
    sent = send(socket->native, data, (size_t)size, MSG_NOSIGNAL);
#else
    sent = send(socket->native, data, (size_t)size, 0);
#endif
  } while (sent < 0 && errno == EINTR);
#endif
  if (sent < 0) {
    sere_socket_set_error("send", sere_socket_last_native_error());
    return -1;
  }
  return (int64_t)sent;
}

int32_t sere_socket_sendall(void* handle, const uint8_t* data, int64_t size) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "sendall");
  if (socket == NULL) return -1;
  if (size < 0 || (size > 0 && data == NULL)) {
    sere_socket_set_error("sendall buffer validation", EINVAL);
    return -1;
  }
  int64_t offset = 0;
  while (offset < size) {
    int64_t remaining = size - offset;
    int count = remaining > 1048576 ? 1048576 : (int)remaining;
#ifdef _WIN32
    int sent = send(socket->native, (const char*)data + offset, count, 0);
    if (sent < 0) {
      sere_socket_set_error("sendall", sere_socket_last_native_error());
      return -1;
    }
#else
    ssize_t sent;
    do {
#ifdef MSG_NOSIGNAL
      sent = send(socket->native, data + offset, (size_t)count, MSG_NOSIGNAL);
#else
      sent = send(socket->native, data + offset, (size_t)count, 0);
#endif
    } while (sent < 0 && errno == EINTR);
    if (sent < 0) {
      sere_socket_set_error("sendall", sere_socket_last_native_error());
      return -1;
    }
#endif
    if (sent == 0) {
      sere_socket_set_error("sendall", 0);
      return -1;
    }
    offset += sent;
  }
  return 0;
}

int64_t sere_socket_recv(void* handle, uint8_t* data, int64_t size) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "recv");
  if (socket == NULL) return -1;
  if (size < 0 || (size > 0 && data == NULL) || size > INT_MAX) {
    sere_socket_set_error("recv buffer validation", EINVAL);
    return -1;
  }
  if (size == 0) return 0;
#ifdef _WIN32
  int received = recv(socket->native, (char*)data, (int)size, 0);
#else
  ssize_t received;
  do {
    received = recv(socket->native, data, (size_t)size, 0);
  } while (received < 0 && errno == EINTR);
#endif
  if (received < 0) {
    sere_socket_set_error("recv", sere_socket_last_native_error());
    return -1;
  }
  return (int64_t)received;
}

int64_t sere_socket_sendto(void* handle, const uint8_t* data, int64_t size,
                           const char* host, int64_t host_len, int32_t port) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "sendto");
  if (socket == NULL) return -1;
  if (size < 0 || size > INT_MAX || (size > 0 && data == NULL) || host_len < 0 ||
      host_len > 1024 || (host_len > 0 && host == NULL) || port < 0 || port > 65535) {
    sere_socket_set_error("sendto argument validation", EINVAL);
    return -1;
  }
  char name[1025], service[16];
  if (host_len > 0) memcpy(name, host, (size_t)host_len);
  name[host_len] = '\0';
  if ((int64_t)strlen(name) != host_len) {
    sere_socket_set_error("sendto address validation", EINVAL);
    return -1;
  }
  (void)snprintf(service, sizeof(service), "%d", port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = sere_native_family(socket->family);
  hints.ai_socktype = sere_native_type(socket->type);
  hints.ai_protocol = socket->proto;
  struct addrinfo* addresses = NULL;
  int error = getaddrinfo(name, service, &hints, &addresses);
  if (error != 0) {
    sere_socket_set_error("sendto address resolution", error);
    return -1;
  }
  int64_t sent = -1;
  int last = 0;
  for (struct addrinfo* address = addresses; address != NULL; address = address->ai_next) {
#ifdef _WIN32
    int result = sendto(socket->native, (const char*)data, (int)size, 0,
                        address->ai_addr, (int)address->ai_addrlen);
#else
    ssize_t result = sendto(socket->native, data, (size_t)size, 0, address->ai_addr,
                            (socklen_t)address->ai_addrlen);
#endif
    if (result >= 0) {
      sent = (int64_t)result;
      sere_socket_store_peer(socket, address->ai_addr, (socklen_t)address->ai_addrlen);
      break;
    }
    last = sere_socket_last_native_error();
  }
  freeaddrinfo(addresses);
  if (sent < 0) sere_socket_set_error("sendto", last);
  return sent;
}

int64_t sere_socket_recvfrom(void* handle, uint8_t* data, int64_t size) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "recvfrom");
  if (socket == NULL) return -1;
  if (size < 0 || size > INT_MAX || (size > 0 && data == NULL)) {
    sere_socket_set_error("recvfrom buffer validation", EINVAL);
    return -1;
  }
  if (size == 0) return 0;
  struct sockaddr_storage peer;
#ifdef _WIN32
  int peer_size = (int)sizeof(peer);
  int received = recvfrom(socket->native, (char*)data, (int)size, 0,
                          (struct sockaddr*)&peer, &peer_size);
  socklen_t stored_peer_size = (socklen_t)peer_size;
#else
  socklen_t peer_size = (socklen_t)sizeof(peer);
  ssize_t received;
  do {
    received = recvfrom(socket->native, data, (size_t)size, 0,
                        (struct sockaddr*)&peer, &peer_size);
  } while (received < 0 && errno == EINTR);
  socklen_t stored_peer_size = peer_size;
#endif
  if (received < 0) {
    sere_socket_set_error("recvfrom", sere_socket_last_native_error());
    return -1;
  }
  sere_socket_store_peer(socket, (struct sockaddr*)&peer, stored_peer_size);
  return (int64_t)received;
}

int32_t sere_socket_shutdown(void* handle, int32_t how) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "shutdown");
  if (socket == NULL) return -1;
#ifdef _WIN32
  int native_how = how == 0 ? SD_RECEIVE : (how == 1 ? SD_SEND : SD_BOTH);
#else
  int native_how = how;
#endif
  if (how < 0 || how > 2 || shutdown(socket->native, native_how) != 0) {
    sere_socket_set_error("shutdown", sere_socket_last_native_error());
    return -1;
  }
  return 0;
}

int32_t sere_socket_setblocking(void* handle, int32_t blocking) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "setblocking");
  if (socket == NULL) return -1;
  if (!sere_set_nonblocking(socket, !blocking)) {
    sere_socket_set_error("setblocking", sere_socket_last_native_error());
    return -1;
  }
  socket->blocking = blocking != 0;
  socket->timeout = blocking ? -1.0 : 0.0;
  sere_clear_timeout(socket);
  return 0;
}

int32_t sere_socket_getblocking(void* handle) {
  SereSocket* socket = sere_socket_get(handle, "getblocking");
  return socket == NULL ? -1 : socket->blocking;
}

int32_t sere_socket_settimeout(void* handle, double seconds) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "settimeout");
  if (socket == NULL) return -1;
  if (seconds < 0.0) {
    sere_socket_set_error("settimeout validation", EINVAL);
    return -1;
  }
  if (!sere_set_nonblocking(socket, seconds == 0.0)) {
    sere_socket_set_error("settimeout mode", sere_socket_last_native_error());
    return -1;
  }
  if (seconds == 0.0) {
    sere_clear_timeout(socket);
  }
  if (seconds > 0.0 && !sere_apply_timeout(socket, seconds)) {
    sere_socket_set_error("settimeout", sere_socket_last_native_error());
    return -1;
  }
  socket->timeout = seconds == 0.0 ? 0.0 : seconds;
  socket->blocking = seconds != 0.0;
  return 0;
}

double sere_socket_gettimeout(void* handle) {
  SereSocket* socket = sere_socket_get(handle, "gettimeout");
  return socket == NULL ? 0.0 : socket->timeout;
}

int32_t sere_socket_setsockopt(void* handle, int32_t level, int32_t option, int32_t value) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "setsockopt");
  if (socket == NULL) return -1;
  int native_level = sere_native_level(level);
  int native_option = sere_native_option(level, option);
#ifdef _WIN32
  int result = setsockopt(socket->native, native_level, native_option,
                          (const char*)&value, (int)sizeof(value));
#else
  int result = setsockopt(socket->native, native_level, native_option,
                          &value, (socklen_t)sizeof(value));
#endif
  if (result != 0) {
    sere_socket_set_error("setsockopt", sere_socket_last_native_error());
    return -1;
  }
  return 0;
}

int32_t sere_socket_getsockopt(void* handle, int32_t level, int32_t option) {
  sere_socket_clear_error();
  SereSocket* socket = sere_socket_get(handle, "getsockopt");
  if (socket == NULL) return -1;
  int value = 0;
#ifdef _WIN32
  int size = (int)sizeof(value);
  int result = getsockopt(socket->native, sere_native_level(level), sere_native_option(level, option),
                          (char*)&value, &size);
#else
  socklen_t size = (socklen_t)sizeof(value);
  int result = getsockopt(socket->native, sere_native_level(level), sere_native_option(level, option),
                          &value, &size);
#endif
  if (result != 0) {
    sere_socket_set_error("getsockopt", sere_socket_last_native_error());
    return -1;
  }
  return value;
}

static int sere_socket_query_name(void* handle, int peer, struct sockaddr_storage* address,
                                  socklen_t* address_size) {
  SereSocket* socket = sere_socket_get(handle, peer ? "getpeername" : "getsockname");
  if (socket == NULL) return -1;
#ifdef _WIN32
  int size = (int)sizeof(*address);
  int result = peer ? getpeername(socket->native, (struct sockaddr*)address, &size)
                    : getsockname(socket->native, (struct sockaddr*)address, &size);
  *address_size = (socklen_t)size;
#else
  socklen_t size = (socklen_t)sizeof(*address);
  int result = peer ? getpeername(socket->native, (struct sockaddr*)address, &size)
                    : getsockname(socket->native, (struct sockaddr*)address, &size);
  *address_size = size;
#endif
  if (result != 0) {
    sere_socket_set_error(peer ? "getpeername" : "getsockname", sere_socket_last_native_error());
    return -1;
  }
  return 0;
}

void sere_socket_name_host(void* handle, int32_t peer, const char** out_data, int64_t* out_len) {
  static const char empty[] = "";
  struct sockaddr_storage address;
  socklen_t address_size = 0;
  char host[SERE_SOCKET_HOST_CAP];
  sere_socket_clear_error();
  if (out_data == NULL || out_len == NULL ||
      sere_socket_query_name(handle, peer, &address, &address_size) != 0 ||
      getnameinfo((struct sockaddr*)&address, address_size, host, (socklen_t)sizeof(host),
                  NULL, 0, NI_NUMERICHOST) != 0) {
    if (out_data != NULL) *out_data = empty;
    if (out_len != NULL) *out_len = 0;
    return;
  }
  size_t length = strlen(host);
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    *out_data = empty;
    *out_len = 0;
    sere_socket_set_error("socket address allocation", ENOMEM);
    return;
  }
  memcpy(copy, host, length + 1);
  *out_data = copy;
  *out_len = (int64_t)length;
}

int32_t sere_socket_name_port(void* handle, int32_t peer) {
  struct sockaddr_storage address;
  socklen_t address_size = 0;
  if (sere_socket_query_name(handle, peer, &address, &address_size) != 0) return -1;
  if (address.ss_family == AF_INET) return (int32_t)ntohs(((struct sockaddr_in*)&address)->sin_port);
  if (address.ss_family == AF_INET6) return (int32_t)ntohs(((struct sockaddr_in6*)&address)->sin6_port);
  return 0;
}

void sere_socket_peer_host(void* handle, const char** out_data, int64_t* out_len) {
  SereSocket* socket = sere_socket_get(handle, "peer address");
  if (socket == NULL || out_data == NULL || out_len == NULL) {
    if (out_data != NULL) *out_data = "";
    if (out_len != NULL) *out_len = 0;
    return;
  }
  size_t length = strlen(socket->peer_host);
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    *out_data = "";
    *out_len = 0;
    sere_socket_set_error("peer address allocation", ENOMEM);
    return;
  }
  memcpy(copy, socket->peer_host, length + 1);
  *out_data = copy;
  *out_len = (int64_t)length;
}

int32_t sere_socket_peer_port(void* handle) {
  SereSocket* socket = sere_socket_get(handle, "peer address");
  return socket == NULL ? -1 : socket->peer_port;
}

void sere_socket_last_error(const char** out_data, int64_t* out_len) {
  static const char empty[] = "";
  size_t length = strlen(sere_socket_error);
  if (out_data == NULL || out_len == NULL) return;
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    *out_data = empty;
    *out_len = 0;
    return;
  }
  memcpy(copy, sere_socket_error, length + 1);
  *out_data = copy;
  *out_len = (int64_t)length;
}

int32_t sere_socket_native_family(int32_t family) { return sere_logical_family(sere_native_family(family)); }
int32_t sere_socket_native_type(int32_t type) { return sere_native_type(type); }
