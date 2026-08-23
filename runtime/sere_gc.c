/// @file sere_gc.c
/// Default and pluggable collectors: none, mark-sweep, and arena.

#include "sere/api/sere_gc.h"
#include "sere_rt.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { kGcMagic = 0x53455245u, kGcMarked = 1u };

typedef struct GcBlock {
  uint32_t magic;
  uint32_t flags;
  uint64_t size;
  struct GcBlock* next;
} GcBlock;

typedef struct ArenaSlab {
  struct ArenaSlab* next;
  uint64_t cap;
  uint64_t used;
  uint8_t data[1];
} ArenaSlab;

typedef struct ArenaState {
  ArenaSlab* slabs;
  uint64_t next_cap;
} ArenaState;

typedef struct PoolFree {
  struct PoolFree* next;
} PoolFree;

typedef struct PoolState {
  uint64_t block_size;
  uint8_t* storage;
  PoolFree* free_list;
  int64_t blocks;
} PoolState;

static SereGcVTable g_table;
static int g_installed = 0;
static SereGcStats g_stats;
static GcBlock* g_blocks = NULL;
static void** g_roots = NULL;
static int64_t g_root_len = 0;
static int64_t g_root_cap = 0;
static ArenaState g_arena;

static void* rawAlloc(uint64_t size) {
  return calloc(1, size == 0 ? 1 : (size_t)size);
}

static void bumpStatsAlloc(uint64_t size) {
  g_stats.bytes_in_use += (int64_t)size;
  g_stats.bytes_allocated += (int64_t)size;
  g_stats.live_blocks += 1;
}

static void bumpStatsFree(uint64_t size) {
  g_stats.bytes_in_use -= (int64_t)size;
  if (g_stats.bytes_in_use < 0) {
    g_stats.bytes_in_use = 0;
  }
  if (g_stats.live_blocks > 0) {
    g_stats.live_blocks -= 1;
  }
}

static GcBlock* asBlock(void* pointer) {
  if (pointer == NULL) {
    return NULL;
  }
  GcBlock* block = ((GcBlock*)pointer) - 1;
  if (block->magic != kGcMagic) {
    return NULL;
  }
  return block;
}

static void* blockAlloc(uint64_t size, int32_t link) {
  GcBlock* block = (GcBlock*)rawAlloc(sizeof(GcBlock) + size);
  if (block == NULL) {
    return NULL;
  }
  block->magic = kGcMagic;
  block->flags = 0;
  block->size = size;
  if (link != 0) {
    block->next = g_blocks;
    g_blocks = block;
  }
  bumpStatsAlloc(size);
  return block + 1;
}

static void unlinkBlock(GcBlock* target) {
  GcBlock** cursor = &g_blocks;
  while (*cursor != NULL) {
    if (*cursor == target) {
      *cursor = target->next;
      return;
    }
    cursor = &(*cursor)->next;
  }
}

static void blockFree(void* pointer, int32_t linked) {
  GcBlock* block = asBlock(pointer);
  if (block == NULL) {
    free(pointer);
    return;
  }
  if (linked != 0) {
    unlinkBlock(block);
  }
  bumpStatsFree(block->size);
  block->magic = 0;
  free(block);
}

static void noneCollect(void* ctx) { (void)ctx; }

static void copyStats(SereGcStats* out, void* ctx) {
  (void)ctx;
  if (out != NULL) {
    *out = g_stats;
  }
}

static void* noneAlloc(uint64_t size, void* ctx) {
  (void)ctx;
  return blockAlloc(size, 0);
}

static void noneFree(void* pointer, void* ctx) {
  (void)ctx;
  blockFree(pointer, 0);
}

static void* sweepAlloc(uint64_t size, void* ctx) {
  (void)ctx;
  return blockAlloc(size, 1);
}

static void sweepFree(void* pointer, void* ctx) {
  (void)ctx;
  blockFree(pointer, 1);
}

