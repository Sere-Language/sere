/// @file sere_rt.c
/// Language runtime: heap, shared boxes, argv lists, and printing.

#include "sere_rt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int32_t refs;
  int32_t pad;
} SereSharedHeader;

/// Optional native hook. Directives live here because this object is always
/// extracted from sere_rt.lib. A fallback only in sere_mod.c never reached the
/// linker unless some Sere_Define* symbol was referenced.
void sere_mod_init_default(void) {}

#if defined(_MSC_VER)
#pragma comment(linker, "/include:sere_mod_init_default")
#pragma comment(linker, "/alternatename:sere_mod_init=sere_mod_init_default")
#else
__attribute__((weak)) void sere_mod_init(void) {}
#endif

void sere_write(const char* data, int64_t len) {
  if (data != NULL && len > 0) {
    fwrite(data, 1, (size_t)len, stdout);
  }
}

void sere_write_nl(void) { fputc('\n', stdout); }

void sere_print_str(const char* data, int64_t len) {
  sere_write(data, len);
  sere_write_nl();
}

void sere_write_i32(int32_t value) { fprintf(stdout, "%d", (int)value); }

void sere_write_i64(int64_t value) { fprintf(stdout, "%lld", (long long)value); }

void sere_write_bool(int8_t value) {
  if (value) {
    sere_write("True", 4);
  } else {
    sere_write("False", 5);
  }
}

void sere_write_ptr(const void* pointer) {
  if (pointer == NULL) {
    sere_write("None", 4);
    return;
  }
  fprintf(stdout, "0x%llx", (unsigned long long)(uintptr_t)pointer);
}

void sere_write_f64(double value) { fprintf(stdout, "%g", value); }

static const char* heapCopy(const char* text, size_t length, int64_t* out_len) {
  char* data = (char*)malloc(length + 1);
  if (data == NULL) {
    *out_len = 0;
    return "";
  }
  memcpy(data, text, length);
  data[length] = '\0';
  *out_len = (int64_t)length;
  return data;
}

const char* sere_str_i32_data(int32_t value, int64_t* out_len) {
  char buf[32];
  const int n = snprintf(buf, sizeof(buf), "%d", (int)value);
  return heapCopy(buf, n < 0 ? 0 : (size_t)n, out_len);
}

const char* sere_str_i64_data(int64_t value, int64_t* out_len) {
  char buf[32];
  const int n = snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return heapCopy(buf, n < 0 ? 0 : (size_t)n, out_len);
}

const char* sere_str_bool_data(int8_t value, int64_t* out_len) {
  if (value) {
    return heapCopy("True", 4, out_len);
  }
  return heapCopy("False", 5, out_len);
}

const char* sere_str_ptr_data(const void* pointer, int64_t* out_len) {
  if (pointer == NULL) {
    return heapCopy("None", 4, out_len);
  }
  char buf[32];
  const int n = snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)(uintptr_t)pointer);
  return heapCopy(buf, n < 0 ? 0 : (size_t)n, out_len);
}

const char* sere_str_f64_data(double value, int64_t* out_len) {
  char buf[64];
  const int n = snprintf(buf, sizeof(buf), "%g", value);
  return heapCopy(buf, n < 0 ? 0 : (size_t)n, out_len);
}

const char* sere_str_concat_data(const char* left, int64_t left_len, const char* right,
                                int64_t right_len, int64_t* out_len) {
  if (left_len < 0) {
    left_len = 0;
  }
  if (right_len < 0) {
    right_len = 0;
  }
  const size_t total = (size_t)left_len + (size_t)right_len;
  char* data = (char*)malloc(total + 1);
  if (data == NULL) {
    *out_len = 0;
    return "";
  }
  if (left != NULL && left_len > 0) {
    memcpy(data, left, (size_t)left_len);
  }
  if (right != NULL && right_len > 0) {
    memcpy(data + (size_t)left_len, right, (size_t)right_len);
  }
  data[total] = '\0';
  *out_len = (int64_t)total;
  return data;
}

