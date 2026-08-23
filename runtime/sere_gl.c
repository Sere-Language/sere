/// @file sere_gl.c
/// OpenGL 2.1+ (compat) with a Win32 WGL window. Shader, buffer, texture, and
/// framebuffer entry points are loaded from the driver. Other hosts stub out.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"
#include "sere_icon.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#pragma comment(lib, "opengl32")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#define SERE_HAS_GL 1
#endif

static void outEmpty(const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = "";
  }
  if (out_len != NULL) {
    *out_len = 0;
  }
}

static void outCString(const char* text, const char** out_data, int64_t* out_len) {
  if (text == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  if (out_data != NULL) {
    *out_data = text;
  }
  if (out_len != NULL) {
    *out_len = (int64_t)strlen(text);
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

static SereList* asList(void* list) { return (SereList*)list; }

#ifndef SERE_HAS_GL

int32_t sere_gl_available(void) { return 0; }
void* sere_gl_window_new(const char* title, int64_t title_len, int32_t width, int32_t height) {
  return sere_gl_window_new_ex(title, title_len, width, height, 0);
}
void* sere_gl_window_new_ex(const char* title, int64_t title_len, int32_t width, int32_t height,
                            int32_t flags) {
  (void)title;
  (void)title_len;
  (void)width;
  (void)height;
  (void)flags;
  return NULL;
}
int32_t sere_gl_window_poll(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_wait(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_should_close(void* window) {
  (void)window;
  return 1;
}
void sere_gl_window_set_should_close(void* window, int32_t value) {
  (void)window;
  (void)value;
}
int32_t sere_gl_window_is_open(void* window) {
  (void)window;
  return 0;
}
void sere_gl_window_swap(void* window) { (void)window; }
void sere_gl_window_make_current(void* window) { (void)window; }
void sere_gl_window_destroy(void* window) { (void)window; }
int32_t sere_gl_window_width(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_height(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_fb_width(void* window) { return sere_gl_window_width(window); }
int32_t sere_gl_window_fb_height(void* window) { return sere_gl_window_height(window); }
int32_t sere_gl_window_x(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_y(void* window) {
  (void)window;
  return 0;
}
void sere_gl_window_set_title(void* window, const char* title, int64_t title_len) {
  (void)window;
  (void)title;
  (void)title_len;
}
void sere_gl_window_title(void* window, const char** out_data, int64_t* out_len) {
  (void)window;
  outEmpty(out_data, out_len);
}
void sere_gl_window_set_vsync(void* window, int32_t enabled) {
  (void)window;
  (void)enabled;
}
int32_t sere_gl_window_vsync(void* window) {
  (void)window;
  return 0;
}
void sere_gl_window_show_cursor(void* window, int32_t show) {
  (void)window;
  (void)show;
}
void sere_gl_window_set_cursor_mode(void* window, int32_t mode) {
  (void)window;
  (void)mode;
}
int32_t sere_gl_window_cursor_mode(void* window) {
  (void)window;
  return 0;
}
void sere_gl_window_set_cursor_pos(void* window, int32_t x, int32_t y) {
  (void)window;
  (void)x;
  (void)y;
}
void sere_gl_window_set_size(void* window, int32_t width, int32_t height) {
  (void)window;
  (void)width;
  (void)height;
}
void sere_gl_window_set_pos(void* window, int32_t x, int32_t y) {
  (void)window;
  (void)x;
  (void)y;
}
void sere_gl_window_set_min_size(void* window, int32_t width, int32_t height) {
  (void)window;
  (void)width;
  (void)height;
}
void sere_gl_window_set_max_size(void* window, int32_t width, int32_t height) {
  (void)window;
  (void)width;
  (void)height;
}
void sere_gl_window_show(void* window) { (void)window; }
void sere_gl_window_hide(void* window) { (void)window; }
void sere_gl_window_minimize(void* window) { (void)window; }
void sere_gl_window_maximize(void* window) { (void)window; }
void sere_gl_window_restore(void* window) { (void)window; }
void sere_gl_window_focus(void* window) { (void)window; }
void sere_gl_window_request_attention(void* window) { (void)window; }
void sere_gl_window_set_fullscreen(void* window, int32_t enabled) {
  (void)window;
  (void)enabled;
}
int32_t sere_gl_window_fullscreen(void* window) {
  (void)window;
  return 0;
}
void sere_gl_window_set_resizable(void* window, int32_t enabled) {
  (void)window;
  (void)enabled;
}
void sere_gl_window_set_decorated(void* window, int32_t enabled) {
  (void)window;
  (void)enabled;
}
void sere_gl_window_set_floating(void* window, int32_t enabled) {
  (void)window;
  (void)enabled;
}
void sere_gl_window_set_opacity(void* window, double opacity) {
  (void)window;
  (void)opacity;
}
int32_t sere_gl_window_resizable(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_decorated(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_floating(void* window) {
  (void)window;
  return 0;
}
double sere_gl_window_opacity(void* window) {
  (void)window;
  return 1.0;
}
double sere_gl_window_content_scale(void* window) {
  (void)window;
  return 1.0;
}
int32_t sere_gl_window_focused(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_minimized(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_maximized(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_visible(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_hovered(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_resized(void* window) {
  (void)window;
  return 0;
}
double sere_gl_window_time(void* window) {
  (void)window;
  return 0.0;
}
double sere_gl_window_dt(void* window) {
  (void)window;
  return 0.0;
}
int32_t sere_gl_window_key(void* window, int32_t vk) {
  (void)window;
  (void)vk;
  return 0;
}
int32_t sere_gl_window_key_pressed(void* window, int32_t vk) {
  (void)window;
  (void)vk;
  return 0;
}
int32_t sere_gl_window_key_released(void* window, int32_t vk) {
  (void)window;
  (void)vk;
  return 0;
}
int32_t sere_gl_window_mods(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_char(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_x(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_y(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_dx(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_dy(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_button(void* window, int32_t button) {
  (void)window;
  (void)button;
  return 0;
}
int32_t sere_gl_window_mouse_button_pressed(void* window, int32_t button) {
  (void)window;
  (void)button;
  return 0;
}
int32_t sere_gl_window_mouse_button_released(void* window, int32_t button) {
  (void)window;
  (void)button;
  return 0;
}
int32_t sere_gl_window_wheel(void* window) {
  (void)window;
  return 0;
}
void sere_gl_window_clipboard_get(void* window, const char** out_data, int64_t* out_len) {
  (void)window;
  outEmpty(out_data, out_len);
}
void sere_gl_window_clipboard_set(void* window, const char* data, int64_t len) {
  (void)window;
  (void)data;
  (void)len;
}
int32_t sere_gl_screen_width(void) { return 0; }
int32_t sere_gl_screen_height(void) { return 0; }
void sere_gl_clear_color(double r, double g, double b, double a) {
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
void sere_gl_clear(uint32_t mask) { (void)mask; }
void sere_gl_viewport(int32_t x, int32_t y, int32_t width, int32_t height) {
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}
void sere_gl_scissor(int32_t x, int32_t y, int32_t width, int32_t height) {
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}
void sere_gl_enable(uint32_t cap) { (void)cap; }
void sere_gl_disable(uint32_t cap) { (void)cap; }
void sere_gl_blend_func(uint32_t src, uint32_t dst) {
  (void)src;
  (void)dst;
}
void sere_gl_blend_func_separate(uint32_t src_rgb, uint32_t dst_rgb, uint32_t src_a, uint32_t dst_a) {
  (void)src_rgb;
  (void)dst_rgb;
  (void)src_a;
  (void)dst_a;
}
void sere_gl_blend_equation(uint32_t mode) { (void)mode; }
void sere_gl_blend_equation_separate(uint32_t rgb, uint32_t alpha) {
  (void)rgb;
  (void)alpha;
}
void sere_gl_blend_color(double r, double g, double b, double a) {
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
void sere_gl_depth_func(uint32_t func) { (void)func; }
void sere_gl_depth_mask(int32_t enabled) { (void)enabled; }
void sere_gl_depth_range(double near_z, double far_z) {
  (void)near_z;
  (void)far_z;
}
void sere_gl_color_mask(int32_t r, int32_t g, int32_t b, int32_t a) {
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
void sere_gl_cull_face(uint32_t mode) { (void)mode; }
void sere_gl_front_face(uint32_t mode) { (void)mode; }
void sere_gl_polygon_mode(uint32_t face, uint32_t mode) {
  (void)face;
  (void)mode;
}
void sere_gl_polygon_offset(double factor, double units) {
  (void)factor;
  (void)units;
}
void sere_gl_line_width(double width) { (void)width; }
void sere_gl_point_size(double size) { (void)size; }
void sere_gl_pixel_store(uint32_t pname, int32_t value) {
  (void)pname;
  (void)value;
}
void sere_gl_hint(uint32_t target, uint32_t mode) {
  (void)target;
  (void)mode;
}
int32_t sere_gl_is_enabled(uint32_t cap) {
  (void)cap;
  return 0;
}
void sere_gl_logic_op(uint32_t op) { (void)op; }
void sere_gl_stencil_func(uint32_t func, int32_t ref, uint32_t mask) {
  (void)func;
  (void)ref;
  (void)mask;
}
void sere_gl_stencil_op(uint32_t sfail, uint32_t dpfail, uint32_t dppass) {
  (void)sfail;
  (void)dpfail;
  (void)dppass;
}
void sere_gl_stencil_mask(uint32_t mask) { (void)mask; }
void sere_gl_clear_depth(double depth) { (void)depth; }
void sere_gl_clear_stencil(int32_t s) { (void)s; }
void sere_gl_draw_buffer(uint32_t buf) { (void)buf; }
void sere_gl_read_buffer(uint32_t buf) { (void)buf; }
void sere_gl_sample_coverage(double value, int32_t invert) {
  (void)value;
  (void)invert;
}
double sere_gl_get_float(uint32_t pname) {
  (void)pname;
  return 0.0;
}
int32_t sere_gl_validate_program(uint32_t program) {
  (void)program;
  return 0;
}
void sere_gl_finish(void) {}
void sere_gl_flush(void) {}
void sere_gl_begin(uint32_t mode) { (void)mode; }
void sere_gl_end(void) {}
void sere_gl_vertex2f(double x, double y) {
  (void)x;
  (void)y;
}
void sere_gl_vertex3f(double x, double y, double z) {
  (void)x;
  (void)y;
  (void)z;
}
void sere_gl_color3f(double r, double g, double b) {
  (void)r;
  (void)g;
  (void)b;
}
void sere_gl_color4f(double r, double g, double b, double a) {
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
void sere_gl_texcoord2f(double u, double v) {
  (void)u;
  (void)v;
}
void sere_gl_normal3f(double x, double y, double z) {
  (void)x;
  (void)y;
  (void)z;
}
uint32_t sere_gl_get_error(void) { return 0; }
int32_t sere_gl_get_integer(uint32_t pname) {
  (void)pname;
  return 0;
}
void sere_gl_get_string(uint32_t name, const char** out_data, int64_t* out_len) {
  (void)name;
  outEmpty(out_data, out_len);
}
uint32_t sere_gl_create_shader(uint32_t kind) {
  (void)kind;
  return 0;
}
void sere_gl_shader_source(uint32_t shader, const char* src, int64_t src_len) {
  (void)shader;
  (void)src;
  (void)src_len;
}
int32_t sere_gl_compile_shader(uint32_t shader) {
  (void)shader;
  return 0;
}
void sere_gl_shader_log(uint32_t shader, const char** out_data, int64_t* out_len) {
  (void)shader;
  outEmpty(out_data, out_len);
}
void sere_gl_delete_shader(uint32_t shader) { (void)shader; }
uint32_t sere_gl_create_program(void) { return 0; }
void sere_gl_attach_shader(uint32_t program, uint32_t shader) {
  (void)program;
  (void)shader;
}
void sere_gl_detach_shader(uint32_t program, uint32_t shader) {
  (void)program;
  (void)shader;
}
int32_t sere_gl_link_program(uint32_t program) {
  (void)program;
  return 0;
}
void sere_gl_program_log(uint32_t program, const char** out_data, int64_t* out_len) {
  (void)program;
  outEmpty(out_data, out_len);
}
void sere_gl_use_program(uint32_t program) { (void)program; }
void sere_gl_delete_program(uint32_t program) { (void)program; }
int32_t sere_gl_uniform_location(uint32_t program, const char* name, int64_t name_len) {
  (void)program;
  (void)name;
  (void)name_len;
  return -1;
}
int32_t sere_gl_attrib_location(uint32_t program, const char* name, int64_t name_len) {
  (void)program;
  (void)name;
  (void)name_len;
  return -1;
}
void sere_gl_bind_attrib(uint32_t program, uint32_t index, const char* name, int64_t name_len) {
  (void)program;
  (void)index;
  (void)name;
  (void)name_len;
}
void sere_gl_uniform1f(int32_t location, double x) {
  (void)location;
  (void)x;
}
void sere_gl_uniform2f(int32_t location, double x, double y) {
  (void)location;
  (void)x;
  (void)y;
}
void sere_gl_uniform3f(int32_t location, double x, double y, double z) {
  (void)location;
  (void)x;
  (void)y;
  (void)z;
}
void sere_gl_uniform4f(int32_t location, double x, double y, double z, double w) {
  (void)location;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}
void sere_gl_uniform1i(int32_t location, int32_t x) {
  (void)location;
  (void)x;
}
void sere_gl_uniform2i(int32_t location, int32_t x, int32_t y) {
  (void)location;
  (void)x;
  (void)y;
}
void sere_gl_uniform3i(int32_t location, int32_t x, int32_t y, int32_t z) {
  (void)location;
  (void)x;
  (void)y;
  (void)z;
}
void sere_gl_uniform4i(int32_t location, int32_t x, int32_t y, int32_t z, int32_t w) {
  (void)location;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}
void sere_gl_uniform_vec(int32_t location, void* values) {
  (void)location;
  (void)values;
}
void sere_gl_uniform_mat3(int32_t location, void* values) {
  (void)location;
  (void)values;
}
void sere_gl_uniform_mat4(int32_t location, void* values) {
  (void)location;
  (void)values;
}
uint32_t sere_gl_gen_buffer(void) { return 0; }
void sere_gl_delete_buffer(uint32_t buffer) { (void)buffer; }
void sere_gl_bind_buffer(uint32_t target, uint32_t buffer) {
  (void)target;
  (void)buffer;
}
void sere_gl_buffer_data_f64(uint32_t target, void* values, uint32_t usage) {
  (void)target;
  (void)values;
  (void)usage;
}
void sere_gl_buffer_data_i32(uint32_t target, void* values, uint32_t usage) {
  (void)target;
  (void)values;
  (void)usage;
}
void sere_gl_buffer_data_bytes(uint32_t target, void* values, uint32_t usage) {
  (void)target;
  (void)values;
  (void)usage;
}
void sere_gl_buffer_sub_f64(uint32_t target, int64_t offset_bytes, void* values) {
  (void)target;
  (void)offset_bytes;
  (void)values;
}
uint32_t sere_gl_gen_vao(void) { return 0; }
void sere_gl_delete_vao(uint32_t vao) { (void)vao; }
void sere_gl_bind_vao(uint32_t vao) { (void)vao; }
void sere_gl_enable_attrib(uint32_t index) { (void)index; }
void sere_gl_disable_attrib(uint32_t index) { (void)index; }
void sere_gl_attrib_pointer(uint32_t index, int32_t size, uint32_t type, int32_t normalized,
                            int32_t stride, int64_t offset) {
  (void)index;
  (void)size;
  (void)type;
  (void)normalized;
  (void)stride;
  (void)offset;
}
void sere_gl_draw_arrays(uint32_t mode, int32_t first, int32_t count) {
  (void)mode;
  (void)first;
  (void)count;
}
void sere_gl_draw_elements(uint32_t mode, int32_t count, uint32_t type, int64_t offset) {
  (void)mode;
  (void)count;
  (void)type;
  (void)offset;
}
uint32_t sere_gl_gen_texture(void) { return 0; }
void sere_gl_delete_texture(uint32_t texture) { (void)texture; }
void sere_gl_bind_texture(uint32_t target, uint32_t texture) {
  (void)target;
  (void)texture;
}
void sere_gl_active_texture(uint32_t unit) { (void)unit; }
void sere_gl_tex_param(uint32_t target, uint32_t pname, int32_t value) {
  (void)target;
  (void)pname;
  (void)value;
}
void sere_gl_tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t width,
                         int32_t height, uint32_t format, uint32_t type, void* pixels) {
  (void)target;
  (void)level;
  (void)internal;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)pixels;
}
void sere_gl_tex_storage(uint32_t target, int32_t internal, int32_t width, int32_t height,
                         uint32_t format) {
  (void)target;
  (void)internal;
  (void)width;
  (void)height;
  (void)format;
}
void sere_gl_tex_sub_image2d(uint32_t target, int32_t level, int32_t x, int32_t y, int32_t width,
                             int32_t height, uint32_t format, uint32_t type, void* pixels) {
  (void)target;
  (void)level;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)pixels;
}
void sere_gl_generate_mipmap(uint32_t target) { (void)target; }
uint32_t sere_gl_gen_framebuffer(void) { return 0; }
void sere_gl_delete_framebuffer(uint32_t fbo) { (void)fbo; }
void sere_gl_bind_framebuffer(uint32_t target, uint32_t fbo) {
  (void)target;
  (void)fbo;
}
void sere_gl_framebuffer_texture2d(uint32_t target, uint32_t attachment, uint32_t textarget,
                                   uint32_t texture, int32_t level) {
  (void)target;
  (void)attachment;
  (void)textarget;
  (void)texture;
  (void)level;
}
uint32_t sere_gl_check_framebuffer(uint32_t target) {
  (void)target;
  return 0;
}
uint32_t sere_gl_gen_renderbuffer(void) { return 0; }
void sere_gl_delete_renderbuffer(uint32_t rbo) { (void)rbo; }
void sere_gl_bind_renderbuffer(uint32_t target, uint32_t rbo) {
  (void)target;
  (void)rbo;
}
void sere_gl_renderbuffer_storage(uint32_t target, uint32_t internal, int32_t width,
                                  int32_t height) {
  (void)target;
  (void)internal;
  (void)width;
  (void)height;
}
void sere_gl_framebuffer_renderbuffer(uint32_t target, uint32_t attachment, uint32_t rbo_target,
                                      uint32_t rbo) {
  (void)target;
  (void)attachment;
  (void)rbo_target;
  (void)rbo;
}
void sere_gl_blit_framebuffer(int32_t src_x0, int32_t src_y0, int32_t src_x1, int32_t src_y1,
                              int32_t dst_x0, int32_t dst_y0, int32_t dst_x1, int32_t dst_y1,
                              uint32_t mask, uint32_t filter) {
  (void)src_x0;
  (void)src_y0;
  (void)src_x1;
  (void)src_y1;
  (void)dst_x0;
  (void)dst_y0;
  (void)dst_x1;
  (void)dst_y1;
  (void)mask;
  (void)filter;
}
void* sere_gl_read_pixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                          uint32_t type) {
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  return sere_array_new(1, 0);
}

#else

typedef unsigned int(APIENTRY* PFNGLCREATESHADERPROC)(unsigned int);
typedef void(APIENTRY* PFNGLSHADERSOURCEPROC)(unsigned int, int, const char* const*, const int*);
typedef void(APIENTRY* PFNGLCOMPILESHADERPROC)(unsigned int);
typedef void(APIENTRY* PFNGLGETSHADERIVPROC)(unsigned int, unsigned int, int*);
typedef void(APIENTRY* PFNGLGETSHADERINFOLOGPROC)(unsigned int, int, int*, char*);
typedef void(APIENTRY* PFNGLDELETESHADERPROC)(unsigned int);
typedef unsigned int(APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRY* PFNGLATTACHSHADERPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLDETACHSHADERPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLLINKPROGRAMPROC)(unsigned int);
typedef void(APIENTRY* PFNGLGETPROGRAMIVPROC)(unsigned int, unsigned int, int*);
typedef void(APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)(unsigned int, int, int*, char*);
typedef void(APIENTRY* PFNGLUSEPROGRAMPROC)(unsigned int);
typedef void(APIENTRY* PFNGLDELETEPROGRAMPROC)(unsigned int);
typedef int(APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(unsigned int, const char*);
typedef int(APIENTRY* PFNGLGETATTRIBLOCATIONPROC)(unsigned int, const char*);
typedef void(APIENTRY* PFNGLBINDATTRIBLOCATIONPROC)(unsigned int, unsigned int, const char*);
typedef void(APIENTRY* PFNGLUNIFORM1FPROC)(int, float);
typedef void(APIENTRY* PFNGLUNIFORM2FPROC)(int, float, float);
typedef void(APIENTRY* PFNGLUNIFORM3FPROC)(int, float, float, float);
typedef void(APIENTRY* PFNGLUNIFORM4FPROC)(int, float, float, float, float);
typedef void(APIENTRY* PFNGLUNIFORM1IPROC)(int, int);
typedef void(APIENTRY* PFNGLUNIFORM2IPROC)(int, int, int);
typedef void(APIENTRY* PFNGLUNIFORM3IPROC)(int, int, int, int);
typedef void(APIENTRY* PFNGLUNIFORM4IPROC)(int, int, int, int, int);
typedef void(APIENTRY* PFNGLUNIFORMMATRIX3FVPROC)(int, int, unsigned char, const float*);
typedef void(APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)(int, int, unsigned char, const float*);
typedef void(APIENTRY* PFNGLBLENDEQUATIONPROC)(unsigned int);
typedef void(APIENTRY* PFNGLBLENDEQUATIONSEPARATEPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLBLENDFUNCSEPARATEPROC)(unsigned int, unsigned int, unsigned int,
                                                   unsigned int);
typedef void(APIENTRY* PFNGLBLENDCOLORPROC)(float, float, float, float);
typedef void(APIENTRY* PFNGLSAMPLECOVERAGEPROC)(float, unsigned char);
typedef void(APIENTRY* PFNGLVALIDATEPROGRAMPROC)(unsigned int);
typedef void(APIENTRY* PFNGLBLITFRAMEBUFFERPROC)(int, int, int, int, int, int, int, int,
                                                 unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLGENBUFFERSPROC)(int, unsigned int*);
typedef void(APIENTRY* PFNGLDELETEBUFFERSPROC)(int, const unsigned int*);
typedef void(APIENTRY* PFNGLBINDBUFFERPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLBUFFERDATAPROC)(unsigned int, intptr_t, const void*, unsigned int);
typedef void(APIENTRY* PFNGLBUFFERSUBDATAPROC)(unsigned int, intptr_t, intptr_t, const void*);
typedef void(APIENTRY* PFNGLGENVERTEXARRAYSPROC)(int, unsigned int*);
typedef void(APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)(int, const unsigned int*);
typedef void(APIENTRY* PFNGLBINDVERTEXARRAYPROC)(unsigned int);
typedef void(APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC)(unsigned int);
typedef void(APIENTRY* PFNGLDISABLEVERTEXATTRIBARRAYPROC)(unsigned int);
typedef void(APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)(unsigned int, int, unsigned int, unsigned char,
                                                     int, const void*);
typedef void(APIENTRY* PFNGLDRAWARRAYSPROC)(unsigned int, int, int);
typedef void(APIENTRY* PFNGLDRAWELEMENTSPROC)(unsigned int, int, unsigned int, const void*);
typedef void(APIENTRY* PFNGLACTIVETEXTUREPROC)(unsigned int);
typedef void(APIENTRY* PFNGLGENERATEMIPMAPPROC)(unsigned int);
typedef void(APIENTRY* PFNGLTEXIMAGE2DPROC)(unsigned int, int, int, int, int, int, unsigned int,
                                            unsigned int, const void*);
typedef void(APIENTRY* PFNGLTEXSUBIMAGE2DPROC)(unsigned int, int, int, int, int, int, unsigned int,
                                               unsigned int, const void*);
typedef void(APIENTRY* PFNGLGENFRAMEBUFFERSPROC)(int, unsigned int*);
typedef void(APIENTRY* PFNGLDELETEFRAMEBUFFERSPROC)(int, const unsigned int*);
typedef void(APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DPROC)(unsigned int, unsigned int, unsigned int,
                                                      unsigned int, int);
typedef unsigned int(APIENTRY* PFNGLCHECKFRAMEBUFFERSTATUSPROC)(unsigned int);
typedef void(APIENTRY* PFNGLGENRENDERBUFFERSPROC)(int, unsigned int*);
typedef void(APIENTRY* PFNGLDELETERENDERBUFFERSPROC)(int, const unsigned int*);
typedef void(APIENTRY* PFNGLBINDRENDERBUFFERPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLRENDERBUFFERSTORAGEPROC)(unsigned int, unsigned int, int, int);
typedef void(APIENTRY* PFNGLFRAMEBUFFERRENDERBUFFERPROC)(unsigned int, unsigned int, unsigned int,
                                                         unsigned int);
typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALEXTPROC)(int);

static void* glProc(const char* name) {
  PROC proc = wglGetProcAddress(name);
  if (proc == NULL || proc == (PROC)(uintptr_t)1 || proc == (PROC)(uintptr_t)2 ||
      proc == (PROC)(uintptr_t)3 || proc == (PROC)(uintptr_t)-1) {
    HMODULE module = GetModuleHandleA("opengl32.dll");
    proc = module == NULL ? NULL : GetProcAddress(module, name);
  }
  return (void*)proc;
}

#define SERE_GL_LOAD(Type, name)                                                                   \
  static Type fn = NULL;                                                                           \
  if (fn == NULL) {                                                                                \
    fn = (Type)glProc(name);                                                                       \
  }                                                                                                \
  if (fn == NULL) {                                                                                \
    return;                                                                                        \
  }

#define SERE_GL_LOAD_RET(Type, name, fallback)                                                     \
  static Type fn = NULL;                                                                           \
  if (fn == NULL) {                                                                                \
    fn = (Type)glProc(name);                                                                       \
  }                                                                                                \
  if (fn == NULL) {                                                                                \
    return fallback;                                                                               \
  }

enum {
  SERE_GL_WIN_VISIBLE = 1,
  SERE_GL_WIN_RESIZABLE = 2,
  SERE_GL_WIN_DECORATED = 4,
  SERE_GL_WIN_MAXIMIZED = 8,
  SERE_GL_WIN_FLOATING = 16,
  SERE_GL_WIN_FOCUSED = 32,
  SERE_GL_WIN_FULLSCREEN = 64,
  SERE_GL_WIN_VSYNC = 128,
  SERE_GL_WIN_DEFAULT =
      SERE_GL_WIN_VISIBLE | SERE_GL_WIN_RESIZABLE | SERE_GL_WIN_DECORATED | SERE_GL_WIN_FOCUSED |
      SERE_GL_WIN_VSYNC
};

enum {
  SERE_GL_CURSOR_NORMAL = 0,
  SERE_GL_CURSOR_HIDDEN = 1,
  SERE_GL_CURSOR_DISABLED = 2,
  SERE_GL_CURSOR_CAPTURED = 3
};

typedef struct SereGlWindow {
  HWND hwnd;
  HDC hdc;
  HGLRC glrc;
  char* title;
  int32_t width;
  int32_t height;
  int32_t alive;
  int32_t should_close;
  int32_t wheel;
  int32_t resized;
  int32_t focused;
  int32_t minimized;
  int32_t maximized;
  int32_t fullscreen;
  int32_t vsync;
  int32_t cursor_mode;
  int32_t cursor_visible;
  int32_t skip_mouse;
  int32_t last_char;
  int32_t mouse_x;
  int32_t mouse_y;
  int32_t mouse_dx;
  int32_t mouse_dy;
  int32_t min_w;
  int32_t min_h;
  int32_t max_w;
  int32_t max_h;
  int32_t resizable;
  int32_t decorated;
  int32_t floating;
  double opacity;
  int32_t restore_x;
  int32_t restore_y;
  int32_t restore_w;
  int32_t restore_h;
  LONG restore_style;
  uint8_t keys_prev[256];
  uint8_t keys_now[256];
  double start;
  double last;
  double freq;
} SereGlWindow;

static SereGlWindow* asWin(void* handle) { return (SereGlWindow*)handle; }

static int32_t normalizeFlags(int32_t flags) { return flags == 0 ? SERE_GL_WIN_DEFAULT : flags; }

static DWORD windowStyle(int32_t flags) {
  if ((flags & SERE_GL_WIN_FULLSCREEN) != 0 || (flags & SERE_GL_WIN_DECORATED) == 0) {
    return WS_POPUP;
  }
  DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  if ((flags & SERE_GL_WIN_RESIZABLE) != 0) {
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
  }
  return style;
}

static void clientToScreenPoint(HWND hwnd, int32_t x, int32_t y, POINT* out) {
  POINT point = {x, y};
  ClientToScreen(hwnd, &point);
  if (out != NULL) {
    *out = point;
  }
}

static void applyCursor(SereGlWindow* window) {
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  const int wantVisible = window->cursor_mode == SERE_GL_CURSOR_NORMAL ||
                          window->cursor_mode == SERE_GL_CURSOR_CAPTURED;
  if (wantVisible != window->cursor_visible) {
    ShowCursor(wantVisible ? TRUE : FALSE);
    window->cursor_visible = wantVisible;
  }
  if (window->cursor_mode == SERE_GL_CURSOR_DISABLED ||
      window->cursor_mode == SERE_GL_CURSOR_CAPTURED) {
    RECT client;
    GetClientRect(window->hwnd, &client);
    POINT topLeft = {client.left, client.top};
    POINT bottomRight = {client.right, client.bottom};
    ClientToScreen(window->hwnd, &topLeft);
    ClientToScreen(window->hwnd, &bottomRight);
    RECT clip = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&clip);
  } else {
    ClipCursor(NULL);
  }
}

static void snapshotKeys(SereGlWindow* window) {
  if (window == NULL) {
    return;
  }
  memcpy(window->keys_prev, window->keys_now, sizeof(window->keys_now));
  for (int i = 0; i < 256; ++i) {
    window->keys_now[i] = (GetAsyncKeyState(i) & 0x8000) != 0 ? 1 : 0;
  }
}

static int32_t keyDown(const SereGlWindow* window, int32_t vk) {
  if (window == NULL || vk < 0 || vk > 255) {
    return 0;
  }
  return window->keys_now[vk];
}

static int32_t mouseVk(int32_t button) {
  if (button == 1) {
    return VK_RBUTTON;
  }
  if (button == 2) {
    return VK_MBUTTON;
  }
  if (button == 3) {
    return VK_XBUTTON1;
  }
  if (button == 4) {
    return VK_XBUTTON2;
  }
  return VK_LBUTTON;
}

static void updateMouse(SereGlWindow* window) {
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  POINT point;
  GetCursorPos(&point);
  ScreenToClient(window->hwnd, &point);
  const int32_t x = (int32_t)point.x;
  const int32_t y = (int32_t)point.y;
  if (window->cursor_mode == SERE_GL_CURSOR_DISABLED) {
    const int32_t cx = window->width / 2;
    const int32_t cy = window->height / 2;
    if (window->skip_mouse) {
      window->mouse_dx = 0;
      window->mouse_dy = 0;
      window->skip_mouse = 0;
    } else {
      window->mouse_dx = x - cx;
      window->mouse_dy = y - cy;
    }
    POINT center;
    clientToScreenPoint(window->hwnd, cx, cy, &center);
    SetCursorPos(center.x, center.y);
    window->mouse_x = cx;
    window->mouse_y = cy;
  } else {
    if (window->skip_mouse) {
      window->mouse_dx = 0;
      window->mouse_dy = 0;
      window->skip_mouse = 0;
    } else {
      window->mouse_dx = x - window->mouse_x;
      window->mouse_dy = y - window->mouse_y;
    }
    window->mouse_x = x;
    window->mouse_y = y;
  }
}

static void applyFullscreen(SereGlWindow* window, int32_t enabled) {
  if (window == NULL || window->hwnd == NULL || enabled == window->fullscreen) {
    return;
  }
  if (enabled) {
    RECT rect;
    GetWindowRect(window->hwnd, &rect);
    window->restore_x = rect.left;
    window->restore_y = rect.top;
    window->restore_w = rect.right - rect.left;
    window->restore_h = rect.bottom - rect.top;
    window->restore_style = GetWindowLongA(window->hwnd, GWL_STYLE);
    SetWindowLongA(window->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(window->hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
    window->fullscreen = 1;
    return;
  }
  SetWindowLongA(window->hwnd, GWL_STYLE, window->restore_style | WS_VISIBLE);
  SetWindowPos(window->hwnd, HWND_NOTOPMOST, window->restore_x, window->restore_y,
               window->restore_w, window->restore_h, SWP_FRAMECHANGED);
  window->fullscreen = 0;
}

static void applyChrome(SereGlWindow* window) {
  if (window == NULL || window->hwnd == NULL || window->fullscreen) {
    return;
  }
  int32_t flags = 0;
  if (window->resizable) {
    flags |= SERE_GL_WIN_RESIZABLE;
  }
  if (window->decorated) {
    flags |= SERE_GL_WIN_DECORATED;
  }
  SetWindowLongA(window->hwnd, GWL_STYLE, windowStyle(flags) | WS_VISIBLE);
  SetWindowPos(window->hwnd, window->floating ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static int32_t pumpWindow(SereGlWindow* window) {
  if (window == NULL) {
    return 0;
  }
  if (!window->alive) {
    window->should_close = 1;
    return 0;
  }
  window->wheel = 0;
  window->resized = 0;
  window->last_char = 0;
  MSG msg;
  while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      window->should_close = 1;
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  snapshotKeys(window);
  updateMouse(window);
  applyCursor(window);
  return window->should_close ? 0 : 1;
}

static double nowSeconds(const SereGlWindow* window) {
  LARGE_INTEGER ticks;
  QueryPerformanceCounter(&ticks);
  return window == NULL || window->freq <= 0.0 ? 0.0 : (double)ticks.QuadPart / window->freq;
}

static LRESULT CALLBACK sereGlWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  SereGlWindow* window = (SereGlWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
  if (msg == WM_CLOSE || (msg == WM_SYSCOMMAND && wparam == SC_CLOSE)) {
    if (window != NULL) {
      window->should_close = 1;
    }
    return 0;
  }
  if (msg == WM_DESTROY) {
    if (window != NULL) {
      window->alive = 0;
      window->should_close = 1;
      window->hwnd = NULL;
    }
    return 0;
  }
  if (msg == WM_SETFOCUS && window != NULL) {
    window->focused = 1;
  }
  if (msg == WM_KILLFOCUS && window != NULL) {
    window->focused = 0;
  }
  if (msg == WM_CHAR && window != NULL && wparam >= 32 && wparam != 127) {
    window->last_char = (int32_t)wparam;
  }
  if (msg == WM_GETMINMAXINFO && window != NULL) {
    MINMAXINFO* info = (MINMAXINFO*)lparam;
    if (window->min_w > 0) {
      info->ptMinTrackSize.x = window->min_w;
    }
    if (window->min_h > 0) {
      info->ptMinTrackSize.y = window->min_h;
    }
    if (window->max_w > 0) {
      info->ptMaxTrackSize.x = window->max_w;
    }
    if (window->max_h > 0) {
      info->ptMaxTrackSize.y = window->max_h;
    }
    return 0;
  }
  if (msg == WM_SIZE && window != NULL) {
    window->width = (int32_t)LOWORD(lparam);
    window->height = (int32_t)HIWORD(lparam);
    window->resized = 1;
    window->minimized = wparam == SIZE_MINIMIZED ? 1 : 0;
    window->maximized = wparam == SIZE_MAXIMIZED ? 1 : 0;
  }
  if (msg == WM_MOUSEWHEEL && window != NULL) {
    window->wheel += (int32_t)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
  }
  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static void registerClass(void) {
  static int ready = 0;
  if (ready) {
    return;
  }
  WNDCLASSEXA wc;
  memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = sereGlWndProc;
  wc.hInstance = GetModuleHandleA(NULL);
  wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
  wc.hIcon = (HICON)sere_icon_hicon(0);
  wc.hIconSm = (HICON)sere_icon_hicon(1);
  wc.lpszClassName = "SereGLWindow";
  RegisterClassExA(&wc);
  ready = 1;
}

static void infoLog(int is_shader, uint32_t id, const char** out_data, int64_t* out_len) {
  int length = 0;
  if (is_shader) {
    PFNGLGETSHADERIVPROC getIv = (PFNGLGETSHADERIVPROC)glProc("glGetShaderiv");
    PFNGLGETSHADERINFOLOGPROC getLog = (PFNGLGETSHADERINFOLOGPROC)glProc("glGetShaderInfoLog");
    if (getIv == NULL || getLog == NULL) {
      outEmpty(out_data, out_len);
      return;
    }
    getIv(id, 0x8B84, &length);
    char* buf = (char*)malloc((size_t)length + 1);
    if (buf == NULL) {
      outEmpty(out_data, out_len);
      return;
    }
    getLog(id, length + 1, NULL, buf);
    outCString(buf, out_data, out_len);
    return;
  }
  PFNGLGETPROGRAMIVPROC getIv = (PFNGLGETPROGRAMIVPROC)glProc("glGetProgramiv");
  PFNGLGETPROGRAMINFOLOGPROC getLog = (PFNGLGETPROGRAMINFOLOGPROC)glProc("glGetProgramInfoLog");
  if (getIv == NULL || getLog == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  getIv(id, 0x8B84, &length);
  char* buf = (char*)malloc((size_t)length + 1);
  if (buf == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  getLog(id, length + 1, NULL, buf);
  outCString(buf, out_data, out_len);
}

static float* f64ToFloat(void* list, int* count) {
  SereList* typed = asList(list);
  if (typed == NULL || typed->data == NULL || typed->stride != (int64_t)sizeof(double) ||
      typed->len <= 0) {
    if (count != NULL) {
      *count = 0;
    }
    return NULL;
  }
  const int n = (int)typed->len;
  float* out = (float*)malloc((size_t)n * sizeof(float));
  if (out == NULL) {
    if (count != NULL) {
      *count = 0;
    }
    return NULL;
  }
  const double* src = (const double*)typed->data;
  for (int i = 0; i < n; ++i) {
    out[i] = (float)src[i];
  }
  if (count != NULL) {
    *count = n;
  }
  return out;
}

static const void* listBytes(void* list, int64_t* nbytes) {
  SereList* typed = asList(list);
  if (typed == NULL || typed->data == NULL || typed->len <= 0 || typed->stride <= 0) {
    if (nbytes != NULL) {
      *nbytes = 0;
    }
    return NULL;
  }
  if (nbytes != NULL) {
    *nbytes = typed->len * typed->stride;
  }
  return typed->data;
}

static uint32_t genOne(const char* name) {
  typedef void(APIENTRY* GenFn)(int, unsigned int*);
  GenFn fn = (GenFn)glProc(name);
  if (fn == NULL) {
    return 0;
  }
  unsigned int id = 0;
  fn(1, &id);
  return id;
}

static void deleteOne(const char* name, uint32_t id) {
  typedef void(APIENTRY* DeleteFn)(int, const unsigned int*);
  DeleteFn fn = (DeleteFn)glProc(name);
  if (fn != NULL) {
    fn(1, &id);
  }
}

int32_t sere_gl_available(void) { return 1; }

void* sere_gl_window_new(const char* title, int64_t title_len, int32_t width, int32_t height) {
  return sere_gl_window_new_ex(title, title_len, width, height, 0);
}

void* sere_gl_window_new_ex(const char* title, int64_t title_len, int32_t width, int32_t height,
                            int32_t flags) {
  registerClass();
  char* caption = toCString(title, title_len);
  SereGlWindow* window = (SereGlWindow*)calloc(1, sizeof(SereGlWindow));
  if (window == NULL) {
    free(caption);
    return NULL;
  }
  flags = normalizeFlags(flags);
  window->title = caption;
  window->width = width <= 0 ? 640 : width;
  window->height = height <= 0 ? 480 : height;
  window->alive = 1;
  window->cursor_visible = 1;
  window->skip_mouse = 1;
  window->opacity = 1.0;
  window->resizable = (flags & SERE_GL_WIN_RESIZABLE) != 0 ? 1 : 0;
  window->decorated = (flags & SERE_GL_WIN_DECORATED) != 0 ? 1 : 0;
  window->floating = (flags & SERE_GL_WIN_FLOATING) != 0 ? 1 : 0;
  window->vsync = (flags & SERE_GL_WIN_VSYNC) != 0 ? 1 : 0;
  const DWORD style = windowStyle(flags);
  const DWORD exStyle = (flags & SERE_GL_WIN_FLOATING) != 0 ? WS_EX_TOPMOST : 0;
  RECT rect = {0, 0, window->width, window->height};
  AdjustWindowRectEx(&rect, style, FALSE, exStyle);
  window->hwnd =
      CreateWindowExA(exStyle, "SereGLWindow", caption == NULL ? "Sere" : caption, style,
                      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                      NULL, NULL, GetModuleHandleA(NULL), NULL);
  if (window->hwnd == NULL) {
    free(caption);
    free(window);
    return NULL;
  }
  SetWindowLongPtrA(window->hwnd, GWLP_USERDATA, (LONG_PTR)window);
  sere_icon_apply_hwnd(window->hwnd);
  window->hdc = GetDC(window->hwnd);
  PIXELFORMATDESCRIPTOR pfd;
  memset(&pfd, 0, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  const int format = ChoosePixelFormat(window->hdc, &pfd);
  SetPixelFormat(window->hdc, format, &pfd);
  window->glrc = wglCreateContext(window->hdc);
  wglMakeCurrent(window->hdc, window->glrc);
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  window->freq = (double)freq.QuadPart;
  window->start = nowSeconds(window);
  window->last = window->start;
  if ((flags & SERE_GL_WIN_VISIBLE) != 0) {
    ShowWindow(window->hwnd, (flags & SERE_GL_WIN_MAXIMIZED) != 0 ? SW_MAXIMIZE : SW_SHOW);
  }
  if ((flags & SERE_GL_WIN_FOCUSED) != 0) {
    SetForegroundWindow(window->hwnd);
    window->focused = 1;
  }
  if ((flags & SERE_GL_WIN_FULLSCREEN) != 0) {
    applyFullscreen(window, 1);
  }
  if (window->vsync) {
    sere_gl_window_set_vsync(window, 1);
  }
  snapshotKeys(window);
  updateMouse(window);
  return window;
}

int32_t sere_gl_window_poll(void* handle) { return pumpWindow(asWin(handle)); }

int32_t sere_gl_window_wait(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->should_close || !window->alive) {
    return 0;
  }
  WaitMessage();
  return pumpWindow(window);
}

int32_t sere_gl_window_should_close(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL || window->should_close || !window->alive ? 1 : 0;
}

void sere_gl_window_set_should_close(void* handle, int32_t value) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL) {
    window->should_close = value != 0 ? 1 : 0;
  }
}

int32_t sere_gl_window_is_open(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window != NULL && window->alive && !window->should_close ? 1 : 0;
}

void sere_gl_window_swap(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window != NULL && window->hdc != NULL) {
    SwapBuffers(window->hdc);
  }
}

void sere_gl_window_make_current(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window != NULL) {
    wglMakeCurrent(window->hdc, window->glrc);
  }
}

void sere_gl_window_destroy(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return;
  }
  ClipCursor(NULL);
  if (!window->cursor_visible) {
    ShowCursor(TRUE);
    window->cursor_visible = 1;
  }
  wglMakeCurrent(NULL, NULL);
  if (window->glrc != NULL) {
    wglDeleteContext(window->glrc);
  }
  if (window->hdc != NULL && window->hwnd != NULL) {
    ReleaseDC(window->hwnd, window->hdc);
  }
  if (window->hwnd != NULL) {
    DestroyWindow(window->hwnd);
  }
  free(window->title);
  free(window);
}

int32_t sere_gl_window_width(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->width;
}

int32_t sere_gl_window_height(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->height;
}

int32_t sere_gl_window_fb_width(void* handle) { return sere_gl_window_width(handle); }

int32_t sere_gl_window_fb_height(void* handle) { return sere_gl_window_height(handle); }

int32_t sere_gl_window_x(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return 0;
  }
  RECT rect;
  GetWindowRect(window->hwnd, &rect);
  return (int32_t)rect.left;
}

int32_t sere_gl_window_y(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return 0;
  }
  RECT rect;
  GetWindowRect(window->hwnd, &rect);
  return (int32_t)rect.top;
}

void sere_gl_window_set_title(void* handle, const char* title, int64_t title_len) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return;
  }
  free(window->title);
  window->title = toCString(title, title_len);
  if (window->hwnd != NULL) {
    SetWindowTextA(window->hwnd, window->title == NULL ? "Sere" : window->title);
  }
}

void sere_gl_window_title(void* handle, const char** out_data, int64_t* out_len) {
  SereGlWindow* window = asWin(handle);
  outCString(window == NULL ? "" : window->title, out_data, out_len);
}

void sere_gl_window_set_vsync(void* handle, int32_t enabled) {
  SereGlWindow* window = asWin(handle);
  PFNWGLSWAPINTERVALEXTPROC swap = (PFNWGLSWAPINTERVALEXTPROC)glProc("wglSwapIntervalEXT");
  if (swap != NULL) {
    swap(enabled != 0 ? 1 : 0);
  }
  if (window != NULL) {
    window->vsync = enabled != 0 ? 1 : 0;
  }
}

int32_t sere_gl_window_vsync(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->vsync;
}

void sere_gl_window_show_cursor(void* handle, int32_t show) {
  sere_gl_window_set_cursor_mode(handle, show != 0 ? SERE_GL_CURSOR_NORMAL : SERE_GL_CURSOR_HIDDEN);
}

void sere_gl_window_set_cursor_mode(void* handle, int32_t mode) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return;
  }
  if (mode < SERE_GL_CURSOR_NORMAL || mode > SERE_GL_CURSOR_CAPTURED) {
    mode = SERE_GL_CURSOR_NORMAL;
  }
  if (window->cursor_mode != mode) {
    window->skip_mouse = 1;
  }
  window->cursor_mode = mode;
  applyCursor(window);
}

int32_t sere_gl_window_cursor_mode(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->cursor_mode;
}

void sere_gl_window_set_cursor_pos(void* handle, int32_t x, int32_t y) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  POINT point;
  clientToScreenPoint(window->hwnd, x, y, &point);
  SetCursorPos(point.x, point.y);
  window->mouse_x = x;
  window->mouse_y = y;
}

void sere_gl_window_set_size(void* handle, int32_t width, int32_t height) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  RECT rect = {0, 0, width, height};
  AdjustWindowRectEx(&rect, (DWORD)GetWindowLongA(window->hwnd, GWL_STYLE), FALSE,
                     (DWORD)GetWindowLongA(window->hwnd, GWL_EXSTYLE));
  SetWindowPos(window->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
               SWP_NOMOVE | SWP_NOZORDER);
}

void sere_gl_window_set_pos(void* handle, int32_t x, int32_t y) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  SetWindowPos(window->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void sere_gl_window_set_min_size(void* handle, int32_t width, int32_t height) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL) {
    window->min_w = width;
    window->min_h = height;
  }
}

void sere_gl_window_set_max_size(void* handle, int32_t width, int32_t height) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL) {
    window->max_w = width;
    window->max_h = height;
  }
}

void sere_gl_window_show(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    ShowWindow(window->hwnd, SW_SHOW);
  }
}

void sere_gl_window_hide(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    ShowWindow(window->hwnd, SW_HIDE);
  }
}

void sere_gl_window_minimize(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    ShowWindow(window->hwnd, SW_MINIMIZE);
  }
}

void sere_gl_window_maximize(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    ShowWindow(window->hwnd, SW_MAXIMIZE);
  }
}

void sere_gl_window_restore(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    if (window->fullscreen) {
      applyFullscreen(window, 0);
    }
    ShowWindow(window->hwnd, SW_RESTORE);
  }
}

void sere_gl_window_focus(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    SetForegroundWindow(window->hwnd);
    SetFocus(window->hwnd);
  }
}

void sere_gl_window_request_attention(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window != NULL && window->hwnd != NULL) {
    FlashWindow(window->hwnd, TRUE);
  }
}

void sere_gl_window_set_fullscreen(void* handle, int32_t enabled) {
  applyFullscreen(asWin(handle), enabled != 0 ? 1 : 0);
}

int32_t sere_gl_window_fullscreen(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->fullscreen;
}

void sere_gl_window_set_resizable(void* handle, int32_t enabled) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return;
  }
  window->resizable = enabled != 0 ? 1 : 0;
  applyChrome(window);
}

void sere_gl_window_set_decorated(void* handle, int32_t enabled) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return;
  }
  window->decorated = enabled != 0 ? 1 : 0;
  applyChrome(window);
}

