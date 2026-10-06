/// @file mp_posix.c
/// POSIX implementation of the multiprocessing primitives: `posix_spawn`
/// process creation, anonymous pipes, POSIX shared memory, named semaphores and
/// named events built from a semaphore plus a shared flag.
///
/// Descriptors are inherited across `posix_spawn`, so a pipe's file descriptor
/// number is what travels to the child. Blocking operations use `poll` and
/// `sem_timedwait`; the one exception is `mpWaitMany` waiting on process
/// sentinels, which has no portable pollable form and is documented as such.

#include "mp_internal.h"

#if defined(_WIN32)
#error "mp_posix.c is only built on POSIX hosts"
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;

/* ------------------------------------------------------------------ */
/* Structs                                                             */
/* ------------------------------------------------------------------ */

struct MpProcess {
  pid_t pid;
  int32_t daemon;
  int32_t reaped;
  int32_t exitCode;
};

struct MpConn {
  int readFd;
  int writeFd;
  int32_t ownsRead;
  int32_t ownsWrite;
  int32_t readClosed;
  int32_t writeClosed;
};

struct MpShm {
  int fd;
  void* view;
  int64_t size;
  char* name;
  int32_t created;
};

struct MpSem {
  sem_t* sem;
  char* name;
};

struct MpEvent {
  sem_t* sem;
  volatile int32_t* flag;
  volatile int32_t* waiters;
  char* semName;
  char* shmName;
  int fd;
  int64_t size;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

int64_t mpNowMs(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

void mpRandomBytes(void* out, int64_t len) {
  if (len <= 0 || out == NULL) {
    return;
  }
  int fd = open("/dev/urandom", O_RDONLY);
  int64_t done = 0;
  if (fd >= 0) {
    while (done < len) {
      const ssize_t n = read(fd, (uint8_t*)out + done, (size_t)(len - done));
      if (n <= 0) {
        break;
      }
      done += n;
    }
    close(fd);
  }
  if (done < len) {
    uint64_t seed = ((uint64_t)mpNowMs() << 20) ^ ((uint64_t)getpid() << 8);
    for (int64_t index = done; index < len; ++index) {
      seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
      ((uint8_t*)out)[index] = (uint8_t)(seed >> 33);
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

int64_t mpSelfPid(void) { return (int64_t)getpid(); }

int32_t mpCpuCount(void) {
  long count = sysconf(_SC_NPROCESSORS_ONLN);
  return (int32_t)(count <= 0 ? 1 : count);
}

const char* mpProgramPath(void) {
  static char buffer[4096];
  const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (n > 0) {
    buffer[n] = '\0';
    return buffer;
  }
  return NULL;
}

static void sleepMs(int64_t ms) {
  struct timespec request;
  request.tv_sec = ms / 1000;
  request.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&request, NULL);
}

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
    const ssize_t n = read((int)handle, buffer + len, (size_t)(cap - len));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      free(buffer);
      return 0;
    }
    if (n == 0) {
      break;
    }
    len += n;
  }
  *out = buffer;
  *outLen = len;
  return 1;
}

/* ------------------------------------------------------------------ */
/* Processes                                                           */
/* ------------------------------------------------------------------ */

MpProcess* mpSpawn(const char* payload, int64_t len, int32_t daemon, char* err, size_t errcap) {
  int fds[2];
  if (pipe(fds) != 0) {
    snprintf(err, errcap, "pipe failed (%s)", strerror(errno));
    return NULL;
  }
  // The child needs the read end across exec; the parent keeps the write end.
  fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) & ~FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);

  const char* program = mpProgramPath();
  if (program == NULL) {
    close(fds[0]);
    close(fds[1]);
    snprintf(err, errcap, "cannot resolve the executable path");
    return NULL;
  }
  char handleArg[64];
  snprintf(handleArg, sizeof(handleArg), "--sere-mp-child=%d", fds[0]);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