static GcBlock* blockContaining(const void* pointer) {
  for (GcBlock* block = g_blocks; block != NULL; block = block->next) {
    const uint8_t* start = (const uint8_t*)(block + 1);
    const uint8_t* end = start + block->size;
    const uint8_t* p = (const uint8_t*)pointer;
    if (p >= start && p < end) {
      return block;
    }
  }
  return NULL;
}

static void markPointer(void* pointer) {
  GcBlock* block = asBlock(pointer);
  if (block == NULL) {
    block = blockContaining(pointer);
  }
  if (block != NULL) {
    block->flags |= kGcMarked;
  }
}

static int32_t markPayload(GcBlock* block) {
  int32_t marked_new = 0;
  uint8_t* data = (uint8_t*)(block + 1);
  const uint64_t words = block->size / sizeof(void*);
  for (uint64_t index = 0; index < words; ++index) {
    void* word = NULL;
    memcpy(&word, data + index * sizeof(void*), sizeof(void*));
    GcBlock* found = asBlock(word);
    if (found == NULL) {
      found = blockContaining(word);
    }
    if (found != NULL && (found->flags & kGcMarked) == 0) {
      found->flags |= kGcMarked;
      marked_new = 1;
    }
  }
  return marked_new;
}

static void sweepCollect(void* ctx) {
  (void)ctx;
  if (g_root_len == 0) {
    return;
  }
  for (GcBlock* block = g_blocks; block != NULL; block = block->next) {
    block->flags &= ~kGcMarked;
  }
  for (int64_t index = 0; index < g_root_len; ++index) {
    markPointer(g_roots[index]);
  }
  int32_t grew = 1;
  while (grew != 0) {
    grew = 0;
    for (GcBlock* block = g_blocks; block != NULL; block = block->next) {
      if ((block->flags & kGcMarked) != 0 && markPayload(block) != 0) {
        grew = 1;
      }
    }
  }
  GcBlock* block = g_blocks;
  while (block != NULL) {
    GcBlock* next = block->next;
    if ((block->flags & kGcMarked) == 0) {
      unlinkBlock(block);
      bumpStatsFree(block->size);
      block->magic = 0;
      free(block);
    }
    block = next;
  }
  g_stats.collections += 1;
}

static ArenaSlab* newSlab(uint64_t cap) {
  if (cap < 4096) {
    cap = 4096;
  }
  ArenaSlab* slab = (ArenaSlab*)rawAlloc(sizeof(ArenaSlab) + cap);
  if (slab == NULL) {
    return NULL;
  }
  slab->cap = cap;
  slab->used = 0;
  return slab;
}

static void* arenaAlloc(uint64_t size, void* ctx) {
  (void)ctx;
  uint64_t need = size + 16;
  need = (need + 15u) & ~15u;
  if (g_arena.slabs == NULL || g_arena.slabs->used + need > g_arena.slabs->cap) {
    uint64_t cap = g_arena.next_cap == 0 ? 65536 : g_arena.next_cap;
    if (cap < need) {
      cap = need;
    }
    ArenaSlab* slab = newSlab(cap);
    if (slab == NULL) {
      return NULL;
    }
    slab->next = g_arena.slabs;
    g_arena.slabs = slab;
    g_arena.next_cap = cap * 2;
  }
  void* pointer = g_arena.slabs->data + g_arena.slabs->used;
  g_arena.slabs->used += need;
  bumpStatsAlloc(size);
  return pointer;
}

static void arenaFree(void* pointer, void* ctx) {
  (void)pointer;
  (void)ctx;
}

static void arenaCollect(void* ctx) {
  (void)ctx;
  ArenaSlab* slab = g_arena.slabs;
  while (slab != NULL) {
    ArenaSlab* next = slab->next;
    free(slab);
    slab = next;
  }
  g_arena.slabs = NULL;
  g_arena.next_cap = 65536;
  g_stats.bytes_in_use = 0;
  g_stats.live_blocks = 0;
  g_stats.collections += 1;
}

static void arenaShutdown(void* ctx) { arenaCollect(ctx); }