void sere_gl_window_set_floating(void* handle, int32_t enabled) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return;
  }
  window->floating = enabled != 0 ? 1 : 0;
  applyChrome(window);
}

void sere_gl_window_set_opacity(void* handle, double opacity) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  if (opacity < 0.0) {
    opacity = 0.0;
  }
  if (opacity > 1.0) {
    opacity = 1.0;
  }
  const LONG ex = GetWindowLongA(window->hwnd, GWL_EXSTYLE);
  SetWindowLongA(window->hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
  window->opacity = opacity;
  SetLayeredWindowAttributes(window->hwnd, 0, (BYTE)(opacity * 255.0 + 0.5), LWA_ALPHA);
}

int32_t sere_gl_window_resizable(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->resizable;
}

int32_t sere_gl_window_decorated(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->decorated;
}

int32_t sere_gl_window_floating(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->floating;
}

double sere_gl_window_opacity(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 1.0 : window->opacity;
}

double sere_gl_window_content_scale(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hdc == NULL) {
    return 1.0;
  }
  const int dpi = GetDeviceCaps(window->hdc, LOGPIXELSX);
  if (dpi <= 0) {
    return 1.0;
  }
  return (double)dpi / 96.0;
}

int32_t sere_gl_window_focused(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->focused;
}

int32_t sere_gl_window_minimized(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->minimized;
}

