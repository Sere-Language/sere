/// @file sere_sys.c
/// Stdlib C ABI: I/O, files, paths, process, strings, math, and time.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <shlobj.h>
#pragma comment(lib, "shell32")
#pragma comment(lib, "bcrypt")
#else
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/types.h>
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

static SereStr ownBytes(const char* data, int64_t len) {
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
    if (sere_runtime_console_write(2, data, len)) {
      return;
    }
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

// File handle I/O operations (Python-like)
void* sere_fs_open(const char* path, int64_t path_len, const char* mode, int64_t mode_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return NULL;
  }
  char* cmode = toCString(mode, mode_len);
  if (cmode == NULL) {
    free(cpath);
    return NULL;
  }
  FILE* file = fopen(cpath, cmode);
  free(cpath);
  free(cmode);
  return (void*)file;
}

int32_t sere_fs_close(void* handle) {
  if (handle == NULL) {
    return 0;
  }
  FILE* file = (FILE*)handle;
  return fclose(file) == 0 ? 1 : 0;
}

void sere_fs_read_all(void* handle, const char** out_data, int64_t* out_len) {
  if (handle == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  FILE* file = (FILE*)handle;

  // Seek to end to get size
  if (fseek(file, 0, SEEK_END) != 0) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }

  const long size = ftell(file);
  if (size < 0 || size > kMaxTextBytes) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }

  char* buf = (char*)malloc((size_t)size + 1);
  if (buf == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }

  const size_t n = fread(buf, 1, (size_t)size, file);
  buf[n] = '\0';
  outStr(ownBytes(buf, (int64_t)n), out_data, out_len);
}

int32_t sere_fs_write_all(void* handle, const char* data, int64_t data_len) {
  if (handle == NULL) {
    return 0;
  }
  FILE* file = (FILE*)handle;
  const size_t want = data == NULL || data_len <= 0 ? 0 : (size_t)data_len;
  const size_t wrote = want == 0 ? 0 : fwrite(data, 1, want, file);
  return wrote == want ? 1 : 0;
}

int32_t sere_fs_read_bytes(void* handle, char* buffer, int64_t buffer_len) {
  if (handle == NULL || buffer == NULL || buffer_len <= 0) {
    return -1;
  }
  FILE* file = (FILE*)handle;
  const size_t n = fread(buffer, 1, (size_t)buffer_len, file);
  return (int32_t)n;
}

int32_t sere_fs_write_bytes(void* handle, const char* data, int64_t data_len) {
  if (handle == NULL) {
    return 0;
  }
  FILE* file = (FILE*)handle;
  const size_t want = data == NULL || data_len <= 0 ? 0 : (size_t)data_len;
  const size_t wrote = want == 0 ? 0 : fwrite(data, 1, want, file);
  return wrote == want ? 1 : 0;
}

void sere_fs_seek(void* handle, int64_t offset, int32_t whence) {
  if (handle == NULL) {
    return;
  }
  FILE* file = (FILE*)handle;
  fseek(file, (long)offset, whence);
}

