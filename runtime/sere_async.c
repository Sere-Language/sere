/// @file sere_async.c
/// Cooperative single-threaded executor used by async/await.
///
/// The executor is deliberately generic: work items are { fn, arg } wakers so
/// that generated coroutine glue, timers, and (later) event-backend
/// completions can all post wakeups through one queue. The loop never
/// busy-spins: when nothing is runnable it waits (blocking the single worker
/// thread) until the earliest timer deadline.

#include "sere_rt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct SereAsyncWaker {
  void (*fn)(void* arg);
  void* arg;
} SereAsyncWaker;

// Ready queue (FIFO ring).
typedef struct SereAsyncQueue {
  SereAsyncWaker* items;
  int64_t cap;
  int64_t head;
  int64_t tail;
  int64_t count;
} SereAsyncQueue;

// Timer min-heap ordered by deadline (in milliseconds, GetTickCount64 epoch).
typedef struct SereAsyncTimer {
  int64_t deadline;
  SereAsyncWaker waker;
} SereAsyncTimer;

typedef struct SereAsyncState {
  SereAsyncQueue ready;
  SereAsyncTimer* timers;
  int64_t timer_cap;
  int64_t timer_len;
} SereAsyncState;

static SereAsyncState g_async;

static void queueInit(SereAsyncQueue* q) {
  q->cap = 64;
  q->items = (SereAsyncWaker*)malloc((size_t)q->cap * sizeof(SereAsyncWaker));
  q->head = 0;
  q->tail = 0;
  q->count = 0;
}

static void queueGrow(SereAsyncQueue* q) {
  int64_t newCap = q->cap * 2;
  SereAsyncWaker* items = (SereAsyncWaker*)malloc((size_t)newCap * sizeof(SereAsyncWaker));
  for (int64_t i = 0; i < q->count; ++i) {
    items[i] = q->items[(q->head + i) % q->cap];
  }
  free(q->items);
  q->items = items;
  q->cap = newCap;
  q->head = 0;
  q->tail = q->count;
}

static void queuePush(SereAsyncQueue* q, void (*fn)(void*), void* arg) {
  if (q->count == q->cap) {
    queueGrow(q);
  }
  q->items[q->tail] = (SereAsyncWaker){fn, arg};
  q->tail = (q->tail + 1) % q->cap;
  q->count += 1;
}

static int queuePop(SereAsyncQueue* q, SereAsyncWaker* out) {
  if (q->count == 0) {
    return 0;
  }
  *out = q->items[q->head];
  q->head = (q->head + 1) % q->cap;
  q->count -= 1;
  return 1;
}

static void timerSwap(SereAsyncTimer* a, SereAsyncTimer* b) {
  SereAsyncTimer tmp = *a;
  *a = *b;
  *b = tmp;
}

static void timerSiftUp(int64_t index) {
  while (index > 0) {
    int64_t parent = (index - 1) / 2;
    if (g_async.timers[parent].deadline <= g_async.timers[index].deadline) {
      break;
    }
    timerSwap(&g_async.timers[parent], &g_async.timers[index]);
    index = parent;
  }
}

static void timerSiftDown(int64_t index) {
  const int64_t len = g_async.timer_len;
  while (true) {
    int64_t left = index * 2 + 1;
    int64_t right = left + 1;
    int64_t smallest = index;
    if (left < len && g_async.timers[left].deadline < g_async.timers[smallest].deadline) {
      smallest = left;
    }
    if (right < len && g_async.timers[right].deadline < g_async.timers[smallest].deadline) {
      smallest = right;
    }
    if (smallest == index) {
      break;
    }
    timerSwap(&g_async.timers[index], &g_async.timers[smallest]);
    index = smallest;
  }
}

static void timerPush(int64_t deadline, void (*fn)(void*), void* arg) {
  if (g_async.timer_len == g_async.timer_cap) {
    int64_t newCap = g_async.timer_cap == 0 ? 64 : g_async.timer_cap * 2;
    g_async.timers =
        (SereAsyncTimer*)realloc(g_async.timers, (size_t)newCap * sizeof(SereAsyncTimer));
    g_async.timer_cap = newCap;
  }
  const int64_t index = g_async.timer_len++;
  g_async.timers[index] = (SereAsyncTimer){deadline, {fn, arg}};
  timerSiftUp(index);
}

static void timerPopEarliest(SereAsyncTimer* out) {
  *out = g_async.timers[0];
  g_async.timers[0] = g_async.timers[--g_async.timer_len];
  timerSiftDown(0);
}

static int64_t nowMs(void) {
#if defined(_WIN32)
  return (int64_t)GetTickCount64();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static void idleWaitUntil(int64_t deadline) {
  if (deadline < 0) {
    return;
  }
  const int64_t now = nowMs();
  int64_t delay = deadline - now;
  if (delay < 0) {
    delay = 0;
  }
#if defined(_WIN32)
  Sleep((DWORD)delay);
#else
  struct timespec ts;
  ts.tv_sec = delay / 1000;
  ts.tv_nsec = (delay % 1000) * 1000000;
  nanosleep(&ts, NULL);
#endif
}

/// Post a waker onto the ready queue. Safe to call from any generated task.
void sere_async_post(void (*fn)(void* arg), void* arg) {
  if (fn == NULL) {
    return;
  }
  queuePush(&g_async.ready, fn, arg);
}

/// Schedule a waker to fire after `ms` milliseconds. Does not block.
void sere_async_sleep_ms(void (*fn)(void* arg), void* arg, int64_t ms) {
  if (fn == NULL) {
    return;
  }
  if (ms <= 0) {
    sere_async_post(fn, arg);
    return;
  }
  timerPush(nowMs() + ms, fn, arg);
}

/// Run the executor until the ready queue and timer heap are both empty.
/// Wakers must not call this re-entrantly.
void sere_async_run(void) {
  if (g_async.ready.items == NULL) {
    queueInit(&g_async.ready);
  }
  while (true) {
    SereAsyncWaker waker;
    if (queuePop(&g_async.ready, &waker)) {
      waker.fn(waker.arg);
      continue;
    }
    if (g_async.timer_len == 0) {
      break;
    }
    // Nothing runnable: wait until the nearest timer fires, then re-schedule
    // any expired timers onto the ready queue.
    SereAsyncTimer first = g_async.timers[0];
    idleWaitUntil(first.deadline);
    const int64_t now = nowMs();
    while (g_async.timer_len > 0 && g_async.timers[0].deadline <= now) {
      timerPopEarliest(&first);
      queuePush(&g_async.ready, first.waker.fn, first.waker.arg);
    }
  }
}

/// Milliseconds of monotonic time. Exposed for sleep primitives.
int64_t sere_async_now_ms(void) {
  return nowMs();
}
