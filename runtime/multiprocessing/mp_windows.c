/// @file mp_windows.c
/// Windows implementation of the multiprocessing primitives: process creation
/// and control, anonymous pipes, shared memory, named semaphores and named
/// events.
///
/// Every primitive is kernel-backed, so blocking operations use kernel waits
/// rather than polling. Anonymous pipes are made inheritable and their handle
/// values are handed to the child on its command line, matching how CPython's
/// spawn start method transfers descriptors on Windows.

// Must precede every system header so `_snwprintf` is not marked deprecated.
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "mp_internal.h"

#ifndef _WIN32
#error "mp_windows.c is only built on Windows"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Structs                                                             */
/* ------------------------------------------------------------------ */

struct MpProcess {
  HANDLE handle;
  DWORD pid;
  int32_t daemon;
  int32_t reaped;
};

struct MpConn {
  int64_t readHandle;
  int64_t writeHandle;
  int32_t ownsRead;
  int32_t ownsWrite;
};

struct MpShm {
  HANDLE mapping;
  void* view;
  int64_t size;
  char* name;
};

struct MpSem {
  HANDLE handle;
  int32_t owned;
};

struct MpEvent {
  HANDLE handle;
  int32_t owned;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static wchar_t* wideFromUtf8(const char* text) {
  if (text == NULL) {
    text = "";
  }
  const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (needed <= 0) {
    return NULL;
  }
  wchar_t* wide = (wchar_t*)calloc((size_t)needed + 1, sizeof(wchar_t));
  if (wide == NULL) {
    return NULL;
  }
  MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, needed);
  return wide;
}

static char* utf8FromWide(const wchar_t* wide) {
  if (wide == NULL) {
    return NULL;
  }
  const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
  if (needed <= 0) {
    return NULL;
  }
  char* text = (char*)calloc((size_t)needed + 1, 1);
  if (text == NULL) {
    return NULL;
  }
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, needed, NULL, NULL);
  return text;
}

