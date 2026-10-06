/// Bounded HTTP transport for wsgi.simple_server. One request per connection;
/// WSGI response policy lives in Sere, not in this socket layer.
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Socket;
#define BAD_SOCKET INVALID_SOCKET
#define closeSocket closesocket
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int Socket;
#define BAD_SOCKET (-1)
#define closeSocket close
#endif

// POSIX recv() takes a size_t length; Winsock takes an int. Going through this
// macro keeps both the Linux and the Windows build free of narrowing warnings.
#ifdef _WIN32
#define SERE_IO_LENGTH(n) ((int)(n))
#else
#define SERE_IO_LENGTH(n) ((size_t)(n))
#endif

enum { HeaderLimit = 65536, LineLimit = 8192, HeaderCount = 100 };
typedef struct { Socket socket; int timeout; int port; } WsgiServer;
typedef struct {
  Socket socket;
  char raw[HeaderLimit + 1];
  char method[LineLimit + 1], target[LineLimit + 1], protocol[16];
  char remote[128];
  char* headers;
  char* pending;
  int pendingSize;
  int pendingOffset;
  int64_t remaining;
} WsgiRequest;

static char* copyText(const char* data, int64_t size) {
  if (size < 0 || size > INT_MAX) return NULL;
  char* result = (char*)malloc((size_t)size + 1);
  if (result) { memcpy(result, data, (size_t)size); result[size] = 0; }
  return result;
}

static void textResult(const char* text, const char** data, int64_t* size) {
  *size = (int64_t)strlen(text);
  *data = copyText(text, *size);
  if (!*data) { *data = ""; *size = 0; }
}

static int sendAll(Socket socket, const char* data, int64_t size) {
  while (size > 0) {
    int count = size > 1048576 ? 1048576 : (int)size;
#ifdef _WIN32
    int sent = send(socket, data, count, 0);
#else
    int sent = (int)send(socket, data, (size_t)count, MSG_NOSIGNAL);
#endif
    if (sent <= 0) return 0;
    data += sent;
    size -= sent;
  }
  return 1;
}

static int tokenChar(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || (c && strchr("!#$%&'*+-.^_`|~", c));
}

static int equalName(const char* a, const char* b) {
  while (*a && *b) {
    unsigned char c = (unsigned char)*a++, d = (unsigned char)*b++;
    if (c >= 'A' && c <= 'Z') c += 32;
    if (d >= 'A' && d <= 'Z') d += 32;
    if (c != d) return 0;
  }
  return *a == *b;
}

void* sere_wsgi_listen(const char* host, int64_t size, int32_t port, int32_t backlog,
                       int32_t timeout, int32_t reuse) {
  if (port < 0 || port > 65535 || backlog < 1 || timeout < 1 || size < 0) return NULL;
#ifdef _WIN32
  WSADATA startup;
  if (WSAStartup(MAKEWORD(2, 2), &startup)) return NULL;
#endif
  char* name = copyText(host, size);
  if (!name) return NULL;
  struct addrinfo hints, *addresses = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  char service[8];
  snprintf(service, sizeof(service), "%d", port);
  int error = getaddrinfo(*name ? name : NULL, service, &hints, &addresses);
  free(name);
  if (error) return NULL;
  Socket socketHandle = BAD_SOCKET;
  for (struct addrinfo* addr = addresses; addr; addr = addr->ai_next) {
    socketHandle = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (socketHandle == BAD_SOCKET) continue;
    if (reuse) {
      int yes = 1;
      setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    }
    if (!bind(socketHandle, addr->ai_addr, (socklen_t)addr->ai_addrlen) &&
        !listen(socketHandle, backlog)) break;
    closeSocket(socketHandle);
    socketHandle = BAD_SOCKET;
  }
  freeaddrinfo(addresses);
  if (socketHandle == BAD_SOCKET) return NULL;
  WsgiServer* server = (WsgiServer*)calloc(1, sizeof(*server));
  if (!server) { closeSocket(socketHandle); return NULL; }
  server->socket = socketHandle;
  server->timeout = timeout;
  struct sockaddr_storage bound;
#ifdef _WIN32
  int boundSize = sizeof(bound);
#else
  socklen_t boundSize = sizeof(bound);
#endif
  if (!getsockname(socketHandle, (struct sockaddr*)&bound, &boundSize)) {
    server->port = bound.ss_family == AF_INET
        ? ntohs(((struct sockaddr_in*)&bound)->sin_port)
        : ntohs(((struct sockaddr_in6*)&bound)->sin6_port);
  }
  return server;
}