int32_t sere_gl_window_maximized(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->maximized;
}

int32_t sere_gl_window_visible(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window != NULL && window->hwnd != NULL && IsWindowVisible(window->hwnd) ? 1 : 0;
}

int32_t sere_gl_window_hovered(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || window->hwnd == NULL) {
    return 0;
  }
  POINT point;
  GetCursorPos(&point);
  return WindowFromPoint(point) == window->hwnd ? 1 : 0;
}

int32_t sere_gl_window_resized(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->resized;
}

double sere_gl_window_time(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0.0 : nowSeconds(window) - window->start;
}

double sere_gl_window_dt(void* handle) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL) {
    return 0.0;
  }
  const double now = nowSeconds(window);
  const double dt = now - window->last;
  window->last = now;
  return dt < 0.0 ? 0.0 : dt;
}

int32_t sere_gl_window_key(void* handle, int32_t vk) { return keyDown(asWin(handle), vk); }

int32_t sere_gl_window_key_pressed(void* handle, int32_t vk) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || vk < 0 || vk > 255) {
    return 0;
  }
  return window->keys_now[vk] && !window->keys_prev[vk] ? 1 : 0;
}

int32_t sere_gl_window_key_released(void* handle, int32_t vk) {
  SereGlWindow* window = asWin(handle);
  if (window == NULL || vk < 0 || vk > 255) {
    return 0;
  }
  return !window->keys_now[vk] && window->keys_prev[vk] ? 1 : 0;
}