void* sere_alloc(uint64_t size) { return sere_gc_alloc(size); }

void sere_free(void* pointer) { sere_gc_free(pointer); }

void* sere_shared_new(uint64_t size) {
  const size_t bytes = sizeof(SereSharedHeader) + (size_t)size;
  SereSharedHeader* header = (SereSharedHeader*)calloc(1, bytes);
  if (header == NULL) {
    return NULL;
  }
  header->refs = 1;
  return header + 1;
}

static SereSharedHeader* sharedHeader(void* payload) {
  return ((SereSharedHeader*)payload) - 1;
}

void sere_shared_retain(void* payload) {
  if (payload != NULL) {
    sharedHeader(payload)->refs += 1;
  }
}

void sere_shared_release(void* payload) {
  if (payload == NULL) {
    return;
  }
  SereSharedHeader* header = sharedHeader(payload);
  header->refs -= 1;
  if (header->refs <= 0) {
    free(header);
  }
}

int64_t sere_list_len(void* list) {
  const SereList* typed = (const SereList*)list;
  if (typed == NULL) {
    return 0;
  }
  return typed->len;
}

void* sere_list_new(int64_t stride) {
  SereList* list = (SereList*)calloc(1, sizeof(SereList));
  if (list == NULL) {
    return NULL;
  }
  list->stride = stride <= 0 ? 1 : stride;
  return list;
}

void* sere_array_new(int64_t stride, int64_t length) {
  SereList* list = (SereList*)sere_list_new(stride);
  if (list == NULL) {
    return NULL;
  }
  if (length < 0) {
    length = 0;
  }
  const size_t bytes = (size_t)length * (size_t)list->stride;
  list->data = bytes == 0 ? NULL : calloc((size_t)length, (size_t)list->stride);
  list->len = length;
  list->cap = length;
  return list;
}

void sere_list_push(void* list, const void* item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || item == NULL) {
    return;
  }
  if (typed->len >= typed->cap) {
    const int64_t cap = typed->cap == 0 ? 4 : typed->cap * 2;
    void* data = realloc(typed->data, (size_t)(cap * typed->stride));
    typed->data = data;
    typed->cap = cap;
  }
  memcpy((char*)typed->data + (size_t)(typed->len * typed->stride), item,
         (size_t)typed->stride);
  typed->len += 1;
}

void* sere_list_slice(void* list, int64_t start, int64_t end, int32_t has_start, int32_t has_end) {
  SereList* typed = (SereList*)list;
  if (typed == NULL) {
    return sere_list_new(1);
  }
  int64_t length = typed->len;
  int64_t begin = has_start ? start : 0;
  int64_t stop = has_end ? end : length;
  if (begin < 0) {
    begin += length;
  }
  if (stop < 0) {
    stop += length;
  }
  if (begin < 0) {
    begin = 0;
  }
  if (stop > length) {
    stop = length;
  }
  if (begin > stop) {
    begin = stop;
  }
  SereList* out = (SereList*)sere_list_new(typed->stride);
  for (int64_t index = begin; index < stop; ++index) {
    sere_list_push(out, (char*)typed->data + (size_t)(index * typed->stride));
  }
  return out;
}

void* sere_list_item(void* list, int64_t index) {
  SereList* typed = (SereList*)list;
  if (typed == NULL) {
    fprintf(stderr, "error: sequence index out of range\n");
    exit(1);
  }
  if (index < 0) {
    index += typed->len;
  }
  if (index < 0 || index >= typed->len) {
    fprintf(stderr, "error: sequence index out of range\n");
    exit(1);
  }
  return (char*)typed->data + (size_t)(index * typed->stride);
}

void* sere_list_from_argv(int argc, char** argv) {
  SereList* list = (SereList*)calloc(1, sizeof(SereList));
  if (list == NULL) {
    return NULL;
  }
  list->stride = (int64_t)sizeof(SereStr);
  for (int index = 0; index < argc; ++index) {
    SereStr item;
    item.data = argv[index] == NULL ? "" : argv[index];
    item.len = (int64_t)strlen(item.data);
    sere_list_push(list, &item);
  }
  return list;
}

