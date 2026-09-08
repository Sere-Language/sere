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
static wchar_t* toWidePrefixed(const char* prefix, const wchar_t* rest) {
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
  CloseHandle(pi.hProcess);
}

#else

#include <sys/wait.h>
#include <unistd.h>

void sere_proc_run(const char* cmd, int64_t cmd_len, const char** out_text, int64_t* out_text_len) {
  outEmpty(out_text, out_text_len);
  (void)cmd_len;
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
  waitpid(pid, NULL, 0);
}

#endif
