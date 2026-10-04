/// @file sere_runtime.c
/// Runtime introspection for `sys`: process arguments, version and target
/// metadata, standard streams, runtime state, and diagnostics.
///
/// The compiler links this into every program. Values that describe the build
/// are injected by CMake (`SERE_RUNTIME_*`); values that describe the running
/// process are captured once by `sere_process_init_args`, which the generated
/// C entry point calls before any module initializer runs.

#include "sere_rt.h"

#include "sere/Version.h"
#include "sere/api/sere_gc.h"

#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <shellapi.h>
#else
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif

#ifndef SERE_RUNTIME_BUILD
#define SERE_RUNTIME_BUILD "unknown"
#endif
#ifndef SERE_RUNTIME_OPT
#define SERE_RUNTIME_OPT "unknown"
#endif
#ifndef SERE_TARGET_TRIPLE
#define SERE_TARGET_TRIPLE ""
#endif

/* ==========================================================================
 * shared helpers
 * ========================================================================== */
static SereStr rtEmpty(void);
static SereStr rtCopy(const char* text);
static SereStr rtCopyBytes(const char* data, int64_t len);
static void rtOut(SereStr value, const char** out_data, int64_t* out_len);

/* ==========================================================================
 * process state captured at start-up
 * ========================================================================== */

static int32_t gArgc = 0;
static char** gArgv = NULL;
static int64_t gStartNs = 0;
static uint64_t gMainThread = 0;
static char gBackend[16] = "native";

#ifdef _WIN32
/// UTF-8 copy of the process arguments built from the real UTF-16 command line.
/// The C runtime's own argv goes through the ANSI code page, which loses every
/// character outside it, so the wide form is the only lossless source.
static int32_t rtCaptureWideArgs(void) {
  LPWSTR commandLine = GetCommandLineW();
  if (commandLine == NULL) {
    return 0;
  }
  int count = 0;
  LPWSTR* wide = CommandLineToArgvW(commandLine, &count);
  if (wide == NULL || count <= 0) {
    return 0;
  }
  char** utf8 = (char**)calloc((size_t)count, sizeof(char*));
  if (utf8 == NULL) {
    LocalFree(wide);
    return 0;
  }
  for (int index = 0; index < count; ++index) {
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, NULL, 0, NULL, NULL);
    char* text = needed > 0 ? (char*)malloc((size_t)needed) : NULL;
    if (text == NULL) {
      utf8[index] = "";
      continue;
    }
    (void)WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, text, needed, NULL, NULL);
    utf8[index] = text;
  }
  LocalFree(wide);
  gArgv = utf8;
  gArgc = count;
  return 1;
}
#endif

/// Monotonic nanoseconds. Kept local so the runtime metadata does not depend on
/// the `os` bindings, which are optional for embedded hosts.
static int64_t rtMonotonicNs(void) {
#ifdef _WIN32
  static LARGE_INTEGER frequency;
  static int ready = 0;
  LARGE_INTEGER counter;
  if (!ready) {
    QueryPerformanceFrequency(&frequency);
    ready = 1;
  }
  QueryPerformanceCounter(&counter);
  if (frequency.QuadPart == 0) {
    return 0;
  }
  return (int64_t)((counter.QuadPart / frequency.QuadPart) * 1000000000LL +
                   ((counter.QuadPart % frequency.QuadPart) * 1000000000LL) / frequency.QuadPart);
#else
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0;
  }
  return (int64_t)now.tv_sec * 1000000000LL + (int64_t)now.tv_nsec;
#endif
}

static uint64_t rtCurrentThread(void) {
#ifdef _WIN32
  return (uint64_t)GetCurrentThreadId();
#else
  return (uint64_t)(uintptr_t)pthread_self();
#endif
}

int64_t sere_runtime_now_ns(void) { return rtMonotonicNs(); }