int32_t sere_wsgi_port(void* handle) { return handle ? ((WsgiServer*)handle)->port : 0; }

void sere_wsgi_close(void* handle) {
  WsgiServer* server = (WsgiServer*)handle;
  if (!server) return;
  closeSocket(server->socket);
  free(server);
#ifdef _WIN32
  WSACleanup();
#endif
}

void sere_wsgi_request_close(void* handle) {
  WsgiRequest* req = (WsgiRequest*)handle;
  if (!req) return;
  closeSocket(req->socket);
  free(req);
}

/// Parse a complete header block in place. Reject ambiguous framing before any
/// application code runs; request bodies are read lazily and length bounded.
static int parseHeaders(WsgiRequest* req, int headerEnd, int received) {
  char* raw = req->raw;
  char* end = strstr(raw, "\r\n");
  if (!end || end - raw > LineLimit) return 414;
  *end = 0;
  char* first = strchr(raw, ' ');
  if (!first || first == raw) return 400;
  *first++ = 0;
  for (char* p = raw; *p; ++p) if (!tokenChar((unsigned char)*p)) return 400;
  char* second = strchr(first, ' ');
  if (!second || second == first) return 400;
  *second++ = 0;
  if (strcmp(second, "HTTP/1.0") && strcmp(second, "HTTP/1.1")) return 505;
  if (*first != '/' && strcmp(first, "*")) return 400;
  for (char* p = first; *p; ++p) {
    if ((unsigned char)*p <= 32 || (unsigned char)*p == 127 || *p == '#') return 400;
    if (*p == '%') {
      for (int i = 1; i <= 2; ++i) {
        char c = p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return 400;
      }
      p += 2;
    }
  }
  strcpy(req->method, raw);
  strcpy(req->target, first);
  strcpy(req->protocol, second);
  req->headers = end + 2;
  int count = 0, lengthSeen = 0, hostSeen = 0, expect = 0;
  char* line = req->headers;
  while (*line != '\r') {
    end = strstr(line, "\r\n");
    if (!end) return 400;
    if (end - line > LineLimit || ++count > HeaderCount) return 431;
    char* colon = (char*)memchr(line, ':', (size_t)(end - line));
    if (!colon || colon == line) return 400;
    for (char* p = line; p != colon; ++p) if (!tokenChar((unsigned char)*p)) return 400;
    for (char* p = colon + 1; p != end; ++p)
      if (((unsigned char)*p < 32 && *p != '\t') || (unsigned char)*p == 127) return 400;
    *colon = 0;
    *end = 0;
    char* value = colon + 1;
    while (*value == ' ' || *value == '\t') ++value;
    char* last = end;
    while (last > value && (last[-1] == ' ' || last[-1] == '\t')) --last;
    char saved = *last;
    *last = 0;
    if (equalName(line, "Content-Length")) {
      // Reject even identical duplicates: intermediaries disagree about them.
      if (lengthSeen++ || !*value) return 400;
      int64_t length = 0;
      for (char* p = value; *p; ++p) {
        if (*p < '0' || *p > '9' || length > (INT64_MAX - (*p - '0')) / 10) return 400;
        length = length * 10 + (*p - '0');
      }
      req->remaining = length;
    } else if (equalName(line, "Transfer-Encoding")) return 501;
    else if (equalName(line, "Host")) { if (hostSeen++ || !*value) return 400; }
    else if (equalName(line, "Expect")) {
      if (!equalName(value, "100-continue")) return 417;
      expect = 1;
    }
    *last = saved;
    *colon = ':';
    *end = '\r';
    line = end + 2;
  }
  if (!strcmp(req->protocol, "HTTP/1.1") && !hostSeen) return 400;
  *line = 0;
  req->pending = raw + headerEnd;
  req->pendingSize = received - headerEnd;
  if (expect && req->remaining && !sendAll(req->socket, "HTTP/1.1 100 Continue\r\n\r\n", 25))
    return 400;
  return 0;
}

