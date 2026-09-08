/// @file sere_proc.c
/// Subprocess support: spawn a child process capturing stdout+stderr.
/// Runs the parent's pending stdio flushes, so writes from the Sere
/// program land in the terminal before the child's captured output
/// is printed.

#include "sere_rt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Exit status of the most recently spawned child (127/-1 if it could
// not be spawned), mirroring what /bin/sh -c reports.
static int32_t last_status = 0;

int32_t sere_proc_status(void) {
  return last_status;
}

static void outEmpty(const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = "";
  }
  if (out_len != NULL) {
    *out_len = 0;
  }
}

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

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static wchar_t* toWide(const char* data, int64_t len) {
  if (data == NULL) {
    data = "";
    len = 0;
  }
  if (len < 0) {
    len = (int64_t)strlen(data);
  }
  const int needed = MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
  wchar_t* wide = (wchar_t*)calloc((size_t)needed + 1, sizeof(wchar_t));
  if (wide == NULL) {
    return NULL;
  }
  if (needed > 0) {
    MultiByteToWideChar(CP_UTF8, 0, data, (int)len, wide, needed);
  }
  return wide;
}

/// Convert UTF-8 to a wide string with a prefix prepended ("cmd /c ").
static wchar_t* toWidePrefixed(const wchar_t* prefix, const wchar_t* rest) {
  const size_t plen = wcslen(prefix);
  const size_t rlen = wcslen(rest);
  wchar_t* out = (wchar_t*)calloc(plen + rlen + 1, sizeof(wchar_t));
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, prefix, plen * sizeof(wchar_t));
  memcpy(out + plen, rest, rlen * sizeof(wchar_t));
  return out;
}

void sere_proc_run(const char* cmd, int64_t cmd_len, const char** out_text, int64_t* out_text_len) {
  outEmpty(out_text, out_text_len);
  fflush(NULL);
  last_status = -1;
  wchar_t* wcmd = toWide(cmd, cmd_len);
  if (wcmd == NULL) {
    return;
  }
  wchar_t* cmdline = toWidePrefixed(L"cmd /c ", wcmd);
  free(wcmd);
  if (cmdline == NULL) {
    return;
  }
  SECURITY_ATTRIBUTES sa;
  memset(&sa, 0, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE readEnd = NULL;
  HANDLE writeEnd = 0;
  if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) {
    free(cmdline);
    return;
  }
  // Our own write end must not be inherited by the child.
  SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput = writeEnd;
  si.hStdError = writeEnd;
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));
  const BOOL spawned =
      CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
  free(cmdline);
  CloseHandle(writeEnd);
  if (!spawned) {
    CloseHandle(readEnd);
    return;
  }
  last_status = 0;
  CloseHandle(pi.hThread);

  // Read the child's combined stdout+stderr into a growing UTF-8 buffer.
  size_t cap = 4096;
  size_t used = 0;
  char* text = (char*)malloc(cap);
  if (text == NULL) {
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(readEnd);
    return;
  }
  while (1) {
    if (used + 4096 + 1 > cap) {
      cap *= 2;
      char* grown = (char*)realloc(text, cap);
      if (grown == NULL) {
        free(text);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(readEnd);
        return;
      }
      text = grown;
    }
    DWORD got = 0;
    if (!ReadFile(readEnd, text + used, 4096, &got, NULL) || got == 0) {
      break;
    }
    used += got;
  }
  text[used] = '\0';
  outOwned(text, (int64_t)used, out_text, out_text_len);
  CloseHandle(readEnd);
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  if (GetExitCodeProcess(pi.hProcess, &code) && code != STILL_ACTIVE) {
    last_status = (int32_t)code;
  }
  CloseHandle(pi.hProcess);
}

#else

#include <sys/wait.h>
#include <unistd.h>