/// Captures the process arguments. The generated C entry point calls this
/// before the first module initializer, so `sys.argv` sees them.
void sere_process_init_args(int32_t argc, char** argv) {
#ifdef _WIN32
  // The wide command line is the authoritative source; the parameters are only
  // a fallback for a host that starts the runtime without one.
  if (!rtCaptureWideArgs()) {
    gArgc = argc;
    gArgv = argv;
  }
#else
  gArgc = argc;
  gArgv = argv;
#endif
  if (gStartNs == 0) {
    gStartNs = rtMonotonicNs();
  }
  if (gMainThread == 0) {
    gMainThread = rtCurrentThread();
  }
}

/// Whether the runtime has captured the process arguments, which makes its own
/// UTF-8 copy authoritative over the platform's argv.
int32_t sere_process_args_captured(void) { return gArgv != NULL ? 1 : 0; }

/// Records which code generator produced this program. Each backend emits its
/// own name from its entry-point wrapper, so the value is never guessed.
void sere_process_set_backend(const char* name, int64_t name_len) {
  if (name == NULL || name_len <= 0) {
    return;
  }
  size_t length = (size_t)name_len;
  if (length >= sizeof(gBackend)) {
    length = sizeof(gBackend) - 1;
  }
  memcpy(gBackend, name, length);
  gBackend[length] = '\0';
}

int32_t sere_process_argc(void) { return gArgc; }

/// Builds the argument list directly rather than deferring to
/// `sere_list_from_argv`, which in turn prefers this capture: routing one
/// through the other would recurse forever.
void* sere_process_argv(void) {
  SereList* list = (SereList*)calloc(1, sizeof(SereList));
  if (list == NULL) {
    return NULL;
  }
  list->stride = (int64_t)sizeof(SereStr);
  for (int32_t index = 0; index < gArgc && gArgv != NULL; ++index) {
    SereStr item;
    item.data = gArgv[index] == NULL ? "" : gArgv[index];
    item.len = (int64_t)strlen(item.data);
    sere_list_push(list, &item);
  }
  return list;
}

/// The program path exactly as the platform passed it, which is what `argv[0]`
/// holds. It may be relative.
void sere_process_program_path(const char** out_data, int64_t* out_len) {
  if (gArgc <= 0 || gArgv == NULL || gArgv[0] == NULL) {
    rtOut(rtEmpty(), out_data, out_len);
    return;
  }
  rtOut(rtCopy(gArgv[0]), out_data, out_len);
}

/// The final component of `argv[0]`, so a program can name itself without its
/// directory.
void sere_process_program_name(const char** out_data, int64_t* out_len) {
  if (gArgc <= 0 || gArgv == NULL || gArgv[0] == NULL) {
    rtOut(rtEmpty(), out_data, out_len);
    return;
  }
  const char* path = gArgv[0];
  const char* name = path;
  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') {
      name = cursor + 1;
    }
  }
  rtOut(rtCopy(name), out_data, out_len);
}

int64_t sere_runtime_start_ns(void) {
  if (gStartNs == 0) {
    gStartNs = rtMonotonicNs();
  }
  return gStartNs;
}

int64_t sere_runtime_uptime_ns(void) {
  const int64_t start = sere_runtime_start_ns();
  if (start == 0) {
    return 0;
  }
  const int64_t now = rtMonotonicNs();
  return now > start ? now - start : 0;
}

/* ==========================================================================
 * version and implementation metadata
 * ========================================================================== */

int32_t sere_runtime_version_major(void) { return (int32_t)SERE_VERSION_MAJOR; }
int32_t sere_runtime_version_minor(void) { return (int32_t)SERE_VERSION_MINOR; }
int32_t sere_runtime_version_patch(void) { return (int32_t)SERE_VERSION_PATCH; }

void sere_runtime_version_string(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(SERE_VERSION_STRING), out_data, out_len);
}

void sere_runtime_version_pre_release(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(SERE_VERSION_PRE_RELEASE), out_data, out_len);
}