enum {
  kDictKeyBytes = 0,
  kDictKeyI32 = 1,
  kDictKeyI64 = 2,
  kDictKeyStr = 3,
};

typedef struct {
  void* keys;
  void* vals;
  uint8_t* state;
  int64_t len;
  int64_t cap;
  int64_t key_stride;
  int64_t val_stride;
  int32_t key_kind;
} SereDict;

static uint64_t hashBytes(const char* data, size_t length) {
  uint64_t hash = 1469598103934665603ULL;
  for (size_t index = 0; index < length; ++index) {
    hash ^= (unsigned char)data[index];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static uint64_t hashKey(const SereDict* dict, const void* key) {
  if (dict->key_kind == kDictKeyI32) {
    return (uint64_t)(uint32_t)(*(const int32_t*)key);
  }
  if (dict->key_kind == kDictKeyI64) {
    return (uint64_t)(*(const int64_t*)key);
  }
  if (dict->key_kind == kDictKeyStr) {
    const SereStr* str = (const SereStr*)key;
    return hashBytes(str->data == NULL ? "" : str->data, (size_t)(str->len < 0 ? 0 : str->len));
  }
  return hashBytes((const char*)key, (size_t)dict->key_stride);
}

static int keysEqual(const SereDict* dict, const void* left, const void* right) {
  if (dict->key_kind == kDictKeyStr) {
    const SereStr* a = (const SereStr*)left;
    const SereStr* b = (const SereStr*)right;
    if (a->len != b->len) {
      return 0;
    }
    if (a->len <= 0) {
      return 1;
    }
    return memcmp(a->data, b->data, (size_t)a->len) == 0;
  }
  return memcmp(left, right, (size_t)dict->key_stride) == 0;
}

static void dictGrow(SereDict* dict) {
  const int64_t oldCap = dict->cap;
  void* oldKeys = dict->keys;
  void* oldVals = dict->vals;
  uint8_t* oldState = dict->state;
  dict->cap = oldCap == 0 ? 8 : oldCap * 2;
  dict->keys = calloc((size_t)dict->cap, (size_t)dict->key_stride);
  dict->vals = calloc((size_t)dict->cap, (size_t)dict->val_stride);
  dict->state = (uint8_t*)calloc((size_t)dict->cap, 1);
  dict->len = 0;
  for (int64_t index = 0; index < oldCap; ++index) {
    if (oldState != NULL && oldState[index] == 1) {
      sere_dict_set(dict, (char*)oldKeys + (size_t)(index * dict->key_stride),
                    (char*)oldVals + (size_t)(index * dict->val_stride));
    }
  }
  free(oldKeys);
  free(oldVals);
  free(oldState);
}

void* sere_dict_new(int64_t key_stride, int64_t val_stride, int32_t key_kind) {
  SereDict* dict = (SereDict*)calloc(1, sizeof(SereDict));
  if (dict == NULL) {
    return NULL;
  }
  dict->key_stride = key_stride <= 0 ? 1 : key_stride;
  dict->val_stride = val_stride <= 0 ? 1 : val_stride;
  dict->key_kind = key_kind;
  dictGrow(dict);
  return dict;
}

void sere_dict_set(void* dict, const void* key, const void* value) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL || key == NULL || value == NULL) {
    return;
  }
  if (typed->cap == 0 || typed->len * 2 >= typed->cap) {
    dictGrow(typed);
  }
  uint64_t slot = hashKey(typed, key) % (uint64_t)typed->cap;
  for (int64_t n = 0; n < typed->cap; ++n) {
    const int64_t index = (int64_t)((slot + (uint64_t)n) % (uint64_t)typed->cap);
    void* keySlot = (char*)typed->keys + (size_t)(index * typed->key_stride);
    if (typed->state[index] == 0) {
      memcpy(keySlot, key, (size_t)typed->key_stride);
      memcpy((char*)typed->vals + (size_t)(index * typed->val_stride), value,
             (size_t)typed->val_stride);
      typed->state[index] = 1;
      typed->len += 1;
      return;
    }
    if (keysEqual(typed, keySlot, key)) {
      memcpy((char*)typed->vals + (size_t)(index * typed->val_stride), value,
             (size_t)typed->val_stride);
      return;
    }
  }
}

