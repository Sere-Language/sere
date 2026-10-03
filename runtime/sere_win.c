/// @file sere_win.c
/// Windows API surface for the Sere stdlib. Stubbed on other hosts.
///
/// Every exported function is declared in `sere_rt.h` and bound by
/// `stdlib/windows.sere`. The file mirrors itself across the `_WIN32` split so
/// the stub block stays a drop-in replacement for the real one:
///
///   shared helpers | error handling | handles | process and thread | wait |
///   synchronization | pipes | console handles and modes | console input
///   events | console cursor and screen buffer | positioned screen-buffer I/O |
///   direct console I/O | raw handle I/O | handle kinds | console title and
///   code pages | keyboard | window handles | window class and creation |
///   window messages | pseudoconsole | desktop and path helpers
///
/// Strings cross the boundary as UTF-8 and are converted centrally through
/// `winWide` / `outWinWide`, so only the W entry points are used.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

/* ---- shared helpers ---- */

static void outEmpty(const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = "";
  }
  if (out_len != NULL) {
    *out_len = 0;
  }
}

#ifdef _WIN32
/// Adopts a malloc'd UTF-8 buffer as the Sere string out-params. Windows-only
/// because the non-Windows stubs always report empty results.
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
#endif

#ifndef _WIN32

/* ---- error handling ---- */
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
uint32_t sere_win_last_error(void) { return 0; }
void sere_win_set_last_error(uint32_t code) { (void)code; }
void sere_win_error_message(uint32_t code, const char** out_data, int64_t* out_len) {
  (void)code;
  outEmpty(out_data, out_len);
}
uint64_t sere_win_tick_count64(void) { return 0; }
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
int32_t sere_win_process_id(void) { return 0; }
int32_t sere_win_thread_id(void) { return 0; }
int32_t sere_win_tick_count(void) { return 0; }
void sere_win_sleep(int32_t milliseconds) { (void)milliseconds; }
void sere_win_current_directory(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
void sere_win_temp_directory(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
void sere_win_env_get(const char* name, int64_t name_len, const char** out_data, int64_t* out_len) {
  (void)name;
  (void)name_len;
  outEmpty(out_data, out_len);
}
int32_t sere_win_env_set(const char* name, int64_t name_len, const char* value, int64_t value_len) {
  (void)name;
  (void)name_len;
  (void)value;
  (void)value_len;
  return 0;
}
int32_t sere_win_file_exists(const char* path, int64_t path_len) {
  (void)path;
  (void)path_len;
  return 0;
}
int32_t sere_win_directory_exists(const char* path, int64_t path_len) {
  (void)path;
  (void)path_len;
  return 0;
}
int32_t sere_win_create_directory(const char* path, int64_t path_len) {
  (void)path;
  (void)path_len;
  return 0;
}
int32_t sere_win_delete_file(const char* path, int64_t path_len) {
  (void)path;
  (void)path_len;
  return 0;
}
void sere_win_full_path(const char* path, int64_t path_len, const char** out_data, int64_t* out_len) {
  (void)path;
  (void)path_len;
  outEmpty(out_data, out_len);
}
void sere_win_foreground_title(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
int32_t sere_win_virtual_screen_width(void) { return 0; }
int32_t sere_win_virtual_screen_height(void) { return 0; }
int32_t sere_win_key_down(int32_t virtual_key) { (void)virtual_key; return 0; }
int32_t sere_win_show_cursor(int32_t visible) { (void)visible; return 0; }
void sere_win_os_version(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }

/* ---- handles ---- */
int32_t sere_win_handle_valid(void* handle) {
  (void)handle;
  return 0;
}
int32_t sere_win_handle_is_null(void* handle) { return handle == NULL ? 1 : 0; }
int32_t sere_win_close_handle(void* handle) {
  (void)handle;
  return 0;
}
void* sere_win_duplicate_handle(void* handle, uint32_t access, uint32_t options) {
  (void)handle;
  (void)access;
  (void)options;
  return NULL;
}
void* sere_win_current_process_handle(void) { return NULL; }
void* sere_win_current_thread_handle(void) { return NULL; }
void* sere_win_std_handle(int32_t which) {
  (void)which;
  return NULL;
}
int32_t sere_win_set_std_handle(int32_t which, void* handle) {
  (void)which;
  (void)handle;
  return 0;
}
int32_t sere_win_get_handle_information(void* handle, uint32_t* out_flags) {
  (void)handle;
  if (out_flags != NULL) {
    *out_flags = 0;
  }
  return 0;
}
int32_t sere_win_set_handle_information(void* handle, uint32_t mask, uint32_t flags) {
  (void)handle;
  (void)mask;
  (void)flags;
  return 0;
}

/* ---- process and thread ---- */
void* sere_win_open_process(uint32_t access, int32_t inherit, uint32_t process_id) {
  (void)access;
  (void)inherit;
  (void)process_id;
  return NULL;
}
int32_t sere_win_terminate_process(void* handle, uint32_t exit_code) {
  (void)handle;
  (void)exit_code;
  return 0;
}
int32_t sere_win_exit_code_process(void* handle, uint32_t* out_code) {
  (void)handle;
  if (out_code != NULL) {
    *out_code = 0;
  }
  return 0;
}

/* ---- wait ---- */
uint32_t sere_win_wait_single(void* handle, uint32_t milliseconds) {
  (void)handle;
  (void)milliseconds;
  return 0xFFFFFFFFu;
}
uint32_t sere_win_wait_multiple(void* handles, int32_t wait_all, uint32_t milliseconds) {
  (void)handles;
  (void)wait_all;
  (void)milliseconds;
  return 0xFFFFFFFFu;
}

/* ---- synchronization ---- */
void* sere_win_event_create(int32_t manual_reset, int32_t initial_state) {
  (void)manual_reset;
  (void)initial_state;
  return NULL;
}
int32_t sere_win_event_set(void* handle) {
  (void)handle;
  return 0;
}
int32_t sere_win_event_reset(void* handle) {
  (void)handle;
  return 0;
}
int32_t sere_win_event_pulse(void* handle) {
  (void)handle;
  return 0;
}
void* sere_win_mutex_create(int32_t initial_owner) {
  (void)initial_owner;
  return NULL;
}
int32_t sere_win_mutex_release(void* handle) {
  (void)handle;
  return 0;
}
void* sere_win_semaphore_create(int32_t initial_count, int32_t maximum_count) {
  (void)initial_count;
  (void)maximum_count;
  return NULL;
}
int32_t sere_win_semaphore_release(void* handle, int32_t release_count, int32_t* out_previous) {
  (void)handle;
  (void)release_count;
  if (out_previous != NULL) {
    *out_previous = 0;
  }
  return 0;
}

/* ---- pipes ---- */
int32_t sere_win_create_pipe(uint32_t size, void** out_read, void** out_write) {
  (void)size;
  if (out_read != NULL) {
    *out_read = NULL;
  }
  if (out_write != NULL) {
    *out_write = NULL;
  }
  return 0;
}

/* ---- console handles and modes ---- */
int32_t sere_win_handle_is_console(void* handle) {
  (void)handle;
  return 0;
}
int32_t sere_win_console_mode(void* handle, uint32_t* out_mode) {
  (void)handle;
  if (out_mode != NULL) {
    *out_mode = 0;
  }
  return 0;
}
int32_t sere_win_set_console_mode(void* handle, uint32_t mode) {
  (void)handle;
  (void)mode;
  return 0;
}

/* ---- console input events ---- */
uint32_t sere_win_console_event_count(void* handle) {
  (void)handle;
  return 0;
}
int32_t sere_win_console_read_event(void* handle, int32_t* fields, int32_t field_count) {
  (void)handle;
  (void)fields;
  (void)field_count;
  return 0;
}
int32_t sere_win_console_peek_event(void* handle, int32_t* fields, int32_t field_count) {
  (void)handle;
  (void)fields;
  (void)field_count;
  return 0;
}
int32_t sere_win_console_flush_input(void* handle) {
  (void)handle;
  return 0;
}

/* ---- console cursor and screen buffer ---- */
int32_t sere_win_console_screen_info(void* handle, int32_t* fields, int32_t field_count) {
  (void)handle;
  (void)fields;
  (void)field_count;
  return 0;
}
int32_t sere_win_console_cursor_info(void* handle, uint32_t* out_size, int32_t* out_visible) {
  (void)handle;
  if (out_size != NULL) {
    *out_size = 0;
  }
  if (out_visible != NULL) {
    *out_visible = 0;
  }
  return 0;
}
int32_t sere_win_console_set_cursor_info(void* handle, uint32_t size, int32_t visible) {
  (void)handle;
  (void)size;
  (void)visible;
  return 0;
}
int32_t sere_win_console_set_cursor_pos(void* handle, int32_t x, int32_t y) {
  (void)handle;
  (void)x;
  (void)y;
  return 0;
}
int32_t sere_win_console_largest_size(void* handle, int32_t* out_width, int32_t* out_height) {
  (void)handle;
  if (out_width != NULL) {
    *out_width = 0;
  }
  if (out_height != NULL) {
    *out_height = 0;
  }
  return 0;
}
int32_t sere_win_console_set_buffer_size(void* handle, int32_t width, int32_t height) {
  (void)handle;
  (void)width;
  (void)height;
  return 0;
}
int32_t sere_win_console_set_window_rect(
    void* handle, int32_t absolute, int32_t left, int32_t top, int32_t right, int32_t bottom) {
  (void)handle;
  (void)absolute;
  (void)left;
  (void)top;
  (void)right;
  (void)bottom;
  return 0;
}
int32_t sere_win_set_console_text_attribute(void* handle, int32_t attribute) {
  (void)handle;
  (void)attribute;
  return 0;
}

/* ---- positioned screen-buffer I/O ---- */
int32_t sere_win_fill_console_chars(
    void* handle, int32_t char_code, uint32_t count, int32_t x, int32_t y, uint32_t* out_written) {
  (void)handle;
  (void)char_code;
  (void)count;
  (void)x;
  (void)y;
  if (out_written != NULL) {
    *out_written = 0;
  }
  return 0;
}
int32_t sere_win_fill_console_attrs(
    void* handle, int32_t attribute, uint32_t count, int32_t x, int32_t y, uint32_t* out_written) {
  (void)handle;
  (void)attribute;
  (void)count;
  (void)x;
  (void)y;
  if (out_written != NULL) {
    *out_written = 0;
  }
  return 0;
}
int32_t sere_win_write_console_output_chars(
    void* handle, const char* data, int64_t data_len, int32_t x, int32_t y, uint32_t* out_written) {
  (void)handle;
  (void)data;
  (void)data_len;
  (void)x;
  (void)y;
  if (out_written != NULL) {
    *out_written = 0;
  }
  return 0;
}
int32_t sere_win_write_console_output_attrs(
    void* handle, void* attrs, int32_t x, int32_t y, uint32_t* out_written) {
  (void)handle;
  (void)attrs;
  (void)x;
  (void)y;
  if (out_written != NULL) {
    *out_written = 0;
  }
  return 0;
}
void sere_win_read_console_output_chars(
    void* handle, int32_t x, int32_t y, uint32_t count, const char** out_data, int64_t* out_len) {
  (void)handle;
  (void)x;
  (void)y;
  (void)count;
  outEmpty(out_data, out_len);
}
int32_t sere_win_read_console_output_attrs(void* handle, int32_t x, int32_t y, void* attrs) {
  (void)handle;
  (void)x;
  (void)y;
  (void)attrs;
  return 0;
}
int32_t sere_win_scroll_console(void* handle,
                                int32_t left,
                                int32_t top,
                                int32_t right,
                                int32_t bottom,
                                int32_t dx,
                                int32_t dy,
                                int32_t fill_char,
                                int32_t fill_attr) {
  (void)handle;
  (void)left;
  (void)top;
  (void)right;
  (void)bottom;
  (void)dx;
  (void)dy;
  (void)fill_char;
  (void)fill_attr;
  return 0;
}

/* ---- direct console I/O ---- */
int32_t sere_win_write_console(
    void* handle, const char* data, int64_t data_len, uint32_t* out_written) {
  (void)handle;
  (void)data;
  (void)data_len;
  if (out_written != NULL) {
    *out_written = 0;
  }
  return 0;
}
void sere_win_read_console(
    void* handle, uint32_t max_chars, const char** out_data, int64_t* out_len) {
  (void)handle;
  (void)max_chars;
  outEmpty(out_data, out_len);
}

/* ---- raw handle I/O ---- */
int32_t sere_win_handle_write(void* handle, void* data, uint32_t* out_written) {
  (void)handle;
  (void)data;
  if (out_written != NULL) {
    *out_written = 0;
  }
  return 0;
}
void* sere_win_handle_read(void* handle, int64_t count) {
  (void)handle;
  (void)count;
  return NULL;
}

/* ---- handle kinds ---- */
uint32_t sere_win_get_file_type(void* handle) {
  (void)handle;
  return 0;
}

/* ---- console title, allocation, code pages ---- */
void sere_win_console_title(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
int32_t sere_win_set_console_title(const char* data, int64_t data_len) {
  (void)data;
  (void)data_len;
  return 0;
}
void sere_win_console_original_title(const char** out_data, int64_t* out_len) {
  outEmpty(out_data, out_len);
}
int32_t sere_win_alloc_console(void) { return 0; }
int32_t sere_win_free_console(void) { return 0; }
int32_t sere_win_attach_console(uint32_t process_id) {
  (void)process_id;
  return 0;
}
void* sere_win_console_window(void) { return NULL; }
uint32_t sere_win_get_console_cp(void) { return 0; }
int32_t sere_win_set_console_cp(uint32_t code_page) {
  (void)code_page;
  return 0;
}
uint32_t sere_win_get_console_output_cp(void) { return 0; }
int32_t sere_win_set_console_output_cp(uint32_t code_page) {
  (void)code_page;
  return 0;
}

/* ---- keyboard ---- */
int32_t sere_win_get_key_state(int32_t virtual_key) {
  (void)virtual_key;
  return 0;
}
int32_t sere_win_key_state_raw(int32_t virtual_key) {
  (void)virtual_key;
  return 0;
}
int32_t sere_win_get_keyboard_state(void* state) {
  (void)state;
  return 0;
}
void sere_win_to_unicode(int32_t virtual_key,
                         int32_t scan_code,
                         void* state,
                         int32_t flags,
                         const char** out_data,
                         int64_t* out_len) {
  (void)virtual_key;
  (void)scan_code;
  (void)state;
  (void)flags;
  outEmpty(out_data, out_len);
}
uint32_t sere_win_map_virtual_key(uint32_t code, uint32_t map_type) {
  (void)code;
  (void)map_type;
  return 0;
}

/* ---- window handles ---- */
void* sere_win_active_window(void) { return NULL; }
void* sere_win_focus_window(void) { return NULL; }
int32_t sere_win_set_foreground_window(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_bring_window_to_top(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_show_window(void* window, int32_t command) {
  (void)window;
  (void)command;
  return 0;
}
int32_t sere_win_is_window(void* window) { return window != NULL ? 1 : 0; }
int32_t sere_win_is_window_visible(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_is_iconic(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_is_zoomed(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_set_window_text(void* window, const char* data, int64_t data_len) {
  (void)window;
  (void)data;
  (void)data_len;
  return 0;
}
int32_t sere_win_get_window_rect(void* window, int32_t* fields, int32_t field_count) {
  (void)window;
  (void)fields;
  (void)field_count;
  return 0;
}
int32_t sere_win_get_client_rect(void* window, int32_t* fields, int32_t field_count) {
  (void)window;
  (void)fields;
  (void)field_count;
  return 0;
}
int32_t sere_win_move_window(
    void* window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint) {
  (void)window;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)repaint;
  return 0;
}
int32_t sere_win_set_window_pos(
    void* window, int32_t anchor, int32_t x, int32_t y, int32_t width, int32_t height,
    uint32_t flags) {
  (void)window;
  (void)anchor;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)flags;
  return 0;
}
int32_t sere_win_update_window(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_destroy_window(void* window) {
  (void)window;
  return 0;
}
int32_t sere_win_post_message(void* window, uint32_t message, uint64_t wparam, int64_t lparam) {
  (void)window;
  (void)message;
  (void)wparam;
  (void)lparam;
  return 0;
}
int64_t sere_win_send_message(void* window, uint32_t message, uint64_t wparam, int64_t lparam) {
  (void)window;
  (void)message;
  (void)wparam;
  (void)lparam;
  return 0;
}

/* ---- window class and creation ---- */
uint32_t sere_win_register_window_class(const char* name,
                                        int64_t name_len,
                                        uint32_t style,
                                        void* wnd_proc,
                                        int32_t class_extra,
                                        int32_t window_extra,
                                        void* background) {
  (void)name;
  (void)name_len;
  (void)style;
  (void)wnd_proc;
  (void)class_extra;
  (void)window_extra;
  (void)background;
  return 0;
}
int32_t sere_win_unregister_window_class(const char* name, int64_t name_len) {
  (void)name;
  (void)name_len;
  return 0;
}
void* sere_win_create_window(const char* class_name,
                             int64_t class_len,
                             const char* window_name,
                             int64_t window_len,
                             uint32_t style,
                             uint32_t ex_style,
                             int32_t x,
                             int32_t y,
                             int32_t width,
                             int32_t height,
                             void* parent,
                             void* menu) {
  (void)class_name;
  (void)class_len;
  (void)window_name;
  (void)window_len;
  (void)style;
  (void)ex_style;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)parent;
  (void)menu;
  return NULL;
}
int64_t sere_win_def_window_proc(void* window, uint32_t message, uint64_t wparam, int64_t lparam) {
  (void)window;
  (void)message;
  (void)wparam;
  (void)lparam;
  return 0;
}

/* ---- window messages ---- */
int32_t sere_win_get_message(
    int32_t* fields, int32_t field_count, void** out_window, int64_t* out_wparam,
    int64_t* out_lparam) {
  (void)fields;
  (void)field_count;
  if (out_window != NULL) {
    *out_window = NULL;
  }
  if (out_wparam != NULL) {
    *out_wparam = 0;
  }
  if (out_lparam != NULL) {
    *out_lparam = 0;
  }
  return -1;
}
int32_t sere_win_peek_message(int32_t* fields,
                              int32_t field_count,
                              void** out_window,
                              int64_t* out_wparam,
                              int64_t* out_lparam,
                              uint32_t min_message,
                              uint32_t max_message,
                              uint32_t flags) {
  (void)fields;
  (void)field_count;
  (void)min_message;
  (void)max_message;
  (void)flags;
  if (out_window != NULL) {
    *out_window = NULL;
  }
  if (out_wparam != NULL) {
    *out_wparam = 0;
  }
  if (out_lparam != NULL) {
    *out_lparam = 0;
  }
  return 0;
}
int32_t sere_win_translate_message(
    int32_t* fields, int32_t field_count, void* window, uint64_t wparam, int64_t lparam) {
  (void)fields;
  (void)field_count;
  (void)window;
  (void)wparam;
  (void)lparam;
  return 0;
}
int64_t sere_win_dispatch_message(
    int32_t* fields, int32_t field_count, void* window, uint64_t wparam, int64_t lparam) {
  (void)fields;
  (void)field_count;
  (void)window;
  (void)wparam;
  (void)lparam;
  return 0;
}
void sere_win_post_quit_message(int32_t exit_code) { (void)exit_code; }

/* ---- pseudoconsole ---- */
int32_t sere_win_conpty_available(void) { return 0; }
void* sere_win_pseudo_console_create(
    int32_t columns, int32_t rows, void* input_read, void* output_write) {
  (void)columns;
  (void)rows;
  (void)input_read;
  (void)output_write;
  return NULL;
}
int32_t sere_win_pseudo_console_resize(void* pseudo, int32_t columns, int32_t rows) {
  (void)pseudo;
  (void)columns;
  (void)rows;
  return 0;
}
void sere_win_pseudo_console_close(void* pseudo) { (void)pseudo; }

#else

/* ---- Windows-only string helpers ---- */

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

static int32_t winOk(BOOL value) { return value ? 1 : 0; }

static COORD winCoord(int32_t x, int32_t y) {
  COORD coord;
  coord.X = (SHORT)x;
  coord.Y = (SHORT)y;
  return coord;
}

static SMALL_RECT winRect(int32_t left, int32_t top, int32_t right, int32_t bottom) {
  SMALL_RECT rect;
  rect.Left = (SHORT)left;
  rect.Top = (SHORT)top;
  rect.Right = (SHORT)right;
  rect.Bottom = (SHORT)bottom;
  return rect;
}

/// Converts UTF-8 to a NUL-terminated UTF-16 buffer. `len` may contain
/// embedded NULs; the returned buffer always has one extra wide character.
static WCHAR* winWide(const char* data, int64_t len) {
  if (data == NULL || len < 0) {
    data = "";
    len = 0;
  }
  if (len > 0x7FFFFFFF) {
    return NULL;
  }
  int needed = len == 0 ? 0 : MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
  if (needed < 0) {
    return NULL;
  }
  WCHAR* buffer = (WCHAR*)malloc(((size_t)needed + 1) * sizeof(WCHAR));
  if (buffer == NULL) {
    return NULL;
  }
  if (needed > 0 && MultiByteToWideChar(CP_UTF8, 0, data, (int)len, buffer, needed) != needed) {
    free(buffer);
    return NULL;
  }
  buffer[needed] = L'\0';
  return buffer;
}

/// Adopts `chars` UTF-16 code units as a malloc'd UTF-8 Sere string.
static void outWinWide(const WCHAR* text, int32_t chars, const char** out_data, int64_t* out_len) {
  if (text == NULL || chars <= 0) {
    outEmpty(out_data, out_len);
    return;
  }
  const int needed = WideCharToMultiByte(CP_UTF8, 0, text, chars, NULL, 0, NULL, NULL);
  if (needed <= 0) {
    outEmpty(out_data, out_len);
    return;
  }
  char* copy = (char*)malloc((size_t)needed + 1);
  if (copy == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  if (WideCharToMultiByte(CP_UTF8, 0, text, chars, copy, needed, NULL, NULL) != needed) {
    free(copy);
    outEmpty(out_data, out_len);
    return;
  }
  copy[needed] = '\0';
  outOwned(copy, (int64_t)needed, out_data, out_len);
}

static void outWinWideZ(const WCHAR* text, const char** out_data, int64_t* out_len) {
  outWinWide(text, text == NULL ? 0 : (int32_t)wcslen(text), out_data, out_len);
}

/// Result codes shared by `get_message` / `peek_message` wrappers.
#define SERE_WIN_MESSAGE_ERROR (-1)
#define SERE_WIN_MESSAGE_QUIT 0
#define SERE_WIN_MESSAGE_READY 1

int32_t sere_win_available(void) { return 1; }

/* ---- error handling ---- */

int32_t sere_win_message_box(const char* text, int64_t text_len, const char* title,
                             int64_t title_len, int32_t flags) {
  WCHAR* body = winWide(text, text_len);
  WCHAR* caption = winWide(title, title_len);
  const int32_t result = (int32_t)MessageBoxW(
      NULL, body == NULL ? L"" : body, caption == NULL ? L"" : caption, (UINT)flags);
  free(body);
  free(caption);
  return result;
}

int32_t sere_win_beep(int32_t freq, int32_t ms) { return winOk(Beep((DWORD)freq, (DWORD)ms)); }

uint32_t sere_win_last_error(void) { return (uint32_t)GetLastError(); }

void sere_win_set_last_error(uint32_t code) { SetLastError((DWORD)code); }

void sere_win_error_message(uint32_t code, const char** out_data, int64_t* out_len) {
  WCHAR* buffer = NULL;
  const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                          FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                      NULL, (DWORD)code, 0, (LPWSTR)&buffer, 0, NULL);
  if (length == 0 || buffer == NULL) {
    if (buffer != NULL) {
      LocalFree(buffer);
    }
    outEmpty(out_data, out_len);
    return;
  }
  int32_t chars = (int32_t)length;
  while (chars > 0 && (buffer[chars - 1] == L'\r' || buffer[chars - 1] == L'\n' ||
                       buffer[chars - 1] == L' ')) {
    chars--;
  }
  outWinWide(buffer, chars, out_data, out_len);
  LocalFree(buffer);
}

uint64_t sere_win_tick_count64(void) { return (uint64_t)GetTickCount64(); }

static void outWinName(BOOL(WINAPI* fn)(LPWSTR, LPDWORD), const char** out_data,
                       int64_t* out_len) {
  WCHAR buffer[256];
  DWORD count = (DWORD)(sizeof(buffer) / sizeof(buffer[0]));
  buffer[0] = L'\0';
  if (!fn(buffer, &count)) {
    outEmpty(out_data, out_len);
    return;
  }
  buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = L'\0';
  outWinWideZ(buffer, out_data, out_len);
}

void sere_win_computer_name(const char** out_data, int64_t* out_len) {
  outWinName(GetComputerNameW, out_data, out_len);
}

void sere_win_user_name(const char** out_data, int64_t* out_len) {
  outWinName(GetUserNameW, out_data, out_len);
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

int32_t sere_win_set_cursor(int32_t x, int32_t y) { return winOk(SetCursorPos(x, y)); }

int32_t sere_win_screen_width(void) { return GetSystemMetrics(SM_CXSCREEN); }

int32_t sere_win_screen_height(void) { return GetSystemMetrics(SM_CYSCREEN); }

void sere_win_clipboard_get(const char** out_data, int64_t* out_len) {
  if (!OpenClipboard(NULL)) {
    outEmpty(out_data, out_len);
    return;
  }
  const HANDLE handle = GetClipboardData(CF_UNICODETEXT);
  if (handle == NULL) {
    CloseClipboard();
    outEmpty(out_data, out_len);
    return;
  }
  const WCHAR* text = (const WCHAR*)GlobalLock(handle);
  outWinWideZ(text, out_data, out_len);
  if (text != NULL) {
    GlobalUnlock(handle);
  }
  CloseClipboard();
}

int32_t sere_win_clipboard_set(const char* data, int64_t len) {
  WCHAR* wide = winWide(data, len);
  if (wide == NULL) {
    return 0;
  }
  const SIZE_T bytes = (wcslen(wide) + 1) * sizeof(WCHAR);
  const HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (mem == NULL) {
    free(wide);
    return 0;
  }
  void* dest = GlobalLock(mem);
  if (dest == NULL) {
    GlobalFree(mem);
    free(wide);
    return 0;
  }
  memcpy(dest, wide, bytes);
  GlobalUnlock(mem);
  free(wide);
  if (!OpenClipboard(NULL)) {
    GlobalFree(mem);
    return 0;
  }
  EmptyClipboard();
  SetClipboardData(CF_UNICODETEXT, mem);
  CloseClipboard();
  return 1;
}

void* sere_win_foreground(void) { return (void*)GetForegroundWindow(); }

void sere_win_window_text(void* hwnd, const char** out_data, int64_t* out_len) {
  if (hwnd == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  const HWND window = (HWND)hwnd;
  const int length = GetWindowTextLengthW(window);
  if (length <= 0) {
    outEmpty(out_data, out_len);
    return;
  }
  WCHAR* buffer = (WCHAR*)malloc(((size_t)length + 1) * sizeof(WCHAR));
  if (buffer == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  const int wrote = GetWindowTextW(window, buffer, length + 1);
  outWinWide(buffer, wrote > 0 ? wrote : 0, out_data, out_len);
  free(buffer);
}

void sere_win_debug(const char* data, int64_t len) {
  WCHAR* text = winWide(data, len);
  if (text != NULL) {
    OutputDebugStringW(text);
    free(text);
  }
}

int32_t sere_win_open(const char* path, int64_t path_len) {
  WCHAR* cpath = winWide(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  const HINSTANCE result = ShellExecuteW(NULL, L"open", cpath, NULL, NULL, SW_SHOWNORMAL);
  free(cpath);
  return (INT_PTR)result > 32 ? 1 : 0;
}

int32_t sere_win_process_id(void) { return (int32_t)GetCurrentProcessId(); }
int32_t sere_win_thread_id(void) { return (int32_t)GetCurrentThreadId(); }
int32_t sere_win_tick_count(void) { return (int32_t)GetTickCount(); }
void sere_win_sleep(int32_t milliseconds) {
  if (milliseconds > 0)
    Sleep((DWORD)milliseconds);
}

static void outWinDirectory(DWORD(WINAPI* fn)(DWORD, WCHAR*), const char** out_data,
                            int64_t* out_len) {
  WCHAR buffer[32768];
  const DWORD length = fn((DWORD)(sizeof(buffer) / sizeof(buffer[0])), buffer);
  if (length == 0 || length >= sizeof(buffer) / sizeof(buffer[0])) {
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)length, out_data, out_len);
}

void sere_win_current_directory(const char** out_data, int64_t* out_len) {
  outWinDirectory(GetCurrentDirectoryW, out_data, out_len);
}

void sere_win_temp_directory(const char** out_data, int64_t* out_len) {
  WCHAR buffer[32768];
  const DWORD length = GetTempPathW((DWORD)(sizeof(buffer) / sizeof(buffer[0])), buffer);
  if (length == 0 || length >= sizeof(buffer) / sizeof(buffer[0])) {
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)length, out_data, out_len);
}

void sere_win_env_get(const char* name, int64_t name_len, const char** out_data, int64_t* out_len) {
  WCHAR* cname = winWide(name, name_len);
  if (cname == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  WCHAR buffer[32768];
  const DWORD length =
      GetEnvironmentVariableW(cname, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
  free(cname);
  if (length == 0 || length >= sizeof(buffer) / sizeof(buffer[0])) {
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)length, out_data, out_len);
}

int32_t sere_win_env_set(const char* name, int64_t name_len, const char* value, int64_t value_len) {
  WCHAR* cname = winWide(name, name_len);
  WCHAR* cvalue = winWide(value, value_len);
  const BOOL ok = cname != NULL && cvalue != NULL && SetEnvironmentVariableW(cname, cvalue);
  free(cname);
  free(cvalue);
  return winOk(ok);
}

static DWORD winAttributes(const char* path, int64_t path_len) {
  WCHAR* cpath = winWide(path, path_len);
  if (cpath == NULL)
    return INVALID_FILE_ATTRIBUTES;
  const DWORD attributes = GetFileAttributesW(cpath);
  free(cpath);
  return attributes;
}

int32_t sere_win_file_exists(const char* path, int64_t path_len) {
  const DWORD attributes = winAttributes(path, path_len);
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

int32_t sere_win_directory_exists(const char* path, int64_t path_len) {
  const DWORD attributes = winAttributes(path, path_len);
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 1 : 0;
}

int32_t sere_win_create_directory(const char* path, int64_t path_len) {
  WCHAR* cpath = winWide(path, path_len);
  const BOOL ok = cpath != NULL && CreateDirectoryW(cpath, NULL);
  free(cpath);
  return winOk(ok);
}

int32_t sere_win_delete_file(const char* path, int64_t path_len) {
  WCHAR* cpath = winWide(path, path_len);
  const BOOL ok = cpath != NULL && DeleteFileW(cpath);
  free(cpath);
  return winOk(ok);
}

void sere_win_full_path(const char* path, int64_t path_len, const char** out_data, int64_t* out_len) {
  WCHAR* cpath = winWide(path, path_len);
  if (cpath == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  WCHAR buffer[32768];
  const DWORD length =
      GetFullPathNameW(cpath, (DWORD)(sizeof(buffer) / sizeof(buffer[0])), buffer, NULL);
  free(cpath);
  if (length == 0 || length >= sizeof(buffer) / sizeof(buffer[0])) {
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)length, out_data, out_len);
}

void sere_win_foreground_title(const char** out_data, int64_t* out_len) {
  sere_win_window_text((void*)GetForegroundWindow(), out_data, out_len);
}

int32_t sere_win_virtual_screen_width(void) { return GetSystemMetrics(SM_CXVIRTUALSCREEN); }
int32_t sere_win_virtual_screen_height(void) { return GetSystemMetrics(SM_CYVIRTUALSCREEN); }
int32_t sere_win_key_down(int32_t virtual_key) {
  return (GetAsyncKeyState((int)virtual_key) & 0x8000) != 0 ? 1 : 0;
}
int32_t sere_win_show_cursor(int32_t visible) { return ShowCursor(visible != 0) >= 0 ? 1 : 0; }
void sere_win_os_version(const char** out_data, int64_t* out_len) {
  const char* value = "Windows";
  char* copy = toCString(value, 7);
  outOwned(copy, copy == NULL ? 0 : 7, out_data, out_len);
}

/* ---- handles ---- */

int32_t sere_win_handle_valid(void* handle) {
  return handle != NULL && handle != INVALID_HANDLE_VALUE ? 1 : 0;
}

int32_t sere_win_handle_is_null(void* handle) { return handle == NULL ? 1 : 0; }

int32_t sere_win_close_handle(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(CloseHandle((HANDLE)handle));
}

void* sere_win_duplicate_handle(void* handle, uint32_t access, uint32_t options) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return NULL;
  }
  HANDLE duplicate = NULL;
  if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)handle, GetCurrentProcess(), &duplicate,
                       (DWORD)access, FALSE, (DWORD)options)) {
    return NULL;
  }
  return (void*)duplicate;
}

void* sere_win_current_process_handle(void) { return (void*)GetCurrentProcess(); }
void* sere_win_current_thread_handle(void) { return (void*)GetCurrentThread(); }

void* sere_win_std_handle(int32_t which) {
  const HANDLE handle = GetStdHandle((DWORD)which);
  return handle == INVALID_HANDLE_VALUE ? NULL : (void*)handle;
}

int32_t sere_win_set_std_handle(int32_t which, void* handle) {
  return winOk(SetStdHandle((DWORD)which, (HANDLE)handle));
}

int32_t sere_win_get_handle_information(void* handle, uint32_t* out_flags) {
  DWORD flags = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE ||
      !GetHandleInformation((HANDLE)handle, &flags)) {
    if (out_flags != NULL) {
      *out_flags = 0;
    }
    return 0;
  }
  if (out_flags != NULL) {
    *out_flags = (uint32_t)flags;
  }
  return 1;
}

int32_t sere_win_set_handle_information(void* handle, uint32_t mask, uint32_t flags) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(SetHandleInformation((HANDLE)handle, (DWORD)mask, (DWORD)flags));
}

/* ---- process and thread ---- */

void* sere_win_open_process(uint32_t access, int32_t inherit, uint32_t process_id) {
  return (void*)OpenProcess((DWORD)access, inherit != 0, (DWORD)process_id);
}

int32_t sere_win_terminate_process(void* handle, uint32_t exit_code) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(TerminateProcess((HANDLE)handle, (UINT)exit_code));
}

int32_t sere_win_exit_code_process(void* handle, uint32_t* out_code) {
  DWORD code = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE ||
      !GetExitCodeProcess((HANDLE)handle, &code)) {
    if (out_code != NULL) {
      *out_code = 0;
    }
    return 0;
  }
  if (out_code != NULL) {
    *out_code = (uint32_t)code;
  }
  return 1;
}

/* ---- wait ---- */

uint32_t sere_win_wait_single(void* handle, uint32_t milliseconds) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return WAIT_FAILED;
  }
  return (uint32_t)WaitForSingleObject((HANDLE)handle, (DWORD)milliseconds);
}