int32_t sere_gl_window_mods(void* handle) {
  int32_t mods = 0;
  if (sere_gl_window_key(handle, VK_SHIFT) || sere_gl_window_key(handle, VK_LSHIFT) ||
      sere_gl_window_key(handle, VK_RSHIFT)) {
    mods |= 1;
  }
  if (sere_gl_window_key(handle, VK_CONTROL) || sere_gl_window_key(handle, VK_LCONTROL) ||
      sere_gl_window_key(handle, VK_RCONTROL)) {
    mods |= 2;
  }
  if (sere_gl_window_key(handle, VK_MENU) || sere_gl_window_key(handle, VK_LMENU) ||
      sere_gl_window_key(handle, VK_RMENU)) {
    mods |= 4;
  }
  if (sere_gl_window_key(handle, VK_LWIN) || sere_gl_window_key(handle, VK_RWIN)) {
    mods |= 8;
  }
  if (sere_gl_window_key(handle, VK_CAPITAL)) {
    mods |= 16;
  }
  if (sere_gl_window_key(handle, VK_NUMLOCK)) {
    mods |= 32;
  }
  return mods;
}

int32_t sere_gl_window_char(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->last_char;
}

int32_t sere_gl_window_mouse_x(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->mouse_x;
}

int32_t sere_gl_window_mouse_y(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->mouse_y;
}

