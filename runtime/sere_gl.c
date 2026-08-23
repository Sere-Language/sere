/// @file sere_gl.c
/// OpenGL 2.1+ (compat) with a Win32 WGL window. Shader, buffer, texture, and
/// framebuffer entry points are loaded from the driver. Other hosts stub out.

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
  (void)title;
  (void)title_len;
  (void)width;
  (void)height;
  return NULL;
}
int32_t sere_gl_window_poll(void* window) {
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
void sere_gl_window_show_cursor(void* window, int32_t show) {
  (void)window;
  (void)show;
}
void sere_gl_window_set_size(void* window, int32_t width, int32_t height) {
  (void)window;
  (void)width;
  (void)height;
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
int32_t sere_gl_window_mouse_x(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_y(void* window) {
  (void)window;
  return 0;
}
int32_t sere_gl_window_mouse_button(void* window, int32_t button) {
  (void)window;
  (void)button;
  return 0;
}
int32_t sere_gl_window_wheel(void* window) {
  (void)window;
  return 0;
}
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
void sere_gl_depth_func(uint32_t func) { (void)func; }
void sere_gl_depth_mask(int32_t enabled) { (void)enabled; }
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
void sere_gl_line_width(double width) { (void)width; }
void sere_gl_point_size(double size) { (void)size; }
void sere_gl_pixel_store(uint32_t pname, int32_t value) {
  (void)pname;
  (void)value;
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
void sere_gl_uniform_vec(int32_t location, void* values) {
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
typedef void(APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)(int, int, unsigned char, const float*);
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

typedef struct SereGlWindow {
  HWND hwnd;
  HDC hdc;
  HGLRC glrc;
  char* title;
  int32_t width;
  int32_t height;
  int32_t alive;
  int32_t wheel;
  double start;
  double last;
  double freq;
} SereGlWindow;

static double nowSeconds(const SereGlWindow* window) {
  LARGE_INTEGER ticks;
  QueryPerformanceCounter(&ticks);
  return window == NULL || window->freq <= 0.0 ? 0.0 : (double)ticks.QuadPart / window->freq;
}

static LRESULT CALLBACK sereGlWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  SereGlWindow* window = (SereGlWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
  if (msg == WM_CLOSE) {
    if (window != NULL) {
      window->alive = 0;
    }
    DestroyWindow(hwnd);
    return 0;
  }
  if (msg == WM_DESTROY) {
    return 0;
  }
  if (msg == WM_SIZE && window != NULL) {
    window->width = (int32_t)LOWORD(lparam);
    window->height = (int32_t)HIWORD(lparam);
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
  WNDCLASSA wc;
  memset(&wc, 0, sizeof(wc));
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = sereGlWndProc;
  wc.hInstance = GetModuleHandleA(NULL);
  wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
  wc.lpszClassName = "SereGLWindow";
  RegisterClassA(&wc);
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
  registerClass();
  char* caption = toCString(title, title_len);
  SereGlWindow* window = (SereGlWindow*)calloc(1, sizeof(SereGlWindow));
  if (window == NULL) {
    free(caption);
    return NULL;
  }
  window->title = caption;
  window->width = width <= 0 ? 640 : width;
  window->height = height <= 0 ? 480 : height;
  window->alive = 1;
  window->hwnd = CreateWindowExA(
      0, "SereGLWindow", caption == NULL ? "Sere" : caption, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, window->width, window->height, NULL, NULL,
      GetModuleHandleA(NULL), NULL);
  if (window->hwnd == NULL) {
    free(caption);
    free(window);
    return NULL;
  }
  SetWindowLongPtrA(window->hwnd, GWLP_USERDATA, (LONG_PTR)window);
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
  ShowWindow(window->hwnd, SW_SHOW);
  return window;
}

int32_t sere_gl_window_poll(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL || !window->alive) {
    return 0;
  }
  window->wheel = 0;
  MSG msg;
  while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      window->alive = 0;
      return 0;
    }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  return window->alive;
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
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL) {
    return;
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
  SereGlWindow* window = (SereGlWindow*)handle;
  return window == NULL ? 0 : window->width;
}

int32_t sere_gl_window_height(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  return window == NULL ? 0 : window->height;
}

void sere_gl_window_set_title(void* handle, const char* title, int64_t title_len) {
  SereGlWindow* window = (SereGlWindow*)handle;
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
  SereGlWindow* window = (SereGlWindow*)handle;
  outCString(window == NULL ? "" : window->title, out_data, out_len);
}

void sere_gl_window_set_vsync(void* handle, int32_t enabled) {
  (void)handle;
  PFNWGLSWAPINTERVALEXTPROC swap = (PFNWGLSWAPINTERVALEXTPROC)glProc("wglSwapIntervalEXT");
  if (swap != NULL) {
    swap(enabled != 0 ? 1 : 0);
  }
}

void sere_gl_window_show_cursor(void* handle, int32_t show) {
  (void)handle;
  ShowCursor(show != 0 ? TRUE : FALSE);
}

void sere_gl_window_set_size(void* handle, int32_t width, int32_t height) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL || window->hwnd == NULL) {
    return;
  }
  RECT rect = {0, 0, width, height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  SetWindowPos(window->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
               SWP_NOMOVE | SWP_NOZORDER);
}

double sere_gl_window_time(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  return window == NULL ? 0.0 : nowSeconds(window) - window->start;
}

double sere_gl_window_dt(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL) {
    return 0.0;
  }
  const double now = nowSeconds(window);
  const double dt = now - window->last;
  window->last = now;
  return dt < 0.0 ? 0.0 : dt;
}

int32_t sere_gl_window_key(void* handle, int32_t vk) {
  (void)handle;
  return (GetAsyncKeyState(vk) & 0x8000) != 0 ? 1 : 0;
}

int32_t sere_gl_window_mouse_x(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL || window->hwnd == NULL) {
    return 0;
  }
  POINT point;
  GetCursorPos(&point);
  ScreenToClient(window->hwnd, &point);
  return (int32_t)point.x;
}

int32_t sere_gl_window_mouse_y(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL || window->hwnd == NULL) {
    return 0;
  }
  POINT point;
  GetCursorPos(&point);
  ScreenToClient(window->hwnd, &point);
  return (int32_t)point.y;
}

int32_t sere_gl_window_mouse_button(void* handle, int32_t button) {
  (void)handle;
  int vk = VK_LBUTTON;
  if (button == 1) {
    vk = VK_RBUTTON;
  } else if (button == 2) {
    vk = VK_MBUTTON;
  }
  return (GetAsyncKeyState(vk) & 0x8000) != 0 ? 1 : 0;
}

int32_t sere_gl_window_wheel(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  return window == NULL ? 0 : window->wheel;
}

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
void sere_gl_depth_func(uint32_t func) { glDepthFunc(func); }
void sere_gl_depth_mask(int32_t enabled) { glDepthMask(enabled != 0 ? GL_TRUE : GL_FALSE); }
void sere_gl_color_mask(int32_t r, int32_t g, int32_t b, int32_t a) {
  glColorMask(r != 0, g != 0, b != 0, a != 0);
}
void sere_gl_cull_face(uint32_t mode) { glCullFace(mode); }
void sere_gl_front_face(uint32_t mode) { glFrontFace(mode); }
void sere_gl_polygon_mode(uint32_t face, uint32_t mode) { glPolygonMode(face, mode); }
void sere_gl_line_width(double width) { glLineWidth((float)width); }
void sere_gl_point_size(double size) { glPointSize((float)size); }
void sere_gl_pixel_store(uint32_t pname, int32_t value) { glPixelStorei(pname, value); }
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