uint32_t sere_win_wait_multiple(void* handles, int32_t wait_all, uint32_t milliseconds) {
  const SereList* list = (const SereList*)handles;
  if (list == NULL || list->data == NULL || list->len <= 0 || list->len > 64 ||
      list->stride != (int64_t)sizeof(void*)) {
    return WAIT_FAILED;
  }
  return (uint32_t)WaitForMultipleObjects((DWORD)list->len, (const HANDLE*)list->data,
                                          wait_all != 0, (DWORD)milliseconds);
}

/* ---- synchronization ---- */

void* sere_win_event_create(int32_t manual_reset, int32_t initial_state) {
  return (void*)CreateEventW(NULL, manual_reset != 0, initial_state != 0, NULL);
}

int32_t sere_win_event_set(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(SetEvent((HANDLE)handle));
}

int32_t sere_win_event_reset(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(ResetEvent((HANDLE)handle));
}

int32_t sere_win_event_pulse(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(PulseEvent((HANDLE)handle));
}

void* sere_win_mutex_create(int32_t initial_owner) {
  return (void*)CreateMutexW(NULL, initial_owner != 0, NULL);
}

int32_t sere_win_mutex_release(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(ReleaseMutex((HANDLE)handle));
}

void* sere_win_semaphore_create(int32_t initial_count, int32_t maximum_count) {
  if (initial_count < 0 || maximum_count < 0 || initial_count > maximum_count) {
    return NULL;
  }
  return (void*)CreateSemaphoreW(NULL, (LONG)initial_count, (LONG)maximum_count, NULL);
}