const char* mpProgramPath(void) {
  static char* cached = NULL;
  if (cached != NULL) {
    return cached;
  }
  wchar_t buffer[32768];
  DWORD length = GetModuleFileNameW(NULL, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
  if (length == 0 || length >= sizeof(buffer) / sizeof(buffer[0])) {
    return NULL;
  }
  buffer[length] = L'\0';
  cached = utf8FromWide(buffer);
  return cached;
}

int64_t mpNowMs(void) { return (int64_t)GetTickCount64(); }

void mpRandomBytes(void* out, int64_t len) {
  if (len <= 0 || out == NULL) {
    return;
  }
  if (BCryptGenRandom(NULL, (PUCHAR)out, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
    // Fall back to a time/pid mix rather than leaving the buffer uninitialized.
    uint64_t seed = ((uint64_t)GetTickCount64() << 20) ^ ((uint64_t)GetCurrentProcessId() << 8);
    uint8_t* bytes = (uint8_t*)out;
    for (int64_t index = 0; index < len; ++index) {
      seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
      bytes[index] = (uint8_t)(seed >> 33);
    }
  }
}

void mpMakeName(const char* prefix, char* out, size_t cap) {
  uint8_t raw[16];
  mpRandomBytes(raw, (int64_t)sizeof(raw));
  static const char* kHex = "0123456789abcdef";
  size_t written = 0;
  const size_t plen = strlen(prefix);
  for (size_t index = 0; index < plen && written + 1 < cap; ++index) {
    out[written++] = prefix[index];
  }
  for (size_t index = 0; index < sizeof(raw) && written + 2 < cap; ++index) {
    out[written++] = kHex[raw[index] >> 4];
    out[written++] = kHex[raw[index] & 0x0f];
  }
  out[written] = '\0';
}

int32_t mpHandleArg(int32_t argc, char** argv, const char* prefix, int64_t* out) {
  if (argv == NULL || prefix == NULL) {
    return 0;
  }
  const size_t plen = strlen(prefix);
  for (int32_t index = 1; index < argc; ++index) {
    if (argv[index] != NULL && strncmp(argv[index], prefix, plen) == 0) {
      *out = (int64_t)strtoll(argv[index] + plen, NULL, 10);
      return 1;
    }
  }
  return 0;
}

int64_t mpSelfPid(void) { return (int64_t)GetCurrentProcessId(); }

int32_t mpCpuCount(void) {
  DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  if (count == 0) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    count = info.dwNumberOfProcessors;
  }
  return (int32_t)(count == 0 ? 1 : count);
}

/* ------------------------------------------------------------------ */
/* Processes                                                           */
/* ------------------------------------------------------------------ */

int32_t mpReadAllFromHandle(int64_t handle, uint8_t** out, int64_t* outLen) {
  *out = NULL;
  *outLen = 0;
  uint8_t* buffer = NULL;
  int64_t len = 0;
  int64_t cap = 0;
  for (;;) {
    if (len == cap) {
      const int64_t grown = cap == 0 ? 65536 : cap * 2;
      uint8_t* resized = (uint8_t*)realloc(buffer, (size_t)grown);
      if (resized == NULL) {
        free(buffer);
        return 0;
      }
      buffer = resized;
      cap = grown;
    }
    DWORD read = 0;
    if (!ReadFile((HANDLE)(intptr_t)handle, buffer + len, (DWORD)(cap - len), &read, NULL)) {
      const DWORD code = GetLastError();
      // A closed writer surfaces as ERROR_BROKEN_PIPE (or ERROR_HANDLE_EOF)
      // rather than a zero-byte read on anonymous pipes.
      if (code == ERROR_BROKEN_PIPE || code == ERROR_HANDLE_EOF) {
        break;
      }
      char message[128];
      snprintf(message, sizeof(message), "ReadFile failed (%lu) on handle %lld",
               (unsigned long)code, (long long)handle);
      mpSetLastError(message);
      free(buffer);
      return 0;
    }
    if (read == 0) {
      break;
    }
    len += (int64_t)read;
  }
  *out = buffer;
  *outLen = len;
  return 1;
}

MpProcess* mpSpawn(const char* payload, int64_t len, int32_t daemon, char* err, size_t errcap) {
  SECURITY_ATTRIBUTES security;
  memset(&security, 0, sizeof(security));
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;

  HANDLE readEnd = NULL;
  HANDLE writeEnd = NULL;
  if (!CreatePipe(&readEnd, &writeEnd, &security, 0)) {
    snprintf(err, errcap, "CreatePipe failed (%lu)", (unsigned long)GetLastError());
    return NULL;
  }
  // Only the read end crosses into the child; if the write end were inherited
  // the child would never observe end of input.
  SetHandleInformation(writeEnd, HANDLE_FLAG_INHERIT, 0);

  const char* program = mpProgramPath();
  wchar_t* wideProgram = wideFromUtf8(program);
  if (wideProgram == NULL) {
    CloseHandle(readEnd);
    CloseHandle(writeEnd);
    snprintf(err, errcap, "cannot resolve the executable path");
    return NULL;
  }
  wchar_t commandLine[32768];
  _snwprintf(commandLine, sizeof(commandLine) / sizeof(wchar_t) - 1, L"\"%ls\" --sere-mp-child=%lld",
             wideProgram, (long long)(intptr_t)readEnd);
  commandLine[sizeof(commandLine) / sizeof(wchar_t) - 1] = L'\0';
  free(wideProgram);

  STARTUPINFOW startup;
  memset(&startup, 0, sizeof(startup));
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION info;
  memset(&info, 0, sizeof(info));
  const BOOL started = CreateProcessW(NULL,
                                      commandLine,
                                      NULL,
                                      NULL,
                                      TRUE,
                                      CREATE_UNICODE_ENVIRONMENT,
                                      NULL,
                                      NULL,
                                      &startup,
                                      &info);
  if (!started) {
    const DWORD code = GetLastError();
    CloseHandle(readEnd);
    CloseHandle(writeEnd);
    snprintf(err, errcap, "CreateProcess failed (%lu)", (unsigned long)code);
    return NULL;
  }
  CloseHandle(info.hThread);
  CloseHandle(readEnd);

  MpProcess* process = (MpProcess*)calloc(1, sizeof(MpProcess));
  if (process == NULL) {
    CloseHandle(writeEnd);
    TerminateProcess(info.hProcess, 1);
    CloseHandle(info.hProcess);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  process->handle = info.hProcess;
  process->pid = info.dwProcessId;
  process->daemon = daemon;
  process->reaped = 0;

  // Deliver the payload. A large payload blocks until the child drains it.
  const uint8_t* cursor = (const uint8_t*)payload;
  int64_t remaining = len;
  int32_t ok = 1;
  DWORD writeError = 0;
  while (remaining > 0) {
    DWORD written = 0;
    const DWORD chunk = remaining > 0x7fffffff ? 0x7fffffff : (DWORD)remaining;
    if (!WriteFile(writeEnd, cursor, chunk, &written, NULL) || written == 0) {
      writeError = GetLastError();
      ok = 0;
      break;
    }
    cursor += written;
    remaining -= (int64_t)written;
  }
  CloseHandle(writeEnd);
  if (!ok) {
    snprintf(err, errcap, "cannot send the child payload (%lu)", (unsigned long)writeError);
    TerminateProcess(process->handle, 1);
    (void)WaitForSingleObject(process->handle, 2000);
    CloseHandle(process->handle);
    free(process);
    return NULL;
  }
  return process;
}

void mpProcessRelease(MpProcess* process) {
  if (process == NULL) {
    return;
  }
  if (process->handle != NULL) {
    CloseHandle(process->handle);
    process->handle = NULL;
  }
  free(process);
}

int32_t mpWait(MpProcess* process, int64_t timeoutMs) {
  if (process == NULL || process->handle == NULL) {
    return 0;
  }
  const DWORD timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
  const DWORD result = WaitForSingleObject(process->handle, timeout);
  if (result == WAIT_TIMEOUT) {
    return 1;
  }
  if (result == WAIT_OBJECT_0) {
    process->reaped = 1;
    return 0;
  }
  return 1;
}

int32_t mpIsAlive(MpProcess* process) {
  if (process == NULL || process->handle == NULL) {
    return 0;
  }
  const DWORD result = WaitForSingleObject(process->handle, 0);
  if (result == WAIT_TIMEOUT) {
    return 1;
  }
  if (result == WAIT_OBJECT_0) {
    process->reaped = 1;
    return 0;
  }
  return -1;
}

int32_t mpExitCode(MpProcess* process) {
  if (process == NULL || process->handle == NULL) {
    return -1;
  }
  DWORD code = 0;
  if (!GetExitCodeProcess(process->handle, &code)) {
    return -1;
  }
  if (code == STILL_ACTIVE) {
    return -1;
  }
  return (int32_t)code;
}

int64_t mpProcessPid(MpProcess* process) {
  return process == NULL ? -1 : (int64_t)process->pid;
}

void mpTerminate(MpProcess* process) {
  if (process != NULL && process->handle != NULL) {
    TerminateProcess(process->handle, 1);
  }
}

void mpKill(MpProcess* process) {
  if (process != NULL && process->handle != NULL) {
    TerminateProcess(process->handle, 1);
  }
}

void* mpSentinel(MpProcess* process) {
  return process == NULL ? NULL : (void*)process->handle;
}

void mpSetDaemon(MpProcess* process, int32_t daemon) {
  if (process != NULL) {
    process->daemon = daemon;
  }
}

int32_t mpIsDaemon(MpProcess* process) { return process == NULL ? 0 : process->daemon; }

int32_t mpWaitMany(void* const* handles, int32_t count, int64_t timeoutMs) {
  if (count <= 0) {
    return -1;
  }
  if (count > MAXIMUM_WAIT_OBJECTS) {
    count = MAXIMUM_WAIT_OBJECTS;
  }
  HANDLE objects[MAXIMUM_WAIT_OBJECTS];
  for (int32_t index = 0; index < count; ++index) {
    objects[index] = (HANDLE)handles[index];
  }
  const DWORD timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
  const DWORD result = WaitForMultipleObjects((DWORD)count, objects, FALSE, timeout);
  if (result == WAIT_TIMEOUT || result == WAIT_FAILED) {
    return -1;
  }
  const DWORD index = result - WAIT_OBJECT_0;
  return (int32_t)index;
}

/* ------------------------------------------------------------------ */
/* Pipes                                                               */
/* ------------------------------------------------------------------ */

static int32_t createPipe(HANDLE* readEnd, HANDLE* writeEnd) {
  SECURITY_ATTRIBUTES security;
  memset(&security, 0, sizeof(security));
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  return CreatePipe(readEnd, writeEnd, &security, 0) ? 1 : 0;
}

int32_t mpPipeDuplex(MpConn** a, MpConn** b) {
  HANDLE r1 = NULL, w1 = NULL, r2 = NULL, w2 = NULL;
  if (!createPipe(&r1, &w1)) {
    return 0;
  }
  if (!createPipe(&r2, &w2)) {
    CloseHandle(r1);
    CloseHandle(w1);
    return 0;
  }
  MpConn* left = (MpConn*)calloc(1, sizeof(MpConn));
  MpConn* right = (MpConn*)calloc(1, sizeof(MpConn));
  if (left == NULL || right == NULL) {
    free(left);
    free(right);
    CloseHandle(r1);
    CloseHandle(w1);
    CloseHandle(r2);
    CloseHandle(w2);
    return 0;
  }
  left->readHandle = (int64_t)(intptr_t)r1;
  left->writeHandle = (int64_t)(intptr_t)w2;
  left->ownsRead = 1;
  left->ownsWrite = 1;
  right->readHandle = (int64_t)(intptr_t)r2;
  right->writeHandle = (int64_t)(intptr_t)w1;
  right->ownsRead = 1;
  right->ownsWrite = 1;
  *a = left;
  *b = right;
  return 1;
}

int32_t mpPipeSimplex(MpConn** readEnd, MpConn** writeEnd) {
  HANDLE r = NULL, w = NULL;
  if (!createPipe(&r, &w)) {
    return 0;
  }
  MpConn* reader = (MpConn*)calloc(1, sizeof(MpConn));
  MpConn* writer = (MpConn*)calloc(1, sizeof(MpConn));
  if (reader == NULL || writer == NULL) {
    free(reader);
    free(writer);
    CloseHandle(r);
    CloseHandle(w);
    return 0;
  }
  reader->readHandle = (int64_t)(intptr_t)r;
  reader->writeHandle = -1;
  reader->ownsRead = 1;
  writer->readHandle = -1;
  writer->writeHandle = (int64_t)(intptr_t)w;
  writer->ownsWrite = 1;
  *readEnd = reader;
  *writeEnd = writer;
  return 1;
}

MpConn* mpConnFromHandles(int64_t readHandle, int64_t writeHandle, int32_t ownsRead,
                          int32_t ownsWrite) {
  MpConn* conn = (MpConn*)calloc(1, sizeof(MpConn));
  if (conn == NULL) {
    return NULL;
  }
  conn->readHandle = readHandle;
  conn->writeHandle = writeHandle;
  conn->ownsRead = ownsRead;
  conn->ownsWrite = ownsWrite;
  return conn;
}

int64_t mpConnReadHandle(MpConn* conn) { return conn == NULL ? -1 : conn->readHandle; }
int64_t mpConnWriteHandle(MpConn* conn) { return conn == NULL ? -1 : conn->writeHandle; }

int64_t mpConnRead(MpConn* conn, void* buffer, int64_t capacity, int64_t timeoutMs) {
  if (conn == NULL || conn->readHandle < 0) {
    return -2;
  }
  if (timeoutMs != 0) {
    const DWORD timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
    const DWORD state = WaitForSingleObject((HANDLE)(intptr_t)conn->readHandle, timeout);
    if (state == WAIT_TIMEOUT) {
      return -1;
    }
    if (state != WAIT_OBJECT_0) {
      return -2;
    }
  }
  DWORD read = 0;
  if (!ReadFile((HANDLE)(intptr_t)conn->readHandle, buffer, (DWORD)capacity, &read, NULL)) {
    const DWORD code = GetLastError();
    if (code == ERROR_BROKEN_PIPE || code == ERROR_HANDLE_EOF) {
      return 0; // end of input
    }
    if (timeoutMs == 0 && code == ERROR_NO_DATA) {
      return 0;
    }
    return -2;
  }
  return (int64_t)read;
}

int32_t mpConnWrite(MpConn* conn, const void* data, int64_t len) {
  if (conn == NULL || conn->writeHandle < 0) {
    return 0;
  }
  const uint8_t* cursor = (const uint8_t*)data;
  int64_t remaining = len;
  while (remaining > 0) {
    DWORD written = 0;
    const DWORD chunk = remaining > 0x7fffffff ? 0x7fffffff : (DWORD)remaining;
    if (!WriteFile((HANDLE)(intptr_t)conn->writeHandle, cursor, chunk, &written, NULL) ||
        written == 0) {
      return 0;
    }
    cursor += written;
    remaining -= (int64_t)written;
  }
  return 1;
}

int32_t mpConnPoll(MpConn* conn, int64_t timeoutMs) {
  if (conn == NULL || conn->readHandle < 0) {
    return 0;
  }
  const DWORD timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
  const DWORD state = WaitForSingleObject((HANDLE)(intptr_t)conn->readHandle, timeout);
  return state == WAIT_OBJECT_0 ? 1 : 0;
}

void mpConnCloseRead(MpConn* conn) {
  if (conn != NULL && conn->readHandle >= 0 && conn->ownsRead) {
    CloseHandle((HANDLE)(intptr_t)conn->readHandle);
  }
  if (conn != NULL) {
    conn->readHandle = -1;
  }
}

void mpConnCloseWrite(MpConn* conn) {
  if (conn != NULL && conn->writeHandle >= 0 && conn->ownsWrite) {
    CloseHandle((HANDLE)(intptr_t)conn->writeHandle);
  }
  if (conn != NULL) {
    conn->writeHandle = -1;
  }
}

void mpConnClose(MpConn* conn) {
  if (conn == NULL) {
    return;
  }
  mpConnCloseRead(conn);
  mpConnCloseWrite(conn);
  free(conn);
}

int32_t mpConnReadable(MpConn* conn) { return conn != NULL && conn->readHandle >= 0; }
int32_t mpConnWritable(MpConn* conn) { return conn != NULL && conn->writeHandle >= 0; }
int32_t mpConnClosed(MpConn* conn) {
  return conn == NULL || (conn->readHandle < 0 && conn->writeHandle < 0);
}

/* ------------------------------------------------------------------ */
/* Shared memory                                                       */
/* ------------------------------------------------------------------ */

static MpShm* shmWrap(HANDLE mapping, const char* name) {
  void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (view == NULL) {
    return NULL;
  }
  MEMORY_BASIC_INFORMATION info;
  memset(&info, 0, sizeof(info));
  const SIZE_T size = VirtualQuery(view, &info, sizeof(info)) == 0 ? 0 : info.RegionSize;
  MpShm* shm = (MpShm*)calloc(1, sizeof(MpShm));
  if (shm == NULL) {
    UnmapViewOfFile(view);
    return NULL;
  }
  shm->mapping = mapping;
  shm->view = view;
  shm->size = (int64_t)size;
  shm->name = name == NULL ? NULL : _strdup(name);
  return shm;
}

MpShm* mpShmCreate(const char* name, int64_t size, char* err, size_t errcap) {
  const int64_t bytes = size <= 0 ? 1 : size;
  wchar_t* wideName = wideFromUtf8(name);
  if (wideName == NULL) {
    snprintf(err, errcap, "invalid shared memory name");
    return NULL;
  }
  HANDLE mapping =
      CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)(bytes >> 32),
                         (DWORD)(bytes & 0xffffffffu), wideName);
  free(wideName);
  if (mapping == NULL) {
    snprintf(err, errcap, "CreateFileMapping failed (%lu)", (unsigned long)GetLastError());
    return NULL;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mapping);
    snprintf(err, errcap, "a shared memory block named '%s' already exists", name);
    return NULL;
  }
  MpShm* shm = shmWrap(mapping, name);
  if (shm == NULL) {
    CloseHandle(mapping);
    snprintf(err, errcap, "MapViewOfFile failed (%lu)", (unsigned long)GetLastError());
    return NULL;
  }
  shm->size = bytes;
  return shm;
}

