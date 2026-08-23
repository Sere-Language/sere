/// @file sere_icon.h
/// Embedded Sere logo used as the default window and executable icon.

#ifndef SERE_ICON_H
#define SERE_ICON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const unsigned char* sere_icon_ico_bytes(void);
size_t sere_icon_ico_size(void);

#ifdef _WIN32
void* sere_icon_hicon(int small_icon);
void sere_icon_apply_hwnd(void* hwnd);
#endif

#ifdef __cplusplus
}
#endif

#endif