int32_t sere_win_semaphore_release(void* handle, int32_t release_count, int32_t* out_previous) {
  LONG previous = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || release_count < 0) {
    if (out_previous != NULL) {
      *out_previous = 0;
    }
    return 0;
  }
  if (!ReleaseSemaphore((HANDLE)handle, (LONG)release_count, &previous)) {
    if (out_previous != NULL) {
      *out_previous = 0;
    }
    return 0;
  }
  if (out_previous != NULL) {
    *out_previous = (int32_t)previous;
  }
  return 1;
}

/* ---- pipes ---- */

int32_t sere_win_create_pipe(uint32_t size, void** out_read, void** out_write) {
  HANDLE read_pipe = NULL;
  HANDLE write_pipe = NULL;
  SECURITY_ATTRIBUTES attributes;
  memset(&attributes, 0, sizeof(attributes));
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;
  if (!CreatePipe(&read_pipe, &write_pipe, &attributes, (DWORD)size)) {
    if (out_read != NULL) {
      *out_read = NULL;
    }
    if (out_write != NULL) {
      *out_write = NULL;
    }
    return 0;
  }
  if (out_read != NULL) {
    *out_read = (void*)read_pipe;
  }
  if (out_write != NULL) {
    *out_write = (void*)write_pipe;
  }
  return 1;
}