#if !defined(__GLIBC__)
  // musl and the BSDs need the descriptor marked inheritable explicitly.
  // glibc inherits it by default (FD_CLOEXEC was cleared above) and has no
  // posix_spawn_file_actions_addinherit_np at all.
  posix_spawn_file_actions_addinherit_np(&actions, fds[0]);
#endif
  char* argv[3];
  argv[0] = (char*)program;
  argv[1] = handleArg;
  argv[2] = NULL;
  pid_t pid = 0;
  const int spawnResult = posix_spawn(&pid, program, &actions, NULL, argv, environ);
  posix_spawn_file_actions_destroy(&actions);
  if (spawnResult != 0) {
    close(fds[0]);
    close(fds[1]);
    snprintf(err, errcap, "posix_spawn failed (%s)", strerror(spawnResult));
    return NULL;
  }
  close(fds[0]);

  MpProcess* process = (MpProcess*)calloc(1, sizeof(MpProcess));
  if (process == NULL) {
    close(fds[1]);
    kill(pid, SIGKILL);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  process->pid = pid;
  process->daemon = daemon;
  process->exitCode = -1;

  const uint8_t* cursor = (const uint8_t*)payload;
  int64_t remaining = len;
  while (remaining > 0) {
    const ssize_t written = write(fds[1], cursor, (size_t)remaining);
    if (written <= 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    cursor += written;
    remaining -= written;
  }
  close(fds[1]);
  return process;
}

void mpProcessRelease(MpProcess* process) {
  if (process == NULL) {
    return;
  }
  if (!process->reaped) {
    // Never leave a zombie behind.
    int status = 0;
    const pid_t result = waitpid(process->pid, &status, WNOHANG);
    if (result == process->pid) {
      process->reaped = 1;
    }
  }
  free(process);
}

static void recordExit(MpProcess* process, int status) {
  process->reaped = 1;
  if (WIFEXITED(status)) {
    process->exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    process->exitCode = 128 + WTERMSIG(status);
  } else {
    process->exitCode = -1;
  }
}

int32_t mpWait(MpProcess* process, int64_t timeoutMs) {
  if (process == NULL) {
    return 0;
  }
  if (process->reaped) {
    return 0;
  }
  const int64_t deadline = timeoutMs < 0 ? -1 : mpNowMs() + timeoutMs;
  for (;;) {
    int status = 0;
    const pid_t result = waitpid(process->pid, &status, WNOHANG);
    if (result == process->pid) {
      recordExit(process, status);
      return 0;
    }
    if (result < 0 && errno != EINTR) {
      process->reaped = 1;
      return 0;
    }
    if (timeoutMs == 0) {
      return 1;
    }
    if (deadline >= 0 && mpNowMs() >= deadline) {
      return 1;
    }
    sleepMs(2);
  }
}

int32_t mpIsAlive(MpProcess* process) {
  if (process == NULL) {
    return 0;
  }
  if (process->reaped) {
    return 0;
  }
  if (mpWait(process, 0) == 0) {
    return 0;
  }
  return 1;
}

int32_t mpExitCode(MpProcess* process) {
  if (process == NULL || !process->reaped) {
    return -1;
  }
  return process->exitCode;
}

int64_t mpProcessPid(MpProcess* process) { return process == NULL ? -1 : (int64_t)process->pid; }

void mpTerminate(MpProcess* process) {
  if (process != NULL && !process->reaped) {
    kill(process->pid, SIGTERM);
  }
}

void mpKill(MpProcess* process) {
  if (process != NULL && !process->reaped) {
    kill(process->pid, SIGKILL);
  }
}

void* mpSentinel(MpProcess* process) { return (void*)(intptr_t)(process == NULL ? -1 : process->pid); }

void mpSetDaemon(MpProcess* process, int32_t daemon) {
  if (process != NULL) {
    process->daemon = daemon;
  }
}

int32_t mpIsDaemon(MpProcess* process) { return process == NULL ? 0 : process->daemon; }

int32_t mpWaitMany(void* const* handles, int32_t count, int64_t timeoutMs) {
  // Connection descriptors poll directly; process sentinels are polled through
  // `waitpid`, which is the only portable way to observe a child exiting.
  const int64_t deadline = timeoutMs < 0 ? -1 : mpNowMs() + timeoutMs;
  for (;;) {
    for (int32_t index = 0; index < count; ++index) {
      const int64_t value = (int64_t)(intptr_t)handles[index];
      if (value <= 0) {
        continue;
      }
      struct pollfd descriptor;
      descriptor.fd = (int)value;
      descriptor.events = POLLIN;
      descriptor.revents = 0;
      if (poll(&descriptor, 1, 0) > 0) {
        return index;
      }
      int status = 0;
      const pid_t result = waitpid((pid_t)value, &status, WNOHANG);
      if (result == (pid_t)value) {
        // Re-queue the exit so the owning MpProcess still sees it.
        return index;
      }
    }
    if (timeoutMs == 0) {
      return -1;
    }
    if (deadline >= 0 && mpNowMs() >= deadline) {
      return -1;
    }
    sleepMs(2);
  }
}

/* ------------------------------------------------------------------ */
/* Pipes                                                               */
/* ------------------------------------------------------------------ */

static MpConn* connNew(int readFd, int writeFd) {
  MpConn* conn = (MpConn*)calloc(1, sizeof(MpConn));
  if (conn == NULL) {
    return NULL;
  }
  conn->readFd = readFd;
  conn->writeFd = writeFd;
  conn->ownsRead = readFd >= 0;
  conn->ownsWrite = writeFd >= 0;
  return conn;
}

int32_t mpPipeDuplex(MpConn** a, MpConn** b) {
  int first[2];
  int second[2];
  if (pipe(first) != 0 || pipe(second) != 0) {
    return 0;
  }
  // Nothing may be close-on-exec: the descriptors travel to children.
  for (int index = 0; index < 2; ++index) {
    fcntl(first[index], F_SETFD, fcntl(first[index], F_GETFD) & ~FD_CLOEXEC);
    fcntl(second[index], F_SETFD, fcntl(second[index], F_GETFD) & ~FD_CLOEXEC);
  }
  MpConn* left = connNew(first[0], second[1]);
  MpConn* right = connNew(second[0], first[1]);
  if (left == NULL || right == NULL) {
    free(left);
    free(right);
    close(first[0]);
    close(first[1]);
    close(second[0]);
    close(second[1]);
    return 0;
  }
  *a = left;
  *b = right;
  return 1;
}

int32_t mpPipeSimplex(MpConn** readEnd, MpConn** writeEnd) {
  int fds[2];
  if (pipe(fds) != 0) {
    return 0;
  }
  fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) & ~FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) & ~FD_CLOEXEC);
  MpConn* reader = connNew(fds[0], -1);
  MpConn* writer = connNew(-1, fds[1]);
  if (reader == NULL || writer == NULL) {
    free(reader);
    free(writer);
    close(fds[0]);
    close(fds[1]);
    return 0;
  }
  *readEnd = reader;
  *writeEnd = writer;
  return 1;
}