void sere_runtime_version_build(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(SERE_VERSION_BUILD), out_data, out_len);
}

void sere_runtime_implementation_name(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy("sere"), out_data, out_len);
}

void sere_runtime_backend(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(gBackend), out_data, out_len);
}

void sere_runtime_gc_name(const char** out_data, int64_t* out_len) { sere_gc_name(out_data, out_len); }

/* ==========================================================================
 * target metadata
 *
 * Everything here comes from the compiler that built this object, never from
 * the machine that happens to run the program, so cross-compiled binaries
 * report their own target.
 * ========================================================================== */

static const char* rtTargetArch(void) {
#if defined(_M_X64) || defined(__x86_64__)
  return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
  return "aarch64";
#elif defined(_M_IX86) || defined(__i386__)
  return "x86";
#elif defined(_M_ARM) || defined(__arm__)
  return "arm";
#else
  return "unknown";
#endif
}

static const char* rtTargetOs(void) {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

static const char* rtTargetAbi(void) {
#if defined(_MSC_VER)
  return "msvc";
#elif defined(__APPLE__)
  return "darwin";
#elif defined(__GLIBC__)
  return "gnu";
#elif defined(__linux__)
  return "musl";
#else
  return "unknown";
#endif
}

/// The platform tag programs switch on: `win32`, `linux`, `darwin`.
static const char* rtPlatformTag(void) {
#if defined(_WIN32)
  return "win32";
#elif defined(__APPLE__)
  return "darwin";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

void sere_runtime_target_arch(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(rtTargetArch()), out_data, out_len);
}

void sere_runtime_target_os(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(rtTargetOs()), out_data, out_len);
}

void sere_runtime_target_abi(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(rtTargetAbi()), out_data, out_len);
}

void sere_runtime_platform_tag(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(rtPlatformTag()), out_data, out_len);
}

/// The vendor field of the composed triple, which describes the ABI family
/// rather than a company.
static const char* rtTargetVendor(void) {
#if defined(_WIN32)
  return "pc";
#elif defined(__APPLE__)
  return "apple";
#else
  return "unknown";
#endif
}

void sere_runtime_target_triple(const char** out_data, int64_t* out_len) {
  if (SERE_TARGET_TRIPLE[0] != '\0') {
    rtOut(rtCopy(SERE_TARGET_TRIPLE), out_data, out_len);
    return;
  }
  char buffer[128];
  (void)snprintf(buffer,
                 sizeof(buffer),
                 "%s-%s-%s-%s",
                 rtTargetArch(),
                 rtTargetVendor(),
                 rtTargetOs(),
                 rtTargetAbi());
  rtOut(rtCopy(buffer), out_data, out_len);
}

int32_t sere_runtime_pointer_size(void) { return (int32_t)sizeof(void*); }

int32_t sere_runtime_pointer_bits(void) { return (int32_t)(sizeof(void*) * 8); }

/// The largest value a pointer-sized signed index can hold, which is what sizes
/// a container.
int64_t sere_runtime_max_size(void) { return (int64_t)PTRDIFF_MAX; }

/// 0 for a little-endian target, 1 for a big-endian one.
int32_t sere_runtime_byteorder(void) {
  const union {
    uint32_t value;
    unsigned char bytes[4];
  } probe = {1u};
  return probe.bytes[0] == 1 ? 0 : 1;
}

void sere_runtime_build_mode(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(SERE_RUNTIME_BUILD), out_data, out_len);
}

void sere_runtime_optimization(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy(SERE_RUNTIME_OPT), out_data, out_len);
}

int32_t sere_runtime_is_debug(void) {
#ifdef NDEBUG
  return 0;
#else
  return 1;
#endif
}

/// Sere lowers `assert` into every build, so this reports the language's real
/// behaviour rather than a configuration switch.
int32_t sere_runtime_assertions_enabled(void) { return 1; }