/* ---- console handles and modes ---- */

int32_t sere_win_handle_is_console(void* handle) {
  DWORD mode = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(GetConsoleMode((HANDLE)handle, &mode));
}

int32_t sere_win_console_mode(void* handle, uint32_t* out_mode) {
  DWORD mode = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE ||
      !GetConsoleMode((HANDLE)handle, &mode)) {
    if (out_mode != NULL) {
      *out_mode = 0;
    }
    return 0;
  }
  if (out_mode != NULL) {
    *out_mode = (uint32_t)mode;
  }
  return 1;
}

int32_t sere_win_set_console_mode(void* handle, uint32_t mode) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(SetConsoleMode((HANDLE)handle, (DWORD)mode));
}

/* ---- console input events ---- */

uint32_t sere_win_console_event_count(void* handle) {
  DWORD count = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE ||
      !GetNumberOfConsoleInputEvents((HANDLE)handle, &count)) {
    return 0;
  }
  return (uint32_t)count;
}

/// Flattens one `INPUT_RECORD` into the fixed `fields` buffer shared by the
/// Sere `ConsoleInputEvent` decoder. Slot 0 carries the event kind.
static int32_t winDecodeEvent(const INPUT_RECORD* record, int32_t* fields, int32_t field_count) {
  if (record == NULL || fields == NULL || field_count < 8) {
    return 0;
  }
  for (int32_t i = 0; i < field_count; i++) {
    fields[i] = 0;
  }
  switch (record->EventType) {
    case KEY_EVENT:
      fields[0] = 1;
      fields[1] = record->Event.KeyEvent.bKeyDown ? 1 : 0;
      fields[2] = (int32_t)record->Event.KeyEvent.wRepeatCount;
      fields[3] = (int32_t)record->Event.KeyEvent.wVirtualKeyCode;
      fields[4] = (int32_t)record->Event.KeyEvent.wVirtualScanCode;
      fields[5] = (int32_t)record->Event.KeyEvent.uChar.UnicodeChar;
      fields[6] = (int32_t)record->Event.KeyEvent.dwControlKeyState;
      return 1;
    case MOUSE_EVENT:
      fields[0] = 2;
      fields[1] = (int32_t)record->Event.MouseEvent.dwMousePosition.X;
      fields[2] = (int32_t)record->Event.MouseEvent.dwMousePosition.Y;
      fields[3] = (int32_t)record->Event.MouseEvent.dwButtonState;
      fields[4] = (int32_t)record->Event.MouseEvent.dwControlKeyState;
      fields[5] = (int32_t)record->Event.MouseEvent.dwEventFlags;
      return 1;
    case WINDOW_BUFFER_SIZE_EVENT:
      fields[0] = 3;
      fields[1] = (int32_t)record->Event.WindowBufferSizeEvent.dwSize.X;
      fields[2] = (int32_t)record->Event.WindowBufferSizeEvent.dwSize.Y;
      return 1;
    case FOCUS_EVENT:
      fields[0] = 4;
      fields[1] = record->Event.FocusEvent.bSetFocus ? 1 : 0;
      return 1;
    case MENU_EVENT:
      fields[0] = 5;
      fields[1] = (int32_t)record->Event.MenuEvent.dwCommandId;
      return 1;
    default:
      return 0;
  }
}

