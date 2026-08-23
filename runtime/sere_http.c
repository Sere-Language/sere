/// @file sere_http.c
/// HTTP client (WinHTTP) and a small HTTP/1.1 accept loop for WSGI.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#endif

static void outEmpty(const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = "";
  }
  if (out_len != NULL) {
    *out_len = 0;
  }
}

static void outOwned(char* data, int64_t len, const char** out_data, int64_t* out_len) {
  if (data == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  if (out_data != NULL) {
    *out_data = data;
  }
  if (out_len != NULL) {
    *out_len = len;
  }
}

static char* dupRange(const char* data, int64_t len) {
  if (data == NULL || len < 0) {
    len = 0;
  }
  char* copy = (char*)malloc((size_t)len + 1);
  if (copy == NULL) {
    return NULL;
  }
  if (len > 0) {
    memcpy(copy, data, (size_t)len);
  }
  copy[len] = '\0';
  return copy;
}

#ifdef _WIN32

static int32_t gLastHttpStatus = 0;

int32_t sere_http_last_status(void) { return gLastHttpStatus; }

static wchar_t* toWide(const char* data, int64_t len) {
  if (data == NULL) {
    data = "";
    len = 0;
  }
  if (len < 0) {
    len = (int64_t)strlen(data);
  }
  const int needed = MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
  wchar_t* wide = (wchar_t*)calloc((size_t)needed + 1, sizeof(wchar_t));
  if (wide == NULL) {
    return NULL;
  }
  if (needed > 0) {
    MultiByteToWideChar(CP_UTF8, 0, data, (int)len, wide, needed);
  }
  return wide;
}

static int gWsaReady = 0;

static int ensureWsa(void) {
  if (gWsaReady) {
    return 1;
  }
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    return 0;
  }
  gWsaReady = 1;
  return 1;
}

typedef struct {
  SOCKET sock;
  char method[16];
  char path[2048];
  char* body;
  int64_t body_len;
} SereHttpConn;

static void connFree(SereHttpConn* conn) {
  if (conn == NULL) {
    return;
  }
  if (conn->sock != INVALID_SOCKET) {
    closesocket(conn->sock);
  }
  free(conn->body);
  free(conn);
}

void sere_http_request(const char* method, int64_t method_len, const char* url, int64_t url_len,
                       const char* body, int64_t body_len, int32_t timeout_ms,
                       const char** out_text, int64_t* out_text_len) {
  outEmpty(out_text, out_text_len);
  gLastHttpStatus = 0;
  wchar_t* wurl = toWide(url, url_len);
  wchar_t* wmethod = toWide(method, method_len);
  if (wurl == NULL || wmethod == NULL) {
    free(wurl);
    free(wmethod);
    return;
  }
  URL_COMPONENTS parts;
  memset(&parts, 0, sizeof(parts));
  wchar_t host[256];
  wchar_t path[2048];
  wchar_t extra[1024];
  parts.dwStructSize = sizeof(parts);
  parts.lpszHostName = host;
  parts.dwHostNameLength = 256;
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = 2048;
  parts.lpszExtraInfo = extra;
  parts.dwExtraInfoLength = 1024;
  if (!WinHttpCrackUrl(wurl, 0, 0, &parts)) {
    free(wurl);
    free(wmethod);
    return;
  }
  HINTERNET session = WinHttpOpen(L"Sere/0.2", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (session == NULL) {
    free(wurl);
    free(wmethod);
    return;
  }
  if (timeout_ms <= 0) {
    timeout_ms = 30000;
  }
  WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
  INTERNET_PORT port = parts.nPort;
  HINTERNET connect = WinHttpConnect(session, host, port, 0);
  if (connect == NULL) {
    WinHttpCloseHandle(session);
    free(wurl);
    free(wmethod);
    return;
  }
  wchar_t fullPath[3072];
  fullPath[0] = 0;
  wcsncat(fullPath, path, 2047);
  if (parts.dwExtraInfoLength > 0) {
    wcsncat(fullPath, extra, 1023);
  }
  DWORD flags = 0;
  if (parts.nScheme == INTERNET_SCHEME_HTTPS) {
    flags |= WINHTTP_FLAG_SECURE;
  }
  HINTERNET request = WinHttpOpenRequest(connect, wmethod, fullPath, NULL, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  free(wurl);
  free(wmethod);
  if (request == NULL) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return;
  }
  const DWORD sendLen = body == NULL || body_len <= 0 ? 0 : (DWORD)body_len;
  BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               sendLen == 0 ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body, sendLen,
                               sendLen, 0);
  if (ok) {
    ok = WinHttpReceiveResponse(request, NULL);
  }
  if (!ok) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return;
  }
  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
  gLastHttpStatus = (int32_t)status;
  size_t cap = 4096;
  size_t used = 0;
  char* text = (char*)malloc(cap);
  if (text == NULL) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return;
  }
  while (1) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
      break;
    }
    if (used + available + 1 > cap) {
      cap = (used + available + 1) * 2;
      char* grown = (char*)realloc(text, cap);
      if (grown == NULL) {
        free(text);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return;
      }
      text = grown;
    }
    DWORD read = 0;
    if (!WinHttpReadData(request, text + used, available, &read) || read == 0) {
      break;
    }
    used += read;
  }
  text[used] = '\0';
  outOwned(text, (int64_t)used, out_text, out_text_len);
  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
}