/* ==========================================================================
 * runtime limits and floating-point model
 * ========================================================================== */

double sere_runtime_float_max(void) { return (double)DBL_MAX; }
double sere_runtime_float_min(void) { return (double)DBL_MIN; }
double sere_runtime_float_epsilon(void) { return (double)DBL_EPSILON; }
int32_t sere_runtime_float_radix(void) { return (int32_t)FLT_RADIX; }
int32_t sere_runtime_float_mantissa_bits(void) { return (int32_t)DBL_MANT_DIG; }
int32_t sere_runtime_float_max_exp(void) { return (int32_t)DBL_MAX_EXP; }
int32_t sere_runtime_float_min_exp(void) { return (int32_t)DBL_MIN_EXP; }

/* ==========================================================================
 * execution environment
 * ========================================================================== */

/// Sere text is UTF-8 everywhere, so both the default and the filesystem
/// encoding are fixed rather than locale-dependent.
void sere_runtime_default_encoding(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy("utf-8"), out_data, out_len);
}

void sere_runtime_filesystem_encoding(const char** out_data, int64_t* out_len) {
  rtOut(rtCopy("utf-8"), out_data, out_len);
}

void sere_runtime_executable_suffix(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  rtOut(rtCopy(".exe"), out_data, out_len);
#else
  rtOut(rtEmpty(), out_data, out_len);
#endif
}

void sere_runtime_shared_library_suffix(const char** out_data, int64_t* out_len) {
#if defined(_WIN32)
  rtOut(rtCopy(".dll"), out_data, out_len);
#elif defined(__APPLE__)
  rtOut(rtCopy(".dylib"), out_data, out_len);
#else
  rtOut(rtCopy(".so"), out_data, out_len);
#endif
}

uint64_t sere_runtime_thread_id(void) { return rtCurrentThread(); }

uint64_t sere_runtime_main_thread_id(void) {
  if (gMainThread == 0) {
    gMainThread = rtCurrentThread();
  }
  return gMainThread;
}

int32_t sere_runtime_is_main_thread(void) {
  return rtCurrentThread() == sere_runtime_main_thread_id() ? 1 : 0;
}

int32_t sere_runtime_has_threads(void) { return 1; }

int32_t sere_runtime_has_async(void) { return 1; }

int32_t sere_runtime_has_ffi(void) { return 1; }

int32_t sere_runtime_has_debug_symbols(void) {
#ifdef NDEBUG
  return 0;
#else
  return 1;
#endif
}

/* ==========================================================================
 * standard streams and shutdown
 * ========================================================================== */

/// Writes UTF-8 to one of the standard streams. On a Windows console the text
/// goes through the wide API, because the console renders raw bytes in its
/// active code page and would mangle anything outside it. Returns 0 when the
/// stream is redirected, so the caller writes the bytes unchanged.
int32_t sere_runtime_console_write(int32_t stream, const char* data, int64_t len) {
#if defined(_WIN32)
  if (data == NULL || len <= 0) {
    return 0;
  }
  FILE* file = stream == 2 ? stderr : stdout;
  HANDLE handle = (HANDLE)_get_osfhandle(_fileno(file));
  if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
    return 0;
  }
  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode)) {
    return 0;
  }
  const int needed = MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
  if (needed <= 0) {
    return 0;
  }
  WCHAR* wide = (WCHAR*)malloc((size_t)needed * sizeof(WCHAR));
  if (wide == NULL) {
    return 0;
  }
  (void)MultiByteToWideChar(CP_UTF8, 0, data, (int)len, wide, needed);
  DWORD written = 0;
  DWORD offset = 0;
  while (offset < (DWORD)needed &&
         WriteConsoleW(handle, wide + offset, (DWORD)needed - offset, &written, NULL)) {
    offset += written;
  }
  free(wide);
  return 1;
#else
  (void)stream;
  (void)data;
  (void)len;
  return 0;
#endif
}