int32_t sere_gl_window_mouse_dx(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->mouse_dx;
}

int32_t sere_gl_window_mouse_dy(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->mouse_dy;
}

int32_t sere_gl_window_mouse_button(void* handle, int32_t button) {
  return sere_gl_window_key(handle, mouseVk(button));
}

int32_t sere_gl_window_mouse_button_pressed(void* handle, int32_t button) {
  return sere_gl_window_key_pressed(handle, mouseVk(button));
}

int32_t sere_gl_window_mouse_button_released(void* handle, int32_t button) {
  return sere_gl_window_key_released(handle, mouseVk(button));
}

int32_t sere_gl_window_wheel(void* handle) {
  SereGlWindow* window = asWin(handle);
  return window == NULL ? 0 : window->wheel;
}

void sere_gl_window_clipboard_get(void* handle, const char** out_data, int64_t* out_len) {
  (void)handle;
  sere_win_clipboard_get(out_data, out_len);
}

void sere_gl_window_clipboard_set(void* handle, const char* data, int64_t len) {
  (void)handle;
  sere_win_clipboard_set(data, len);
}

int32_t sere_gl_screen_width(void) { return (int32_t)GetSystemMetrics(SM_CXSCREEN); }

int32_t sere_gl_screen_height(void) { return (int32_t)GetSystemMetrics(SM_CYSCREEN); }