MpConn* mpConnFromHandles(int64_t readHandle, int64_t writeHandle, int32_t ownsRead,
                          int32_t ownsWrite) {
  MpConn* conn = connNew((int)readHandle, (int)writeHandle);
  if (conn == NULL) {
    return NULL;
  }
  conn->ownsRead = ownsRead;
  conn->ownsWrite = ownsWrite;
  return conn;
}

int64_t mpConnReadHandle(MpConn* conn) { return conn == NULL ? -1 : (int64_t)conn->readFd; }
int64_t mpConnWriteHandle(MpConn* conn) { return conn == NULL ? -1 : (int64_t)conn->writeFd; }

int64_t mpConnRead(MpConn* conn, void* buffer, int64_t capacity, int64_t timeoutMs) {
  if (conn == NULL || conn->readFd < 0) {
    return -2;
  }
  if (timeoutMs != 0) {
    struct pollfd descriptor;
    descriptor.fd = conn->readFd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    const int ready = poll(&descriptor, 1, timeoutMs < 0 ? -1 : (int)timeoutMs);
    if (ready == 0) {
      return -1;
    }
    if (ready < 0) {
      return errno == EINTR ? -1 : -2;
    }
  }
  const ssize_t n = read(conn->readFd, buffer, (size_t)capacity);
  if (n < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? -1 : -2;
  }
  return (int64_t)n;
}