int32_t sere_dict_get(void* dict, const void* key, void* out_value) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL || key == NULL || typed->cap == 0) {
    if (out_value != NULL && typed != NULL) {
      memset(out_value, 0, (size_t)typed->val_stride);
    }
    return 0;
  }
  uint64_t slot = hashKey(typed, key) % (uint64_t)typed->cap;
  for (int64_t n = 0; n < typed->cap; ++n) {
    const int64_t index = (int64_t)((slot + (uint64_t)n) % (uint64_t)typed->cap);
    if (typed->state[index] == 0) {
      break;
    }
    void* keySlot = (char*)typed->keys + (size_t)(index * typed->key_stride);
    if (keysEqual(typed, keySlot, key)) {
      if (out_value != NULL) {
        memcpy(out_value, (char*)typed->vals + (size_t)(index * typed->val_stride),
               (size_t)typed->val_stride);
      }
      return 1;
    }
  }
  if (out_value != NULL) {
    memset(out_value, 0, (size_t)typed->val_stride);
  }
  return 0;
}

int64_t sere_dict_len(void* dict) {
  const SereDict* typed = (const SereDict*)dict;
  return typed == NULL ? 0 : typed->len;
}

void sere_str_index(const char* data, int64_t len, int64_t index, const char** out_data,
                    int64_t* out_len) {
  if (index < 0) {
    index += len;
  }
  if (data == NULL || index < 0 || index >= len) {
    fprintf(stderr, "error: string index out of range\n");
    exit(1);
  }
  *out_data = heapCopy(data + index, 1, out_len);
}

void sere_str_slice(const char* data, int64_t len, int64_t start, int64_t end, int32_t has_start,
                    int32_t has_end, const char** out_data, int64_t* out_len) {
  int64_t begin = has_start ? start : 0;
  int64_t stop = has_end ? end : len;
  if (begin < 0) {
    begin += len;
  }
  if (stop < 0) {
    stop += len;
  }
  if (begin < 0) {
    begin = 0;
  }
  if (stop > len) {
    stop = len;
  }
  if (begin > stop) {
    begin = stop;
  }
  *out_data = heapCopy(data == NULL ? "" : data + begin, (size_t)(stop - begin), out_len);
}

int32_t sere_str_contains(const char* hay, int64_t hay_len, const char* needle, int64_t needle_len) {
  if (needle_len == 0) {
    return 1;
  }
  if (hay == NULL || needle == NULL || needle_len > hay_len) {
    return 0;
  }
  for (int64_t index = 0; index + needle_len <= hay_len; ++index) {
    if (memcmp(hay + index, needle, (size_t)needle_len) == 0) {
      return 1;
    }
  }
  return 0;
}

int32_t sere_str_eq(const char* left, int64_t left_len, const char* right, int64_t right_len) {
  if (left_len != right_len) {
    return 0;
  }
  if (left_len == 0) {
    return 1;
  }
  return memcmp(left, right, (size_t)left_len) == 0;
}

int32_t sere_str_cmp(const char* left, int64_t left_len, const char* right, int64_t right_len) {
  const int64_t n = left_len < right_len ? left_len : right_len;
  const int cmp = memcmp(left == NULL ? "" : left, right == NULL ? "" : right, (size_t)n);
  if (cmp != 0) {
    return cmp < 0 ? -1 : 1;
  }
  if (left_len == right_len) {
    return 0;
  }
  return left_len < right_len ? -1 : 1;
}