void* sere_http_listen(const char* host, int64_t host_len, int32_t port) {
  if (!ensureWsa() || port <= 0 || port > 65535) {
    return NULL;
  }
  char* chost = dupRange(host, host_len);
  if (chost == NULL) {
    return NULL;
  }
  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET) {
    free(chost);
    return NULL;
  }
  BOOL reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short)port);
  if (chost[0] == '\0' || strcmp(chost, "0.0.0.0") == 0) {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else if (InetPtonA(AF_INET, chost, &addr.sin_addr) != 1) {
    closesocket(sock);
    free(chost);
    return NULL;
  }
  free(chost);
  if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(sock, 16) != 0) {
    closesocket(sock);
    return NULL;
  }
  SOCKET* server = (SOCKET*)malloc(sizeof(SOCKET));
  if (server == NULL) {
    closesocket(sock);
    return NULL;
  }
  *server = sock;
  return server;
}

static int parseRequest(SereHttpConn* conn, char* raw, size_t length) {
  strncpy(conn->method, "GET", sizeof(conn->method) - 1);
  strncpy(conn->path, "/", sizeof(conn->path) - 1);
  conn->body = NULL;
  conn->body_len = 0;
  char* lineEnd = strstr(raw, "\r\n");
  if (lineEnd == NULL) {
    return 0;
  }
  *lineEnd = '\0';
  char* sp1 = strchr(raw, ' ');
  if (sp1 == NULL) {
    return 0;
  }
  *sp1 = '\0';
  strncpy(conn->method, raw, sizeof(conn->method) - 1);
  char* sp2 = strchr(sp1 + 1, ' ');
  if (sp2 != NULL) {
    *sp2 = '\0';
  }
  strncpy(conn->path, sp1 + 1, sizeof(conn->path) - 1);
  char* headersEnd = strstr(lineEnd + 2, "\r\n\r\n");
  if (headersEnd == NULL) {
    return 1;
  }
  const char* payload = headersEnd + 4;
  const size_t headerBytes = (size_t)(payload - raw);
  if (length > headerBytes) {
    conn->body_len = (int64_t)(length - headerBytes);
    conn->body = dupRange(payload, conn->body_len);
  }
  return 1;
}

void* sere_http_accept(void* server) {
  if (server == NULL || !ensureWsa()) {
    return NULL;
  }
  SOCKET listenSock = *(SOCKET*)server;
  SOCKET client = accept(listenSock, NULL, NULL);
  if (client == INVALID_SOCKET) {
    return NULL;
  }
  char buffer[16384];
  const int got = recv(client, buffer, (int)sizeof(buffer) - 1, 0);
  if (got <= 0) {
    closesocket(client);
    return NULL;
  }
  buffer[got] = '\0';
  SereHttpConn* conn = (SereHttpConn*)calloc(1, sizeof(SereHttpConn));
  if (conn == NULL) {
    closesocket(client);
    return NULL;
  }
  conn->sock = client;
  if (!parseRequest(conn, buffer, (size_t)got)) {
    connFree(conn);
    return NULL;
  }
  return conn;
}

