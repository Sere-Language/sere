/// Example custom collector. Build a static lib and link:
///   clang-cl /c examples/custom_gc.c /I include
///   sere examples/gc_mem.sere --link custom_gc.obj
/// sere_mod_init runs from the generated C main before Sere globals.

#include "sere/api/sere_gc.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int64_t g_bytes = 0;
static int64_t g_blocks = 0;

static void* countingAlloc(uint64_t size, void* ctx) {
  (void)ctx;
  const size_t bytes = size == 0 ? 1 : (size_t)size;
  void* pointer = calloc(1, bytes);
  if (pointer != NULL) {
    g_bytes += (int64_t)bytes;
    g_blocks += 1;
  }
  return pointer;
}

static void countingFree(void* pointer, void* ctx) {
  (void)ctx;
  free(pointer);
}

static void countingStats(SereGcStats* out, void* ctx) {
  (void)ctx;
  if (out == NULL) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->bytes_in_use = g_bytes;
  out->bytes_allocated = g_bytes;
  out->live_blocks = g_blocks;
}

void sere_mod_init(void) {
  static SereGcVTable table;
  memset(&table, 0, sizeof(table));
  table.name = "counting";
  table.alloc = countingAlloc;
  table.free = countingFree;
  table.stats = countingStats;
  sere_gc_install(&table);
}