void sere_proc_run(const char* cmd, int64_t cmd_len, const char** out_text, int64_t* out_text_len) {
  outEmpty(out_text, out_text_len);
  (void)cmd_len;
  last_status = -1;
  if (cmd == NULL || cmd[0] == '\0') {
    return;
  }
  fflush(NULL);
  int fds[2];
  if (pipe(fds) != 0) {
    return;
  }
  const pid_t pid = fork();
  if (pid < 0) {
    last_status = -1;
    close(fds[0]);
    close(fds[1]);
    return;
  }
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);
    execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
    _exit(127);
  }
  close(fds[1]);
  last_status = 0;
  size_t cap = 4096;
  size_t used = 0;
  char* text = (char*)malloc(cap);
  if (text == NULL) {
    close(fds[0]);
    waitpid(pid, NULL, 0);
    return;
  }
  while (1) {
    if (used + 4096 + 1 > cap) {
      cap *= 2;
      char* grown = (char*)realloc(text, cap);
      if (grown == NULL) {
        free(text);
        close(fds[0]);
        waitpid(pid, NULL, 0);
        return;
      }
      text = grown;
    }
    const ssize_t got = read(fds[0], text + used, 4096);
    if (got <= 0) {
      break;
    }
    used += (size_t)got;
  }
  close(fds[0]);
  text[used] = '\0';
  outOwned(text, (int64_t)used, out_text, out_text_len);
  int wstatus = 0;
  waitpid(pid, &wstatus, 0);
  if (WIFEXITED(wstatus)) {
    last_status = (int32_t)WEXITSTATUS(wstatus);
  } else if (WIFSIGNALED(wstatus)) {
    last_status = 128 + (int32_t)WTERMSIG(wstatus);
  }
}

#endif

// ---------------------------------------------------------------------------
// Popen-style interactive process: pipes for stdin and merged stdout/stderr,
// with poll / wait / kill / communicate support.
// ---------------------------------------------------------------------------

struct sere_proc {
#ifdef _WIN32
  HANDLE process;
  HANDLE stdin_write;
  HANDLE stdout_read;
#else
  pid_t pid;
  int stdin_fd;
  int stdout_fd;
#endif
  int32_t status;
};

#ifdef _WIN32

sere_proc_t* sere_proc_open(const char* cmd, int64_t cmd_len) {
  (void)cmd_len;
  fflush(NULL);
  wchar_t* wcmd = toWide(cmd, cmd_len);
  if (wcmd == NULL) {
    return NULL;
  }
  wchar_t* cmdline = toWidePrefixed(L"cmd /c ", wcmd);
  free(wcmd);
  if (cmdline == NULL) {
    return NULL;
  }
  sere_proc_t* proc = (sere_proc_t*)calloc(1, sizeof(sere_proc_t));
  if (proc == NULL) {
    free(cmdline);
    return NULL;
  }
  SECURITY_ATTRIBUTES sa;
  memset(&sa, 0, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE in_r = NULL;
  HANDLE in_w = NULL;
  HANDLE out_r = NULL;
  HANDLE out_w = NULL;
  if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 0)) {
    goto fail;
  }
  // Ends we keep on our side must not leak into the child.
  SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = in_r;
  si.hStdOutput = out_w;
  si.hStdError = out_w;
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));
  if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
    goto fail;
  }
  proc->process = pi.hProcess;
  CloseHandle(pi.hThread);
  proc->stdin_write = in_w;
  proc->stdout_read = out_r;
  proc->status = -1;
  CloseHandle(in_r);
  CloseHandle(out_w);
  free(cmdline);
  return proc;

fail:
  if (in_r != NULL) {
    CloseHandle(in_r);
  }
  if (in_w != NULL) {
    CloseHandle(in_w);
  }
  if (out_r != NULL) {
    CloseHandle(out_r);
  }
  if (out_w != NULL) {
    CloseHandle(out_w);
  }
  free(proc);
  free(cmdline);
  return NULL;
}

void sere_proc_write(sere_proc_t* proc, const char* data, int64_t len) {
  if (proc == NULL || proc->stdin_write == NULL) {
    return;
  }
  DWORD written = 0;
  WriteFile(proc->stdin_write, data, (DWORD)(len < 0 ? 0 : len), &written, NULL);
}

void sere_proc_close_stdin(sere_proc_t* proc) {
  if (proc != NULL && proc->stdin_write != NULL) {
    CloseHandle(proc->stdin_write);
    proc->stdin_write = NULL;
  }
}