void sere_http_req_method(void* req, const char** out_data, int64_t* out_len) {
  SereHttpConn* conn = (SereHttpConn*)req;
  if (conn == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(dupRange(conn->method, (int64_t)strlen(conn->method)), (int64_t)strlen(conn->method),
           out_data, out_len);
}

void sere_http_req_path(void* req, const char** out_data, int64_t* out_len) {
  SereHttpConn* conn = (SereHttpConn*)req;
  if (conn == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(dupRange(conn->path, (int64_t)strlen(conn->path)), (int64_t)strlen(conn->path), out_data,
           out_len);
}

void sere_http_req_body(void* req, const char** out_data, int64_t* out_len) {
  SereHttpConn* conn = (SereHttpConn*)req;
  if (conn == NULL || conn->body == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(dupRange(conn->body, conn->body_len), conn->body_len, out_data, out_len);
}

void sere_http_reply(void* req, int32_t status, const char* content_type, int64_t content_type_len,
                     const char* body, int64_t body_len) {
  SereHttpConn* conn = (SereHttpConn*)req;
  if (conn == NULL || conn->sock == INVALID_SOCKET) {
    return;
  }
  const char* ctype = content_type == NULL || content_type_len <= 0 ? "text/plain" : content_type;
  if (body == NULL || body_len < 0) {
    body = "";
    body_len = 0;
  }
  char header[512];
  const int headerLen =
      snprintf(header, sizeof(header),
               "HTTP/1.1 %d OK\r\nContent-Type: %.*s\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n",
               (int)status, content_type_len <= 0 ? (int)strlen(ctype) : (int)content_type_len, ctype,
               (long long)body_len);
  if (headerLen > 0) {
    send(conn->sock, header, headerLen, 0);
  }
  if (body_len > 0) {
    send(conn->sock, body, (int)body_len, 0);
  }
  connFree(conn);
}

void sere_http_close(void* server) {
  if (server == NULL) {
    return;
  }
  SOCKET sock = *(SOCKET*)server;
  if (sock != INVALID_SOCKET) {
    closesocket(sock);
  }
  free(server);
}

#else

int32_t sere_http_last_status(void) { return 0; }

void sere_http_request(const char* method, int64_t method_len, const char* url, int64_t url_len,
                       const char* body, int64_t body_len, int32_t timeout_ms,
                       const char** out_text, int64_t* out_text_len) {
  (void)method;
  (void)method_len;
  (void)url;
  (void)url_len;
  (void)body;
  (void)body_len;
  (void)timeout_ms;
  outEmpty(out_text, out_text_len);
}

void* sere_http_listen(const char* host, int64_t host_len, int32_t port) {
  (void)host;
  (void)host_len;
  (void)port;
  return NULL;
}

void* sere_http_accept(void* server) {
  (void)server;
  return NULL;
}

void sere_http_req_method(void* req, const char** out_data, int64_t* out_len) {
  (void)req;
  outEmpty(out_data, out_len);
}

void sere_http_req_path(void* req, const char** out_data, int64_t* out_len) {
  (void)req;
  outEmpty(out_data, out_len);
}

void sere_http_req_body(void* req, const char** out_data, int64_t* out_len) {
  (void)req;
  outEmpty(out_data, out_len);
}

void sere_http_reply(void* req, int32_t status, const char* content_type, int64_t content_type_len,
                     const char* body, int64_t body_len) {
  (void)req;
  (void)status;
  (void)content_type;
  (void)content_type_len;
  (void)body;
  (void)body_len;
}

void sere_http_close(void* ptr) { (void)ptr; }

#endif