int32_t sere_win_console_read_event(void* handle, int32_t* fields, int32_t field_count) {
  INPUT_RECORD record;
  DWORD read = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  memset(&record, 0, sizeof(record));
  if (!ReadConsoleInputW((HANDLE)handle, &record, 1, &read) || read == 0) {
    return 0;
  }
  return winDecodeEvent(&record, fields, field_count);
}

int32_t sere_win_console_peek_event(void* handle, int32_t* fields, int32_t field_count) {
  INPUT_RECORD record;
  DWORD read = 0;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  memset(&record, 0, sizeof(record));
  if (!PeekConsoleInputW((HANDLE)handle, &record, 1, &read) || read == 0) {
    return 0;
  }
  return winDecodeEvent(&record, fields, field_count);
}

int32_t sere_win_console_flush_input(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(FlushConsoleInputBuffer((HANDLE)handle));
}

/* ---- console cursor and screen buffer ---- */

int32_t sere_win_console_screen_info(void* handle, int32_t* fields, int32_t field_count) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (fields == NULL || field_count < 11) {
    return 0;
  }
  for (int32_t i = 0; i < field_count; i++) {
    fields[i] = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE ||
      !GetConsoleScreenBufferInfo((HANDLE)handle, &info)) {
    return 0;
  }
  fields[0] = (int32_t)info.dwSize.X;
  fields[1] = (int32_t)info.dwSize.Y;
  fields[2] = (int32_t)info.dwCursorPosition.X;
  fields[3] = (int32_t)info.dwCursorPosition.Y;
  fields[4] = (int32_t)info.wAttributes;
  fields[5] = (int32_t)info.srWindow.Left;
  fields[6] = (int32_t)info.srWindow.Top;
  fields[7] = (int32_t)info.srWindow.Right;
  fields[8] = (int32_t)info.srWindow.Bottom;
  fields[9] = (int32_t)info.dwMaximumWindowSize.X;
  fields[10] = (int32_t)info.dwMaximumWindowSize.Y;
  return 1;
}

int32_t sere_win_console_cursor_info(void* handle, uint32_t* out_size, int32_t* out_visible) {
  CONSOLE_CURSOR_INFO info;
  memset(&info, 0, sizeof(info));
  if (handle == NULL || handle == INVALID_HANDLE_VALUE ||
      !GetConsoleCursorInfo((HANDLE)handle, &info)) {
    if (out_size != NULL) {
      *out_size = 0;
    }
    if (out_visible != NULL) {
      *out_visible = 0;
    }
    return 0;
  }
  if (out_size != NULL) {
    *out_size = (uint32_t)info.dwSize;
  }
  if (out_visible != NULL) {
    *out_visible = info.bVisible ? 1 : 0;
  }
  return 1;
}

