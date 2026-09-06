/// @file sere_sys.c
/// Stdlib C ABI: I/O, files, paths, process, strings, math, and time.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#endif

static const int64_t kMaxTextBytes = 64LL * 1024 * 1024;

static SereStr emptyStr(void) {
  SereStr result;
  result.data = "";
  result.len = 0;
  return result;
}

static SereStr ownBytes(char* data, int64_t len) {
  SereStr result;
  if (data == NULL) {
    return emptyStr();
  }
  result.data = data;
  result.len = len;
  return result;
}

static SereStr copyCString(const char* text) {
  if (text == NULL) {
    return emptyStr();
  }
  const size_t n = strlen(text);
  char* copy = (char*)malloc(n + 1);
  if (copy == NULL) {
    return emptyStr();
  }
  memcpy(copy, text, n + 1);
  return ownBytes(copy, (int64_t)n);
}

static void outStr(SereStr value, const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = value.data == NULL ? "" : value.data;
  }
  if (out_len != NULL) {
    *out_len = value.len;
  }
}

static SereStr copyBytes(const char* data, int64_t len) {
  if (data == NULL || len <= 0) {
    return emptyStr();
  }
  char* copy = (char*)malloc((size_t)len + 1);
  if (copy == NULL) {
    return emptyStr();
  }
  memcpy(copy, data, (size_t)len);
  copy[len] = '\0';
  return ownBytes(copy, len);
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

static int isSep(char ch) {
#ifdef _WIN32
  return ch == '/' || ch == '\\';
#else
  return ch == '/';
#endif
}

#ifdef _WIN32
static char pathSep(void) { return '\\'; }
#else
static char pathSep(void) { return '/'; }
#endif

static SereStr readLineImpl(void) {
  size_t cap = 128;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (buf == NULL) {
    return emptyStr();
  }
  for (;;) {
    if (len + 1 >= cap) {
      cap *= 2;
      char* grown = (char*)realloc(buf, cap);
      if (grown == NULL) {
        free(buf);
        return emptyStr();
      }
      buf = grown;
    }
    const int ch = fgetc(stdin);
    if (ch == EOF) {
      break;
    }
    if (ch == '\n') {
      break;
    }
    if (ch == '\r') {
      const int next = fgetc(stdin);
      if (next != '\n' && next != EOF) {
        ungetc(next, stdin);
      }
      break;
    }
    buf[len++] = (char)ch;
  }
  buf[len] = '\0';
  return ownBytes(buf, (int64_t)len);
}

void sere_io_read_line(const char** out_data, int64_t* out_len) {
  outStr(readLineImpl(), out_data, out_len);
}

void sere_io_eprint(const char* data, int64_t len) {
  if (data != NULL && len > 0) {
    fwrite(data, 1, (size_t)len, stderr);
  }
}

void sere_io_eprint_nl(void) { fputc('\n', stderr); }

static SereStr readTextImpl(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return emptyStr();
  }
  FILE* file = fopen(cpath, "rb");
  free(cpath);
  if (file == NULL) {
    return emptyStr();
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return emptyStr();
  }
  const long size = ftell(file);
  if (size < 0 || size > kMaxTextBytes || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return emptyStr();
  }
  char* buf = (char*)malloc((size_t)size + 1);
  if (buf == NULL) {
    fclose(file);
    return emptyStr();
  }
  const size_t n = fread(buf, 1, (size_t)size, file);
  fclose(file);
  buf[n] = '\0';
  return ownBytes(buf, (int64_t)n);
}

void sere_fs_read_text(const char* path, int64_t path_len, const char** out_data, int64_t* out_len) {
  outStr(readTextImpl(path, path_len), out_data, out_len);
}

int32_t sere_fs_write_text(const char* path, int64_t path_len, const char* data, int64_t data_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  FILE* file = fopen(cpath, "wb");
  free(cpath);
  if (file == NULL) {
    return 0;
  }
  const size_t want = data == NULL || data_len <= 0 ? 0 : (size_t)data_len;
  const size_t wrote = want == 0 ? 0 : fwrite(data, 1, want, file);
  const int ok = fclose(file) == 0 && wrote == want;
  return ok ? 1 : 0;
}