void sere_str_repeat(const char* data, int64_t len, int64_t count, const char** out_data,
                     int64_t* out_len) {
  if (count < 0) {
    count = 0;
  }
  const size_t total = (size_t)len * (size_t)count;
  char* copy = (char*)malloc(total + 1);
  if (copy == NULL) {
    *out_data = "";
    *out_len = 0;
    return;
  }
  for (int64_t n = 0; n < count; ++n) {
    if (data != NULL && len > 0) {
      memcpy(copy + (size_t)(n * len), data, (size_t)len);
    }
  }
  copy[total] = '\0';
  *out_data = copy;
  *out_len = (int64_t)total;
}

static int g_has_error = 0;
static char g_error_type[256];
static char g_error_message[1024];

void sere_raise(const char* type, const char* message, int64_t message_len) {
  g_has_error = 1;
  snprintf(g_error_type, sizeof(g_error_type), "%s", type == NULL ? "Error" : type);
  if (message == NULL || message_len <= 0) {
    g_error_message[0] = '\0';
    return;
  }
  if (message_len >= (int64_t)sizeof(g_error_message)) {
    message_len = (int64_t)sizeof(g_error_message) - 1;
  }
  memcpy(g_error_message, message, (size_t)message_len);
  g_error_message[message_len] = '\0';
}

int32_t sere_has_error(void) { return g_has_error; }

void sere_clear_error(void) { g_has_error = 0; }

int32_t sere_error_isa(const char* name) {
  if (!g_has_error) {
    return 0;
  }
  if (name == NULL || name[0] == '\0') {
    return 1;
  }
  const size_t want = strlen(name);
  const char* cursor = g_error_type;
  while (*cursor != '\0') {
    const char* start = cursor;
    while (*cursor != '\0' && *cursor != ';') {
      ++cursor;
    }
    if ((size_t)(cursor - start) == want && memcmp(start, name, want) == 0) {
      return 1;
    }
    if (*cursor == ';') {
      ++cursor;
    }
  }
  return 0;
}

const char* sere_error_type(void) { return g_error_type; }

const char* sere_error_message(int64_t* out_len) {
  const size_t n = strlen(g_error_message);
  if (out_len != NULL) {
    *out_len = (int64_t)n;
  }
  return g_error_message;
}

void sere_panic(const char* message, int64_t len) {
  fprintf(stderr, "panic: ");
  if (message != NULL && len > 0) {
    fwrite(message, 1, (size_t)len, stderr);
  }
  fputc('\n', stderr);
  abort();
}

int32_t sere_list_contains_i32(void* list, int32_t value) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || typed->stride != 4) {
    return 0;
  }
  for (int64_t index = 0; index < typed->len; ++index) {
    int32_t item = 0;
    memcpy(&item, (char*)typed->data + (size_t)(index * typed->stride), 4);
    if (item == value) {
      return 1;
    }
  }
  return 0;
}

int32_t sere_list_contains(void* list, const void* item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || item == NULL || typed->data == NULL) {
    return 0;
  }
  for (int64_t index = 0; index < typed->len; ++index) {
    if (memcmp((char*)typed->data + (size_t)(index * typed->stride), item,
               (size_t)typed->stride) == 0) {
      return 1;
    }
  }
  return 0;
}

void* sere_list_concat(void* left, void* right) {
  SereList* lhs = (SereList*)left;
  SereList* rhs = (SereList*)right;
  const int64_t stride = lhs != NULL ? lhs->stride : (rhs != NULL ? rhs->stride : 1);
  SereList* out = (SereList*)sere_list_new(stride);
  if (lhs != NULL) {
    for (int64_t index = 0; index < lhs->len; ++index) {
      sere_list_push(out, (char*)lhs->data + (size_t)(index * lhs->stride));
    }
  }
  if (rhs != NULL) {
    for (int64_t index = 0; index < rhs->len; ++index) {
      sere_list_push(out, (char*)rhs->data + (size_t)(index * rhs->stride));
    }
  }
  return out;
}