/// Flushes the buffered standard streams the runtime writes through.
void sere_runtime_flush(void) {
  (void)fflush(stdout);
  (void)fflush(stderr);
}

/// Terminates with `code` after flushing, matching what a normal return from
/// `main` would do.
void sere_runtime_exit(int32_t code) {
  sere_runtime_flush();
  exit((int)code);
}

/// Terminates abnormally. Buffered output is not flushed, which is the point:
/// `abort` signals a bug rather than an orderly shutdown.
void sere_runtime_abort(void) {
  (void)fflush(stderr);
  abort();
}

/// Traps into an attached debugger where one exists. Without a debugger this
/// terminates the process, like a failed `assert` in C.
void sere_runtime_breakpoint(void) {
#ifdef _WIN32
  if (IsDebuggerPresent()) {
    DebugBreak();
  } else {
    abort();
  }
#else
  raise(SIGTRAP);
#endif
}

/// Writes to the process's debugger/diagnostic sink: the Windows debug output
/// channel when one is attached, otherwise the standard error stream.
void sere_runtime_debug_write(const char* data, int64_t len) {
  if (data == NULL || len <= 0) {
    return;
  }
#ifdef _WIN32
  {
    int needed = MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
    if (needed > 0) {
      WCHAR* wide = (WCHAR*)malloc(((size_t)needed + 1) * sizeof(WCHAR));
      if (wide != NULL) {
        (void)MultiByteToWideChar(CP_UTF8, 0, data, (int)len, wide, needed);
        wide[needed] = L'\0';
        OutputDebugStringW(wide);
        free(wide);
        return;
      }
    }
  }
#endif
  (void)fwrite(data, 1, (size_t)len, stderr);
  (void)fflush(stderr);
}

/* ==========================================================================
 * exception state
 * ========================================================================== */

int32_t sere_runtime_has_exception(void) { return sere_has_error(); }

/// The most derived class name of the handled exception. The runtime stores the
/// inheritance chain separated by ';', and callers want the first entry.
void sere_runtime_exception_type(const char** out_data, int64_t* out_len) {
  const char* name = sere_error_type();
  if (name == NULL) {
    rtOut(rtEmpty(), out_data, out_len);
    return;
  }
  const char* end = strchr(name, ';');
  if (end == NULL) {
    rtOut(rtCopy(name), out_data, out_len);
    return;
  }
  rtOut(rtCopyBytes(name, (int64_t)(end - name)), out_data, out_len);
}

void sere_runtime_exception_message(const char** out_data, int64_t* out_len) {
  int64_t length = 0;
  const char* text = sere_error_message(&length);
  if (text == NULL || length <= 0) {
    rtOut(rtEmpty(), out_data, out_len);
    return;
  }
  rtOut(rtCopyBytes(text, length), out_data, out_len);
}

/* ==========================================================================
 * helpers
 * ========================================================================== */

static SereStr rtEmpty(void) {
  SereStr result;
  result.data = "";
  result.len = 0;
  return result;
}

static SereStr rtCopy(const char* text) {
  if (text == NULL) {
    return rtEmpty();
  }
  const size_t length = strlen(text);
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    return rtEmpty();
  }
  memcpy(copy, text, length + 1);
  SereStr result;
  result.data = copy;
  result.len = (int64_t)length;
  return result;
}

static SereStr rtCopyBytes(const char* data, int64_t len) {
  if (data == NULL || len <= 0) {
    return rtEmpty();
  }
  char* copy = (char*)malloc((size_t)len + 1);
  if (copy == NULL) {
    return rtEmpty();
  }
  memcpy(copy, data, (size_t)len);
  copy[len] = '\0';
  SereStr result;
  result.data = copy;
  result.len = len;
  return result;
}

static void rtOut(SereStr value, const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = value.data == NULL ? "" : value.data;
  }
  if (out_len != NULL) {
    *out_len = value.len;
  }
}
