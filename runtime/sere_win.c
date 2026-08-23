/// @file sere_win.c
/// Windows API surface for the Sere stdlib. Stubbed on other hosts.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "advapi32")
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

static char* toCString(const char* data, int64_t len) {
  if (data == NULL || len < 0) {
    data = "";
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

#ifndef _WIN32
int32_t sere_win_available(void) { return 0; }
int32_t sere_win_message_box(const char* text, int64_t text_len, const char* title,
                             int64_t title_len, int32_t flags) {
  (void)text;
  (void)text_len;
  (void)title;
  (void)title_len;
  (void)flags;
  return 0;
}
int32_t sere_win_beep(int32_t freq, int32_t ms) {
  (void)freq;
  (void)ms;
  return 0;
}
int32_t sere_win_last_error(void) { return 0; }
void sere_win_computer_name(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
void sere_win_user_name(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
int32_t sere_win_cursor_x(void) { return 0; }
int32_t sere_win_cursor_y(void) { return 0; }
int32_t sere_win_set_cursor(int32_t x, int32_t y) {
  (void)x;
  (void)y;
  return 0;
}
int32_t sere_win_screen_width(void) { return 0; }
int32_t sere_win_screen_height(void) { return 0; }
void sere_win_clipboard_get(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
int32_t sere_win_clipboard_set(const char* data, int64_t len) {
  (void)data;
  (void)len;
  return 0;
}
void* sere_win_foreground(void) { return NULL; }
void sere_win_window_text(void* hwnd, const char** out_data, int64_t* out_len) {
  (void)hwnd;
  outEmpty(out_data, out_len);
}
void sere_win_debug(const char* data, int64_t len) {
  (void)data;
  (void)len;
}
int32_t sere_win_open(const char* path, int64_t path_len) {
  (void)path;
  (void)path_len;
  return 0;
}
#else

int32_t sere_win_available(void) { return 1; }

int32_t sere_win_message_box(const char* text, int64_t text_len, const char* title,
                             int64_t title_len, int32_t flags) {
  char* body = toCString(text, text_len);
  char* caption = toCString(title, title_len);
  const int32_t result =
      (int32_t)MessageBoxA(NULL, body == NULL ? "" : body, caption == NULL ? "" : caption,
                           (UINT)flags);
  free(body);
  free(caption);
  return result;
}

int32_t sere_win_beep(int32_t freq, int32_t ms) { return Beep((DWORD)freq, (DWORD)ms) ? 1 : 0; }

int32_t sere_win_last_error(void) { return (int32_t)GetLastError(); }

static void outWinString(BOOL(WINAPI* fn)(char*, DWORD*), const char** out_data, int64_t* out_len) {
  char buf[256];
  DWORD n = (DWORD)sizeof(buf);
  if (!fn(buf, &n)) {
    outEmpty(out_data, out_len);
    return;
  }
  buf[sizeof(buf) - 1] = '\0';
  char* copy = toCString(buf, (int64_t)strlen(buf));
  outOwned(copy, copy == NULL ? 0 : (int64_t)strlen(copy), out_data, out_len);
}

void sere_win_computer_name(const char** out_data, int64_t* out_len) {
  outWinString(GetComputerNameA, out_data, out_len);
}

void sere_win_user_name(const char** out_data, int64_t* out_len) {
  outWinString(GetUserNameA, out_data, out_len);
}

int32_t sere_win_cursor_x(void) {
  POINT point;
  GetCursorPos(&point);
  return (int32_t)point.x;
}

int32_t sere_win_cursor_y(void) {
  POINT point;
  GetCursorPos(&point);
  return (int32_t)point.y;
}

int32_t sere_win_set_cursor(int32_t x, int32_t y) { return SetCursorPos(x, y) ? 1 : 0; }

int32_t sere_win_screen_width(void) { return GetSystemMetrics(SM_CXSCREEN); }

int32_t sere_win_screen_height(void) { return GetSystemMetrics(SM_CYSCREEN); }

void sere_win_clipboard_get(const char** out_data, int64_t* out_len) {
  if (!OpenClipboard(NULL)) {
    outEmpty(out_data, out_len);
    return;
  }
  HANDLE handle = GetClipboardData(CF_TEXT);
  if (handle == NULL) {
    CloseClipboard();
    outEmpty(out_data, out_len);
    return;
  }
  const char* text = (const char*)GlobalLock(handle);
  char* copy = toCString(text == NULL ? "" : text, text == NULL ? 0 : (int64_t)strlen(text));
  GlobalUnlock(handle);
  CloseClipboard();
  outOwned(copy, copy == NULL ? 0 : (int64_t)strlen(copy), out_data, out_len);
}

int32_t sere_win_clipboard_set(const char* data, int64_t len) {
  if (len < 0) {
    len = 0;
  }
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)len + 1);
  if (mem == NULL) {
    return 0;
  }
  char* dest = (char*)GlobalLock(mem);
  if (dest == NULL) {
    GlobalFree(mem);
    return 0;
  }
  if (data != NULL && len > 0) {
    memcpy(dest, data, (size_t)len);
  }
  dest[len] = '\0';
  GlobalUnlock(mem);
  if (!OpenClipboard(NULL)) {
    GlobalFree(mem);
    return 0;
  }
  EmptyClipboard();
  SetClipboardData(CF_TEXT, mem);
  CloseClipboard();
  return 1;
}

void* sere_win_foreground(void) { return (void*)GetForegroundWindow(); }

void sere_win_window_text(void* hwnd, const char** out_data, int64_t* out_len) {
  if (hwnd == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  const int n = GetWindowTextLengthA((HWND)hwnd);
  char* copy = (char*)malloc((size_t)n + 1);
  if (copy == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  const int wrote = GetWindowTextA((HWND)hwnd, copy, n + 1);
  copy[wrote < 0 ? 0 : wrote] = '\0';
  outOwned(copy, wrote < 0 ? 0 : wrote, out_data, out_len);
}

void sere_win_debug(const char* data, int64_t len) {
  char* text = toCString(data, len);
  if (text != NULL) {
    OutputDebugStringA(text);
    free(text);
  }
}

int32_t sere_win_open(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  const HINSTANCE ok = ShellExecuteA(NULL, "open", cpath, NULL, NULL, SW_SHOWNORMAL);
  free(cpath);
  return (INT_PTR)ok > 32 ? 1 : 0;
}

#endif