#ifdef _WIN32
static int pathStat(const char* path, struct _stat* out) { return _stat(path, out); }
#else
static int pathStat(const char* path, struct stat* out) { return stat(path, out); }
#endif

int32_t sere_fs_exists(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
#ifdef _WIN32
  struct _stat info;
#else
  struct stat info;
#endif
  const int32_t ok = pathStat(cpath, &info) == 0 ? 1 : 0;
  free(cpath);
  return ok;
}

int32_t sere_fs_is_file(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
#ifdef _WIN32
  struct _stat info;
  const int32_t ok = pathStat(cpath, &info) == 0 && (info.st_mode & _S_IFREG) != 0 ? 1 : 0;
#else
  struct stat info;
  const int32_t ok = pathStat(cpath, &info) == 0 && S_ISREG(info.st_mode) ? 1 : 0;
#endif
  free(cpath);
  return ok;
}

int32_t sere_fs_is_dir(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
#ifdef _WIN32
  struct _stat info;
  const int32_t ok = pathStat(cpath, &info) == 0 && (info.st_mode & _S_IFDIR) != 0 ? 1 : 0;
#else
  struct stat info;
  const int32_t ok = pathStat(cpath, &info) == 0 && S_ISDIR(info.st_mode) ? 1 : 0;
#endif
  free(cpath);
  return ok;
}

int32_t sere_fs_remove(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
#ifdef _WIN32
  const int32_t ok = _unlink(cpath) == 0 || _rmdir(cpath) == 0 ? 1 : 0;
#else
  const int32_t ok = unlink(cpath) == 0 || rmdir(cpath) == 0 ? 1 : 0;
#endif
  free(cpath);
  return ok;
}

int32_t sere_fs_mkdir(const char* path, int64_t path_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
#ifdef _WIN32
  const int32_t ok = _mkdir(cpath) == 0 ? 1 : 0;
#else
  const int32_t ok = mkdir(cpath, 0777) == 0 ? 1 : 0;
#endif
  free(cpath);
  return ok;
}

static SereStr pathJoinImpl(const char* left, int64_t left_len, const char* right, int64_t right_len) {
  if (right == NULL) {
    right = "";
    right_len = 0;
  }
  if (sere_path_is_abs(right, right_len) || left == NULL || left_len <= 0) {
    return copyBytes(right, right_len);
  }
  const int left_sep = isSep(left[left_len - 1]);
  const int64_t extra = left_sep ? 0 : 1;
  const int64_t total = left_len + extra + right_len;
  char* out = (char*)malloc((size_t)total + 1);
  if (out == NULL) {
    return emptyStr();
  }
  memcpy(out, left, (size_t)left_len);
  int64_t n = left_len;
  if (!left_sep) {
    out[n++] = pathSep();
  }
  if (right_len > 0) {
    memcpy(out + n, right, (size_t)right_len);
    n += right_len;
  }
  out[n] = '\0';
  return ownBytes(out, n);
}

void sere_path_join(const char* left, int64_t left_len, const char* right, int64_t right_len,
                    const char** out_data, int64_t* out_len) {
  outStr(pathJoinImpl(left, left_len, right, right_len), out_data, out_len);
}

static int64_t lastSep(const char* path, int64_t path_len) {
  for (int64_t i = path_len - 1; i >= 0; --i) {
    if (isSep(path[i])) {
      return i;
    }
  }
  return -1;
}

static SereStr pathDirnameImpl(const char* path, int64_t path_len) {
  if (path == NULL || path_len <= 0) {
    return copyCString(".");
  }
  const int64_t sep = lastSep(path, path_len);
  if (sep < 0) {
    return copyCString(".");
  }
  if (sep == 0) {
    char* out = (char*)malloc(2);
    if (out == NULL) {
      return emptyStr();
    }
    out[0] = path[0];
    out[1] = '\0';
    return ownBytes(out, 1);
  }
  char* out = (char*)malloc((size_t)sep + 1);
  if (out == NULL) {
    return emptyStr();
  }
  memcpy(out, path, (size_t)sep);
  out[sep] = '\0';
  return ownBytes(out, sep);
}