int32_t sere_win_console_set_cursor_info(void* handle, uint32_t size, int32_t visible) {
  CONSOLE_CURSOR_INFO info;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  info.dwSize = (DWORD)size;
  info.bVisible = visible != 0;
  return winOk(SetConsoleCursorInfo((HANDLE)handle, &info));
}

int32_t sere_win_console_set_cursor_pos(void* handle, int32_t x, int32_t y) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(SetConsoleCursorPosition((HANDLE)handle, winCoord(x, y)));
}

int32_t sere_win_console_largest_size(void* handle, int32_t* out_width, int32_t* out_height) {
  COORD size;
  if (out_width != NULL) {
    *out_width = 0;
  }
  if (out_height != NULL) {
    *out_height = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  size = GetLargestConsoleWindowSize((HANDLE)handle);
  if (size.X <= 0 || size.Y <= 0) {
    return 0;
  }
  if (out_width != NULL) {
    *out_width = (int32_t)size.X;
  }
  if (out_height != NULL) {
    *out_height = (int32_t)size.Y;
  }
  return 1;
}

int32_t sere_win_console_set_buffer_size(void* handle, int32_t width, int32_t height) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || width <= 0 || height <= 0) {
    return 0;
  }
  return winOk(SetConsoleScreenBufferSize((HANDLE)handle, winCoord(width, height)));
}

int32_t sere_win_console_set_window_rect(
    void* handle, int32_t absolute, int32_t left, int32_t top, int32_t right, int32_t bottom) {
  SMALL_RECT rect;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  rect = winRect(left, top, right, bottom);
  return winOk(SetConsoleWindowInfo((HANDLE)handle, absolute != 0, &rect));
}

int32_t sere_win_set_console_text_attribute(void* handle, int32_t attribute) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return winOk(SetConsoleTextAttribute((HANDLE)handle, (WORD)attribute));
}

/* ---- positioned screen-buffer I/O ---- */

int32_t sere_win_fill_console_chars(
    void* handle, int32_t char_code, uint32_t count, int32_t x, int32_t y, uint32_t* out_written) {
  DWORD written = 0;
  if (out_written != NULL) {
    *out_written = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  if (!FillConsoleOutputCharacterW((HANDLE)handle, (WCHAR)char_code, (DWORD)count, winCoord(x, y),
                                   &written)) {
    return 0;
  }
  if (out_written != NULL) {
    *out_written = (uint32_t)written;
  }
  return 1;
}

int32_t sere_win_fill_console_attrs(
    void* handle, int32_t attribute, uint32_t count, int32_t x, int32_t y, uint32_t* out_written) {
  DWORD written = 0;
  if (out_written != NULL) {
    *out_written = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  if (!FillConsoleOutputAttribute((HANDLE)handle, (WORD)attribute, (DWORD)count, winCoord(x, y),
                                  &written)) {
    return 0;
  }
  if (out_written != NULL) {
    *out_written = (uint32_t)written;
  }
  return 1;
}

int32_t sere_win_write_console_output_chars(
    void* handle, const char* data, int64_t data_len, int32_t x, int32_t y, uint32_t* out_written) {
  DWORD written = 0;
  if (out_written != NULL) {
    *out_written = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  WCHAR* wide = winWide(data, data_len);
  if (wide == NULL) {
    return 0;
  }
  const int32_t chars = (int32_t)wcslen(wide);
  const BOOL ok = chars > 0 && WriteConsoleOutputCharacterW((HANDLE)handle, wide, (DWORD)chars,
                                                            winCoord(x, y), &written);
  free(wide);
  if (!ok) {
    return 0;
  }
  if (out_written != NULL) {
    *out_written = (uint32_t)written;
  }
  return 1;
}

int32_t sere_win_write_console_output_attrs(
    void* handle, void* attrs, int32_t x, int32_t y, uint32_t* out_written) {
  DWORD written = 0;
  const SereList* list = (const SereList*)attrs;
  if (out_written != NULL) {
    *out_written = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || list == NULL || list->data == NULL ||
      list->len <= 0 || list->stride != (int64_t)sizeof(WORD)) {
    return 0;
  }
  if (!WriteConsoleOutputAttribute((HANDLE)handle, (const WORD*)list->data, (DWORD)list->len,
                                   winCoord(x, y), &written)) {
    return 0;
  }
  if (out_written != NULL) {
    *out_written = (uint32_t)written;
  }
  return 1;
}

void sere_win_read_console_output_chars(
    void* handle, int32_t x, int32_t y, uint32_t count, const char** out_data, int64_t* out_len) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || count == 0) {
    outEmpty(out_data, out_len);
    return;
  }
  if (count > 1 << 20) {
    count = 1 << 20;
  }
  WCHAR* buffer = (WCHAR*)calloc((size_t)count + 1, sizeof(WCHAR));
  if (buffer == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  DWORD read = 0;
  if (!ReadConsoleOutputCharacterW((HANDLE)handle, buffer, (DWORD)count, winCoord(x, y), &read) ||
      read == 0) {
    free(buffer);
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)read, out_data, out_len);
  free(buffer);
}

int32_t sere_win_read_console_output_attrs(void* handle, int32_t x, int32_t y, void* attrs) {
  SereList* list = (SereList*)attrs;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || list == NULL || list->data == NULL ||
      list->len <= 0 || list->stride != (int64_t)sizeof(WORD)) {
    return 0;
  }
  DWORD read = 0;
  if (!ReadConsoleOutputAttribute((HANDLE)handle, (WORD*)list->data, (DWORD)list->len,
                                  winCoord(x, y), &read)) {
    return 0;
  }
  list->len = (int64_t)read;
  return 1;
}

int32_t sere_win_scroll_console(void* handle,
                                int32_t left,
                                int32_t top,
                                int32_t right,
                                int32_t bottom,
                                int32_t dx,
                                int32_t dy,
                                int32_t fill_char,
                                int32_t fill_attr) {
  SMALL_RECT region;
  SMALL_RECT clip;
  CHAR_INFO fill;
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  region = winRect(left, top, right, bottom);
  clip = region;
  fill.Char.UnicodeChar = (WCHAR)fill_char;
  fill.Attributes = (WORD)fill_attr;
  return winOk(ScrollConsoleScreenBufferW((HANDLE)handle, &region, &clip, winCoord(dx, dy), &fill));
}

/* ---- direct console I/O ---- */

int32_t sere_win_write_console(
    void* handle, const char* data, int64_t data_len, uint32_t* out_written) {
  DWORD written = 0;
  if (out_written != NULL) {
    *out_written = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  WCHAR* wide = winWide(data, data_len);
  if (wide == NULL) {
    return 0;
  }
  const int32_t chars = (int32_t)wcslen(wide);
  const BOOL ok = WriteConsoleW((HANDLE)handle, wide, (DWORD)chars, &written, NULL);
  free(wide);
  if (!ok) {
    return 0;
  }
  if (out_written != NULL) {
    *out_written = (uint32_t)written;
  }
  return 1;
}

void sere_win_read_console(
    void* handle, uint32_t max_chars, const char** out_data, int64_t* out_len) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || max_chars == 0) {
    outEmpty(out_data, out_len);
    return;
  }
  if (max_chars > 1 << 20) {
    max_chars = 1 << 20;
  }
  WCHAR* buffer = (WCHAR*)calloc((size_t)max_chars + 1, sizeof(WCHAR));
  if (buffer == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  DWORD read = 0;
  if (!ReadConsoleW((HANDLE)handle, buffer, (DWORD)max_chars, &read, NULL) || read == 0) {
    free(buffer);
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)read, out_data, out_len);
  free(buffer);
}

/* ---- raw handle I/O ---- */

int32_t sere_win_handle_write(void* handle, void* data, uint32_t* out_written) {
  const SereList* list = (const SereList*)data;
  DWORD written = 0;
  if (out_written != NULL) {
    *out_written = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || list == NULL || list->data == NULL ||
      list->stride != 1 || list->len < 0 || list->len > 0x7FFFFFFF) {
    return 0;
  }
  if (!WriteFile((HANDLE)handle, list->data, (DWORD)list->len, &written, NULL)) {
    return 0;
  }
  if (out_written != NULL) {
    *out_written = (uint32_t)written;
  }
  return 1;
}

void* sere_win_handle_read(void* handle, int64_t count) {
  if (count < 0) {
    count = 0;
  }
  if (count > 1 << 30) {
    count = 1 << 30;
  }
  SereList* list = (SereList*)sere_array_new(1, count);
  if (list == NULL) {
    return sere_array_new(1, 0);
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || count == 0) {
    list->len = 0;
    return list;
  }
  DWORD read = 0;
  if (!ReadFile((HANDLE)handle, list->data, (DWORD)count, &read, NULL)) {
    read = 0;
  }
  list->len = (int64_t)read;
  return list;
}

/* ---- handle kinds ---- */

uint32_t sere_win_get_file_type(void* handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return FILE_TYPE_UNKNOWN;
  }
  return (uint32_t)GetFileType((HANDLE)handle);
}