void* sere_wsgi_accept(void* handle) {
  WsgiServer* server = (WsgiServer*)handle;
  if (!server) return NULL;
  // Polling permits a Sere serve loop to observe shutdown between requests.
  fd_set ready;
  FD_ZERO(&ready);
  FD_SET(server->socket, &ready);
  struct timeval poll = {0, 200000};
  if (select((int)server->socket + 1, &ready, NULL, NULL, &poll) <= 0) return NULL;
  struct sockaddr_storage peer;
#ifdef _WIN32
  int peerSize = sizeof(peer);
#else
  socklen_t peerSize = sizeof(peer);
#endif
  Socket client = accept(server->socket, (struct sockaddr*)&peer, &peerSize);
  if (client == BAD_SOCKET) return NULL;
#ifdef _WIN32
  DWORD timeout = (DWORD)server->timeout;
#else
  struct timeval timeout = {server->timeout / 1000, (server->timeout % 1000) * 1000};
#endif
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
  setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
  WsgiRequest* req = (WsgiRequest*)calloc(1, sizeof(*req));
  if (!req) { closeSocket(client); return NULL; }
  req->socket = client;
  getnameinfo((struct sockaddr*)&peer, peerSize, req->remote, sizeof(req->remote), NULL, 0,
              NI_NUMERICHOST);
  int used = 0, status = 431;
  while (used < HeaderLimit) {
    int space = HeaderLimit - used;
    int got = (int)recv(client, req->raw + used, SERE_IO_LENGTH(space > 4096 ? 4096 : space), 0);
    if (got <= 0) { status = 400; break; }
    if (memchr(req->raw + used, 0, (size_t)got)) {
      // NUL in a body is legal; check only the header below after finding its end.
    }
    used += got;
    req->raw[used] = 0;
    int boundary = 0;
    for (int i = used - got > 3 ? used - got - 3 : 0; i + 3 < used; ++i)
      if (!memcmp(req->raw + i, "\r\n\r\n", 4)) { boundary = i + 4; break; }
    if (boundary) {
      if (memchr(req->raw, 0, (size_t)boundary)) status = 400;
      else status = parseHeaders(req, boundary, used);
      if (!status) return req;
      break;
    }
    if (used > LineLimit && !strstr(req->raw, "\r\n")) { status = 414; break; }
  }
  char error[160];
  int size = snprintf(error, sizeof(error),
      "HTTP/1.0 %d Request Rejected\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", status);
  sendAll(client, error, size);
  sere_wsgi_request_close(req);
  return NULL;
}

void sere_wsgi_meta(void* handle, int32_t field, const char** data, int64_t* size) {
  WsgiRequest* req = (WsgiRequest*)handle;
  const char* text = "";
  if (req) {
    switch (field) {
    case 0: text = req->method; break;
    case 1: text = req->target; break;
    case 2: text = req->protocol; break;
    case 3: text = req->headers; break;
    case 4: text = req->remote; break;
    }
  }
  textResult(text, data, size);
}

int32_t sere_wsgi_read(void* handle, char* data, int32_t size) {
  WsgiRequest* req = (WsgiRequest*)handle;
  if (!req || !data || size <= 0 || !req->remaining) return 0;
  if (size > req->remaining) size = (int)req->remaining;
  int available = req->pendingSize - req->pendingOffset;
  int got;
  if (available > 0) {
    got = size < available ? size : available;
    memcpy(data, req->pending + req->pendingOffset, (size_t)got);
    req->pendingOffset += got;
  } else got = (int)recv(req->socket, data, SERE_IO_LENGTH(size), 0);
  if (got <= 0) return -1;
  req->remaining -= got;
  return got;
}

int32_t sere_wsgi_write(void* handle, const char* data, int64_t size) {
  WsgiRequest* req = (WsgiRequest*)handle;
  return req && size >= 0 && sendAll(req->socket, data, size);
}