void sere_path_dirname(const char* path, int64_t path_len, const char** out_data, int64_t* out_len) {
  outStr(pathDirnameImpl(path, path_len), out_data, out_len);
}

static SereStr pathBasenameImpl(const char* path, int64_t path_len) {
  if (path == NULL || path_len <= 0) {
    return emptyStr();
  }
  const int64_t sep = lastSep(path, path_len);
  const int64_t start = sep < 0 ? 0 : sep + 1;
  const int64_t n = path_len - start;
  char* out = (char*)malloc((size_t)n + 1);
  if (out == NULL) {
    return emptyStr();
  }
  if (n > 0) {
    memcpy(out, path + start, (size_t)n);
  }
  out[n] = '\0';
  return ownBytes(out, n);
}

void sere_path_basename(const char* path, int64_t path_len, const char** out_data, int64_t* out_len) {
  outStr(pathBasenameImpl(path, path_len), out_data, out_len);
}

static SereStr pathExtImpl(const char* path, int64_t path_len) {
  SereStr base = pathBasenameImpl(path, path_len);
  int64_t dot = -1;
  for (int64_t i = base.len - 1; i > 0; --i) {
    if (base.data[i] == '.') {
      dot = i;
      break;
    }
  }
  SereStr result = emptyStr();
  if (dot >= 0) {
    result = copyBytes(base.data + dot, base.len - dot);
  }
  if (base.len > 0) {
    free((void*)base.data);
  }
  return result;
}

void sere_path_ext(const char* path, int64_t path_len, const char** out_data, int64_t* out_len) {
  outStr(pathExtImpl(path, path_len), out_data, out_len);
}

int32_t sere_path_is_abs(const char* path, int64_t path_len) {
  if (path == NULL || path_len <= 0) {
    return 0;
  }
  if (isSep(path[0])) {
    return 1;
  }
#ifdef _WIN32
  if (path_len >= 2 && path[1] == ':' &&
      ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) {
    return 1;
  }
#endif
  return 0;
}

static SereStr getcwdImpl(void) {
  char buf[4096];
#ifdef _WIN32
  if (_getcwd(buf, (int)sizeof(buf)) == NULL) {
    return emptyStr();
  }
#else
  if (getcwd(buf, sizeof(buf)) == NULL) {
    return emptyStr();
  }
#endif
  return copyCString(buf);
}

void sere_os_getcwd(const char** out_data, int64_t* out_len) {
  outStr(getcwdImpl(), out_data, out_len);
}

static SereStr getenvImpl(const char* name, int64_t name_len) {
  char* key = toCString(name, name_len);
  if (key == NULL) {
    return emptyStr();
  }
  const char* value = getenv(key);
  free(key);
  return copyCString(value == NULL ? "" : value);
}

void sere_os_getenv(const char* name, int64_t name_len, const char** out_data, int64_t* out_len) {
  outStr(getenvImpl(name, name_len), out_data, out_len);
}

void sere_os_exit(int32_t code) { exit((int)code); }

#ifdef _WIN32
void* sere_os_listdir(const char* path, int64_t path_len) {
  void* list = sere_list_new((int64_t)sizeof(SereStr));
  char* cpath = toCString(path, path_len);
  if (list == NULL || cpath == NULL) {
    free(cpath);
    return list;
  }
  char pattern[MAX_PATH];
  const size_t n = strlen(cpath);
  if (n + 3 >= sizeof(pattern)) {
    free(cpath);
    return list;
  }
  memcpy(pattern, cpath, n + 1);
  if (n > 0 && !isSep(cpath[n - 1])) {
    pattern[n] = '\\';
    pattern[n + 1] = '*';
    pattern[n + 2] = '\0';
  } else {
    pattern[n] = '*';
    pattern[n + 1] = '\0';
  }
  free(cpath);
  WIN32_FIND_DATAA data;
  HANDLE handle = FindFirstFileA(pattern, &data);
  if (handle == INVALID_HANDLE_VALUE) {
    return list;
  }
  do {
    if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
      continue;
    }
    SereStr item = copyCString(data.cFileName);
    sere_list_push(list, &item);
  } while (FindNextFileA(handle, &data));
  FindClose(handle);
  return list;
}
#else
void* sere_os_listdir(const char* path, int64_t path_len) {
  void* list = sere_list_new((int64_t)sizeof(SereStr));
  char* cpath = toCString(path, path_len);
  if (list == NULL || cpath == NULL) {
    free(cpath);
    return list;
  }
  DIR* dir = opendir(cpath);
  free(cpath);
  if (dir == NULL) {
    return list;
  }
  for (;;) {
    const struct dirent* entry = readdir(dir);
    if (entry == NULL) {
      break;
    }
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    SereStr item = copyCString(entry->d_name);
    sere_list_push(list, &item);
  }
  closedir(dir);
  return list;
}
#endif