MpShm* mpShmOpen(const char* name, char* err, size_t errcap) {
  wchar_t* wideName = wideFromUtf8(name);
  if (wideName == NULL) {
    snprintf(err, errcap, "invalid shared memory name");
    return NULL;
  }
  HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wideName);
  free(wideName);
  if (mapping == NULL) {
    snprintf(err, errcap, "no shared memory block named '%s'", name);
    return NULL;
  }
  MpShm* shm = shmWrap(mapping, name);
  if (shm == NULL) {
    CloseHandle(mapping);
    snprintf(err, errcap, "MapViewOfFile failed (%lu)", (unsigned long)GetLastError());
    return NULL;
  }
  return shm;
}

void* mpShmPtr(MpShm* shm) { return shm == NULL ? NULL : shm->view; }
int64_t mpShmSize(MpShm* shm) { return shm == NULL ? 0 : shm->size; }
const char* mpShmName(MpShm* shm) { return shm == NULL ? NULL : shm->name; }

void mpShmClose(MpShm* shm) {
  if (shm == NULL) {
    return;
  }
  if (shm->view != NULL) {
    UnmapViewOfFile(shm->view);
  }
  if (shm->mapping != NULL) {
    CloseHandle(shm->mapping);
  }
  free(shm->name);
  free(shm);
}