int32_t mpConnWrite(MpConn* conn, const void* data, int64_t len) {
  if (conn == NULL || conn->writeFd < 0) {
    return 0;
  }
  const uint8_t* cursor = (const uint8_t*)data;
  int64_t remaining = len;
  while (remaining > 0) {
    const ssize_t written = write(conn->writeFd, cursor, (size_t)remaining);
    if (written <= 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    cursor += written;
    remaining -= written;
  }
  return 1;
}

int32_t mpConnPoll(MpConn* conn, int64_t timeoutMs) {
  if (conn == NULL || conn->readFd < 0) {
    return 0;
  }
  struct pollfd descriptor;
  descriptor.fd = conn->readFd;
  descriptor.events = POLLIN;
  descriptor.revents = 0;
  const int ready = poll(&descriptor, 1, timeoutMs < 0 ? -1 : (int)timeoutMs);
  return ready > 0 ? 1 : 0;
}

void mpConnCloseRead(MpConn* conn) {
  if (conn != NULL && conn->readFd >= 0 && conn->ownsRead) {
    close(conn->readFd);
  }
  if (conn != NULL) {
    conn->readFd = -1;
    conn->readClosed = 1;
  }
}

void mpConnCloseWrite(MpConn* conn) {
  if (conn != NULL && conn->writeFd >= 0 && conn->ownsWrite) {
    close(conn->writeFd);
  }
  if (conn != NULL) {
    conn->writeFd = -1;
    conn->writeClosed = 1;
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

int32_t mpConnReadable(MpConn* conn) { return conn != NULL && conn->readFd >= 0; }
int32_t mpConnWritable(MpConn* conn) { return conn != NULL && conn->writeFd >= 0; }
int32_t mpConnClosed(MpConn* conn) {
  return conn == NULL || (conn->readFd < 0 && conn->writeFd < 0);
}

/* ------------------------------------------------------------------ */
/* Shared memory                                                       */
/* ------------------------------------------------------------------ */

static char* shmOsName(const char* name) {
  const size_t n = strlen(name);
  char* out = (char*)malloc(n + 2);
  if (out == NULL) {
    return NULL;
  }
  out[0] = '/';
  memcpy(out + 1, name, n + 1);
  return out;
}

MpShm* mpShmCreate(const char* name, int64_t size, char* err, size_t errcap) {
  const int64_t bytes = size <= 0 ? 1 : size;
  char* osName = shmOsName(name);
  if (osName == NULL) {
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  const int fd = shm_open(osName, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0) {
    snprintf(err, errcap, "shm_open('%s') failed (%s)", name, strerror(errno));
    free(osName);
    return NULL;
  }
  if (ftruncate(fd, (off_t)bytes) != 0) {
    snprintf(err, errcap, "ftruncate failed (%s)", strerror(errno));
    close(fd);
    shm_unlink(osName);
    free(osName);
    return NULL;
  }
  void* view = mmap(NULL, (size_t)bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    snprintf(err, errcap, "mmap failed (%s)", strerror(errno));
    close(fd);
    shm_unlink(osName);
    free(osName);
    return NULL;
  }
  MpShm* shm = (MpShm*)calloc(1, sizeof(MpShm));
  if (shm == NULL) {
    munmap(view, (size_t)bytes);
    close(fd);
    shm_unlink(osName);
    free(osName);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  shm->fd = fd;
  shm->view = view;
  shm->size = bytes;
  shm->name = osName;
  shm->created = 1;
  return shm;
}

MpShm* mpShmOpen(const char* name, char* err, size_t errcap) {
  char* osName = shmOsName(name);
  if (osName == NULL) {
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  const int fd = shm_open(osName, O_RDWR, 0600);
  if (fd < 0) {
    snprintf(err, errcap, "no shared memory named '%s'", name);
    free(osName);
    return NULL;
  }
  struct stat info;
  if (fstat(fd, &info) != 0) {
    snprintf(err, errcap, "fstat failed (%s)", strerror(errno));
    close(fd);
    free(osName);
    return NULL;
  }
  void* view = mmap(NULL, (size_t)info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    snprintf(err, errcap, "mmap failed (%s)", strerror(errno));
    close(fd);
    free(osName);
    return NULL;
  }
  MpShm* shm = (MpShm*)calloc(1, sizeof(MpShm));
  if (shm == NULL) {
    munmap(view, (size_t)info.st_size);
    close(fd);
    free(osName);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  shm->fd = fd;
  shm->view = view;
  shm->size = (int64_t)info.st_size;
  shm->name = osName;
  return shm;
}

void* mpShmPtr(MpShm* shm) { return shm == NULL ? NULL : shm->view; }
int64_t mpShmSize(MpShm* shm) { return shm == NULL ? 0 : shm->size; }
const char* mpShmName(MpShm* shm) { return shm == NULL ? NULL : shm->name; }

void mpShmClose(MpShm* shm) {
  if (shm == NULL) {
    return;
  }
  if (shm->view != NULL && shm->size > 0) {
    munmap(shm->view, (size_t)shm->size);
  }
  if (shm->fd >= 0) {
    close(shm->fd);
  }
  free(shm->name);
  free(shm);
}

void mpShmUnlink(MpShm* shm) {
  if (shm == NULL) {
    return;
  }
  if (shm->name != NULL) {
    shm_unlink(shm->name);
  }
  mpShmClose(shm);
}

int32_t mpShmUnlinkName(const char* name) {
  char* osName = shmOsName(name);
  if (osName == NULL) {
    return 0;
  }
  const int result = shm_unlink(osName) == 0 ? 1 : 0;
  free(osName);
  return result;
}

/* ------------------------------------------------------------------ */
/* Named semaphores and events                                         */
/* ------------------------------------------------------------------ */

static char* semOsName(const char* name) { return shmOsName(name); }

MpSem* mpSemCreate(const char* name, int64_t initial, int64_t maximum, char* err, size_t errcap) {
  char* osName = semOsName(name);
  if (osName == NULL) {
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  sem_unlink(osName);
  const int64_t value = initial < 0 ? 0 : (initial > maximum ? maximum : initial);
  sem_t* sem = sem_open(osName, O_CREAT | O_EXCL, 0600, (unsigned)value);
  if (sem == SEM_FAILED) {
    snprintf(err, errcap, "sem_open failed (%s)", strerror(errno));
    free(osName);
    return NULL;
  }
  MpSem* handle = (MpSem*)calloc(1, sizeof(MpSem));
  if (handle == NULL) {
    sem_close(sem);
    sem_unlink(osName);
    free(osName);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  handle->sem = sem;
  handle->name = osName;
  return handle;
}

MpSem* mpSemOpen(const char* name, char* err, size_t errcap) {
  char* osName = semOsName(name);
  if (osName == NULL) {
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  sem_t* sem = sem_open(osName, 0);
  if (sem == SEM_FAILED) {
    snprintf(err, errcap, "no semaphore named '%s'", name);
    free(osName);
    return NULL;
  }
  MpSem* handle = (MpSem*)calloc(1, sizeof(MpSem));
  if (handle == NULL) {
    sem_close(sem);
    free(osName);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  handle->sem = sem;
  handle->name = osName;
  return handle;
}

int32_t mpSemWait(MpSem* sem, int64_t timeoutMs) {
  if (sem == NULL) {
    return -1;
  }
  if (timeoutMs < 0) {
    while (sem_wait(sem->sem) != 0) {
      if (errno != EINTR) {
        return -1;
      }
    }
    return 0;
  }
  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  const int64_t total = (int64_t)deadline.tv_nsec / 1000000 + timeoutMs;
  deadline.tv_sec += total / 1000;
  deadline.tv_nsec = (total % 1000) * 1000000;
  for (;;) {
    if (sem_timedwait(sem->sem, &deadline) == 0) {
      return 0;
    }
    if (errno == EINTR) {
      continue;
    }
    return errno == ETIMEDOUT ? 1 : -1;
  }
}

int32_t mpSemTryWait(MpSem* sem) {
  if (sem == NULL) {
    return -1;
  }
  return sem_trywait(sem->sem) == 0 ? 0 : 1;
}

void mpSemPost(MpSem* sem) {
  if (sem != NULL) {
    sem_post(sem->sem);
  }
}

void mpSemClose(MpSem* sem) {
  if (sem == NULL) {
    return;
  }
  if (sem->sem != NULL) {
    sem_close(sem->sem);
  }
  free(sem->name);
  free(sem);
}

void mpSemUnlink(MpSem* sem) {
  if (sem == NULL) {
    return;
  }
  if (sem->name != NULL) {
    sem_unlink(sem->name);
  }
  mpSemClose(sem);
}

MpEvent* mpEventCreate(const char* name, int32_t manualReset, char* err, size_t errcap) {
  (void)manualReset;
  char semName[256];
  char shmName[256];
  snprintf(semName, sizeof(semName), "%s-s", name);
  snprintf(shmName, sizeof(shmName), "%s-f", name);
  char* osSem = semOsName(semName);
  sem_unlink(osSem);
  sem_t* sem = sem_open(osSem, O_CREAT | O_EXCL, 0600, 0);
  if (sem == SEM_FAILED) {
    snprintf(err, errcap, "sem_open failed (%s)", strerror(errno));
    free(osSem);
    return NULL;
  }
  char* osShm = shmOsName(shmName);
  shm_unlink(osShm);
  const int fd = shm_open(osShm, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0 || ftruncate(fd, (off_t)sizeof(int32_t) * 2) != 0) {
    snprintf(err, errcap, "shared flag creation failed (%s)", strerror(errno));
    if (fd >= 0) {
      close(fd);
    }
    sem_close(sem);
    sem_unlink(osSem);
    free(osSem);
    free(osShm);
    return NULL;
  }
  void* view = mmap(NULL, sizeof(int32_t) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    snprintf(err, errcap, "mmap failed (%s)", strerror(errno));
    close(fd);
    shm_unlink(osShm);
    sem_close(sem);
    sem_unlink(osSem);
    free(osSem);
    free(osShm);
    return NULL;
  }
  memset(view, 0, sizeof(int32_t) * 2);
  MpEvent* event = (MpEvent*)calloc(1, sizeof(MpEvent));
  if (event == NULL) {
    munmap(view, sizeof(int32_t) * 2);
    close(fd);
    shm_unlink(osShm);
    sem_close(sem);
    sem_unlink(osSem);
    free(osSem);
    free(osShm);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  event->sem = sem;
  event->flag = (volatile int32_t*)view;
  event->waiters = (volatile int32_t*)((int32_t*)view + 1);
  event->semName = osSem;
  event->shmName = osShm;
  event->fd = fd;
  event->size = sizeof(int32_t) * 2;
  return event;
}

MpEvent* mpEventOpen(const char* name, char* err, size_t errcap) {
  char semName[256];
  char shmName[256];
  snprintf(semName, sizeof(semName), "%s-s", name);
  snprintf(shmName, sizeof(shmName), "%s-f", name);
  char* osSem = semOsName(semName);
  char* osShm = shmOsName(shmName);
  sem_t* sem = sem_open(osSem, 0);
  if (sem == SEM_FAILED) {
    snprintf(err, errcap, "no event named '%s'", name);
    free(osSem);
    free(osShm);
    return NULL;
  }
  const int fd = shm_open(osShm, O_RDWR, 0600);
  if (fd < 0) {
    snprintf(err, errcap, "no event named '%s'", name);
    sem_close(sem);
    free(osSem);
    free(osShm);
    return NULL;
  }
  void* view = mmap(NULL, sizeof(int32_t) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    snprintf(err, errcap, "mmap failed (%s)", strerror(errno));
    close(fd);
    sem_close(sem);
    free(osSem);
    free(osShm);
    return NULL;
  }
  MpEvent* event = (MpEvent*)calloc(1, sizeof(MpEvent));
  if (event == NULL) {
    munmap(view, sizeof(int32_t) * 2);
    close(fd);
    sem_close(sem);
    free(osSem);
    free(osShm);
    snprintf(err, errcap, "out of memory");
    return NULL;
  }
  event->sem = sem;
  event->flag = (volatile int32_t*)view;
  event->waiters = (volatile int32_t*)((int32_t*)view + 1);
  event->semName = osSem;
  event->shmName = osShm;
  event->fd = fd;
  event->size = sizeof(int32_t) * 2;
  return event;
}

int32_t mpEventWait(MpEvent* event, int64_t timeoutMs) {
  if (event == NULL) {
    return -1;
  }
  const int64_t deadline = timeoutMs < 0 ? -1 : mpNowMs() + timeoutMs;
  for (;;) {
    if (__atomic_load_n((int32_t*)event->flag, __ATOMIC_ACQUIRE) != 0) {
      return 0;
    }
    if (timeoutMs == 0) {
      return 1;
    }
    __atomic_add_fetch((int32_t*)event->waiters, 1, __ATOMIC_ACQ_REL);
    int64_t remaining = deadline < 0 ? -1 : deadline - mpNowMs();
    if (remaining < 0) {
      remaining = 0;
    }
    const int32_t result = mpSemWait(
        &(MpSem){.sem = event->sem, .name = NULL}, remaining);
    __atomic_sub_fetch((int32_t*)event->waiters, 1, __ATOMIC_ACQ_REL);
    if (__atomic_load_n((int32_t*)event->flag, __ATOMIC_ACQUIRE) != 0) {
      return 0;
    }
    if (result == 1 || (deadline >= 0 && mpNowMs() >= deadline)) {
      return 1;
    }
  }
}

void mpEventSet(MpEvent* event) {
  if (event == NULL) {
    return;
  }
  __atomic_store_n((int32_t*)event->flag, 1, __ATOMIC_RELEASE);
  const int32_t waiters = __atomic_load_n((int32_t*)event->waiters, __ATOMIC_ACQUIRE);
  for (int32_t index = 0; index < waiters + 1; ++index) {
    sem_post(event->sem);
  }
}

void mpEventClear(MpEvent* event) {
  if (event != NULL) {
    __atomic_store_n((int32_t*)event->flag, 0, __ATOMIC_RELEASE);
  }
}

void mpEventClose(MpEvent* event) {
  if (event == NULL) {
    return;
  }
  if (event->flag != NULL) {
    munmap((void*)event->flag, (size_t)event->size);
  }
  if (event->fd >= 0) {
    close(event->fd);
  }
  if (event->sem != NULL) {
    sem_close(event->sem);
  }
  free(event->semName);
  free(event->shmName);
  free(event);
}

void mpEventUnlink(MpEvent* event) {
  if (event == NULL) {
    return;
  }
  if (event->semName != NULL) {
    sem_unlink(event->semName);
  }
  if (event->shmName != NULL) {
    shm_unlink(event->shmName);
  }
  mpEventClose(event);
}