void sere_gl_clear_color(double r, double g, double b, double a) {
  glClearColor((float)r, (float)g, (float)b, (float)a);
}
void sere_gl_clear(uint32_t mask) { glClear(mask); }
void sere_gl_viewport(int32_t x, int32_t y, int32_t width, int32_t height) {
  glViewport(x, y, width, height);
}
void sere_gl_scissor(int32_t x, int32_t y, int32_t width, int32_t height) {
  glScissor(x, y, width, height);
}
void sere_gl_enable(uint32_t cap) { glEnable(cap); }
void sere_gl_disable(uint32_t cap) { glDisable(cap); }
void sere_gl_blend_func(uint32_t src, uint32_t dst) { glBlendFunc(src, dst); }
void sere_gl_blend_func_separate(uint32_t src_rgb, uint32_t dst_rgb, uint32_t src_a, uint32_t dst_a) {
  SERE_GL_LOAD(PFNGLBLENDFUNCSEPARATEPROC, "glBlendFuncSeparate");
  fn(src_rgb, dst_rgb, src_a, dst_a);
}
void sere_gl_blend_equation(uint32_t mode) {
  SERE_GL_LOAD(PFNGLBLENDEQUATIONPROC, "glBlendEquation");
  fn(mode);
}
void sere_gl_blend_equation_separate(uint32_t rgb, uint32_t alpha) {
  SERE_GL_LOAD(PFNGLBLENDEQUATIONSEPARATEPROC, "glBlendEquationSeparate");
  fn(rgb, alpha);
}
void sere_gl_blend_color(double r, double g, double b, double a) {
  SERE_GL_LOAD(PFNGLBLENDCOLORPROC, "glBlendColor");
  fn((float)r, (float)g, (float)b, (float)a);
}
void sere_gl_depth_func(uint32_t func) { glDepthFunc(func); }
void sere_gl_depth_mask(int32_t enabled) { glDepthMask(enabled != 0 ? GL_TRUE : GL_FALSE); }
void sere_gl_depth_range(double near_z, double far_z) { glDepthRange(near_z, far_z); }
void sere_gl_color_mask(int32_t r, int32_t g, int32_t b, int32_t a) {
  glColorMask(r != 0, g != 0, b != 0, a != 0);
}
void sere_gl_cull_face(uint32_t mode) { glCullFace(mode); }
void sere_gl_front_face(uint32_t mode) { glFrontFace(mode); }
void sere_gl_polygon_mode(uint32_t face, uint32_t mode) { glPolygonMode(face, mode); }
void sere_gl_polygon_offset(double factor, double units) {
  glPolygonOffset((float)factor, (float)units);
}
void sere_gl_line_width(double width) { glLineWidth((float)width); }
void sere_gl_point_size(double size) { glPointSize((float)size); }
void sere_gl_pixel_store(uint32_t pname, int32_t value) { glPixelStorei(pname, value); }
void sere_gl_hint(uint32_t target, uint32_t mode) { glHint(target, mode); }
int32_t sere_gl_is_enabled(uint32_t cap) { return glIsEnabled(cap) ? 1 : 0; }
void sere_gl_logic_op(uint32_t op) { glLogicOp(op); }
void sere_gl_stencil_func(uint32_t func, int32_t ref, uint32_t mask) {
  glStencilFunc(func, ref, mask);
}
void sere_gl_stencil_op(uint32_t sfail, uint32_t dpfail, uint32_t dppass) {
  glStencilOp(sfail, dpfail, dppass);
}
void sere_gl_stencil_mask(uint32_t mask) { glStencilMask(mask); }
void sere_gl_clear_depth(double depth) { glClearDepth(depth); }
void sere_gl_clear_stencil(int32_t s) { glClearStencil(s); }
void sere_gl_draw_buffer(uint32_t buf) { glDrawBuffer(buf); }
void sere_gl_read_buffer(uint32_t buf) { glReadBuffer(buf); }
void sere_gl_sample_coverage(double value, int32_t invert) {
  SERE_GL_LOAD(PFNGLSAMPLECOVERAGEPROC, "glSampleCoverage");
  fn((float)value, invert != 0 ? 1 : 0);
}
double sere_gl_get_float(uint32_t pname) {
  GLfloat value = 0.0f;
  glGetFloatv(pname, &value);
  return (double)value;
}
int32_t sere_gl_validate_program(uint32_t program) {
  SERE_GL_LOAD_RET(PFNGLVALIDATEPROGRAMPROC, "glValidateProgram", 0);
  fn(program);
  PFNGLGETPROGRAMIVPROC getIv = (PFNGLGETPROGRAMIVPROC)glProc("glGetProgramiv");
  int status = 0;
  if (getIv != NULL) {
    getIv(program, 0x8B83, &status);
  }
  return status != 0 ? 1 : 0;
}
void sere_gl_finish(void) { glFinish(); }
void sere_gl_flush(void) { glFlush(); }
void sere_gl_begin(uint32_t mode) { glBegin(mode); }
void sere_gl_end(void) { glEnd(); }
void sere_gl_vertex2f(double x, double y) { glVertex2f((float)x, (float)y); }
void sere_gl_vertex3f(double x, double y, double z) { glVertex3f((float)x, (float)y, (float)z); }
void sere_gl_color3f(double r, double g, double b) { glColor3f((float)r, (float)g, (float)b); }
void sere_gl_color4f(double r, double g, double b, double a) {
  glColor4f((float)r, (float)g, (float)b, (float)a);
}
void sere_gl_texcoord2f(double u, double v) { glTexCoord2f((float)u, (float)v); }
void sere_gl_normal3f(double x, double y, double z) { glNormal3f((float)x, (float)y, (float)z); }
uint32_t sere_gl_get_error(void) { return (uint32_t)glGetError(); }