static int64_t sereEpochMillis(void) {
#ifdef _WIN32
  FILETIME fileTime;
  GetSystemTimeAsFileTime(&fileTime);
  ULARGE_INTEGER value;
  value.LowPart = fileTime.dwLowDateTime;
  value.HighPart = fileTime.dwHighDateTime;
  return (int64_t)((value.QuadPart / 10000ULL) - 11644473600000ULL);
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
#endif
}

int64_t sere_time_now_ms(void) {
  return sereEpochMillis();
}

void sere_time_sleep_ms(int32_t ms) {
  if (ms <= 0) {
    return;
  }
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
#endif
}

void sere_time_get_timestamp(const char** out_data, int64_t* out_len) {
  char buffer[32];
  const int64_t millis = sereEpochMillis();
  snprintf(buffer, sizeof(buffer), "%lld.%03lld", (long long)(millis / 1000),
           (long long)(millis % 1000));
  outStr(copyCString(buffer), out_data, out_len);
}

static SereStr mapAscii(const char* data, int64_t len, int (*fn)(int)) {
  if (data == NULL || len <= 0) {
    return emptyStr();
  }
  char* out = (char*)malloc((size_t)len + 1);
  if (out == NULL) {
    return emptyStr();
  }
  for (int64_t i = 0; i < len; ++i) {
    out[i] = (char)fn((unsigned char)data[i]);
  }
  out[len] = '\0';
  return ownBytes(out, len);
}

void sere_string_upper(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  outStr(mapAscii(data, len, toupper), out_data, out_len);
}

void sere_string_lower(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  outStr(mapAscii(data, len, tolower), out_data, out_len);
}

static SereStr stripImpl(const char* data, int64_t len) {
  if (data == NULL || len <= 0) {
    return emptyStr();
  }
  int64_t begin = 0;
  int64_t end = len;
  while (begin < end && isspace((unsigned char)data[begin])) {
    begin += 1;
  }
  while (end > begin && isspace((unsigned char)data[end - 1])) {
    end -= 1;
  }
  return copyBytes(data + begin, end - begin);
}

void sere_string_strip(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  outStr(stripImpl(data, len), out_data, out_len);
}

int32_t sere_string_starts_with(const char* data, int64_t len, const char* prefix,
                               int64_t prefix_len) {
  if (prefix_len <= 0) {
    return 1;
  }
  if (data == NULL || prefix == NULL || prefix_len > len) {
    return 0;
  }
  return memcmp(data, prefix, (size_t)prefix_len) == 0 ? 1 : 0;
}

int32_t sere_string_ends_with(const char* data, int64_t len, const char* suffix,
                             int64_t suffix_len) {
  if (suffix_len <= 0) {
    return 1;
  }
  if (data == NULL || suffix == NULL || suffix_len > len) {
    return 0;
  }
  return memcmp(data + (len - suffix_len), suffix, (size_t)suffix_len) == 0 ? 1 : 0;
}

void sere_string_repeat(const char* data, int64_t len, int64_t count, const char** out_data,
                        int64_t* out_len) {
  sere_str_repeat(data, len, count, out_data, out_len);
}

static uint64_t g_rng = 0x9E3779B97F4A7C15ULL;

void sere_random_seed(int64_t seed) { g_rng = (uint64_t)seed | 1ULL; }

static uint64_t rngNext(void) {
  uint64_t x = g_rng;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  g_rng = x;
  return x * 2685821657736338717ULL;
}

int32_t sere_random_i32(void) { return (int32_t)rngNext(); }

int64_t sere_random_i64(void) { return (int64_t)rngNext(); }