static SereStr pathJoinImpl(const char* left, int64_t left_len, const char* right, int64_t right_len) {
  if (right == NULL) {
    right = "";
    right_len = 0;
  }
  if (sere_path_is_abs(right, right_len) || left == NULL || left_len <= 0) {
    return copyBytes(right, right_len);
  }
  const int leftSep = isSep(left[left_len - 1]);
  const int64_t extra = leftSep ? 0 : 1;
  const int64_t total = left_len + extra + right_len;
  char* out = (char*)malloc((size_t)total + 1);
  if (out == NULL) {
    return emptyStr();
  }
  memcpy(out, left, (size_t)left_len);
  int64_t n = left_len;
  if (!leftSep) {
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

#ifdef _WIN32
/// Reads a UTF-8-named environment variable through the wide API.
static SereStr osEnvWide(const char* name, int64_t name_len);
#endif

static SereStr getenvImpl(const char* name, int64_t name_len) {
  char* key = toCString(name, name_len);
  if (key == NULL) {
    return emptyStr();
  }
#ifdef _WIN32
  // The wide API is the source of truth because env_set writes through it, and
  // the CRT's narrow copy can lag behind a SetEnvironmentVariable call.
  SereStr wide = osEnvWide(key, (int64_t)strlen(key));
  if (wide.len > 0) {
    free(key);
    return wide;
  }
#endif
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
  return ((int64_t)ts.tv_sec * 1000) + (int64_t)(ts.tv_nsec / 1000000);
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

static uint64_t gRng = 0x9E3779B97F4A7C15ULL;

void sere_random_seed(int64_t seed) { gRng = (uint64_t)seed | 1ULL; }

static uint64_t rngNext(void) {
  uint64_t x = gRng;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  gRng = x;
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

int64_t sere_random_range_i64(int64_t low, int64_t high) {
  if (high <= low)
    return low;
  const uint64_t span = (uint64_t)high - (uint64_t)low;
  const uint64_t limit = UINT64_MAX - (UINT64_MAX % span);
  uint64_t value;
  do {
    value = rngNext();
  } while (value >= limit);
  return (int64_t)((uint64_t)low + (value % span));
}

double sere_random_between_f64(double low, double high) {
  if (high <= low)
    return low;
  return low + (high - low) * sere_random_f64();
}

int32_t sere_random_bool(void) { return (int32_t)(rngNext() & 1ULL); }

int32_t sere_random_chance(double probability) {
  if (probability <= 0.0)
    return 0;
  if (probability >= 1.0)
    return 1;
  return sere_random_f64() < probability ? 1 : 0;
}

int32_t sere_random_bits(int32_t bits) {
  if (bits <= 0)
    return 0;
  if (bits >= 31)
    return (int32_t)(rngNext() >> 33);
  return (int32_t)(rngNext() & ((1ULL << bits) - 1ULL));
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

static void raiseFileHashError(const char* path, int error) {
  const char* detail = strerror(error);
  const size_t size = strlen(path) + strlen(detail) + 32;
  char* message = (char*)malloc(size);
  if (message == NULL) {
    const char fallback[] = "not enough memory to hash file";
    sere_raise("FileHashError;Exception", fallback, sizeof(fallback) - 1);
    return;
  }
  const int length = snprintf(message, size, "cannot hash '%s': %s", path, detail);
  sere_raise("FileHashError;Exception", message, length);
  free(message);
}

int64_t sere_hash_file(const char* path, int64_t path_len) {
  if (path == NULL || path_len <= 0 || memchr(path, 0, (size_t)path_len) != NULL) {
    raiseFileHashError("", EINVAL);
    return 0;
  }
  char* name = toCString(path, path_len);
  if (name == NULL) {
    raiseFileHashError("", ENOMEM);
    return 0;
  }
  FILE* file = NULL;
#ifdef _WIN32
  if (path_len > INT_MAX) {
    raiseFileHashError(name, EINVAL);
    free(name);
    return 0;
  }
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
  wchar_t* wide = count > 0 ? (wchar_t*)malloc((size_t)count * sizeof(wchar_t)) : NULL;
  if (wide == NULL) {
    raiseFileHashError(name, count == 0 ? EINVAL : ENOMEM);
    free(name);
    return 0;
  }
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count);
  file = _wfopen(wide, L"rb");
  free(wide);
#else
  file = fopen(name, "rb");
#endif
  if (file == NULL) {
    raiseFileHashError(name, errno);
    free(name);
    return 0;
  }
  uint64_t hash = 14695981039346656037ULL;
  unsigned char buffer[65536];
  size_t countRead;
  while ((countRead = fread(buffer, 1, sizeof(buffer), file)) != 0) {
    for (size_t index = 0; index < countRead; ++index) {
      hash ^= buffer[index];
      hash *= 1099511628211ULL;
    }
  }
  const int readError = ferror(file) ? (errno == 0 ? EIO : errno) : 0;
  const int closeError = fclose(file) != 0 ? (errno == 0 ? EIO : errno) : 0;
  if (readError != 0 || closeError != 0) {
    raiseFileHashError(name, readError != 0 ? readError : closeError);
    free(name);
    return 0;
  }
  free(name);
  return (int64_t)hash;
}

int64_t sere_hash_combine(int64_t left, int64_t right) {
  const uint64_t mix = (uint64_t)left ^ ((uint64_t)right + 0x9E3779B97F4A7C15ULL +
                                         ((uint64_t)left << 6) + ((uint64_t)left >> 2));
  return (int64_t)mix;
}

// SplitMix64 finalizer: avalanches every input bit across every output bit, so
// changing one bit of the input flips roughly half of the output bits. Used for
// individual values and as an optional final step for streaming hashers.
static uint64_t mix64(uint64_t value) {
  value ^= value >> 30;
  value *= 0xBF58476D1CE4E5B9ULL;
  value ^= value >> 27;
  value *= 0x94D049BB133111EBULL;
  value ^= value >> 31;
  return value;
}

// The FNV-1a 64-bit offset basis, exposed so streaming hashers can restart from
// the same seed that sere_hash_fnv1a uses.
int64_t sere_hash_basis(void) { return (int64_t)14695981039346656037ULL; }

// Fold more bytes into a running FNV-1a 64-bit state.
int64_t sere_hash_update(int64_t state, const char* data, int64_t len) {
  uint64_t hash = (uint64_t)state;
  if (data == NULL || len <= 0) {
    return (int64_t)hash;
  }
  for (int64_t index = 0; index < len; ++index) {
    hash ^= (uint8_t)data[index];
    hash *= 1099511628211ULL;
  }
  return (int64_t)hash;
}

// Optional avalanche step for hash-table use. Values folded through FNV-1a keep
// their low bits biased, which matters when a bucket index is state % capacity.
int64_t sere_hash_finish(int64_t state) { return (int64_t)mix64((uint64_t)state); }

// Entry points for callers that pass a byte buffer and an explicit length. They
// duplicate sere_hash_fnv1a / sere_hash_update deliberately: a module cannot
// declare the same C symbol twice with different signatures, because the second
// declaration replaces the first one's argument marshalling.
int64_t sere_hash_fnv1a_bytes(const char* data, int64_t len) {
  return sere_hash_fnv1a(data, len);
}

int64_t sere_hash_update_bytes(int64_t state, const char* data, int64_t len) {
  return sere_hash_update(state, data, len);
}

int64_t sere_hash_int(int64_t value) { return (int64_t)mix64((uint64_t)value); }

// Boolean hashing is domain separated from integer hashing so that
// hash_bool(True) differs from hash_int(1).
int64_t sere_hash_bool(int32_t value) {
  return (int64_t)mix64(0xB001000000000000ULL | (value != 0 ? 1ULL : 0ULL));
}

// -0.0 and 0.0 compare equal, so they must hash equal; all NaNs are folded onto
// a single canonical pattern for the same reason.
int64_t sere_hash_f64(double value) {
  uint64_t bits = 0;
  if (value == 0.0) {
    bits = 0;
  } else if (isnan(value)) {
    bits = 0x7FF8000000000000ULL;
  } else {
    memcpy(&bits, &value, sizeof(bits));
  }
  return (int64_t)mix64(0xF700000000000000ULL ^ bits);
}

int64_t sere_hash_f32(float value) {
  uint32_t bits = 0;
  if (value == 0.0f) {
    bits = 0;
  } else if (isnan(value)) {
    bits = 0x7FC00000U;
  } else {
    memcpy(&bits, &value, sizeof(bits));
  }
  return (int64_t)mix64(0xF300000000000000ULL ^ (uint64_t)bits);
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
  const int64_t outN = ((len + 2) / 3) * 4;
  char* out = (char*)malloc((size_t)outN + 1);
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
  if (sep == NULL || sep_len < 0) {
    SereStr item = copyBytes(data, len);
    sere_list_push(list, &item);
    return list;
  }
  if (sep_len == 0) {
    for (int64_t index = 0; index < len;) {
      unsigned char lead = (unsigned char)data[index];
      int64_t width = lead < 0x80             ? 1
                      : (lead & 0xE0) == 0xC0 ? 2
                      : (lead & 0xF0) == 0xE0 ? 3
                      : (lead & 0xF8) == 0xF0 ? 4
                                              : 1;
      if (index + width > len)
        width = 1;
      SereStr item = copyBytes(data + index, width);
      sere_list_push(list, &item);
      index += width;
    }
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

/* Element kind codes shared with the Serem dialect's ListElementKind. */
enum {
  SERE_LIST_STR = 0,
  SERE_LIST_I32 = 1,
  SERE_LIST_I64 = 2,
  SERE_LIST_F64 = 3,
  SERE_LIST_F32 = 4,
  SERE_LIST_BOOL = 5,
  SERE_LIST_PTR = 6,
  SERE_LIST_I8 = 7,
  SERE_LIST_I16 = 8,
  SERE_LIST_U8 = 9,
  SERE_LIST_U16 = 10,
  /* A boxed `Any`: the slot holds a `SereAnyBox*`, which carries its own
     renderer, so the slot can be formatted without knowing the program. */
  SERE_LIST_ANY = 11,
};

static void
reprAppend(char** output, size_t* length, size_t* capacity, const char* text, size_t text_len) {
  if (text == NULL || text_len == 0) {
    return;
  }
  if (*length + text_len + 1 > *capacity) {
    size_t grown = *capacity == 0 ? 32 : *capacity;
    while (grown < *length + text_len + 1) {
      grown *= 2;
    }
    char* resized = (char*)realloc(*output, grown);
    if (resized == NULL) {
      return;
    }
    *output = resized;
    *capacity = grown;
  }
  memcpy(*output + *length, text, text_len);
  *length += text_len;
}

const char* sere_any_repr_data(void* box) {
  const SereAnyBox* typed = (const SereAnyBox*)box;
  if (typed == NULL || typed->data == NULL) {
    return "None";
  }
  // Every box carries the renderer the generator emitted for its type, so the
  // runtime never has to map a type name onto a formatter of its own.
  if (typed->repr != NULL) {
    const char* text = typed->repr(box);
    if (text != NULL) {
      return text;
    }
  }
  return typed->name == NULL ? "None" : typed->name;
}

typedef struct {
  const char* text;
  int64_t len;
} SlotText;

/// Renders one container slot. Numeric renderings land in `scratch` and do not
/// outlive the call; the string, box, and object renderers hand back storage of
/// their own, like the rest of the runtime's formatting helpers.
static SlotText slotText(const char* item,
                         int32_t kind,
                         const SereSlotRepr* formatter,
                         char* scratch,
                         size_t scratchSize) {
  SlotText result;
  result.text = scratch;
  result.len = 0;
  switch (kind) {
  case SERE_LIST_STR: {
    const SereStr* value = (const SereStr*)item;
    int64_t rendered_len = 0;
    result.text = sere_str_repr_data(value->data, value->len, &rendered_len);
    result.len = rendered_len;
    return result;
  }
  case SERE_LIST_I8: {
    int8_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%d", (int)value);
    break;
  }
  case SERE_LIST_I16: {
    int16_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%d", (int)value);
    break;
  }
  case SERE_LIST_U8: {
    uint8_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%u", (unsigned)value);
    break;
  }
  case SERE_LIST_U16: {
    uint16_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%u", (unsigned)value);
    break;
  }
  case SERE_LIST_I32: {
    int32_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%d", (int)value);
    break;
  }
  case SERE_LIST_I64: {
    int64_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%lld", (long long)value);
    break;
  }
  case SERE_LIST_F64: {
    double value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%g", value);
    break;
  }
  case SERE_LIST_F32: {
    float value = 0;
    memcpy(&value, item, sizeof(value));
    result.len = snprintf(scratch, scratchSize, "%g", (double)value);
    break;
  }
  case SERE_LIST_BOOL: {
    int8_t value = 0;
    memcpy(&value, item, sizeof(value));
    result.text = value ? "True" : "False";
    result.len = value ? 4 : 5;
    return result;
  }
  case SERE_LIST_ANY: {
    void* box = NULL;
    memcpy(&box, item, sizeof(box));
    result.text = sere_any_repr_data(box);
    result.len = (int64_t)strlen(result.text);
    return result;
  }
  default: {
    void* value = NULL;
    memcpy(&value, item, sizeof(value));
    if (value == NULL) {
      result.text = "None";
      result.len = 4;
    } else if (formatter != NULL && formatter->object != NULL) {
      const char* text = formatter->object(value);
      if (text == NULL)
        text = formatter->name == NULL ? "None" : formatter->name;
      result.text = text;
      result.len = (int64_t)strlen(text);
    } else if (formatter != NULL && formatter->name != NULL) {
      // A record with no renderer still names its type, which is what a nested
      // value that has no `__str__` or `__repr__` prints as.
      result.text = formatter->name;
      result.len = (int64_t)strlen(formatter->name);
    } else {
      result.len =
          snprintf(scratch, scratchSize, "0x%llx", (unsigned long long)(uintptr_t)value);
    }
    return result;
  }
  }
  if (result.len < 0) {
    result.len = 0;
  }
  return result;
}

void sere_list_repr_data(void* list, int32_t kind, const char** out_data, int64_t* out_len) {
  const SereList* typed = (const SereList*)list;
  const int64_t stride = (typed == NULL || typed->stride <= 0) ? 1 : typed->stride;
  const int64_t count = (typed == NULL || typed->data == NULL) ? 0 : typed->len;
  size_t capacity = 32;
  size_t length = 0;
  char* output = (char*)malloc(capacity);
  if (output == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  reprAppend(&output, &length, &capacity, "[", 1);
  for (int64_t index = 0; index < count; ++index) {
    const char* item = (const char*)typed->data + (size_t)(index * stride);
    char scratch[64];
    const SlotText text = slotText(item, kind, NULL, scratch, sizeof(scratch));
    if (index != 0) {
      reprAppend(&output, &length, &capacity, ", ", 2);
    }
    reprAppend(&output, &length, &capacity, text.text, (size_t)text.len);
  }
  reprAppend(&output, &length, &capacity, "]", 1);
  output[length] = '\0';
  outStr(ownBytes(output, (int64_t)length), out_data, out_len);
}

void sere_dict_repr_data(void* dict,
                         const SereSlotRepr* key,
                         const SereSlotRepr* value,
                         const char** out_data,
                         int64_t* out_len) {
  const SereList* keys = (const SereList*)sere_dict_keys(dict);
  const SereList* values = (const SereList*)sere_dict_values(dict);
  const int64_t count = keys == NULL ? 0 : keys->len;
  size_t capacity = 32;
  size_t length = 0;
  char* output = (char*)malloc(capacity);
  if (output == NULL) {
    outStr(emptyStr(), out_data, out_len);
    return;
  }
  reprAppend(&output, &length, &capacity, "{", 1);
  for (int64_t index = 0; index < count; ++index) {
    char keyScratch[64];
    char valueScratch[64];
    const char* keySlot = (const char*)keys->data + (size_t)(index * keys->stride);
    const char* valueSlot = (const char*)values->data + (size_t)(index * values->stride);
    const SlotText keyText =
        slotText(keySlot, key == NULL ? SERE_LIST_STR : key->kind, key, keyScratch, sizeof(keyScratch));
    const SlotText valueText = slotText(valueSlot,
                                        value == NULL ? SERE_LIST_STR : value->kind,
                                        value,
                                        valueScratch,
                                        sizeof(valueScratch));
    if (index != 0) {
      reprAppend(&output, &length, &capacity, ", ", 2);
    }
    reprAppend(&output, &length, &capacity, keyText.text, (size_t)keyText.len);
    reprAppend(&output, &length, &capacity, ": ", 2);
    reprAppend(&output, &length, &capacity, valueText.text, (size_t)valueText.len);
  }
  reprAppend(&output, &length, &capacity, "}", 1);
  output[length] = '\0';
  outStr(ownBytes(output, (int64_t)length), out_data, out_len);
}

const char* sere_list_object_repr_data(void* list, const char* (*repr)(void*), const char* name) {
  const SereList* typed = (const SereList*)list;
  size_t capacity = 32;
  size_t length = 0;
  char* output = (char*)malloc(capacity);
  if (output == NULL) return "";
  reprAppend(&output, &length, &capacity, "[", 1);
  if (typed != NULL) {
    for (int64_t index = 0; index < typed->len; ++index) {
      void* object = NULL;
      memcpy(&object, (const char*)typed->data + index * typed->stride, sizeof(object));
      const char* text = repr == NULL ? name : repr(object);
      if (index != 0) reprAppend(&output, &length, &capacity, ", ", 2);
      if (text != NULL) reprAppend(&output, &length, &capacity, text, strlen(text));
    }
  }
  reprAppend(&output, &length, &capacity, "]", 1);
  output[length] = '\0';
  return ownBytes(output, (int64_t)length).data;
}

void sere_list_str_repr_data(void* list, const char** out_data, int64_t* out_len) {
  sere_list_repr_data(list, SERE_LIST_STR, out_data, out_len);
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
  if (list->stride == (int64_t)sizeof(int32_t)) {
    int64_t total = 0;
    for (int64_t index = 0; index < list->len; ++index) {
      char buffer[32];
      const int32_t item = *(const int32_t*)((const char*)list->data +
                                             (size_t)(index * list->stride));
      const int length = snprintf(buffer, sizeof(buffer), "%d", (int)item);
      if (length > 0)
        total += length;
      if (index + 1 < list->len)
        total += sep_len;
    }
    char* out = (char*)malloc((size_t)total + 1);
    if (out == NULL) {
      outStr(emptyStr(), out_data, out_len);
      return;
    }
    int64_t n = 0;
    for (int64_t index = 0; index < list->len; ++index) {
      char buffer[32];
      const int32_t item = *(const int32_t*)((const char*)list->data +
                                             (size_t)(index * list->stride));
      const int length = snprintf(buffer, sizeof(buffer), "%d", (int)item);
      if (length > 0) {
        memcpy(out + n, buffer, (size_t)length);
        n += length;
      }
      if (index + 1 < list->len && sep_len > 0) {
        memcpy(out + n, sep, (size_t)sep_len);
        n += sep_len;
      }
    }
    out[n] = '\0';
    outStr(ownBytes(out, n), out_data, out_len);
    return;
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

/* ==========================================================================
 * Extended operating-system layer (stdlib/os.sere).
 *
 * Owns the OS primitives the standard library builds on: environment, paths,
 * directories, file metadata, descriptors, pipes, clocks, entropy, filesystem
 * capacity, and executable lookup. Windows uses the W APIs; POSIX uses libc.
 * Strings cross the boundary as UTF-8 with the shared out-param convention.
 * ========================================================================== */

/* ---- shared helpers ---- */

static SereStr osCopy(const char* text) { return copyCString(text); }

static void osOut(SereStr value, const char** out_data, int64_t* out_len) {
  outStr(value, out_data, out_len);
}

#ifdef _WIN32
/* ---- Windows UTF-8 / UTF-16 bridge ---- */

static WCHAR* osWide(const char* data, int64_t len) {
  if (data == NULL || len < 0) {
    data = "";
    len = 0;
  }
  if (len > 0x7FFFFFFF) {
    return NULL;
  }
  const int needed =
      len == 0 ? 0 : MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
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

static SereStr osFromWide(const WCHAR* text, int32_t chars) {
  if (text == NULL || chars <= 0) {
    return emptyStr();
  }
  const int needed = WideCharToMultiByte(CP_UTF8, 0, text, chars, NULL, 0, NULL, NULL);
  if (needed <= 0) {
    return emptyStr();
  }
  char* copy = (char*)malloc((size_t)needed + 1);
  if (copy == NULL) {
    return emptyStr();
  }
  if (WideCharToMultiByte(CP_UTF8, 0, text, chars, copy, needed, NULL, NULL) != needed) {
    free(copy);
    return emptyStr();
  }
  copy[needed] = '\0';
  return ownBytes(copy, needed);
}

static SereStr osFromWideZ(const WCHAR* text) {
  return osFromWide(text, text == NULL ? 0 : (int32_t)wcslen(text));
}

/// Reads a UTF-8-named environment variable through the wide API.
static SereStr osEnvWide(const char* name, int64_t name_len) {
  WCHAR* wide = osWide(name, name_len);
  if (wide == NULL) {
    return emptyStr();
  }
  DWORD size = GetEnvironmentVariableW(wide, NULL, 0);
  if (size == 0) {
    free(wide);
    return emptyStr();
  }
  WCHAR* buffer = (WCHAR*)malloc((size_t)size * sizeof(WCHAR));
  if (buffer == NULL) {
    free(wide);
    return emptyStr();
  }
  const DWORD written = GetEnvironmentVariableW(wide, buffer, size);
  free(wide);
  if (written == 0 || written >= size) {
    free(buffer);
    return emptyStr();
  }
  SereStr result = osFromWide(buffer, (int32_t)written);
  free(buffer);
  return result;
}

static SereStr osKnownFolder(int csidl) {
  WCHAR buffer[MAX_PATH];
  if (SHGetFolderPathW(NULL, csidl, NULL, 0, buffer) != S_OK) {
    return emptyStr();
  }
  return osFromWideZ(buffer);
}

static SereStr osLocalAppData(void) {
  SereStr value = osEnvWide("LOCALAPPDATA", 12);
  if (value.len > 0) {
    return value;
  }
  return osKnownFolder(CSIDL_LOCAL_APPDATA);
}
#else
static SereStr osEnvPosix(const char* name, int64_t name_len) {
  char* key = toCString(name, name_len);
  if (key == NULL) {
    return emptyStr();
  }
  const char* value = getenv(key);
  free(key);
  return copyCString(value == NULL ? "" : value);
}

static SereStr osXdg(const char* env_name, const char* home_suffix) {
  const char* explicit = getenv(env_name);
  if (explicit != NULL && explicit[0] != '\0') {
    return copyCString(explicit);
  }
  const char* home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return emptyStr();
  }
  char buffer[4096];
  snprintf(buffer, sizeof(buffer), "%s%s", home, home_suffix);
  return copyCString(buffer);
}
#endif

/* ---- host information ---- */

void sere_os_hostname(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  WCHAR buffer[256];
  DWORD size = (DWORD)(sizeof(buffer) / sizeof(buffer[0]));
  if (!GetComputerNameW(buffer, &size)) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(osFromWide(buffer, (int32_t)size), out_data, out_len);
#else
  char buffer[256];
  if (gethostname(buffer, sizeof(buffer)) != 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  buffer[sizeof(buffer) - 1] = '\0';
  osOut(copyCString(buffer), out_data, out_len);
#endif
}

int64_t sere_os_page_size(void) {
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (int64_t)info.dwPageSize;
#else
  long value = sysconf(_SC_PAGESIZE);
  return value > 0 ? (int64_t)value : 4096;
#endif
}

int64_t sere_os_allocation_granularity(void) {
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (int64_t)info.dwAllocationGranularity;
#else
  return sere_os_page_size();
#endif
}

int64_t sere_os_physical_cpu_count(void) {
#ifdef _WIN32
  DWORD length = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &length);
  if (length == 0) {
    return (int64_t)sere_sys_cpu_count();
  }
  BYTE* buffer = (BYTE*)malloc(length);
  if (buffer == NULL) {
    return (int64_t)sere_sys_cpu_count();
  }
  int64_t cores = 0;
  if (GetLogicalProcessorInformationEx(RelationProcessorCore,
                                       (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buffer,
                                       &length)) {
    DWORD offset = 0;
    while (offset < length) {
      SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* entry =
          (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(buffer + offset);
      if (entry->Relationship == RelationProcessorCore) {
        cores++;
      }
      offset += entry->Size;
    }
  }
  free(buffer);
  return cores > 0 ? cores : (int64_t)sere_sys_cpu_count();
#else
  long value = sysconf(_SC_NPROCESSORS_ONLN);
  return value > 0 ? (int64_t)value : (int64_t)sere_sys_cpu_count();
#endif
}

/// Returns 0 for little-endian hosts and 1 for big-endian hosts.
int32_t sere_os_endianness(void) {
  const union {
    uint32_t value;
    unsigned char bytes[4];
  } probe = {1u};
  return probe.bytes[0] == 1 ? 0 : 1;
}

int32_t sere_os_memory_info(int64_t* fields, int32_t field_count) {
  if (fields == NULL || field_count < 4) {
    return 0;
  }
  for (int32_t i = 0; i < field_count; ++i) {
    fields[i] = 0;
  }
#ifdef _WIN32
  MEMORYSTATUSEX status;
  memset(&status, 0, sizeof(status));
  status.dwLength = sizeof(status);
  if (!GlobalMemoryStatusEx(&status)) {
    return 0;
  }
  fields[0] = (int64_t)status.ullTotalPhys;
  fields[1] = (int64_t)status.ullAvailPhys;
  fields[2] = (int64_t)status.ullTotalVirtual;
  fields[3] = (int64_t)status.ullAvailVirtual;
  return 1;
#else
  const long pages = sysconf(_SC_PHYS_PAGES);
  const long avail = sysconf(_SC_AVPHYS_PAGES);
  const long page = sysconf(_SC_PAGESIZE);
  if (pages <= 0 || page <= 0) {
    return 0;
  }
  fields[0] = (int64_t)pages * (int64_t)page;
  fields[1] = (int64_t)(avail > 0 ? avail : 0) * (int64_t)page;
  fields[2] = fields[0];
  fields[3] = fields[1];
  return 1;
#endif
}

int64_t sere_os_monotonic_ns(void) {
#ifdef _WIN32
  static LARGE_INTEGER frequency;
  static int initialized = 0;
  LARGE_INTEGER counter;
  if (!initialized) {
    QueryPerformanceFrequency(&frequency);
    initialized = 1;
  }
  QueryPerformanceCounter(&counter);
  if (frequency.QuadPart == 0) {
    return 0;
  }
  return (int64_t)((counter.QuadPart / frequency.QuadPart) * 1000000000LL +
                   ((counter.QuadPart % frequency.QuadPart) * 1000000000LL) / frequency.QuadPart);
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

int64_t sere_os_system_time_ns(void) {
#ifdef _WIN32
  FILETIME fileTime;
  GetSystemTimeAsFileTime(&fileTime);
  ULARGE_INTEGER value;
  value.LowPart = fileTime.dwLowDateTime;
  value.HighPart = fileTime.dwHighDateTime;
  // 100ns ticks since 1601 -> ns since 1970.
  return (int64_t)((value.QuadPart - 116444736000000000ULL) * 100ULL);
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

void sere_os_sleep_ns(int64_t nanoseconds) {
  if (nanoseconds <= 0) {
    return;
  }
#ifdef _WIN32
  Sleep((DWORD)((nanoseconds + 999999) / 1000000));
#else
  struct timespec ts;
  ts.tv_sec = (time_t)(nanoseconds / 1000000000LL);
  ts.tv_nsec = (long)(nanoseconds % 1000000000LL);
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
  }
#endif
}

/* ---- environment ---- */

int32_t sere_os_env_has(const char* name, int64_t name_len) {
#ifdef _WIN32
  return osEnvWide(name, name_len).len > 0 ? 1 : 0;
#else
  char* key = toCString(name, name_len);
  if (key == NULL) {
    return 0;
  }
  const int found = getenv(key) != NULL ? 1 : 0;
  free(key);
  return found;
#endif
}

int32_t sere_os_env_remove(const char* name, int64_t name_len) {
#ifdef _WIN32
  WCHAR* wide = osWide(name, name_len);
  if (wide == NULL) {
    return 0;
  }
  const BOOL ok = SetEnvironmentVariableW(wide, NULL);
  free(wide);
  return ok ? 1 : 0;
#else
  char* key = toCString(name, name_len);
  if (key == NULL) {
    return 0;
  }
  const int ok = unsetenv(key) == 0 ? 1 : 0;
  free(key);
  return ok;
#endif
}

/// Sets or clears an environment variable. An empty `value` removes it.
int32_t sere_os_env_set(const char* name, int64_t name_len, const char* value, int64_t value_len) {
#ifdef _WIN32
  WCHAR* key = osWide(name, name_len);
  WCHAR* text = osWide(value, value_len);
  if (key == NULL || text == NULL) {
    free(key);
    free(text);
    return 0;
  }
  const BOOL ok = value_len == 0 ? SetEnvironmentVariableW(key, NULL)
                                 : SetEnvironmentVariableW(key, text);
  free(key);
  free(text);
  return ok ? 1 : 0;
#else
  char* key = toCString(name, name_len);
  char* text = toCString(value, value_len);
  if (key == NULL || text == NULL) {
    free(key);
    free(text);
    return 0;
  }
  const int ok = value_len == 0 ? unsetenv(key) == 0 ? 1 : 0 : setenv(key, text, 1) == 0 ? 1 : 0;
  free(key);
  free(text);
  return ok;
#endif
}

void* sere_os_env_all(void) {
  void* list = sere_list_new((int64_t)sizeof(SereStr));
  if (list == NULL) {
    return NULL;
  }
#ifdef _WIN32
  LPWCH block = GetEnvironmentStringsW();
  if (block == NULL) {
    return list;
  }
  for (LPWCH entry = block; *entry != L'\0'; entry += wcslen(entry) + 1) {
    // Skip the hidden "=C:" style drive variables.
    if (entry[0] == L'=') {
      continue;
    }
    SereStr item = osFromWideZ(entry);
    sere_list_push(list, &item);
  }
  FreeEnvironmentStringsW(block);
#else
  extern char** environ;
  for (char** entry = environ; entry != NULL && *entry != NULL; ++entry) {
    if ((*entry)[0] == '=') {
      continue;
    }
    SereStr item = copyCString(*entry);
    sere_list_push(list, &item);
  }
#endif
  return list;
}

/* ---- working directory and well-known directories ---- */

int32_t sere_os_chdir(const char* path, int64_t path_len) {
#ifdef _WIN32
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  const BOOL ok = SetCurrentDirectoryW(wide);
  free(wide);
  return ok ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  const int ok = chdir(cpath) == 0 ? 1 : 0;
  free(cpath);
  return ok;
#endif
}

void sere_os_home_dir(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  SereStr value = osEnvWide("USERPROFILE", 11);
  if (value.len == 0) {
    value = osKnownFolder(CSIDL_PROFILE);
  }
  osOut(value, out_data, out_len);
#else
  osOut(osEnvPosix("HOME", 4), out_data, out_len);
#endif
}

void sere_os_temp_dir(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  DWORD size = GetTempPathW(0, NULL);
  if (size == 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  WCHAR* buffer = (WCHAR*)malloc((size_t)size * sizeof(WCHAR));
  if (buffer == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  const DWORD written = GetTempPathW(size, buffer);
  SereStr result = osFromWide(buffer, (int32_t)written);
  free(buffer);
  osOut(result, out_data, out_len);
#else
  const char* tmp = getenv("TMPDIR");
  osOut(copyCString(tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp"), out_data, out_len);
#endif
}

void sere_os_config_dir(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  SereStr value = osEnvWide("APPDATA", 7);
  if (value.len == 0) {
    value = osKnownFolder(CSIDL_APPDATA);
  }
  osOut(value, out_data, out_len);
#else
  osOut(osXdg("XDG_CONFIG_HOME", "/.config"), out_data, out_len);
#endif
}

void sere_os_cache_dir(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  osOut(osLocalAppData(), out_data, out_len);
#else
  osOut(osXdg("XDG_CACHE_HOME", "/.cache"), out_data, out_len);
#endif
}

void sere_os_data_dir(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  osOut(osLocalAppData(), out_data, out_len);
#else
  osOut(osXdg("XDG_DATA_HOME", "/.local/share"), out_data, out_len);
#endif
}

void sere_os_dev_null(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  osOut(osCopy("NUL"), out_data, out_len);
#else
  osOut(osCopy("/dev/null"), out_data, out_len);
#endif
}

void sere_os_newline(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  osOut(osCopy("\r\n"), out_data, out_len);
#else
  osOut(osCopy("\n"), out_data, out_len);
#endif
}

int32_t sere_os_path_separator(void) { return (int32_t)(unsigned char)pathSep(); }

int32_t sere_os_path_list_separator(void) {
#ifdef _WIN32
  return ';';
#else
  return ':';
#endif
}

/* ---- path normalization and resolution ---- */

/// Normalizes a native path: collapses separators and `.`/`..`, preserving the
/// root (drive letter, UNC share, or POSIX `/`). Caller frees.
static char* osNormalizeC(const char* path) {
  if (path == NULL) {
    return toCString("", 0);
  }
  char prefix[8];
  size_t plen = 0;
  size_t root = 0;
#ifdef _WIN32
  if (isSep(path[0]) && isSep(path[1])) {
    size_t i = 2;
    int parts = 0;
    while (path[i] != '\0' && parts < 2) {
      if (isSep(path[i])) {
        parts++;
      }
      i++;
    }
    root = i;
    plen = i < sizeof(prefix) ? i : sizeof(prefix);
    memcpy(prefix, path, plen);
  } else if (isSep(path[0])) {
    prefix[0] = pathSep();
    plen = 1;
    root = 1;
  } else if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
             path[1] == ':') {
    prefix[0] = path[0];
    prefix[1] = ':';
    plen = 2;
    root = 2;
    if (isSep(path[2])) {
      prefix[2] = pathSep();
      plen = 3;
      root = 3;
    }
  }
#else
  if (isSep(path[0])) {
    prefix[0] = '/';
    plen = 1;
    root = 1;
  }
#endif
  char* work = toCString(path, (int64_t)strlen(path));
  if (work == NULL) {
    return toCString("", 0);
  }
  const size_t cap = strlen(work) / 2 + 2;
  char** comps = (char**)malloc(cap * sizeof(char*));
  size_t* lens = (size_t*)malloc(cap * sizeof(size_t));
  if (comps == NULL || lens == NULL) {
    free(comps);
    free(lens);
    free(work);
    return toCString("", 0);
  }
  size_t count = 0;
  size_t i = root;
  while (work[i] != '\0') {
    while (work[i] != '\0' && isSep(work[i])) {
      i++;
    }
    const size_t start = i;
    while (work[i] != '\0' && !isSep(work[i])) {
      i++;
    }
    const size_t len = i - start;
    if (len == 0) {
      continue;
    }
    if (len == 1 && work[start] == '.') {
      continue;
    }
    if (len == 2 && work[start] == '.' && work[start + 1] == '.') {
      if (count > 0 &&
          !(lens[count - 1] == 2 && comps[count - 1][0] == '.' && comps[count - 1][1] == '.')) {
        count--;
      } else if (root == 0 && count < cap) {
        comps[count] = work + start;
        lens[count] = 2;
        count++;
      }
      continue;
    }
    if (count < cap) {
      comps[count] = work + start;
      lens[count] = len;
      count++;
    }
  }
  size_t total = plen + 1;
  for (size_t c = 0; c < count; ++c) {
    total += lens[c] + 1;
  }
  char* out = (char*)malloc(total + 1);
  if (out == NULL) {
    free(comps);
    free(lens);
    free(work);
    return toCString("", 0);
  }
  size_t n = 0;
  for (size_t k = 0; k < plen; ++k) {
    out[n++] = prefix[k];
  }
  for (size_t c = 0; c < count; ++c) {
    if (n > 0 && !isSep(out[n - 1])) {
      out[n++] = pathSep();
    }
    memcpy(out + n, comps[c], lens[c]);
    n += lens[c];
  }
  if (n == 0) {
    out[n++] = '.';
  }
  out[n] = '\0';
  free(comps);
  free(lens);
  free(work);
  return out;
}

void sere_os_path_normalize(const char* path, int64_t path_len, const char** out_data,
                            int64_t* out_len) {
  char* cpath = toCString(path, path_len);
  char* normalized = osNormalizeC(cpath);
  free(cpath);
  if (normalized == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(ownBytes(normalized, (int64_t)strlen(normalized)), out_data, out_len);
}

void sere_os_path_absolute(const char* path, int64_t path_len, const char** out_data,
                           int64_t* out_len) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
#ifdef _WIN32
  WCHAR wide[MAX_PATH * 4];
  WCHAR* widePath = osWide(cpath, (int64_t)strlen(cpath));
  if (widePath == NULL) {
    free(cpath);
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  const DWORD needed = GetFullPathNameW(widePath, (DWORD)(sizeof(wide) / sizeof(wide[0])), wide,
                                        NULL);
  free(widePath);
  if (needed == 0 || needed >= sizeof(wide) / sizeof(wide[0])) {
    free(cpath);
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  free(cpath);
  osOut(osFromWide(wide, (int32_t)needed), out_data, out_len);
#else
  char* absolute = NULL;
  if (isSep(cpath[0])) {
    absolute = realpath(cpath, NULL);
  } else {
    char resolved[4096];
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      free(cpath);
      osOut(emptyStr(), out_data, out_len);
      return;
    }
    snprintf(resolved, sizeof(resolved), "%s/%s", cwd, cpath);
    absolute = realpath(resolved, NULL);
    if (absolute == NULL) {
      absolute = osNormalizeC(resolved);
      free(cpath);
      osOut(ownBytes(absolute, (int64_t)strlen(absolute)), out_data, out_len);
      return;
    }
  }
  free(cpath);
  if (absolute == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(ownBytes(absolute, (int64_t)strlen(absolute)), out_data, out_len);
#endif
}

void sere_os_path_real(const char* path, int64_t path_len, const char** out_data,
                       int64_t* out_len) {
#ifdef _WIN32
  // Resolve through a handle so symlinks/junctions are followed.
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  HANDLE handle = CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  free(wide);
  if (handle == INVALID_HANDLE_VALUE) {
    sere_os_path_absolute(path, path_len, out_data, out_len);
    return;
  }
  WCHAR buffer[4096];
  const DWORD written = GetFinalPathNameByHandleW(handle, buffer,
                                                  (DWORD)(sizeof(buffer) / sizeof(buffer[0])), 0);
  CloseHandle(handle);
  if (written == 0 || written >= sizeof(buffer) / sizeof(buffer[0])) {
    sere_os_path_absolute(path, path_len, out_data, out_len);
    return;
  }
  // Strip the \\?\ prefix when present.
  const WCHAR* start = buffer;
  int32_t length = (int32_t)written;
  if (length > 4 && buffer[0] == L'\\' && buffer[1] == L'\\' && buffer[2] == L'?' &&
      buffer[3] == L'\\') {
    start = buffer + 4;
    length -= 4;
  }
  osOut(osFromWide(start, length), out_data, out_len);
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  char* resolved = realpath(cpath, NULL);
  free(cpath);
  if (resolved == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(ownBytes(resolved, (int64_t)strlen(resolved)), out_data, out_len);
#endif
}

void sere_os_path_stem(const char* path, int64_t path_len, const char** out_data,
                       int64_t* out_len) {
  const char* base = NULL;
  int64_t baseLen = 0;
  sere_path_basename(path, path_len, &base, &baseLen);
  if (base == NULL || baseLen <= 0) {
    free((void*)base);
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  int64_t dot = -1;
  for (int64_t i = baseLen - 1; i > 0; --i) {
    if (base[i] == '.') {
      dot = i;
      break;
    }
  }
  const int64_t m = dot < 0 ? baseLen : dot;
  osOut(copyBytes(base, m), out_data, out_len);
  free((void*)base);
}

/* ---- file type and metadata ---- */

int32_t sere_os_is_symlink(const char* path, int64_t path_len) {
#ifdef _WIN32
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  const DWORD attrs = GetFileAttributesW(wide);
  free(wide);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return 0;
  }
  return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  struct stat info;
  const int ok = lstat(cpath, &info) == 0;
  free(cpath);
  return ok && S_ISLNK(info.st_mode) ? 1 : 0;
#endif
}

/// Fills [size, mode, inode, device, nlink, uid, gid, atime_ns, mtime_ns,
/// ctime_ns, birth_ns, kind]. kind: 1 file, 2 directory, 3 symlink, 0 other.
int32_t sere_os_stat_fields(const char* path, int64_t path_len, int32_t follow, int64_t* fields,
                            int32_t field_count) {
  if (fields == NULL || field_count < 12) {
    return 0;
  }
  for (int32_t i = 0; i < field_count; ++i) {
    fields[i] = 0;
  }
#ifdef _WIN32
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExW(wide, GetFileExInfoStandard, &data)) {
    free(wide);
    return 0;
  }
  ULARGE_INTEGER size;
  size.LowPart = data.nFileSizeLow;
  size.HighPart = data.nFileSizeHigh;
  fields[0] = (int64_t)size.QuadPart;
  const DWORD attrs = data.dwFileAttributes;
  const int isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
  const int isLink = (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
  fields[1] = (attrs & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;
  fields[11] = isLink ? 3 : (isDir ? 2 : 1);
  if (follow && isLink) {
    // Report the target kind when asked and the target is a directory.
    DWORD target = GetFileAttributesW(wide);
    if (target != INVALID_FILE_ATTRIBUTES && (target & FILE_ATTRIBUTE_DIRECTORY)) {
      fields[11] = 2;
    }
  }
  ULARGE_INTEGER wtime;
  wtime.LowPart = data.ftLastWriteTime.dwLowDateTime;
  wtime.HighPart = data.ftLastWriteTime.dwHighDateTime;
  fields[8] = (int64_t)((wtime.QuadPart - 116444736000000000ULL) * 100ULL);
  ULARGE_INTEGER atime;
  atime.LowPart = data.ftLastAccessTime.dwLowDateTime;
  atime.HighPart = data.ftLastAccessTime.dwHighDateTime;
  fields[7] = (int64_t)((atime.QuadPart - 116444736000000000ULL) * 100ULL);
  ULARGE_INTEGER ctime;
  ctime.LowPart = data.ftCreationTime.dwLowDateTime;
  ctime.HighPart = data.ftCreationTime.dwHighDateTime;
  fields[9] = (int64_t)((ctime.QuadPart - 116444736000000000ULL) * 100ULL);
  fields[10] = fields[9];
  // GetFileAttributesEx cannot report the link count or a stable file identity,
  // so open the path and ask the handle instead. Failure leaves those fields 0.
  HANDLE handle = CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (handle != INVALID_HANDLE_VALUE) {
    BY_HANDLE_FILE_INFORMATION details;
    if (GetFileInformationByHandle(handle, &details)) {
      fields[2] = ((int64_t)details.nFileIndexHigh << 32) | (int64_t)details.nFileIndexLow;
      fields[3] = (int64_t)details.dwVolumeSerialNumber;
      fields[4] = (int64_t)details.nNumberOfLinks;
    }
    CloseHandle(handle);
  }
  free(wide);
  return 1;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  struct stat info;
  const int ok = (follow ? stat(cpath, &info) : lstat(cpath, &info)) == 0;
  free(cpath);
  if (!ok) {
    return 0;
  }
  fields[0] = (int64_t)info.st_size;
  fields[1] = (int64_t)info.st_mode;
  fields[2] = (int64_t)info.st_ino;
  fields[3] = (int64_t)info.st_dev;
  fields[4] = (int64_t)info.st_nlink;
  fields[5] = (int64_t)info.st_uid;
  fields[6] = (int64_t)info.st_gid;
  fields[7] = (int64_t)info.st_atim.tv_sec * 1000000000LL + (int64_t)info.st_atim.tv_nsec;
  fields[8] = (int64_t)info.st_mtim.tv_sec * 1000000000LL + (int64_t)info.st_mtim.tv_nsec;
  fields[9] = (int64_t)info.st_ctim.tv_sec * 1000000000LL + (int64_t)info.st_ctim.tv_nsec;
  fields[10] = fields[9];
  fields[11] = S_ISLNK(info.st_mode)   ? 3
               : S_ISDIR(info.st_mode) ? 2
               : S_ISREG(info.st_mode) ? 1
                                       : 0;
  return 1;
#endif
}

/* ---- directory operations ---- */

int32_t sere_os_mkdir_mode(const char* path, int64_t path_len, int32_t mode) {
#ifdef _WIN32
  (void)mode;
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  const BOOL ok = CreateDirectoryW(wide, NULL);
  free(wide);
  return ok ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  const int ok = mkdir(cpath, (mode_t)(mode == 0 ? 0777 : mode)) == 0 ? 1 : 0;
  free(cpath);
  return ok;
#endif
}

int32_t sere_os_makedirs(const char* path, int64_t path_len, int32_t mode, int32_t exist_ok) {
  char* cpath = toCString(path, path_len);
  if (cpath == NULL || cpath[0] == '\0') {
    free(cpath);
    return 0;
  }
  // Try the whole path first; walk upward creating missing parents.
  if (sere_os_mkdir_mode(path, path_len, mode)) {
    free(cpath);
    return 1;
  }
  const size_t length = strlen(cpath);
  for (size_t i = 1; i < length; ++i) {
    if (!isSep(cpath[i])) {
      continue;
    }
    char saved = cpath[i];
    if (i == 2 && cpath[1] == ':') {
      continue;
    }
    cpath[i] = '\0';
    (void)sere_os_mkdir_mode(cpath, (int64_t)i, mode);
    cpath[i] = saved;
  }
  const int ok = sere_os_mkdir_mode(cpath, (int64_t)length, mode);
  if (!ok && exist_ok) {
    const int isDir = sere_fs_is_dir(path, path_len);
    free(cpath);
    return isDir;
  }
  free(cpath);
  return ok;
}

int32_t sere_os_rmdir(const char* path, int64_t path_len) {
#ifdef _WIN32
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  const BOOL ok = RemoveDirectoryW(wide);
  free(wide);
  return ok ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  const int ok = rmdir(cpath) == 0 ? 1 : 0;
  free(cpath);
  return ok;
#endif
}

/// Recursively deletes a tree without following symlinks/reparse points.
int32_t sere_os_remove_tree(const char* path, int64_t path_len, int32_t follow_links) {
  (void)follow_links;
  if (sere_os_is_symlink(path, path_len)) {
    return sere_fs_remove(path, path_len);
  }
  if (!sere_fs_is_dir(path, path_len)) {
    return sere_fs_remove(path, path_len);
  }
  void* list = sere_os_listdir(path, path_len);
  if (list != NULL) {
    const SereList* entries = (const SereList*)list;
    for (int64_t index = 0; index < entries->len; ++index) {
      const SereStr* item = (const SereStr*)((char*)entries->data + index * entries->stride);
      const char* child = NULL;
      int64_t childLen = 0;
      sere_path_join(path, path_len, item->data, item->len, &child, &childLen);
      (void)sere_os_remove_tree(child, childLen, follow_links);
      free((void*)child);
    }
  }
  return sere_os_rmdir(path, path_len);
}

/* ---- rename / replace / copy ---- */

int32_t sere_os_rename(const char* src, int64_t src_len, const char* dst, int64_t dst_len) {
#ifdef _WIN32
  WCHAR* from = osWide(src, src_len);
  WCHAR* to = osWide(dst, dst_len);
  if (from == NULL || to == NULL) {
    free(from);
    free(to);
    return 0;
  }
  const BOOL ok = MoveFileExW(from, to, 0);
  free(from);
  free(to);
  return ok ? 1 : 0;
#else
  char* from = toCString(src, src_len);
  char* to = toCString(dst, dst_len);
  if (from == NULL || to == NULL) {
    free(from);
    free(to);
    return 0;
  }
  const int ok = rename(from, to) == 0 ? 1 : 0;
  free(from);
  free(to);
  return ok;
#endif
}

int32_t sere_os_replace(const char* src, int64_t src_len, const char* dst, int64_t dst_len) {
#ifdef _WIN32
  WCHAR* from = osWide(src, src_len);
  WCHAR* to = osWide(dst, dst_len);
  if (from == NULL || to == NULL) {
    free(from);
    free(to);
    return 0;
  }
  const BOOL ok = MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING);
  free(from);
  free(to);
  return ok ? 1 : 0;
#else
  return sere_os_rename(src, src_len, dst, dst_len);
#endif
}

int32_t sere_os_copy_file(const char* src, int64_t src_len, const char* dst, int64_t dst_len,
                          int32_t overwrite) {
  char* from = toCString(src, src_len);
  char* to = toCString(dst, dst_len);
  if (from == NULL || to == NULL) {
    free(from);
    free(to);
    return 0;
  }
  int result = 0;
  FILE* in = fopen(from, "rb");
  if (in == NULL) {
    free(from);
    free(to);
    return 0;
  }
  if (!overwrite && sere_fs_exists(to, (int64_t)strlen(to))) {
    fclose(in);
    free(from);
    free(to);
    return 0;
  }
  FILE* out = fopen(to, "wb");
  if (out == NULL) {
    fclose(in);
    free(from);
    free(to);
    return 0;
  }
  char buffer[65536];
  size_t read = 0;
  while ((read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
    if (fwrite(buffer, 1, read, out) != read) {
      fclose(in);
      fclose(out);
      remove(to);
      free(from);
      free(to);
      return 0;
    }
  }
  result = ferror(in) ? 0 : 1;
  fclose(in);
  if (fclose(out) != 0) {
    result = 0;
  }
  free(from);
  free(to);
  return result;
}

/* ---- links ---- */

int32_t sere_os_symlink(const char* target, int64_t target_len, const char* link,
                        int64_t link_len) {
#ifdef _WIN32
  WCHAR* to = osWide(target, target_len);
  WCHAR* from = osWide(link, link_len);
  if (to == NULL || from == NULL) {
    free(to);
    free(from);
    return 0;
  }
  const DWORD attrs = GetFileAttributesW(to);
  const DWORD flags = (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
                          ? SYMBOLIC_LINK_FLAG_DIRECTORY
                          : 0;
  const BOOL ok = CreateSymbolicLinkW(from, to, flags);
  free(to);
  free(from);
  return ok ? 1 : 0;
#else
  char* to = toCString(target, target_len);
  char* from = toCString(link, link_len);
  if (to == NULL || from == NULL) {
    free(to);
    free(from);
    return 0;
  }
  const int ok = symlink(to, from) == 0 ? 1 : 0;
  free(to);
  free(from);
  return ok;
#endif
}

void sere_os_readlink(const char* path, int64_t path_len, const char** out_data,
                      int64_t* out_len) {
#ifdef _WIN32
  // Report the raw reparse target where possible; fall back to empty.
  HANDLE handle = CreateFileW(osWide(path, path_len), 0,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  WCHAR buffer[4096];
  const DWORD written = GetFinalPathNameByHandleW(handle, buffer,
                                                  (DWORD)(sizeof(buffer) / sizeof(buffer[0])), 0);
  CloseHandle(handle);
  if (written == 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(osFromWide(buffer, (int32_t)written), out_data, out_len);
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  char buffer[4096];
  const ssize_t n = readlink(cpath, buffer, sizeof(buffer) - 1);
  free(cpath);
  if (n <= 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  buffer[n] = '\0';
  osOut(copyCString(buffer), out_data, out_len);
#endif
}

int32_t sere_os_hardlink(const char* existing, int64_t existing_len, const char* link,
                         int64_t link_len) {
#ifdef _WIN32
  WCHAR* to = osWide(existing, existing_len);
  WCHAR* from = osWide(link, link_len);
  if (to == NULL || from == NULL) {
    free(to);
    free(from);
    return 0;
  }
  const BOOL ok = CreateHardLinkW(from, to, NULL);
  free(to);
  free(from);
  return ok ? 1 : 0;
#else
  char* to = toCString(existing, existing_len);
  char* from = toCString(link, link_len);
  if (to == NULL || from == NULL) {
    free(to);
    free(from);
    return 0;
  }
  const int ok = link(to, from) == 0 ? 1 : 0;
  free(to);
  free(from);
  return ok;
#endif
}

/* ---- permissions, access, timestamps ---- */

int32_t sere_os_chmod(const char* path, int64_t path_len, int32_t mode) {
#ifdef _WIN32
  (void)mode;
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  DWORD attrs = GetFileAttributesW(wide);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    free(wide);
    return 0;
  }
  // The low bits are ignored on Windows; read-only maps to 0444 vs 0666.
  const int readOnly = (mode & 0200) == 0;
  if (readOnly) {
    attrs |= FILE_ATTRIBUTE_READONLY;
  } else {
    attrs &= ~(DWORD)FILE_ATTRIBUTE_READONLY;
  }
  const BOOL ok = SetFileAttributesW(wide, attrs);
  free(wide);
  return ok ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  const int ok = chmod(cpath, (mode_t)mode) == 0 ? 1 : 0;
  free(cpath);
  return ok;
#endif
}

int32_t sere_os_umask(int32_t mask) {
#ifdef _WIN32
  (void)mask;
  return 0;
#else
  return (int32_t)umask((mode_t)mask);
#endif
}

int32_t sere_os_access(const char* path, int64_t path_len, int32_t mode) {
#ifdef _WIN32
  (void)mode;
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  const DWORD attrs = GetFileAttributesW(wide);
  free(wide);
  return attrs != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  int flags = F_OK;
  if ((mode & 4) != 0) {
    flags |= R_OK;
  }
  if ((mode & 2) != 0) {
    flags |= W_OK;
  }
  if ((mode & 1) != 0) {
    flags |= X_OK;
  }
  const int ok = access(cpath, flags) == 0 ? 1 : 0;
  free(cpath);
  return ok;
#endif
}

int32_t sere_os_utime(const char* path, int64_t path_len, int64_t access_ns,
                      int64_t modify_ns) {
#ifdef _WIN32
  HANDLE handle = CreateFileW(osWide(path, path_len), FILE_WRITE_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  FILETIME atime;
  FILETIME mtime;
  ULARGE_INTEGER value;
  value.QuadPart = (ULONGLONG)(access_ns / 100) + 116444736000000000ULL;
  atime.dwLowDateTime = value.LowPart;
  atime.dwHighDateTime = value.HighPart;
  value.QuadPart = (ULONGLONG)(modify_ns / 100) + 116444736000000000ULL;
  mtime.dwLowDateTime = value.LowPart;
  mtime.dwHighDateTime = value.HighPart;
  const BOOL ok = SetFileTime(handle, NULL, &atime, &mtime);
  CloseHandle(handle);
  return ok ? 1 : 0;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  struct timespec times[2];
  times[0].tv_sec = (time_t)(access_ns / 1000000000LL);
  times[0].tv_nsec = (long)(access_ns % 1000000000LL);
  times[1].tv_sec = (time_t)(modify_ns / 1000000000LL);
  times[1].tv_nsec = (long)(modify_ns % 1000000000LL);
  const int ok = utimensat(AT_FDCWD, cpath, times, 0) == 0 ? 1 : 0;
  free(cpath);
  return ok;
#endif
}

/* ---- raw file descriptors ---- */

/// Open flags: 1 read, 2 write, 4 create, 8 truncate, 16 append, 32 exclusive.
int64_t sere_os_open_fd(const char* path, int64_t path_len, int32_t flags, int32_t mode) {
#ifdef _WIN32
  (void)mode;
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return -1;
  }
  DWORD access = 0;
  if ((flags & 1) != 0) {
    access |= GENERIC_READ;
  }
  if ((flags & 2) != 0) {
    access |= GENERIC_WRITE;
  }
  if (access == 0) {
    access = GENERIC_READ;
  }
  DWORD creation = OPEN_EXISTING;
  if ((flags & 4) != 0 && (flags & 32) != 0) {
    creation = CREATE_NEW;
  } else if ((flags & 8) != 0) {
    creation = CREATE_ALWAYS;
  } else if ((flags & 4) != 0) {
    creation = OPEN_ALWAYS;
  }
  HANDLE handle =
      CreateFileW(wide, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                  creation, FILE_ATTRIBUTE_NORMAL, NULL);
  free(wide);
  if (handle == INVALID_HANDLE_VALUE) {
    return -1;
  }
  const int fd = _open_osfhandle((intptr_t)handle, _O_BINARY);
  if (fd < 0) {
    CloseHandle(handle);
    return -1;
  }
  if ((flags & 16) != 0) {
    _lseeki64(fd, 0, SEEK_END);
  }
  return (int64_t)fd;
#else
  int oflags = 0;
  if ((flags & 1) != 0 && (flags & 2) != 0) {
    oflags |= O_RDWR;
  } else if ((flags & 2) != 0) {
    oflags |= O_WRONLY;
  } else {
    oflags |= O_RDONLY;
  }
  if ((flags & 4) != 0) {
    oflags |= O_CREAT;
  }
  if ((flags & 8) != 0) {
    oflags |= O_TRUNC;
  }
  if ((flags & 16) != 0) {
    oflags |= O_APPEND;
  }
  if ((flags & 32) != 0) {
    oflags |= O_EXCL;
  }
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return -1;
  }
  const int fd = open(cpath, oflags, (mode_t)(mode == 0 ? 0666 : mode));
  free(cpath);
  return (int64_t)fd;
#endif
}

int32_t sere_os_close_fd(int64_t fd) {
#ifdef _WIN32
  return _close((int)fd) == 0 ? 1 : 0;
#else
  return close((int)fd) == 0 ? 1 : 0;
#endif
}

int64_t sere_os_read_fd(int64_t fd, char* buffer, int64_t length) {
  if (buffer == NULL || length <= 0) {
    return 0;
  }
#ifdef _WIN32
  const unsigned int want = length > 0x7FFFFFFF ? 0x7FFFFFFFu : (unsigned int)length;
  const int n = _read((int)fd, buffer, want);
  return (int64_t)n;
#else
  const ssize_t n = read((int)fd, buffer, (size_t)length);
  return (int64_t)n;
#endif
}

int64_t sere_os_write_fd(int64_t fd, const char* buffer, int64_t length) {
  if (buffer == NULL || length <= 0) {
    return 0;
  }
#ifdef _WIN32
  const unsigned int want = length > 0x7FFFFFFF ? 0x7FFFFFFFu : (unsigned int)length;
  const int n = _write((int)fd, buffer, want);
  return (int64_t)n;
#else
  const ssize_t n = write((int)fd, buffer, (size_t)length);
  return (int64_t)n;
#endif
}

int64_t sere_os_seek_fd(int64_t fd, int64_t offset, int32_t whence) {
  int origin = SEEK_SET;
  if (whence == 1) {
    origin = SEEK_CUR;
  } else if (whence == 2) {
    origin = SEEK_END;
  }
#ifdef _WIN32
  return (int64_t)_lseeki64((int)fd, offset, origin);
#else
  const off_t result = lseek((int)fd, (off_t)offset, origin);
  return (int64_t)result;
#endif
}

int64_t sere_os_tell_fd(int64_t fd) {
#ifdef _WIN32
  return (int64_t)_telli64((int)fd);
#else
  const off_t result = lseek((int)fd, 0, SEEK_CUR);
  return (int64_t)result;
#endif
}

int32_t sere_os_truncate_fd(int64_t fd, int64_t size) {
#ifdef _WIN32
  return _chsize_s((int)fd, size) == 0 ? 1 : 0;
#else
  return ftruncate((int)fd, (off_t)size) == 0 ? 1 : 0;
#endif
}

int32_t sere_os_fsync_fd(int64_t fd) {
#ifdef _WIN32
  return _commit((int)fd) == 0 ? 1 : 0;
#else
  return fsync((int)fd) == 0 ? 1 : 0;
#endif
}

int32_t sere_os_fdatasync_fd(int64_t fd) {
#if defined(_WIN32)
  return _commit((int)fd) == 0 ? 1 : 0;
#elif defined(__APPLE__)
  return fsync((int)fd) == 0 ? 1 : 0;
#else
  return fdatasync((int)fd) == 0 ? 1 : 0;
#endif
}

int64_t sere_os_dup_fd(int64_t fd) {
#ifdef _WIN32
  return (int64_t)_dup((int)fd);
#else
  return (int64_t)dup((int)fd);
#endif
}

int64_t sere_os_dup2_fd(int64_t fd, int64_t target) {
#ifdef _WIN32
  return (int64_t)_dup2((int)fd, (int)target);
#else
  return (int64_t)dup2((int)fd, (int)target);
#endif
}

/// Writes [read_fd, write_fd] into `out`. Returns 1 on success.
int32_t sere_os_pipe_fds(int64_t* out) {
  if (out == NULL) {
    return 0;
  }
  out[0] = -1;
  out[1] = -1;
#ifdef _WIN32
  int fds[2];
  if (_pipe(fds, 65536, _O_BINARY) != 0) {
    return 0;
  }
  out[0] = (int64_t)fds[0];
  out[1] = (int64_t)fds[1];
  return 1;
#else
  int fds[2];
  if (pipe(fds) != 0) {
    return 0;
  }
  out[0] = (int64_t)fds[0];
  out[1] = (int64_t)fds[1];
  return 1;
#endif
}

int32_t sere_os_fd_isatty(int64_t fd) {
#ifdef _WIN32
  return _isatty((int)fd) ? 1 : 0;
#else
  return isatty((int)fd) ? 1 : 0;
#endif
}

/// Writes [columns, rows] into `out`. Returns 1 when the size is known.
int32_t sere_os_fd_terminal_size(int64_t fd, int64_t* out) {
  if (out == NULL) {
    return 0;
  }
  out[0] = 0;
  out[1] = 0;
#ifdef _WIN32
  HANDLE handle = INVALID_HANDLE_VALUE;
  if (fd >= 0) {
    handle = (HANDLE)_get_osfhandle((int)fd);
  }
  if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
  }
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (handle == NULL || !GetConsoleScreenBufferInfo(handle, &info)) {
    return 0;
  }
  out[0] = (int64_t)(info.srWindow.Right - info.srWindow.Left + 1);
  out[1] = (int64_t)(info.srWindow.Bottom - info.srWindow.Top + 1);
  return 1;
#else
  struct winsize size;
  if (ioctl((int)fd, TIOCGWINSZ, &size) != 0) {
    return 0;
  }
  out[0] = (int64_t)size.ws_col;
  out[1] = (int64_t)size.ws_row;
  return 1;
#endif
}

/// Writes [total_bytes, free_bytes, available_bytes, block_size] into `out`.
int32_t sere_os_filesystem_info(const char* path, int64_t path_len, int64_t* out) {
  if (out == NULL) {
    return 0;
  }
  for (int i = 0; i < 4; ++i) {
    out[i] = 0;
  }
#ifdef _WIN32
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return 0;
  }
  ULARGE_INTEGER available;
  ULARGE_INTEGER total;
  ULARGE_INTEGER freeBytes;
  const BOOL ok = GetDiskFreeSpaceExW(wide, &available, &total, &freeBytes);
  DWORD sectorsPerCluster = 0;
  DWORD bytesPerSector = 0;
  if (!GetDiskFreeSpaceW(wide, &sectorsPerCluster, &bytesPerSector, NULL, NULL)) {
    bytesPerSector = 512;
    sectorsPerCluster = 8;
  }
  free(wide);
  if (!ok) {
    return 0;
  }
  out[0] = (int64_t)total.QuadPart;
  out[1] = (int64_t)freeBytes.QuadPart;
  out[2] = (int64_t)available.QuadPart;
  out[3] = (int64_t)bytesPerSector * (int64_t)sectorsPerCluster;
  return 1;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return 0;
  }
  struct statvfs info;
  const int ok = statvfs(cpath, &info) == 0;
  free(cpath);
  if (!ok) {
    return 0;
  }
  const int64_t block = (int64_t)info.f_frsize;
  out[0] = (int64_t)info.f_blocks * block;
  out[1] = (int64_t)info.f_bfree * block;
  out[2] = (int64_t)info.f_bavail * block;
  out[3] = block;
  return 1;
#endif
}

/* ---- executable lookup ---- */

void sere_os_which(const char* name, int64_t name_len, const char** out_data, int64_t* out_len) {
  char* target = toCString(name, name_len);
  if (target == NULL || target[0] == '\0') {
    free(target);
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  char candidate[8192];
#ifdef _WIN32
  // An explicit path (or an extension) is checked directly first.
  if (strchr(target, '\\') != NULL || strchr(target, '/') != NULL || strchr(target, '.') != NULL) {
    if (sere_fs_exists(target, (int64_t)strlen(target))) {
      osOut(copyCString(target), out_data, out_len);
      free(target);
      return;
    }
  }
  const char* pathValue = getenv("PATH");
  const char* extensions = getenv("PATHEXT");
  if (extensions == NULL || extensions[0] == '\0') {
    extensions = ".COM;.EXE;.BAT;.CMD";
  }
  if (pathValue != NULL) {
    const char* cursor = pathValue;
    while (*cursor != '\0') {
      const char* semicolon = strchr(cursor, ';');
      const size_t dirLen = semicolon == NULL ? strlen(cursor) : (size_t)(semicolon - cursor);
      if (dirLen > 0 && dirLen < sizeof(candidate) - 512) {
        memcpy(candidate, cursor, dirLen);
        const char* extCursor = extensions;
        while (*extCursor != '\0') {
          const char* extEnd = strchr(extCursor, ';');
          const size_t extLen = extEnd == NULL ? strlen(extCursor) : (size_t)(extEnd - extCursor);
          size_t n = dirLen;
          if (!isSep(candidate[n - 1])) {
            candidate[n++] = pathSep();
          }
          memcpy(candidate + n, target, strlen(target));
          n += strlen(target);
          if (extLen < sizeof(candidate) - n - 1) {
            memcpy(candidate + n, extCursor, extLen);
            n += extLen;
            candidate[n] = '\0';
            if (sere_fs_exists(candidate, (int64_t)n)) {
              osOut(copyCString(candidate), out_data, out_len);
              free(target);
              return;
            }
          }
          if (extEnd == NULL) {
            break;
          }
          extCursor = extEnd + 1;
        }
      }
      if (semicolon == NULL) {
        break;
      }
      cursor = semicolon + 1;
    }
  }
#else
  if (strchr(target, '/') != NULL) {
    if (access(target, X_OK) == 0) {
      osOut(copyCString(target), out_data, out_len);
      free(target);
      return;
    }
  }
  const char* pathValue = getenv("PATH");
  if (pathValue != NULL) {
    const char* cursor = pathValue;
    while (*cursor != '\0') {
      const char* colon = strchr(cursor, ':');
      const size_t dirLen = colon == NULL ? strlen(cursor) : (size_t)(colon - cursor);
      if (dirLen > 0 && dirLen < sizeof(candidate) - 512) {
        memcpy(candidate, cursor, dirLen);
        size_t n = dirLen;
        if (candidate[n - 1] != '/') {
          candidate[n++] = '/';
        }
        memcpy(candidate + n, target, strlen(target));
        n += strlen(target);
        candidate[n] = '\0';
        if (access(candidate, X_OK) == 0) {
          osOut(copyCString(candidate), out_data, out_len);
          free(target);
          return;
        }
      }
      if (colon == NULL) {
        break;
      }
      cursor = colon + 1;
    }
  }
#endif
  free(target);
  osOut(emptyStr(), out_data, out_len);
}

void sere_os_executable_path(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  WCHAR buffer[4096];
  const DWORD written = GetModuleFileNameW(NULL, buffer, (DWORD)(sizeof(buffer) / sizeof(WCHAR)));
  if (written == 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(osFromWide(buffer, (int32_t)written), out_data, out_len);
#elif defined(__APPLE__)
  char buffer[4096];
  uint32_t size = (uint32_t)sizeof(buffer);
  extern int _NSGetExecutablePath(char*, uint32_t*);
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(copyCString(buffer), out_data, out_len);
#else
  char buffer[4096];
  const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (n <= 0) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  buffer[n] = '\0';
  osOut(copyCString(buffer), out_data, out_len);
#endif
}

/* ---- randomness ---- */

/// Fills `length` bytes with cryptographically secure random data.
int32_t sere_os_random_bytes(char* buffer, int64_t length) {
  if (buffer == NULL || length <= 0) {
    return 0;
  }
#ifdef _WIN32
  return BCryptGenRandom(NULL, (PUCHAR)buffer, (ULONG)length,
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0
             ? 1
             : 0;
#else
  FILE* source = fopen("/dev/urandom", "rb");
  if (source == NULL) {
    return 0;
  }
  const size_t read = fread(buffer, 1, (size_t)length, source);
  fclose(source);
  return read == (size_t)length ? 1 : 0;
#endif
}

/* ---- dynamic libraries ---- */

void* sere_os_library_open(const char* path, int64_t path_len) {
#ifdef _WIN32
  WCHAR* wide = osWide(path, path_len);
  if (wide == NULL) {
    return NULL;
  }
  HMODULE module = LoadLibraryW(wide);
  free(wide);
  return (void*)module;
#else
  char* cpath = toCString(path, path_len);
  if (cpath == NULL) {
    return NULL;
  }
  void* handle = dlopen(cpath, RTLD_NOW | RTLD_LOCAL);
  free(cpath);
  return handle;
#endif
}

void* sere_os_library_symbol(void* library, const char* name, int64_t name_len) {
  if (library == NULL) {
    return NULL;
  }
  char* symbol = toCString(name, name_len);
  if (symbol == NULL) {
    return NULL;
  }
#ifdef _WIN32
  void* address = (void*)GetProcAddress((HMODULE)library, symbol);
#else
  void* address = dlsym(library, symbol);
#endif
  free(symbol);
  return address;
}

int32_t sere_os_library_close(void* library) {
  if (library == NULL) {
    return 0;
  }
#ifdef _WIN32
  return FreeLibrary((HMODULE)library) ? 1 : 0;
#else
  return dlclose(library) == 0 ? 1 : 0;
#endif
}

/* ---- signals and processes ---- */

int32_t sere_os_raise(int32_t signal_number) {
  return raise((int)signal_number) == 0 ? 1 : 0;
}

int32_t sere_os_kill(int64_t pid, int32_t signal_number) {
#ifdef _WIN32
  (void)signal_number;
  HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
  if (process == NULL) {
    return 0;
  }
  const BOOL ok = TerminateProcess(process, 1);
  CloseHandle(process);
  return ok ? 1 : 0;
#else
  return kill((pid_t)pid, signal_number) == 0 ? 1 : 0;
#endif
}

/* ---- current user ---- */

void sere_os_user_name(const char** out_data, int64_t* out_len) {
#ifdef _WIN32
  char buffer[256];
  const DWORD written = GetEnvironmentVariableA("USERNAME", buffer, (DWORD)sizeof(buffer));
  if (written == 0 || written >= sizeof(buffer)) {
    osOut(emptyStr(), out_data, out_len);
    return;
  }
  osOut(copyCString(buffer), out_data, out_len);
#else
  const char* user = getenv("USER");
  if (user == NULL) {
    user = getenv("LOGNAME");
  }
  osOut(copyCString(user == NULL ? "" : user), out_data, out_len);
#endif
}

int64_t sere_os_user_id(void) {
#ifdef _WIN32
  return 0;
#else
  return (int64_t)getuid();
#endif
}

int64_t sere_os_group_id(void) {
#ifdef _WIN32
  return 0;
#else
  return (int64_t)getgid();
#endif
}

/* ---- scalar accessors -------------------------------------------------- *
 * The pointer-out APIs above are awkward to call from Sere, so each aggregate
 * query also has a scalar form. Missing paths report 0 (or kind 0) and callers
 * distinguish "absent" with exists()/is_symlink().                              */

int64_t sere_os_memory_total(void) {
  int64_t fields[4];
  if (!sere_os_memory_info(fields, 4)) {
    return 0;
  }
  return fields[0];
}

int64_t sere_os_memory_available(void) {
  int64_t fields[4];
  if (!sere_os_memory_info(fields, 4)) {
    return 0;
  }
  return fields[1];
}

int64_t sere_os_memory_total_virtual(void) {
  int64_t fields[4];
  if (!sere_os_memory_info(fields, 4)) {
    return 0;
  }
  return fields[2];
}

int64_t sere_os_memory_available_virtual(void) {
  int64_t fields[4];
  if (!sere_os_memory_info(fields, 4)) {
    return 0;
  }
  return fields[3];
}

int64_t sere_os_fs_total_bytes(const char* path, int64_t path_len) {
  int64_t fields[4];
  if (!sere_os_filesystem_info(path, path_len, fields)) {
    return 0;
  }
  return fields[0];
}

int64_t sere_os_fs_free_bytes(const char* path, int64_t path_len) {
  int64_t fields[4];
  if (!sere_os_filesystem_info(path, path_len, fields)) {
    return 0;
  }
  return fields[1];
}

int64_t sere_os_fs_available_bytes(const char* path, int64_t path_len) {
  int64_t fields[4];
  if (!sere_os_filesystem_info(path, path_len, fields)) {
    return 0;
  }
  return fields[2];
}

int64_t sere_os_fs_block_size(const char* path, int64_t path_len) {
  int64_t fields[4];
  if (!sere_os_filesystem_info(path, path_len, fields)) {
    return 0;
  }
  return fields[3];
}

int32_t sere_os_terminal_columns(int64_t fd) {
  int64_t fields[2];
  if (!sere_os_fd_terminal_size(fd, fields)) {
    return 0;
  }
  return (int32_t)fields[0];
}

int32_t sere_os_terminal_rows(int64_t fd) {
  int64_t fields[2];
  if (!sere_os_fd_terminal_size(fd, fields)) {
    return 0;
  }
  return (int32_t)fields[1];
}

/// Reads one of the twelve stat fields. Returns 0 when the path is unreadable.
static int64_t osStatField(const char* path, int64_t path_len, int32_t follow, int32_t index) {
  int64_t fields[12];
  if (!sere_os_stat_fields(path, path_len, follow, fields, 12)) {
    return 0;
  }
  return fields[index];
}

int64_t sere_os_stat_size(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 0);
}

int32_t sere_os_stat_mode(const char* path, int64_t path_len, int32_t follow) {
  return (int32_t)osStatField(path, path_len, follow, 1);
}

int64_t sere_os_stat_inode(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 2);
}

int64_t sere_os_stat_device(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 3);
}

int64_t sere_os_stat_links(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 4);
}

int64_t sere_os_stat_uid(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 5);
}

int64_t sere_os_stat_gid(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 6);
}

int64_t sere_os_stat_atime_ns(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 7);
}

int64_t sere_os_stat_mtime_ns(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 8);
}

int64_t sere_os_stat_ctime_ns(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 9);
}

int64_t sere_os_stat_birth_ns(const char* path, int64_t path_len, int32_t follow) {
  return osStatField(path, path_len, follow, 10);
}

/// 0 other, 1 regular file, 2 directory, 3 symlink.
int32_t sere_os_stat_kind(const char* path, int64_t path_len, int32_t follow) {
  return (int32_t)osStatField(path, path_len, follow, 11);
}

/* ---- pipe objects ---- */

typedef struct OsPipe {
  int64_t readFd;
  int64_t writeFd;
} OsPipe;

void* sere_os_pipe_new(void) {
  OsPipe* pipe = (OsPipe*)malloc(sizeof(OsPipe));
  if (pipe == NULL) {
    return NULL;
  }
  pipe->readFd = -1;
  pipe->writeFd = -1;
  if (!sere_os_pipe_fds((int64_t*)pipe)) {
    free(pipe);
    return NULL;
  }
  return pipe;
}

int64_t sere_os_pipe_read_fd(void* pipe) {
  return pipe == NULL ? -1 : ((OsPipe*)pipe)->readFd;
}

int64_t sere_os_pipe_write_fd(void* pipe) {
  return pipe == NULL ? -1 : ((OsPipe*)pipe)->writeFd;
}

int32_t sere_os_pipe_close(void* pipe) {
  if (pipe == NULL) {
    return 0;
  }
  OsPipe* handle = (OsPipe*)pipe;
  int32_t ok = 1;
  if (handle->readFd >= 0) {
    ok = sere_os_close_fd(handle->readFd) && ok;
  }
  if (handle->writeFd >= 0) {
    ok = sere_os_close_fd(handle->writeFd) && ok;
  }
  free(handle);
  return ok;
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