void mpShmUnlink(MpShm* shm) {
  // Windows destroys a named section once its last handle closes, so `unlink`
  // has nothing to do beyond releasing this process's mapping.
  mpShmClose(shm);
}

int32_t mpShmUnlinkName(const char* name) {
  (void)name;
  return 1;
}

/* ------------------------------------------------------------------ */
/* Named semaphores and events                                         */
/* ------------------------------------------------------------------ */

static MpSem* semWrap(HANDLE handle, int32_t owned) {
  MpSem* sem = (MpSem*)calloc(1, sizeof(MpSem));
  if (sem == NULL) {
    if (owned) {
      CloseHandle(handle);
    }
    return NULL;
  }
  sem->handle = handle;
  sem->owned = owned;
  return sem;
}

MpSem* mpSemCreate(const char* name, int64_t initial, int64_t maximum, char* err, size_t errcap) {
  wchar_t* wideName = wideFromUtf8(name);
  if (wideName == NULL) {
    snprintf(err, errcap, "invalid semaphore name");
    return NULL;
  }
  const LONG maxCount = (LONG)(maximum <= 0 ? 1 : maximum);
  const LONG initCount = (LONG)(initial < 0 ? 0 : (initial > maximum ? maximum : initial));
  HANDLE handle = CreateSemaphoreW(NULL, initCount, maxCount, wideName);
  free(wideName);
  if (handle == NULL) {
    snprintf(err, errcap, "CreateSemaphore failed (%lu)", (unsigned long)GetLastError());
    return NULL;
  }
  return semWrap(handle, 1);
}