int64_t sere_proc_read(sere_proc_t* proc, char* buf, int64_t cap, int32_t wait_ms) {
  if (proc == NULL || proc->stdout_read == NULL || buf == NULL || cap <= 0) {
    return -1;
  }
  if (wait_ms > 0) {
    // Blocking read: returns when the child writes something or closes
    // the stream. Sere's communicate() uses this to wait for a reply.
    DWORD got = 0;
    if (!ReadFile(proc->stdout_read, buf, (DWORD)cap, &got, NULL) || got == 0) {
      return 0;
    }
    return (int64_t)got;
  }
  DWORD available = 0;
  if (!PeekNamedPipe(proc->stdout_read, NULL, 0, NULL, &available, NULL)) {
    return 0;  // pipe closed -> end of output
  }
  if (available == 0) {
    return proc->status >= 0 ? 0 : -1;
  }
  DWORD to_read = available;
  if (to_read > (DWORD)cap) {
    to_read = (DWORD)cap;
  }
  DWORD got = 0;
  if (!ReadFile(proc->stdout_read, buf, to_read, &got, NULL) || got == 0) {
    return 0;
  }
  return (int64_t)got;
}

int32_t sere_proc_poll(sere_proc_t* proc, int32_t* status) {
  if (proc == NULL) {
    return 0;
  }
  DWORD code = 0;
  if (GetExitCodeProcess(proc->process, &code) && code != STILL_ACTIVE) {
    proc->status = (int32_t)code;
    last_status = proc->status;
    if (status != NULL) {
      *status = proc->status;
    }
    return 1;
  }
  return 0;
}

int32_t sere_proc_wait(sere_proc_t* proc) {
  if (proc == NULL) {
    return -1;
  }
  WaitForSingleObject(proc->process, INFINITE);
  sere_proc_poll(proc, NULL);
  return proc->status;
}

void sere_proc_kill(sere_proc_t* proc) {
  if (proc != NULL && proc->process != NULL) {
    TerminateProcess(proc->process, 1);
  }
}

void sere_proc_close(sere_proc_t* proc) {
  if (proc == NULL) {
    return;
  }
  DWORD code = 0;
  if (proc->process != NULL && GetExitCodeProcess(proc->process, &code) &&
      code == STILL_ACTIVE) {
    TerminateProcess(proc->process, 1);
  }
  sere_proc_close_stdin(proc);
  if (proc->stdout_read != NULL) {
    CloseHandle(proc->stdout_read);
  }
  if (proc->process != NULL) {
    CloseHandle(proc->process);
  }
  free(proc);
}

#else

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

sere_proc_t* sere_proc_open(const char* cmd, int64_t cmd_len) {
  (void)cmd_len;
  last_status = -1;
  if (cmd == NULL || cmd[0] == '\0') {
    return NULL;
  }
  fflush(NULL);
  int in_fds[2];
  int out_fds[2];
  if (pipe(in_fds) != 0 || pipe(out_fds) != 0) {
    return NULL;
  }
  sere_proc_t* proc = (sere_proc_t*)calloc(1, sizeof(sere_proc_t));
  if (proc == NULL) {
    close(in_fds[0]);
    close(in_fds[1]);
    close(out_fds[0]);
    close(out_fds[1]);
    return NULL;
  }
  const pid_t pid = fork();
  if (pid < 0) {
    close(in_fds[0]);
    close(in_fds[1]);
    close(out_fds[0]);
    close(out_fds[1]);
    free(proc);
    return NULL;
  }
  if (pid == 0) {
    close(in_fds[1]);
    close(out_fds[0]);
    dup2(in_fds[0], STDIN_FILENO);
    dup2(out_fds[1], STDOUT_FILENO);
    dup2(out_fds[1], STDERR_FILENO);
    close(in_fds[0]);
    close(out_fds[1]);
    execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
    _exit(127);
  }
  close(in_fds[0]);
  close(out_fds[1]);
  // Make the read end non-blocking so wait_ms == 0 polls instead of
  // stalling the whole program on a silent child.
  fcntl(proc->stdout_fd, F_SETFL, fcntl(proc->stdout_fd, F_GETFL) | O_NONBLOCK);
  proc->pid = pid;
  proc->stdin_fd = in_fds[1];
  proc->stdout_fd = out_fds[0];
  proc->status = -1;
  return proc;
}

void sere_proc_write(sere_proc_t* proc, const char* data, int64_t len) {
  if (proc == NULL || proc->stdin_fd < 0) {
    return;
  }
  if (len < 0) {
    len = 0;
  }
  const ssize_t ignored = write(proc->stdin_fd, data, (size_t)len);
  (void)ignored;
}

void sere_proc_close_stdin(sere_proc_t* proc) {
  if (proc != NULL && proc->stdin_fd >= 0) {
    close(proc->stdin_fd);
    proc->stdin_fd = -1;
  }
}

