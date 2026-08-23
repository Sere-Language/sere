/// @file sere_gc.h
/// Pluggable garbage collector and heap ABI.
///
/// Install a builtin from Sere (`import gc; gc.use("mark_sweep")`) or deploy a
/// custom collector from C:
///
///   static void* my_alloc(uint64_t size, void* ctx) { return calloc(1, size); }
///   static void my_free(void* pointer, void* ctx) { free(pointer); }
///   static SereGcVTable gc = {"mine", my_alloc, my_free, NULL, NULL, NULL, NULL, NULL, NULL};
///   extern "C" void sere_mod_init(void) { sere_gc_install(&gc); }
///
/// Link with: sere main.sere --link my_gc.lib
/// Install the collector before the program allocates.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SereGcStats {
  int64_t bytes_in_use;
  int64_t bytes_allocated;
  int64_t live_blocks;
  int64_t collections;
} SereGcStats;

typedef void* (*SereGcAllocFn)(uint64_t size, void* ctx);
typedef void (*SereGcFreeFn)(void* pointer, void* ctx);
typedef void (*SereGcCollectFn)(void* ctx);
typedef void (*SereGcRetainFn)(void* pointer, void* ctx);
typedef void (*SereGcReleaseFn)(void* pointer, void* ctx);
typedef void (*SereGcStatsFn)(SereGcStats* out, void* ctx);
typedef void (*SereGcShutdownFn)(void* ctx);

typedef struct SereGcVTable {
  const char* name;
  SereGcAllocFn alloc;
  SereGcFreeFn free;
  SereGcCollectFn collect;
  SereGcRetainFn retain;
  SereGcReleaseFn release;
  SereGcStatsFn stats;
  SereGcShutdownFn shutdown;
  void* ctx;
} SereGcVTable;

void sere_gc_install(const SereGcVTable* table);
const SereGcVTable* sere_gc_current(void);
void sere_gc_collect(void);
void sere_gc_stats(SereGcStats* out);
void sere_gc_name(const char** out_data, int64_t* out_len);
int32_t sere_gc_use(const char* name, int64_t name_len);
void sere_gc_add_root(void* pointer);
void sere_gc_remove_root(void* pointer);
void sere_gc_retain(void* pointer);
void sere_gc_release(void* pointer);
int64_t sere_gc_bytes_in_use(void);
int64_t sere_gc_bytes_allocated(void);
int64_t sere_gc_live_blocks(void);
int64_t sere_gc_collections(void);

void* sere_arena_new(int64_t cap);
void* sere_arena_alloc(void* arena, int64_t size);
void sere_arena_reset(void* arena);
void sere_arena_destroy(void* arena);
void* sere_pool_new(int64_t block_size, int64_t blocks);
void* sere_pool_alloc(void* pool);
void sere_pool_release(void* pool, void* pointer);
void sere_pool_destroy(void* pool);

#ifdef __cplusplus
}
#endif