MpSem* mpSemOpen(const char* name, char* err, size_t errcap) {
  wchar_t* wideName = wideFromUtf8(name);
  if (wideName == NULL) {
    snprintf(err, errcap, "invalid semaphore name");
    return NULL;
  }
  HANDLE handle = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, wideName);
  free(wideName);
  if (handle == NULL) {
    snprintf(err, errcap, "no semaphore named '%s'", name);
    return NULL;
  }
  return semWrap(handle, 1);
}

int32_t mpSemWait(MpSem* sem, int64_t timeoutMs) {
  if (sem == NULL) {
    return -1;
  }
  const DWORD timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
  const DWORD state = WaitForSingleObject(sem->handle, timeout);
  if (state == WAIT_OBJECT_0) {
    return 0;
  }
  if (state == WAIT_TIMEOUT) {
    return 1;
  }
  return -1;
}

int32_t mpSemTryWait(MpSem* sem) {
  if (sem == NULL) {
    return -1;
  }
  return WaitForSingleObject(sem->handle, 0) == WAIT_OBJECT_0 ? 0 : 1;
}

void mpSemPost(MpSem* sem) {
  if (sem != NULL) {
    ReleaseSemaphore(sem->handle, 1, NULL);
  }
}

void mpSemClose(MpSem* sem) {
  if (sem == NULL) {
    return;
  }
  if (sem->owned && sem->handle != NULL) {
    CloseHandle(sem->handle);
  }
  free(sem);
}