static void installTable(const SereGcVTable* table) {
  if (g_installed != 0 && g_table.shutdown != NULL) {
    g_table.shutdown(g_table.ctx);
  }
  g_table = *table;
  g_installed = 1;
  g_stats.collections = 0;
}

static SereGcVTable makeNone(void) {
  SereGcVTable table;
  memset(&table, 0, sizeof(table));
  table.name = "none";
  table.alloc = noneAlloc;
  table.free = noneFree;
  table.collect = noneCollect;
  table.stats = copyStats;
  return table;
}

static SereGcVTable makeSweep(void) {
  SereGcVTable table = makeNone();
  table.name = "mark_sweep";
  table.alloc = sweepAlloc;
  table.free = sweepFree;
  table.collect = sweepCollect;
  return table;
}

static SereGcVTable makeArena(void) {
  SereGcVTable table;
  memset(&table, 0, sizeof(table));
  table.name = "arena";
  table.alloc = arenaAlloc;
  table.free = arenaFree;
  table.collect = arenaCollect;
  table.stats = copyStats;
  table.shutdown = arenaShutdown;
  return table;
}

static void ensureInstalled(void) {
  if (g_installed == 0) {
    const SereGcVTable table = makeNone();
    installTable(&table);
  }
}

void sere_gc_install(const SereGcVTable* table) {
  if (table == NULL || table->alloc == NULL) {
    return;
  }
  installTable(table);
}

const SereGcVTable* sere_gc_current(void) {
  ensureInstalled();
  return &g_table;
}

void* sere_gc_alloc(uint64_t size) {
  ensureInstalled();
  return g_table.alloc(size, g_table.ctx);
}

void sere_gc_free(void* pointer) {
  ensureInstalled();
  if (g_table.free != NULL) {
    g_table.free(pointer, g_table.ctx);
  }
}

void sere_gc_collect(void) {
  ensureInstalled();
  if (g_table.collect != NULL) {
    g_table.collect(g_table.ctx);
  }
}

void sere_gc_stats(SereGcStats* out) {
  ensureInstalled();
  if (g_table.stats != NULL) {
    g_table.stats(out, g_table.ctx);
    return;
  }
  copyStats(out, NULL);
}

void sere_gc_name(const char** out_data, int64_t* out_len) {
  ensureInstalled();
  const char* name = g_table.name == NULL ? "none" : g_table.name;
  if (out_data != NULL) {
    *out_data = name;
  }
  if (out_len != NULL) {
    *out_len = (int64_t)strlen(name);
  }
}

int32_t sere_gc_use(const char* name, int64_t name_len) {
  if (name == NULL || name_len <= 0) {
    return 0;
  }
  if (name_len == 4 && memcmp(name, "none", 4) == 0) {
    const SereGcVTable table = makeNone();
    installTable(&table);
    return 1;
  }
  if (name_len == 10 && memcmp(name, "mark_sweep", 10) == 0) {
    const SereGcVTable table = makeSweep();
    installTable(&table);
    return 1;
  }
  if (name_len == 5 && memcmp(name, "arena", 5) == 0) {
    const SereGcVTable table = makeArena();
    installTable(&table);
    return 1;
  }
  return 0;
}

void sere_gc_add_root(void* pointer) {
  if (pointer == NULL) {
    return;
  }
  if (g_root_len == g_root_cap) {
    const int64_t cap = g_root_cap == 0 ? 8 : g_root_cap * 2;
    void** grown = (void**)realloc(g_roots, (size_t)cap * sizeof(void*));
    if (grown == NULL) {
      return;
    }
    g_roots = grown;
    g_root_cap = cap;
  }
  g_roots[g_root_len] = pointer;
  g_root_len += 1;
}

void sere_gc_remove_root(void* pointer) {
  for (int64_t index = 0; index < g_root_len; ++index) {
    if (g_roots[index] == pointer) {
      g_roots[index] = g_roots[g_root_len - 1];
      g_root_len -= 1;
      return;
    }
  }
}

void sere_gc_retain(void* pointer) {
  ensureInstalled();
  if (g_table.retain != NULL) {
    g_table.retain(pointer, g_table.ctx);
  }
}