/* ---- console title, allocation, code pages ---- */

static void outWinTitle(DWORD(WINAPI* fn)(LPWSTR, DWORD), const char** out_data, int64_t* out_len) {
  const DWORD size = fn(NULL, 0);
  if (size == 0) {
    outEmpty(out_data, out_len);
    return;
  }
  WCHAR* buffer = (WCHAR*)calloc((size_t)size + 1, sizeof(WCHAR));
  if (buffer == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  const DWORD length = fn(buffer, size);
  if (length == 0) {
    free(buffer);
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, (int32_t)length, out_data, out_len);
  free(buffer);
}

void sere_win_console_title(const char** out_data, int64_t* out_len) {
  outWinTitle(GetConsoleTitleW, out_data, out_len);
}

int32_t sere_win_set_console_title(const char* data, int64_t data_len) {
  WCHAR* title = winWide(data, data_len);
  if (title == NULL) {
    return 0;
  }
  const BOOL ok = SetConsoleTitleW(title);
  free(title);
  return winOk(ok);
}

void sere_win_console_original_title(const char** out_data, int64_t* out_len) {
  outWinTitle(GetConsoleOriginalTitleW, out_data, out_len);
}

int32_t sere_win_alloc_console(void) { return winOk(AllocConsole()); }
int32_t sere_win_free_console(void) { return winOk(FreeConsole()); }
int32_t sere_win_attach_console(uint32_t process_id) { return winOk(AttachConsole((DWORD)process_id)); }
void* sere_win_console_window(void) { return (void*)GetConsoleWindow(); }

uint32_t sere_win_get_console_cp(void) { return (uint32_t)GetConsoleCP(); }
int32_t sere_win_set_console_cp(uint32_t code_page) { return winOk(SetConsoleCP((UINT)code_page)); }
uint32_t sere_win_get_console_output_cp(void) { return (uint32_t)GetConsoleOutputCP(); }
int32_t sere_win_set_console_output_cp(uint32_t code_page) {
  return winOk(SetConsoleOutputCP((UINT)code_page));
}

/* ---- keyboard ---- */

int32_t sere_win_get_key_state(int32_t virtual_key) {
  return (int32_t)(GetKeyState((int)virtual_key) & 0xFFFF);
}

int32_t sere_win_key_state_raw(int32_t virtual_key) {
  return (int32_t)GetAsyncKeyState((int)virtual_key);
}

int32_t sere_win_get_keyboard_state(void* state) {
  SereList* list = (SereList*)state;
  if (list == NULL || list->data == NULL || list->len < 256 || list->stride != 1) {
    return 0;
  }
  if (!GetKeyboardState((BYTE*)list->data)) {
    return 0;
  }
  list->len = 256;
  return 1;
}

void sere_win_to_unicode(int32_t virtual_key,
                         int32_t scan_code,
                         void* state,
                         int32_t flags,
                         const char** out_data,
                         int64_t* out_len) {
  const SereList* list = (const SereList*)state;
  if (list == NULL || list->data == NULL || list->len < 256 || list->stride != 1) {
    outEmpty(out_data, out_len);
    return;
  }
  WCHAR buffer[16];
  const int count =
      ToUnicode((UINT)virtual_key, (UINT)scan_code, (const BYTE*)list->data, buffer, 16,
                (UINT)flags);
  if (count <= 0) {
    outEmpty(out_data, out_len);
    return;
  }
  outWinWide(buffer, count, out_data, out_len);
}

uint32_t sere_win_map_virtual_key(uint32_t code, uint32_t map_type) {
  return (uint32_t)MapVirtualKeyW((UINT)code, (UINT)map_type);
}

/* ---- window handles ---- */

void* sere_win_active_window(void) { return (void*)GetActiveWindow(); }
void* sere_win_focus_window(void) { return (void*)GetFocus(); }

int32_t sere_win_set_foreground_window(void* window) {
  if (window == NULL) {
    return 0;
  }
  return winOk(SetForegroundWindow((HWND)window));
}

int32_t sere_win_bring_window_to_top(void* window) {
  if (window == NULL) {
    return 0;
  }
  return winOk(BringWindowToTop((HWND)window));
}

int32_t sere_win_show_window(void* window, int32_t command) {
  if (window == NULL) {
    return 0;
  }
  return (int32_t)ShowWindow((HWND)window, (int)command);
}

int32_t sere_win_is_window(void* window) { return window != NULL ? winOk(IsWindow((HWND)window)) : 0; }

int32_t sere_win_is_window_visible(void* window) {
  return window != NULL ? winOk(IsWindowVisible((HWND)window)) : 0;
}

int32_t sere_win_is_iconic(void* window) {
  return window != NULL ? winOk(IsIconic((HWND)window)) : 0;
}

int32_t sere_win_is_zoomed(void* window) {
  return window != NULL ? winOk(IsZoomed((HWND)window)) : 0;
}

int32_t sere_win_set_window_text(void* window, const char* data, int64_t data_len) {
  if (window == NULL) {
    return 0;
  }
  WCHAR* text = winWide(data, data_len);
  if (text == NULL) {
    return 0;
  }
  const BOOL ok = SetWindowTextW((HWND)window, text);
  free(text);
  return winOk(ok);
}

static int32_t winRectFields(BOOL(WINAPI* fn)(HWND, LPRECT), void* window, int32_t* fields,
                             int32_t field_count) {
  RECT rect;
  if (fields == NULL || field_count < 4 || window == NULL) {
    return 0;
  }
  for (int32_t i = 0; i < field_count; i++) {
    fields[i] = 0;
  }
  if (!fn((HWND)window, &rect)) {
    return 0;
  }
  fields[0] = (int32_t)rect.left;
  fields[1] = (int32_t)rect.top;
  fields[2] = (int32_t)rect.right;
  fields[3] = (int32_t)rect.bottom;
  return 1;
}

int32_t sere_win_get_window_rect(void* window, int32_t* fields, int32_t field_count) {
  return winRectFields(GetWindowRect, window, fields, field_count);
}

int32_t sere_win_get_client_rect(void* window, int32_t* fields, int32_t field_count) {
  return winRectFields(GetClientRect, window, fields, field_count);
}

int32_t sere_win_move_window(
    void* window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint) {
  if (window == NULL) {
    return 0;
  }
  return winOk(MoveWindow((HWND)window, x, y, width, height, repaint != 0));
}

/// Maps the stable anchor codes 0..3 onto the `HWND_TOP*` z-order values.
static HWND winAnchor(int32_t anchor) {
  switch (anchor) {
    case 0:
      return HWND_TOP;
    case 1:
      return HWND_BOTTOM;
    case 2:
      return HWND_TOPMOST;
    case 3:
      return HWND_NOTOPMOST;
    default:
      return HWND_TOP;
  }
}

int32_t sere_win_set_window_pos(
    void* window, int32_t anchor, int32_t x, int32_t y, int32_t width, int32_t height,
    uint32_t flags) {
  if (window == NULL) {
    return 0;
  }
  return winOk(SetWindowPos((HWND)window, winAnchor(anchor), x, y, width, height, (UINT)flags));
}

int32_t sere_win_update_window(void* window) {
  if (window == NULL) {
    return 0;
  }
  return winOk(UpdateWindow((HWND)window));
}

int32_t sere_win_destroy_window(void* window) {
  if (window == NULL) {
    return 0;
  }
  return winOk(DestroyWindow((HWND)window));
}

int32_t sere_win_post_message(void* window, uint32_t message, uint64_t wparam, int64_t lparam) {
  return winOk(PostMessageW((HWND)window, (UINT)message, (WPARAM)wparam, (LPARAM)lparam));
}

int64_t sere_win_send_message(void* window, uint32_t message, uint64_t wparam, int64_t lparam) {
  if (window == NULL) {
    return 0;
  }
  return (int64_t)SendMessageW((HWND)window, (UINT)message, (WPARAM)wparam, (LPARAM)lparam);
}

/* ---- window class and creation ---- */

/// Unpacks the Sere callable fat pair `{code, env}`. Only top-level,
/// capture-free callbacks are valid as a `WNDPROC`; the environment slot is
/// ignored because Win32 gives us nowhere to carry it.
static void* winWindowProc(void* fat) {
  if (fat == NULL) {
    return NULL;
  }
  return ((void**)fat)[0];
}

uint32_t sere_win_register_window_class(const char* name,
                                        int64_t name_len,
                                        uint32_t style,
                                        void* wnd_proc,
                                        int32_t class_extra,
                                        int32_t window_extra,
                                        void* background) {
  // A NULL callback selects the default window procedure. Sere cannot produce a
  // native WNDPROC pointer yet, so this is the only usable path from Sere today;
  // a non-NULL value must be the Sere callable fat pair described above.
  void* proc = wnd_proc == NULL ? (void*)DefWindowProcW : winWindowProc(wnd_proc);
  if (proc == NULL) {
    return 0;
  }
  WCHAR* wide = winWide(name, name_len);
  if (wide == NULL) {
    return 0;
  }
  WNDCLASSEXW window_class;
  memset(&window_class, 0, sizeof(window_class));
  window_class.cbSize = sizeof(window_class);
  window_class.style = (UINT)style;
  window_class.lpfnWndProc = (WNDPROC)proc;
  window_class.cbClsExtra = class_extra;
  window_class.cbWndExtra = window_extra;
  window_class.hInstance = GetModuleHandleW(NULL);
  window_class.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
  window_class.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
  window_class.hIconSm = window_class.hIcon;
  window_class.hbrBackground = (HBRUSH)background;
  window_class.lpszClassName = wide;
  const ATOM atom = RegisterClassExW(&window_class);
  free(wide);
  return (uint32_t)atom;
}

int32_t sere_win_unregister_window_class(const char* name, int64_t name_len) {
  WCHAR* wide = winWide(name, name_len);
  if (wide == NULL) {
    return 0;
  }
  const BOOL ok = UnregisterClassW(wide, GetModuleHandleW(NULL));
  free(wide);
  return winOk(ok);
}

void* sere_win_create_window(const char* class_name,
                             int64_t class_len,
                             const char* window_name,
                             int64_t window_len,
                             uint32_t style,
                             uint32_t ex_style,
                             int32_t x,
                             int32_t y,
                             int32_t width,
                             int32_t height,
                             void* parent,
                             void* menu) {
  WCHAR* wide_class = winWide(class_name, class_len);
  WCHAR* wide_title = winWide(window_name, window_len);
  if (wide_class == NULL) {
    free(wide_class);
    free(wide_title);
    return NULL;
  }
  const HWND window =
      CreateWindowExW((DWORD)ex_style, wide_class, wide_title == NULL ? L"" : wide_title,
                      (DWORD)style, x, y, width, height, (HWND)parent, (HMENU)menu,
                      GetModuleHandleW(NULL), NULL);
  free(wide_class);
  free(wide_title);
  return (void*)window;
}

int64_t sere_win_def_window_proc(void* window, uint32_t message, uint64_t wparam, int64_t lparam) {
  return (int64_t)DefWindowProcW((HWND)window, (UINT)message, (WPARAM)wparam, (LPARAM)lparam);
}

/* ---- window messages ---- */

static void winFillMessage(const MSG* msg, int32_t* fields, int32_t field_count, void** out_window,
                           int64_t* out_wparam, int64_t* out_lparam) {
  if (fields != NULL && field_count >= 4) {
    fields[0] = 0;
    fields[1] = 0;
    fields[2] = 0;
    fields[3] = 0;
    if (msg != NULL) {
      fields[0] = (int32_t)msg->message;
      fields[1] = (int32_t)msg->time;
      fields[2] = (int32_t)msg->pt.x;
      fields[3] = (int32_t)msg->pt.y;
    }
  }
  if (out_window != NULL) {
    *out_window = msg == NULL ? NULL : (void*)msg->hwnd;
  }
  if (out_wparam != NULL) {
    *out_wparam = msg == NULL ? 0 : (int64_t)msg->wParam;
  }
  if (out_lparam != NULL) {
    *out_lparam = msg == NULL ? 0 : (int64_t)msg->lParam;
  }
}

static void winBuildMessage(MSG* msg, const int32_t* fields, int32_t field_count, void* window,
                            uint64_t wparam, int64_t lparam) {
  memset(msg, 0, sizeof(*msg));
  msg->hwnd = (HWND)window;
  if (fields != NULL && field_count >= 4) {
    msg->message = (UINT)fields[0];
    msg->time = (DWORD)fields[1];
    msg->pt.x = fields[2];
    msg->pt.y = fields[3];
  }
  msg->wParam = (WPARAM)wparam;
  msg->lParam = (LPARAM)lparam;
}

int32_t sere_win_get_message(
    int32_t* fields, int32_t field_count, void** out_window, int64_t* out_wparam,
    int64_t* out_lparam) {
  MSG msg;
  memset(&msg, 0, sizeof(msg));
  const BOOL result = GetMessageW(&msg, NULL, 0, 0);
  winFillMessage(&msg, fields, field_count, out_window, out_wparam, out_lparam);
  if (result == -1) {
    return SERE_WIN_MESSAGE_ERROR;
  }
  return result == 0 ? SERE_WIN_MESSAGE_QUIT : SERE_WIN_MESSAGE_READY;
}

int32_t sere_win_peek_message(int32_t* fields,
                              int32_t field_count,
                              void** out_window,
                              int64_t* out_wparam,
                              int64_t* out_lparam,
                              uint32_t min_message,
                              uint32_t max_message,
                              uint32_t flags) {
  MSG msg;
  memset(&msg, 0, sizeof(msg));
  const BOOL found = PeekMessageW(&msg, NULL, (UINT)min_message, (UINT)max_message, (UINT)flags);
  if (!found) {
    winFillMessage(NULL, fields, field_count, out_window, out_wparam, out_lparam);
    return 0;
  }
  winFillMessage(&msg, fields, field_count, out_window, out_wparam, out_lparam);
  return 1;
}

int32_t sere_win_translate_message(
    int32_t* fields, int32_t field_count, void* window, uint64_t wparam, int64_t lparam) {
  MSG msg;
  winBuildMessage(&msg, fields, field_count, window, wparam, lparam);
  return winOk(TranslateMessage(&msg));
}

int64_t sere_win_dispatch_message(
    int32_t* fields, int32_t field_count, void* window, uint64_t wparam, int64_t lparam) {
  MSG msg;
  winBuildMessage(&msg, fields, field_count, window, wparam, lparam);
  return (int64_t)DispatchMessageW(&msg);
}

void sere_win_post_quit_message(int32_t exit_code) { PostQuitMessage(exit_code); }

/* ---- pseudoconsole ---- */

typedef HRESULT(WINAPI* SereWinCreatePseudoConsoleFn)(COORD, HANDLE, HANDLE, DWORD, HANDLE*);
typedef HRESULT(WINAPI* SereWinResizePseudoConsoleFn)(HANDLE, COORD);
typedef HRESULT(WINAPI* SereWinClosePseudoConsoleFn)(HANDLE);

/// Resolves the ConPTY entry points once so older hosts never fail to load.
static int32_t winConptyResolve(SereWinCreatePseudoConsoleFn* create,
                                SereWinResizePseudoConsoleFn* resize,
                                SereWinClosePseudoConsoleFn* close) {
  static SereWinCreatePseudoConsoleFn cached_create = NULL;
  static SereWinResizePseudoConsoleFn cached_resize = NULL;
  static SereWinClosePseudoConsoleFn cached_close = NULL;
  static int32_t resolved = 0;
  if (!resolved) {
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 != NULL) {
      cached_create =
          (SereWinCreatePseudoConsoleFn)(void*)GetProcAddress(kernel32, "CreatePseudoConsole");
      cached_resize =
          (SereWinResizePseudoConsoleFn)(void*)GetProcAddress(kernel32, "ResizePseudoConsole");
      cached_close =
          (SereWinClosePseudoConsoleFn)(void*)GetProcAddress(kernel32, "ClosePseudoConsole");
    }
    resolved = 1;
  }
  if (create != NULL) {
    *create = cached_create;
  }
  if (resize != NULL) {
    *resize = cached_resize;
  }
  if (close != NULL) {
    *close = cached_close;
  }
  return cached_create != NULL && cached_resize != NULL && cached_close != NULL ? 1 : 0;
}

