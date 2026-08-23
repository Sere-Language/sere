/// @file sere_icon.c
/// Loads the embedded Sere ICO for Win32 windows and file-association icons.

#include "sere_icon.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static const unsigned char kSereIconIco[] = {
#include "sere_icon_ico.inc"
};

const unsigned char* sere_icon_ico_bytes(void) { return kSereIconIco; }

size_t sere_icon_ico_size(void) { return sizeof(kSereIconIco); }

#ifdef _WIN32

static HICON loadFromIco(int cx) {
  const unsigned char* data = kSereIconIco;
  const size_t size = sizeof(kSereIconIco);
  if (data == NULL || size < 6) {
    return NULL;
  }
  const int offset =
      LookupIconIdFromDirectoryEx((PBYTE)data, TRUE, cx, cx, LR_DEFAULTCOLOR);
  if (offset <= 0 || (size_t)offset >= size) {
    return NULL;
  }
  return CreateIconFromResourceEx((PBYTE)(data + offset), (DWORD)(size - (size_t)offset), TRUE,
                                  0x00030000, cx, cx, LR_DEFAULTCOLOR);
}

void* sere_icon_hicon(int small_icon) {
  static HICON big = NULL;
  static HICON small = NULL;
  static int ready = 0;
  if (!ready) {
    ready = 1;
    big = loadFromIco(32);
    small = loadFromIco(16);
    if (small == NULL) {
      small = big;
    }
    if (big == NULL) {
      big = small;
    }
  }
  return small_icon ? (void*)small : (void*)big;
}

void sere_icon_apply_hwnd(void* hwnd) {
  if (hwnd == NULL) {
    return;
  }
  HWND window = (HWND)hwnd;
  HICON big = (HICON)sere_icon_hicon(0);
  HICON small = (HICON)sere_icon_hicon(1);
  if (big != NULL) {
    SendMessageA(window, WM_SETICON, ICON_BIG, (LPARAM)big);
  }
  if (small != NULL) {
    SendMessageA(window, WM_SETICON, ICON_SMALL, (LPARAM)small);
  }
}

#endif