void sere_gc_release(void* pointer) {
  ensureInstalled();
  if (g_table.release != NULL) {
    g_table.release(pointer, g_table.ctx);
  }
}

int64_t sere_gc_bytes_in_use(void) {
  SereGcStats stats;
  memset(&stats, 0, sizeof(stats));
  sere_gc_stats(&stats);
  return stats.bytes_in_use;
}

int64_t sere_gc_bytes_allocated(void) {
  SereGcStats stats;
  memset(&stats, 0, sizeof(stats));
  sere_gc_stats(&stats);
  return stats.bytes_allocated;
}

int64_t sere_gc_live_blocks(void) {
  SereGcStats stats;
  memset(&stats, 0, sizeof(stats));
  sere_gc_stats(&stats);
  return stats.live_blocks;
}

int64_t sere_gc_collections(void) {
  SereGcStats stats;
  memset(&stats, 0, sizeof(stats));
  sere_gc_stats(&stats);
  return stats.collections;
}

void* sere_arena_new(int64_t cap) {
  ArenaState* arena = (ArenaState*)rawAlloc(sizeof(ArenaState));
  if (arena == NULL) {
    return NULL;
  }
  arena->next_cap = cap < 4096 ? 4096 : (uint64_t)cap;
  return arena;
}

void* sere_arena_alloc(void* arena, int64_t size) {
  ArenaState* state = (ArenaState*)arena;
  if (state == NULL || size < 0) {
    return NULL;
  }
  void* saved = g_arena.slabs;
  uint64_t saved_cap = g_arena.next_cap;
  g_arena.slabs = state->slabs;
  g_arena.next_cap = state->next_cap;
  void* pointer = arenaAlloc((uint64_t)size, NULL);
  state->slabs = g_arena.slabs;
  state->next_cap = g_arena.next_cap;
  g_arena.slabs = saved;
  g_arena.next_cap = saved_cap;
  return pointer;
}

void sere_arena_reset(void* arena) {
  ArenaState* state = (ArenaState*)arena;
  if (state == NULL) {
    return;
  }
  ArenaSlab* slab = state->slabs;
  while (slab != NULL) {
    ArenaSlab* next = slab->next;
    free(slab);
    slab = next;
  }
  state->slabs = NULL;
}

void sere_arena_destroy(void* arena) {
  sere_arena_reset(arena);
  free(arena);
}

void* sere_pool_new(int64_t block_size, int64_t blocks) {
  if (block_size < (int64_t)sizeof(PoolFree) || blocks <= 0) {
    return NULL;
  }
  PoolState* pool = (PoolState*)rawAlloc(sizeof(PoolState));
  if (pool == NULL) {
    return NULL;
  }
  pool->block_size = (uint64_t)block_size;
  pool->blocks = blocks;
  pool->storage = (uint8_t*)rawAlloc((uint64_t)block_size * (uint64_t)blocks);
  if (pool->storage == NULL) {
    free(pool);
    return NULL;
  }
  for (int64_t index = blocks - 1; index >= 0; --index) {
    PoolFree* node = (PoolFree*)(pool->storage + (uint64_t)index * pool->block_size);
    node->next = pool->free_list;
    pool->free_list = node;
  }
  return pool;
}

void* sere_pool_alloc(void* pool) {
  PoolState* state = (PoolState*)pool;
  if (state == NULL || state->free_list == NULL) {
    return NULL;
  }
  PoolFree* node = state->free_list;
  state->free_list = node->next;
  memset(node, 0, (size_t)state->block_size);
  return node;
}

void sere_pool_release(void* pool, void* pointer) {
  PoolState* state = (PoolState*)pool;
  if (state == NULL || pointer == NULL) {
    return;
  }
  PoolFree* node = (PoolFree*)pointer;
  node->next = state->free_list;
  state->free_list = node;
}

void sere_pool_destroy(void* pool) {
  PoolState* state = (PoolState*)pool;
  if (state == NULL) {
    return;
  }
  free(state->storage);
  free(state);
}