double sere_random_f64(void) {
  const uint64_t bits = rngNext() >> 11;
  return (double)bits / (double)(1ULL << 53);
}

int32_t sere_random_range(int32_t low, int32_t high) {
  if (high <= low) {
    return low;
  }
  const uint32_t span = (uint32_t)(high - low);
  return low + (int32_t)(rngNext() % (uint64_t)span);
}

int64_t sere_hash_fnv1a(const char* data, int64_t len) {
  uint64_t hash = 14695981039346656037ULL;
  if (data == NULL || len <= 0) {
    return (int64_t)hash;
  }
  for (int64_t index = 0; index < len; ++index) {
    hash ^= (uint8_t)data[index];
    hash *= 1099511628211ULL;
  }
  return (int64_t)hash;
}

int64_t sere_hash_combine(int64_t left, int64_t right) {
  const uint64_t mix = (uint64_t)left ^ ((uint64_t)right + 0x9E3779B97F4A7C15ULL +
                                         ((uint64_t)left << 6) + ((uint64_t)left >> 2));
  return (int64_t)mix;
}

static void cstrOut(const char* text, const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = text == NULL ? "" : text;
  }
  if (out_len != NULL) {
    *out_len = text == NULL ? 0 : (int64_t)strlen(text);
  }
}

void sere_sys_platform(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  cstrOut("windows", out_data, out_len);
#elif defined(__APPLE__)
  cstrOut("macos", out_data, out_len);
#else
  cstrOut("linux", out_data, out_len);
#endif
}

void sere_sys_arch(const char** out_data, int64_t* out_len) {
#if defined(_M_X64) || defined(__x86_64__)
  cstrOut("x86_64", out_data, out_len);
#elif defined(_M_ARM64) || defined(__aarch64__)
  cstrOut("arm64", out_data, out_len);
#else
  cstrOut("unknown", out_data, out_len);
#endif
}

void sere_sys_version(const char** out_data, int64_t* out_len) { cstrOut("0.1.0", out_data, out_len); }

#ifdef _WIN32
int32_t sere_sys_pid(void) { return (int32_t)GetCurrentProcessId(); }

int32_t sere_sys_cpu_count(void) {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (int32_t)info.dwNumberOfProcessors;
}
#else
int32_t sere_sys_pid(void) { return (int32_t)getpid(); }

int32_t sere_sys_cpu_count(void) {
  const long count = sysconf(_SC_NPROCESSORS_ONLN);
  return count > 0 ? (int32_t)count : 1;
}
#endif

void sere_mem_copy(void* dest, const void* src, int64_t size) {
  if (dest == NULL || src == NULL || size <= 0) {
    return;
  }
  memmove(dest, src, (size_t)size);
}

void sere_mem_set(void* dest, int32_t value, int64_t size) {
  if (dest == NULL || size <= 0) {
    return;
  }
  memset(dest, value & 0xff, (size_t)size);
}

