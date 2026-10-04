/// @file mp_internal.h
/// Internal interface shared by the multiprocessing runtime layer.
///
/// The layer is split into a platform-neutral part (`mp_common.c`: the wire
/// codec, the spawnable-target registry, child bootstrapping and the public
/// `sere_mp_*` API declared in `sere_rt.h`) and two platform parts
/// (`mp_windows.c` and `mp_posix.c`) that own every OS-specific primitive.
/// Nothing outside this directory includes this header.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sere_rt.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The boxed `Any` layout, shared with the public runtime header so the
/// declaration and the definition agree on one struct type.
typedef SereMpAny MpAny;

typedef struct MpProcess MpProcess;
typedef struct MpConn MpConn;
typedef struct MpShm MpShm;
typedef struct MpSem MpSem;
typedef struct MpEvent MpEvent;

/* ------------------------------------------------------------------ */
/* Process control                                                     */
/* ------------------------------------------------------------------ */

/// Starts a copy of this executable in multiprocessing-child mode, handing it
/// `payload` over an inherited pipe. Returns NULL and fills `err` on failure.
MpProcess* mpSpawn(const char* payload, int64_t len, int32_t daemon, char* err, size_t errcap);

/// Releases the parent's handle. A still-running child is not stopped; that is
/// what `terminate`/`kill` are for.
void mpProcessRelease(MpProcess* process);

/// Waits up to `timeoutMs` (negative means forever). 0 = exited, 1 = timeout.
int32_t mpWait(MpProcess* process, int64_t timeoutMs);

/// 1 while the child runs, 0 once it has exited, -1 if the handle is invalid.
int32_t mpIsAlive(MpProcess* process);

/// Exit status, or -1 while the child is still running.
int32_t mpExitCode(MpProcess* process);

int64_t mpProcessPid(MpProcess* process);

/// Asks the child to stop (SIGTERM / TerminateProcess).
void mpTerminate(MpProcess* process);

/// Forces the child to stop (SIGKILL / TerminateProcess).
void mpKill(MpProcess* process);

/// A waitable handle suitable for `sere_mp_wait_many`.
void* mpSentinel(MpProcess* process);

void mpSetDaemon(MpProcess* process, int32_t daemon);
int32_t mpIsDaemon(MpProcess* process);

int32_t mpCpuCount(void);
int64_t mpSelfPid(void);

/// Bytes the child should treat as `sys.argv`. The generated `main` hands its
/// own `argc`/`argv` to `sere_mp_bootstrap`, which reads them here.
int32_t mpHandleArg(int32_t argc, char** argv, const char* prefix, int64_t* out);

/// Reads every byte available on an inherited handle until end of input.
int32_t mpReadAllFromHandle(int64_t handle, uint8_t** out, int64_t* outLen);

/// Replaces `argv[0]`'s program path with this executable's real path.
const char* mpProgramPath(void);

/* ------------------------------------------------------------------ */
/* Pipes and connections                                               */
/* ------------------------------------------------------------------ */

/// Creates a duplex pair; each endpoint can read and write.
int32_t mpPipeDuplex(MpConn** a, MpConn** b);

/// Creates a one-way pipe: `readEnd` receives what `writeEnd` sends.
int32_t mpPipeSimplex(MpConn** readEnd, MpConn** writeEnd);

/// Wraps handles inherited from a parent process. `ownsRead`/`ownsWrite`
/// control whether closing the connection closes the handle.
MpConn* mpConnFromHandles(int64_t readHandle, int64_t writeHandle, int32_t ownsRead,
                          int32_t ownsWrite);

int64_t mpConnReadHandle(MpConn* conn);
int64_t mpConnWriteHandle(MpConn* conn);

/// >0 bytes read, 0 end of input, -1 timeout, -2 error.
int64_t mpConnRead(MpConn* conn, void* buffer, int64_t capacity, int64_t timeoutMs);
int32_t mpConnWrite(MpConn* conn, const void* data, int64_t len);
int32_t mpConnPoll(MpConn* conn, int64_t timeoutMs);
void mpConnCloseRead(MpConn* conn);
void mpConnCloseWrite(MpConn* conn);
void mpConnClose(MpConn* conn);
int32_t mpConnReadable(MpConn* conn);
int32_t mpConnWritable(MpConn* conn);
int32_t mpConnClosed(MpConn* conn);

/* ------------------------------------------------------------------ */
/* Shared memory                                                       */
/* ------------------------------------------------------------------ */

MpShm* mpShmCreate(const char* name, int64_t size, char* err, size_t errcap);
MpShm* mpShmOpen(const char* name, char* err, size_t errcap);
void* mpShmPtr(MpShm* shm);
int64_t mpShmSize(MpShm* shm);
const char* mpShmName(MpShm* shm);
void mpShmClose(MpShm* shm);
void mpShmUnlink(MpShm* shm);
int32_t mpShmUnlinkName(const char* name);

/* ------------------------------------------------------------------ */
/* Named semaphores, locks and events                                  */
/* ------------------------------------------------------------------ */

MpSem* mpSemCreate(const char* name, int64_t initial, int64_t maximum, char* err, size_t errcap);
MpSem* mpSemOpen(const char* name, char* err, size_t errcap);
/// 0 acquired, 1 timed out, -1 error.
int32_t mpSemWait(MpSem* sem, int64_t timeoutMs);
int32_t mpSemTryWait(MpSem* sem);
void mpSemPost(MpSem* sem);
void mpSemClose(MpSem* sem);
void mpSemUnlink(MpSem* sem);

MpEvent* mpEventCreate(const char* name, int32_t manualReset, char* err, size_t errcap);
MpEvent* mpEventOpen(const char* name, char* err, size_t errcap);
int32_t mpEventWait(MpEvent* event, int64_t timeoutMs);
void mpEventSet(MpEvent* event);
void mpEventClear(MpEvent* event);
void mpEventClose(MpEvent* event);
void mpEventUnlink(MpEvent* event);

/// Waits for any of up to 64 process sentinels. Returns the index that became
/// ready, or -1 on timeout/error.
int32_t mpWaitMany(void* const* handles, int32_t count, int64_t timeoutMs);

/* ------------------------------------------------------------------ */
/* Shared helpers implemented in mp_common.c                           */
/* ------------------------------------------------------------------ */

/// Fills `out` with `len` cryptographically random bytes.
void mpRandomBytes(void* out, int64_t len);

/// A unique, unguessable object name such as `sere-mp-<32 hex digits>`.
void mpMakeName(const char* prefix, char* out, size_t cap);

/// Current monotonic milliseconds.
int64_t mpNowMs(void);

/// Records a failure that the caller turns into a Sere exception.
void mpSetLastError(const char* message);

const char* mpLastError(void);

#ifdef __cplusplus
}
#endif