int32_t sere_win_conpty_available(void) { return winConptyResolve(NULL, NULL, NULL); }

void* sere_win_pseudo_console_create(
    int32_t columns, int32_t rows, void* input_read, void* output_write) {
  SereWinCreatePseudoConsoleFn create = NULL;
  if (!winConptyResolve(&create, NULL, NULL) || columns <= 0 || rows <= 0 ||
      input_read == NULL || output_write == NULL) {
    return NULL;
  }
  HANDLE pseudo = NULL;
  const HRESULT result = create(winCoord(columns, rows), (HANDLE)input_read,
                                (HANDLE)output_write, 0, &pseudo);
  if (result < 0 || pseudo == NULL) {
    return NULL;
  }
  return (void*)pseudo;
}

int32_t sere_win_pseudo_console_resize(void* pseudo, int32_t columns, int32_t rows) {
  SereWinResizePseudoConsoleFn resize = NULL;
  if (!winConptyResolve(NULL, &resize, NULL) || pseudo == NULL || columns <= 0 || rows <= 0) {
    return 0;
  }
  return resize((HANDLE)pseudo, winCoord(columns, rows)) >= 0 ? 1 : 0;
}

void sere_win_pseudo_console_close(void* pseudo) {
  SereWinClosePseudoConsoleFn close = NULL;
  if (!winConptyResolve(NULL, NULL, &close) || pseudo == NULL) {
    return;
  }
  close((HANDLE)pseudo);
}

#endif