int32_t sere_mem_eq(const void* left, const void* right, int64_t size) {
  if (size <= 0) {
    return 1;
  }
  if (left == NULL || right == NULL) {
    return 0;
  }
  return memcmp(left, right, (size_t)size) == 0 ? 1 : 0;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void sere_b64_encode(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  if (data == NULL || len <= 0) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  const int64_t out_n = ((len + 2) / 3) * 4;
  char* out = (char*)malloc((size_t)out_n + 1);
  if (out == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  int64_t n = 0;
  int64_t index = 0;
  while (index < len) {
    const unsigned char b0 = (unsigned char)data[index++];
    const int32_t has1 = index < len;
    const unsigned char b1 = has1 ? (unsigned char)data[index++] : 0;
    const int32_t has2 = index < len;
    const unsigned char b2 = has2 ? (unsigned char)data[index++] : 0;
    const uint32_t triple = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b2;
    out[n++] = kB64[(triple >> 18) & 63];
    out[n++] = kB64[(triple >> 12) & 63];
    out[n++] = has1 ? kB64[(triple >> 6) & 63] : '=';
    out[n++] = has2 ? kB64[triple & 63] : '=';
  }
  out[n] = '\0';
  outStr(ownBytes(out, n), out_data, out_len);
}

static int32_t b64Value(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return ch - 'a' + 26;
  }
  if (ch >= '0' && ch <= '9') {
    return ch - '0' + 52;
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  return -1;
}

void sere_b64_decode(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  if (data == NULL || len <= 0) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  char* out = (char*)malloc((size_t)len + 1);
  if (out == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  int32_t buf = 0;
  int32_t bits = 0;
  int64_t n = 0;
  for (int64_t index = 0; index < len; ++index) {
    const char ch = data[index];
    if (ch == '=' || ch == '\n' || ch == '\r' || ch == ' ') {
      continue;
    }
    const int32_t value = b64Value(ch);
    if (value < 0) {
      continue;
    }
    buf = (buf << 6) | value;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[n++] = (char)((buf >> bits) & 0xff);
    }
  }
  out[n] = '\0';
  outStr(ownBytes(out, n), out_data, out_len);
}

int64_t sere_string_find(const char* data, int64_t len, const char* needle, int64_t needle_len) {
  if (data == NULL || needle == NULL || needle_len < 0 || needle_len > len) {
    return -1;
  }
  if (needle_len == 0) {
    return 0;
  }
  for (int64_t index = 0; index + needle_len <= len; ++index) {
    if (memcmp(data + index, needle, (size_t)needle_len) == 0) {
      return index;
    }
  }
  return -1;
}

void sere_string_replace(const char* data, int64_t len, const char* old_data, int64_t old_len,
                         const char* new_data, int64_t new_len, const char** out_data,
                         int64_t* out_len) {
  if (data == NULL || len <= 0 || old_data == NULL || old_len <= 0) {
    outStr(copyBytes(data, len), out_data, out_len);
    return;
  }
  if (new_data == NULL) {
    new_data = "";
    new_len = 0;
  }
  int64_t count = 0;
  for (int64_t index = 0; index + old_len <= len;) {
    if (memcmp(data + index, old_data, (size_t)old_len) == 0) {
      count += 1;
      index += old_len;
    } else {
      index += 1;
    }
  }
  const int64_t total = len + count * (new_len - old_len);
  if (total < 0) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  char* out = (char*)malloc((size_t)total + 1);
  if (out == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  int64_t n = 0;
  for (int64_t index = 0; index < len;) {
    if (index + old_len <= len && memcmp(data + index, old_data, (size_t)old_len) == 0) {
      if (new_len > 0) {
        memcpy(out + n, new_data, (size_t)new_len);
        n += new_len;
      }
      index += old_len;
    } else {
      out[n++] = data[index++];
    }
  }
  out[n] = '\0';
  outStr(ownBytes(out, n), out_data, out_len);
}

void* sere_string_split(const char* data, int64_t len, const char* sep, int64_t sep_len) {
  void* list = sere_list_new((int64_t)sizeof(SereStr));
  if (list == NULL) {
    return NULL;
  }
  if (data == NULL) {
    data = "";
    len = 0;
  }
  if (sep == NULL || sep_len <= 0) {
    SereStr item = copyBytes(data, len);
    sere_list_push(list, &item);
    return list;
  }
  int64_t start = 0;
  for (int64_t index = 0; index + sep_len <= len;) {
    if (memcmp(data + index, sep, (size_t)sep_len) == 0) {
      SereStr item = copyBytes(data + start, index - start);
      sere_list_push(list, &item);
      index += sep_len;
      start = index;
    } else {
      index += 1;
    }
  }
  SereStr tail = copyBytes(data + start, len - start);
  sere_list_push(list, &tail);
  return list;
}

void sere_string_join(const char* sep, int64_t sep_len, void* parts, const char** out_data,
                      int64_t* out_len) {
  const SereList* list = (const SereList*)parts;
  if (list == NULL || list->len <= 0) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  if (sep == NULL || sep_len < 0) {
    sep = "";
    sep_len = 0;
  }
  int64_t total = 0;
  for (int64_t index = 0; index < list->len; ++index) {
    const SereStr* item = (const SereStr*)((char*)list->data + (size_t)(index * list->stride));
    total += item->len;
    if (index + 1 < list->len) {
      total += sep_len;
    }
  }
  char* out = (char*)malloc((size_t)total + 1);
  if (out == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  int64_t n = 0;
  for (int64_t index = 0; index < list->len; ++index) {
    const SereStr* item = (const SereStr*)((char*)list->data + (size_t)(index * list->stride));
    if (item->len > 0 && item->data != NULL) {
      memcpy(out + n, item->data, (size_t)item->len);
      n += item->len;
    }
    if (index + 1 < list->len && sep_len > 0) {
      memcpy(out + n, sep, (size_t)sep_len);
      n += sep_len;
    }
  }
  out[n] = '\0';
  outStr(ownBytes(out, n), out_data, out_len);
}

int64_t sere_string_rfind(const char* data, int64_t len, const char* needle, int64_t needle_len) {
  if (data == NULL || needle == NULL || needle_len < 0 || needle_len > len) {
    return -1;
  }
  if (needle_len == 0) {
    return len;
  }
  for (int64_t index = len - needle_len; index >= 0; --index) {
    if (memcmp(data + index, needle, (size_t)needle_len) == 0) {
      return index;
    }
  }
  return -1;
}

int64_t sere_string_count(const char* data, int64_t len, const char* needle, int64_t needle_len) {
  if (data == NULL || needle == NULL || needle_len <= 0 || needle_len > len) {
    return 0;
  }
  int64_t count = 0;
  for (int64_t index = 0; index + needle_len <= len;) {
    if (memcmp(data + index, needle, (size_t)needle_len) == 0) {
      count += 1;
      index += needle_len;
    } else {
      index += 1;
    }
  }
  return count;
}

void sere_string_capitalize(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  SereStr mapped = mapAscii(data, len, tolower);
  if (mapped.len > 0 && mapped.data != NULL) {
    char* mut = (char*)mapped.data;
    mut[0] = (char)toupper((unsigned char)mut[0]);
  }
  outStr(mapped, out_data, out_len);
}

void sere_string_title(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  SereStr mapped = mapAscii(data, len, tolower);
  int cap = 1;
  for (int64_t i = 0; i < mapped.len; ++i) {
    char* ch = (char*)mapped.data + i;
    if (*ch == ' ' || *ch == '\t' || *ch == '\n') {
      cap = 1;
    } else if (cap) {
      *ch = (char)toupper((unsigned char)*ch);
      cap = 0;
    }
  }
  outStr(mapped, out_data, out_len);
}

void sere_string_lstrip(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  int64_t begin = 0;
  while (begin < len && isspace((unsigned char)data[begin])) {
    begin += 1;
  }
  outStr(copyBytes(data + begin, len - begin), out_data, out_len);
}

void sere_string_rstrip(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  int64_t end = len;
  while (end > 0 && isspace((unsigned char)data[end - 1])) {
    end -= 1;
  }
  outStr(copyBytes(data, end), out_data, out_len);
}

int32_t sere_string_is_empty(const char* data, int64_t len) {
  (void)data;
  return len <= 0 ? 1 : 0;
}

int32_t sere_string_is_digit(const char* data, int64_t len) {
  if (len <= 0) {
    return 0;
  }
  for (int64_t i = 0; i < len; ++i) {
    if (!isdigit((unsigned char)data[i])) {
      return 0;
    }
  }
  return 1;
}

int32_t sere_string_is_alpha(const char* data, int64_t len) {
  if (len <= 0) {
    return 0;
  }
  for (int64_t i = 0; i < len; ++i) {
    if (!isalpha((unsigned char)data[i])) {
      return 0;
    }
  }
  return 1;
}

int32_t sere_string_is_space(const char* data, int64_t len) {
  if (len <= 0) {
    return 0;
  }
  for (int64_t i = 0; i < len; ++i) {
    if (!isspace((unsigned char)data[i])) {
      return 0;
    }
  }
  return 1;
}

double sere_math_sqrt(double value) { return sqrt(value); }
double sere_math_sin(double value) { return sin(value); }
double sere_math_cos(double value) { return cos(value); }
double sere_math_abs(double value) { return fabs(value); }
double sere_math_floor(double value) { return floor(value); }
double sere_math_ceil(double value) { return ceil(value); }
double sere_math_pow(double base, double exp) { return pow(base, exp); }