int32_t sere_gl_get_integer(uint32_t pname) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  return (int32_t)value;
}

void sere_gl_get_string(uint32_t name, const char** out_data, int64_t* out_len) {
  const char* text = (const char*)glGetString(name);
  outCString(text, out_data, out_len);
}

uint32_t sere_gl_create_shader(uint32_t kind) {
  SERE_GL_LOAD_RET(PFNGLCREATESHADERPROC, "glCreateShader", 0);
  return fn(kind);
}

void sere_gl_shader_source(uint32_t shader, const char* src, int64_t src_len) {
  SERE_GL_LOAD(PFNGLSHADERSOURCEPROC, "glShaderSource");
  const char* text = src == NULL ? "" : src;
  const int len = src_len < 0 ? 0 : (int)src_len;
  fn(shader, 1, &text, &len);
}

int32_t sere_gl_compile_shader(uint32_t shader) {
  SERE_GL_LOAD_RET(PFNGLCOMPILESHADERPROC, "glCompileShader", 0);
  fn(shader);
  PFNGLGETSHADERIVPROC getIv = (PFNGLGETSHADERIVPROC)glProc("glGetShaderiv");
  int status = 0;
  if (getIv != NULL) {
    getIv(shader, 0x8B81, &status);
  }
  return status != 0 ? 1 : 0;
}

void sere_gl_shader_log(uint32_t shader, const char** out_data, int64_t* out_len) {
  infoLog(1, shader, out_data, out_len);
}

void sere_gl_delete_shader(uint32_t shader) {
  SERE_GL_LOAD(PFNGLDELETESHADERPROC, "glDeleteShader");
  fn(shader);
}

uint32_t sere_gl_create_program(void) {
  SERE_GL_LOAD_RET(PFNGLCREATEPROGRAMPROC, "glCreateProgram", 0);
  return fn();
}

void sere_gl_attach_shader(uint32_t program, uint32_t shader) {
  SERE_GL_LOAD(PFNGLATTACHSHADERPROC, "glAttachShader");
  fn(program, shader);
}

void sere_gl_detach_shader(uint32_t program, uint32_t shader) {
  SERE_GL_LOAD(PFNGLDETACHSHADERPROC, "glDetachShader");
  fn(program, shader);
}

int32_t sere_gl_link_program(uint32_t program) {
  SERE_GL_LOAD_RET(PFNGLLINKPROGRAMPROC, "glLinkProgram", 0);
  fn(program);
  PFNGLGETPROGRAMIVPROC getIv = (PFNGLGETPROGRAMIVPROC)glProc("glGetProgramiv");
  int status = 0;
  if (getIv != NULL) {
    getIv(program, 0x8B82, &status);
  }
  return status != 0 ? 1 : 0;
}

void sere_gl_program_log(uint32_t program, const char** out_data, int64_t* out_len) {
  infoLog(0, program, out_data, out_len);
}

void sere_gl_use_program(uint32_t program) {
  SERE_GL_LOAD(PFNGLUSEPROGRAMPROC, "glUseProgram");
  fn(program);
}

void sere_gl_delete_program(uint32_t program) {
  SERE_GL_LOAD(PFNGLDELETEPROGRAMPROC, "glDeleteProgram");
  fn(program);
}

int32_t sere_gl_uniform_location(uint32_t program, const char* name, int64_t name_len) {
  SERE_GL_LOAD_RET(PFNGLGETUNIFORMLOCATIONPROC, "glGetUniformLocation", -1);
  char* copy = toCString(name, name_len);
  const int loc = fn(program, copy == NULL ? "" : copy);
  free(copy);
  return loc;
}

int32_t sere_gl_attrib_location(uint32_t program, const char* name, int64_t name_len) {
  SERE_GL_LOAD_RET(PFNGLGETATTRIBLOCATIONPROC, "glGetAttribLocation", -1);
  char* copy = toCString(name, name_len);
  const int loc = fn(program, copy == NULL ? "" : copy);
  free(copy);
  return loc;
}

void sere_gl_bind_attrib(uint32_t program, uint32_t index, const char* name, int64_t name_len) {
  SERE_GL_LOAD(PFNGLBINDATTRIBLOCATIONPROC, "glBindAttribLocation");
  char* copy = toCString(name, name_len);
  fn(program, index, copy == NULL ? "" : copy);
  free(copy);
}

void sere_gl_uniform1f(int32_t location, double x) {
  SERE_GL_LOAD(PFNGLUNIFORM1FPROC, "glUniform1f");
  fn(location, (float)x);
}

void sere_gl_uniform2f(int32_t location, double x, double y) {
  SERE_GL_LOAD(PFNGLUNIFORM2FPROC, "glUniform2f");
  fn(location, (float)x, (float)y);
}

void sere_gl_uniform3f(int32_t location, double x, double y, double z) {
  SERE_GL_LOAD(PFNGLUNIFORM3FPROC, "glUniform3f");
  fn(location, (float)x, (float)y, (float)z);
}

void sere_gl_uniform4f(int32_t location, double x, double y, double z, double w) {
  SERE_GL_LOAD(PFNGLUNIFORM4FPROC, "glUniform4f");
  fn(location, (float)x, (float)y, (float)z, (float)w);
}

void sere_gl_uniform1i(int32_t location, int32_t x) {
  SERE_GL_LOAD(PFNGLUNIFORM1IPROC, "glUniform1i");
  fn(location, x);
}

void sere_gl_uniform2i(int32_t location, int32_t x, int32_t y) {
  SERE_GL_LOAD(PFNGLUNIFORM2IPROC, "glUniform2i");
  fn(location, x, y);
}

void sere_gl_uniform3i(int32_t location, int32_t x, int32_t y, int32_t z) {
  SERE_GL_LOAD(PFNGLUNIFORM3IPROC, "glUniform3i");
  fn(location, x, y, z);
}

void sere_gl_uniform4i(int32_t location, int32_t x, int32_t y, int32_t z, int32_t w) {
  SERE_GL_LOAD(PFNGLUNIFORM4IPROC, "glUniform4i");
  fn(location, x, y, z, w);
}