void mpSemUnlink(MpSem* sem) { mpSemClose(sem); }

static MpEvent* eventWrap(HANDLE handle, int32_t owned) {
  MpEvent* event = (MpEvent*)calloc(1, sizeof(MpEvent));
  if (event == NULL) {
    if (owned) {
      CloseHandle(handle);
    }
    return NULL;
  }
  event->handle = handle;
  event->owned = owned;
  return event;
}

MpEvent* mpEventCreate(const char* name, int32_t manualReset, char* err, size_t errcap) {
  wchar_t* wideName = wideFromUtf8(name);
  if (wideName == NULL) {
    snprintf(err, errcap, "invalid event name");
    return NULL;
  }
  HANDLE handle =
      CreateEventW(NULL, manualReset ? TRUE : FALSE, FALSE, wideName);
  free(wideName);
  if (handle == NULL) {
    snprintf(err, errcap, "CreateEvent failed (%lu)", (unsigned long)GetLastError());
    return NULL;
  }
  return eventWrap(handle, 1);
}

MpEvent* mpEventOpen(const char* name, char* err, size_t errcap) {
  wchar_t* wideName = wideFromUtf8(name);
  if (wideName == NULL) {
    snprintf(err, errcap, "invalid event name");
    return NULL;
  }
  HANDLE handle = OpenEventW(EVENT_ALL_ACCESS, FALSE, wideName);
  free(wideName);
  if (handle == NULL) {
    snprintf(err, errcap, "no event named '%s'", name);
    return NULL;
  }
  return eventWrap(handle, 1);
}

int32_t mpEventWait(MpEvent* event, int64_t timeoutMs) {
  if (event == NULL) {
    return -1;
  }
  const DWORD timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
  const DWORD state = WaitForSingleObject(event->handle, timeout);
  if (state == WAIT_OBJECT_0) {
    return 0;
  }
  if (state == WAIT_TIMEOUT) {
    return 1;
  }
  return -1;
}

void mpEventSet(MpEvent* event) {
  if (event != NULL) {
    SetEvent(event->handle);
  }
}

void mpEventClear(MpEvent* event) {
  if (event != NULL) {
    ResetEvent(event->handle);
  }
}

void mpEventClose(MpEvent* event) {
  if (event == NULL) {
    return;
  }
  if (event->owned && event->handle != NULL) {
    CloseHandle(event->handle);
  }
  free(event);
}

void mpEventUnlink(MpEvent* event) { mpEventClose(event); }
