/// @file sere_gl.c
/// OpenGL 1.1 plus lazily loaded shader APIs. A Win32 GL window is provided on Windows.

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
void sere_gl_clear_color(float r, float g, float b, float a) {
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
void sere_gl_enable(uint32_t cap) { (void)cap; }
void sere_gl_disable(uint32_t cap) { (void)cap; }
void sere_gl_begin(uint32_t mode) { (void)mode; }
void sere_gl_end(void) {}
void sere_gl_vertex3f(float x, float y, float z) {
  (void)x;
  (void)y;
  (void)z;
}
void sere_gl_color3f(float r, float g, float b) {
  (void)r;
  (void)g;
  (void)b;
}
uint32_t sere_gl_get_error(void) { return 0; }
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

#else

typedef struct SereGlWindow {
  HWND hwnd;
  HDC hdc;
  HGLRC glrc;
  int32_t width;
  int32_t height;
  int32_t alive;
} SereGlWindow;

static LRESULT CALLBACK sereGlWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_CLOSE) {
    SereGlWindow* window = (SereGlWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (window != NULL) {
      window->alive = 0;
    }
    DestroyWindow(hwnd);
    return 0;
  }
  if (msg == WM_DESTROY) {
    return 0;
  }
  if (msg == WM_SIZE) {
    SereGlWindow* window = (SereGlWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (window != NULL) {
      window->width = (int32_t)LOWORD(lparam);
      window->height = (int32_t)HIWORD(lparam);
    }
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

int32_t sere_gl_available(void) { return 1; }

void* sere_gl_window_new(const char* title, int64_t title_len, int32_t width, int32_t height) {
  registerClass();
  char* caption = toCString(title, title_len);
  SereGlWindow* window = (SereGlWindow*)calloc(1, sizeof(SereGlWindow));
  if (window == NULL) {
    free(caption);
    return NULL;
  }
  window->width = width <= 0 ? 640 : width;
  window->height = height <= 0 ? 480 : height;
  window->alive = 1;
  window->hwnd = CreateWindowExA(
      0, "SereGLWindow", caption == NULL ? "Sere" : caption, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, window->width, window->height, NULL, NULL,
      GetModuleHandleA(NULL), NULL);
  free(caption);
  if (window->hwnd == NULL) {
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
  const int format = ChoosePixelFormat(window->hdc, &pfd);
  SetPixelFormat(window->hdc, format, &pfd);
  window->glrc = wglCreateContext(window->hdc);
  wglMakeCurrent(window->hdc, window->glrc);
  ShowWindow(window->hwnd, SW_SHOW);
  return window;
}

int32_t sere_gl_window_poll(void* handle) {
  SereGlWindow* window = (SereGlWindow*)handle;
  if (window == NULL || !window->alive) {
    return 0;
  }
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

void sere_gl_clear_color(float r, float g, float b, float a) { glClearColor(r, g, b, a); }
void sere_gl_clear(uint32_t mask) { glClear(mask); }
void sere_gl_viewport(int32_t x, int32_t y, int32_t width, int32_t height) {
  glViewport(x, y, width, height);
}
void sere_gl_enable(uint32_t cap) { glEnable(cap); }
void sere_gl_disable(uint32_t cap) { glDisable(cap); }
void sere_gl_begin(uint32_t mode) { glBegin(mode); }
void sere_gl_end(void) { glEnd(); }
void sere_gl_vertex3f(float x, float y, float z) { glVertex3f(x, y, z); }
void sere_gl_color3f(float r, float g, float b) { glColor3f(r, g, b); }
uint32_t sere_gl_get_error(void) { return (uint32_t)glGetError(); }

typedef unsigned int(APIENTRY* PFNGLCREATESHADERPROC)(unsigned int);
typedef void(APIENTRY* PFNGLSHADERSOURCEPROC)(unsigned int, int, const char* const*, const int*);
typedef void(APIENTRY* PFNGLCOMPILESHADERPROC)(unsigned int);
typedef void(APIENTRY* PFNGLGETSHADERIVPROC)(unsigned int, unsigned int, int*);
typedef void(APIENTRY* PFNGLGETSHADERINFOLOGPROC)(unsigned int, int, int*, char*);
typedef void(APIENTRY* PFNGLDELETESHADERPROC)(unsigned int);
typedef unsigned int(APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRY* PFNGLATTACHSHADERPROC)(unsigned int, unsigned int);
typedef void(APIENTRY* PFNGLLINKPROGRAMPROC)(unsigned int);
typedef void(APIENTRY* PFNGLGETPROGRAMIVPROC)(unsigned int, unsigned int, int*);
typedef void(APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)(unsigned int, int, int*, char*);
typedef void(APIENTRY* PFNGLUSEPROGRAMPROC)(unsigned int);
typedef void(APIENTRY* PFNGLDELETEPROGRAMPROC)(unsigned int);

static void* glProc(const char* name) { return (void*)wglGetProcAddress(name); }

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
  static PFNGLGETSHADERIVPROC getIv = NULL;
  if (getIv == NULL) {
    getIv = (PFNGLGETSHADERIVPROC)glProc("glGetShaderiv");
  }
  int status = 0;
  if (getIv != NULL) {
    getIv(shader, 0x8B81, &status);
  }
  return status != 0 ? 1 : 0;
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
    if (out_data != NULL) {
      *out_data = buf;
    }
    if (out_len != NULL) {
      *out_len = length;
    }
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
  if (out_data != NULL) {
    *out_data = buf;
  }
  if (out_len != NULL) {
    *out_len = length;
  }
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

#endif