void sere_gl_uniform_vec(int32_t location, void* values) {
  int count = 0;
  float* data = f64ToFloat(values, &count);
  if (data == NULL) {
    return;
  }
  if (count >= 4) {
    sere_gl_uniform4f(location, data[0], data[1], data[2], data[3]);
  } else if (count == 3) {
    sere_gl_uniform3f(location, data[0], data[1], data[2]);
  } else if (count == 2) {
    sere_gl_uniform2f(location, data[0], data[1]);
  } else if (count == 1) {
    sere_gl_uniform1f(location, data[0]);
  }
  free(data);
}

void sere_gl_uniform_mat3(int32_t location, void* values) {
  SERE_GL_LOAD(PFNGLUNIFORMMATRIX3FVPROC, "glUniformMatrix3fv");
  int count = 0;
  float* data = f64ToFloat(values, &count);
  if (data == NULL || count < 9) {
    free(data);
    return;
  }
  fn(location, 1, 0, data);
  free(data);
}

void sere_gl_uniform_mat4(int32_t location, void* values) {
  SERE_GL_LOAD(PFNGLUNIFORMMATRIX4FVPROC, "glUniformMatrix4fv");
  int count = 0;
  float* data = f64ToFloat(values, &count);
  if (data == NULL || count < 16) {
    free(data);
    return;
  }
  fn(location, 1, 0, data);
  free(data);
}

uint32_t sere_gl_gen_buffer(void) { return genOne("glGenBuffers"); }
void sere_gl_delete_buffer(uint32_t buffer) { deleteOne("glDeleteBuffers", buffer); }

void sere_gl_bind_buffer(uint32_t target, uint32_t buffer) {
  SERE_GL_LOAD(PFNGLBINDBUFFERPROC, "glBindBuffer");
  fn(target, buffer);
}

void sere_gl_buffer_data_f64(uint32_t target, void* values, uint32_t usage) {
  SERE_GL_LOAD(PFNGLBUFFERDATAPROC, "glBufferData");
  int count = 0;
  float* data = f64ToFloat(values, &count);
  fn(target, (intptr_t)count * (intptr_t)sizeof(float), data, usage);
  free(data);
}

void sere_gl_buffer_data_i32(uint32_t target, void* values, uint32_t usage) {
  SERE_GL_LOAD(PFNGLBUFFERDATAPROC, "glBufferData");
  int64_t nbytes = 0;
  const void* data = listBytes(values, &nbytes);
  fn(target, (intptr_t)nbytes, data, usage);
}

void sere_gl_buffer_data_bytes(uint32_t target, void* values, uint32_t usage) {
  sere_gl_buffer_data_i32(target, values, usage);
}

void sere_gl_buffer_sub_f64(uint32_t target, int64_t offset_bytes, void* values) {
  SERE_GL_LOAD(PFNGLBUFFERSUBDATAPROC, "glBufferSubData");
  int count = 0;
  float* data = f64ToFloat(values, &count);
  fn(target, (intptr_t)offset_bytes, (intptr_t)count * (intptr_t)sizeof(float), data);
  free(data);
}

uint32_t sere_gl_gen_vao(void) { return genOne("glGenVertexArrays"); }
void sere_gl_delete_vao(uint32_t vao) { deleteOne("glDeleteVertexArrays", vao); }

void sere_gl_bind_vao(uint32_t vao) {
  SERE_GL_LOAD(PFNGLBINDVERTEXARRAYPROC, "glBindVertexArray");
  fn(vao);
}

void sere_gl_enable_attrib(uint32_t index) {
  SERE_GL_LOAD(PFNGLENABLEVERTEXATTRIBARRAYPROC, "glEnableVertexAttribArray");
  fn(index);
}

void sere_gl_disable_attrib(uint32_t index) {
  SERE_GL_LOAD(PFNGLDISABLEVERTEXATTRIBARRAYPROC, "glDisableVertexAttribArray");
  fn(index);
}

void sere_gl_attrib_pointer(uint32_t index, int32_t size, uint32_t type, int32_t normalized,
                            int32_t stride, int64_t offset) {
  SERE_GL_LOAD(PFNGLVERTEXATTRIBPOINTERPROC, "glVertexAttribPointer");
  fn(index, size, type, normalized != 0 ? 1 : 0, stride, (const void*)(uintptr_t)offset);
}

void sere_gl_draw_arrays(uint32_t mode, int32_t first, int32_t count) {
  glDrawArrays(mode, first, count);
}

void sere_gl_draw_elements(uint32_t mode, int32_t count, uint32_t type, int64_t offset) {
  glDrawElements(mode, count, type, (const void*)(uintptr_t)offset);
}

uint32_t sere_gl_gen_texture(void) {
  unsigned int id = 0;
  glGenTextures(1, &id);
  return id;
}

void sere_gl_delete_texture(uint32_t texture) { glDeleteTextures(1, &texture); }

void sere_gl_bind_texture(uint32_t target, uint32_t texture) { glBindTexture(target, texture); }

void sere_gl_active_texture(uint32_t unit) {
  PFNGLACTIVETEXTUREPROC fn = (PFNGLACTIVETEXTUREPROC)glProc("glActiveTexture");
  if (fn != NULL) {
    fn(unit);
  }
}

void sere_gl_tex_param(uint32_t target, uint32_t pname, int32_t value) {
  glTexParameteri(target, pname, value);
}

void sere_gl_tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t width,
                         int32_t height, uint32_t format, uint32_t type, void* pixels) {
  int64_t nbytes = 0;
  const void* data = listBytes(pixels, &nbytes);
  (void)nbytes;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(target, level, internal, width, height, 0, format, type, data);
}

void sere_gl_tex_storage(uint32_t target, int32_t internal, int32_t width, int32_t height,
                         uint32_t format) {
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(target, 0, internal, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
}

void sere_gl_tex_sub_image2d(uint32_t target, int32_t level, int32_t x, int32_t y, int32_t width,
                             int32_t height, uint32_t format, uint32_t type, void* pixels) {
  int64_t nbytes = 0;
  const void* data = listBytes(pixels, &nbytes);
  (void)nbytes;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(target, level, x, y, width, height, format, type, data);
}

void sere_gl_generate_mipmap(uint32_t target) {
  SERE_GL_LOAD(PFNGLGENERATEMIPMAPPROC, "glGenerateMipmap");
  fn(target);
}

uint32_t sere_gl_gen_framebuffer(void) { return genOne("glGenFramebuffers"); }
void sere_gl_delete_framebuffer(uint32_t fbo) { deleteOne("glDeleteFramebuffers", fbo); }

void sere_gl_bind_framebuffer(uint32_t target, uint32_t fbo) {
  SERE_GL_LOAD(PFNGLBINDFRAMEBUFFERPROC, "glBindFramebuffer");
  fn(target, fbo);
}

void sere_gl_framebuffer_texture2d(uint32_t target, uint32_t attachment, uint32_t textarget,
                                   uint32_t texture, int32_t level) {
  SERE_GL_LOAD(PFNGLFRAMEBUFFERTEXTURE2DPROC, "glFramebufferTexture2D");
  fn(target, attachment, textarget, texture, level);
}

uint32_t sere_gl_check_framebuffer(uint32_t target) {
  SERE_GL_LOAD_RET(PFNGLCHECKFRAMEBUFFERSTATUSPROC, "glCheckFramebufferStatus", 0);
  return fn(target);
}

uint32_t sere_gl_gen_renderbuffer(void) { return genOne("glGenRenderbuffers"); }
void sere_gl_delete_renderbuffer(uint32_t rbo) { deleteOne("glDeleteRenderbuffers", rbo); }

void sere_gl_bind_renderbuffer(uint32_t target, uint32_t rbo) {
  SERE_GL_LOAD(PFNGLBINDRENDERBUFFERPROC, "glBindRenderbuffer");
  fn(target, rbo);
}

void sere_gl_renderbuffer_storage(uint32_t target, uint32_t internal, int32_t width,
                                  int32_t height) {
  SERE_GL_LOAD(PFNGLRENDERBUFFERSTORAGEPROC, "glRenderbufferStorage");
  fn(target, internal, width, height);
}

void sere_gl_framebuffer_renderbuffer(uint32_t target, uint32_t attachment, uint32_t rbo_target,
                                      uint32_t rbo) {
  SERE_GL_LOAD(PFNGLFRAMEBUFFERRENDERBUFFERPROC, "glFramebufferRenderbuffer");
  fn(target, attachment, rbo_target, rbo);
}

void sere_gl_blit_framebuffer(int32_t src_x0, int32_t src_y0, int32_t src_x1, int32_t src_y1,
                              int32_t dst_x0, int32_t dst_y0, int32_t dst_x1, int32_t dst_y1,
                              uint32_t mask, uint32_t filter) {
  SERE_GL_LOAD(PFNGLBLITFRAMEBUFFERPROC, "glBlitFramebuffer");
  fn(src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0, dst_x1, dst_y1, mask, filter);
}

void* sere_gl_read_pixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                          uint32_t type) {
  int32_t w = width < 0 ? 0 : width;
  int32_t h = height < 0 ? 0 : height;
  int32_t bpp = 4;
  if (format == GL_RGB) {
    bpp = 3;
  } else if (format == GL_RED || format == GL_DEPTH_COMPONENT) {
    bpp = 1;
  }
  SereList* out = (SereList*)sere_array_new(1, (int64_t)w * (int64_t)h * (int64_t)bpp);
  if (out == NULL || out->data == NULL) {
    return out;
  }
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(x, y, w, h, format, type, out->data);
  return out;
}

#endif