int64_t sere_proc_read(sere_proc_t* proc, char* buf, int64_t cap, int32_t wait_ms) {
  if (proc == NULL || proc->stdout_fd < 0 || buf == NULL || cap <= 0) {
    return -1;
  }
  if (wait_ms > 0) {
    // Wait up to wait_ms for output to become available.
    struct pollfd pfd;
    pfd.fd = proc->stdout_fd;
    pfd.events = POLLIN;
    const int ready = poll(&pfd, 1, (int)wait_ms);
    if (ready <= 0) {
      return -1;
    }
  }
  const ssize_t got = read(proc->stdout_fd, buf, (size_t)cap);
  if (got > 0) {
    return (int64_t)got;
  }
  if (got == 0) {
    return 0;  // end of output
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
    return -1;
  }
  return 0;
}

int32_t sere_proc_poll(sere_proc_t* proc, int32_t* status) {
  if (proc == NULL || proc->status >= 0) {
    if (status != NULL && proc != NULL) {
      *status = proc->status;
    }
    return proc != NULL && proc->status >= 0 ? 1 : 0;
  }
  int wstatus = 0;
  const pid_t r = waitpid(proc->pid, &wstatus, WNOHANG);
  if (r == 0) {
    return 0;
  }
  if (WIFEXITED(wstatus)) {
    proc->status = (int32_t)WEXITSTATUS(wstatus);
  } else if (WIFSIGNALED(wstatus)) {
    proc->status = 128 + (int32_t)WTERMSIG(wstatus);
  } else {
    proc->status = -1;
  }
  last_status = proc->status;
  if (status != NULL) {
    *status = proc->status;
  }
  return 1;
}

int32_t sere_proc_wait(sere_proc_t* proc) {
  if (proc == NULL) {
    return -1;
  }
  sere_proc_poll(proc, NULL);
  if (proc->status >= 0) {
    return proc->status;
  }
  int wstatus = 0;
  waitpid(proc->pid, &wstatus, 0);
  if (WIFEXITED(wstatus)) {
    proc->status = (int32_t)WEXITSTATUS(wstatus);
  } else if (WIFSIGNALED(wstatus)) {
    proc->status = 128 + (int32_t)WTERMSIG(wstatus);
  } else {
    proc->status = -1;
  }
  last_status = proc->status;
  return proc->status;
}

void sere_proc_kill(sere_proc_t* proc) {
  if (proc != NULL && proc->status < 0) {
    kill(proc->pid, SIGKILL);
  }
}

void sere_proc_close(sere_proc_t* proc) {
  if (proc == NULL) {
    return;
  }
  sere_proc_poll(proc, NULL);
  if (proc->status < 0) {
    kill(proc->pid, SIGKILL);
  }
  sere_proc_close_stdin(proc);
  if (proc->stdout_fd >= 0) {
    close(proc->stdout_fd);
    proc->stdout_fd = -1;
  }
  int wstatus = 0;
  waitpid(proc->pid, &wstatus, 0);
  free(proc);
}

#endif

// ---------------------------------------------------------------------------
// communicate: read all remaining output from the child. Equivalent to
// Python's Popen.communicate() for the stdout/stderr-merged case.
// ---------------------------------------------------------------------------

#ifndef SERE_PROC_CHUNK
#define SERE_PROC_CHUNK 4096
#endif

sere_proc_buffer_t sere_proc_communicate(sere_proc_t* proc) {
  sere_proc_buffer_t result;
  result.bytes = NULL;
  result.len = 0;
  if (proc == NULL) {
    return result;
  }
  size_t size = 0;
  size_t cap = 0;
  char* data = NULL;
  for (;;) {
    if (size + 1 >= cap) {
      cap = cap == 0 ? SERE_PROC_CHUNK : cap * 2;
      char* grown = (char*)realloc(data, cap);
      if (grown == NULL) {
        break;
      }
      data = grown;
    }
    const int64_t got = sere_proc_read(proc, data + size, (int64_t)(cap - size - 1), 1000);
    if (got <= 0) {
      break;
    }
    size += (size_t)got;
  }
  if (data == NULL) {
    data = (char*)calloc(1, 1);
  } else {
    data[size] = '\0';
  }
  result.bytes = data;
  result.len = (int64_t)size;
  return result;
}
