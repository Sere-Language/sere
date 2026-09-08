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
void sere_mod_init(void) __attribute__((weak, alias("sere_mod_init_default")));
#endif

void sere_write(const char* data, int64_t len) {
  if (data != NULL && len > 0) {
    fwrite(data, 1, (size_t)len, stdout);
  }
}


const char* sere_input(const char* prompt, int64_t len) {
  sere_write(prompt, len);
  char* buffer = (char*)malloc(1024);
  if (buffer == NULL) {
    return NULL;
  }
  fgets(buffer, 1024, stdin);
  return buffer;
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

const char* sere_str_repr_data(const char* data, int64_t len, int64_t* out_len) {
  if (data == NULL || len < 0) len = 0;
  char quote = '\'';
  if (len > 0 && memchr(data, '\'', (size_t)len) != NULL &&
      memchr(data, '"', (size_t)len) == NULL) quote = '"';
  char* out = (char*)malloc((size_t)len * 4 + 3);
  if (out == NULL) { *out_len = 0; return ""; }
  size_t n = 0;
  out[n++] = quote;
  for (int64_t i = 0; i < len; ++i) {
    const unsigned char ch = (unsigned char)data[i];
    if (ch == (unsigned char)quote || ch == '\\') {
      out[n++] = '\\'; out[n++] = (char)ch;
    } else if (ch == '\n' || ch == '\r' || ch == '\t') {
      out[n++] = '\\'; out[n++] = ch == '\n' ? 'n' : ch == '\r' ? 'r' : 't';
    } else if (ch < 32 || ch == 127) {
      static const char hex[] = "0123456789abcdef";
      out[n++] = '\\'; out[n++] = 'x';
      out[n++] = hex[ch >> 4]; out[n++] = hex[ch & 15];
    } else {
      out[n++] = (char)ch;
    }
  }
  out[n++] = quote;
  out[n] = 0;
  *out_len = (int64_t)n;
  return out;
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

void sere_list_remove(void* list, int64_t index) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || typed->data == NULL || index < 0 || index >= typed->len) {
    return;
  }
  if (index + 1 < typed->len) {
    memmove((char*)typed->data + (size_t)(index * typed->stride),
            (char*)typed->data + (size_t)((index + 1) * typed->stride),
            (size_t)((typed->len - index - 1) * typed->stride));
  }
  typed->len -= 1;
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
  int64_t* order;
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
  int64_t* oldOrder = dict->order;
  const int64_t oldLen = dict->len;
  dict->cap = oldCap == 0 ? 8 : oldCap * 2;
  dict->keys = calloc((size_t)dict->cap, (size_t)dict->key_stride);
  dict->vals = calloc((size_t)dict->cap, (size_t)dict->val_stride);
  dict->state = (uint8_t*)calloc((size_t)dict->cap, 1);
  dict->order = (int64_t*)calloc((size_t)dict->cap, sizeof(int64_t));
  dict->len = 0;
  for (int64_t position = 0; position < oldLen; ++position) {
    const int64_t index = oldOrder[position];
    if (oldState != NULL && oldState[index] == 1) {
      sere_dict_set(dict, (char*)oldKeys + (size_t)(index * dict->key_stride),
                    (char*)oldVals + (size_t)(index * dict->val_stride));
    }
  }
  free(oldKeys);
  free(oldVals);
  free(oldState);
  free(oldOrder);
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
  int64_t tombstone = -1;
  for (int64_t n = 0; n < typed->cap; ++n) {
    const int64_t index = (int64_t)((slot + (uint64_t)n) % (uint64_t)typed->cap);
    void* keySlot = (char*)typed->keys + (size_t)(index * typed->key_stride);
    if (typed->state[index] == 0) {
      const int64_t dest = tombstone >= 0 ? tombstone : index;
      void* destKey = (char*)typed->keys + (size_t)(dest * typed->key_stride);
      memcpy(destKey, key, (size_t)typed->key_stride);
      memcpy((char*)typed->vals + (size_t)(dest * typed->val_stride), value,
             (size_t)typed->val_stride);
      typed->state[dest] = 1;
      typed->order[typed->len] = dest;
      typed->len += 1;
      return;
    }
    if (typed->state[index] == 2) {
      if (tombstone < 0) {
        tombstone = index;
      }
      continue;
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
    if (typed->state[index] == 2) {
      continue;
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

int32_t sere_dict_del(void* dict, const void* key) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL || key == NULL || typed->cap == 0) {
    return 0;
  }
  uint64_t slot = hashKey(typed, key) % (uint64_t)typed->cap;
  for (int64_t n = 0; n < typed->cap; ++n) {
    const int64_t index = (int64_t)((slot + (uint64_t)n) % (uint64_t)typed->cap);
    if (typed->state[index] == 0) {
      return 0;
    }
    if (typed->state[index] == 2) {
      continue;
    }
    void* keySlot = (char*)typed->keys + (size_t)(index * typed->key_stride);
    if (keysEqual(typed, keySlot, key)) {
      typed->state[index] = 2;
      for (int64_t position = 0; position < typed->len; ++position) {
        if (typed->order[position] == index) {
          memmove(typed->order + position, typed->order + position + 1,
              (size_t)(typed->len - position - 1) * sizeof(int64_t));
          break;
        }
      }
      typed->len -= 1;
      return 1;
    }
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

// Pending exceptions and handled exceptions have separate lifetimes. A handler
// keeps its exception alive while nested calls and nested handlers run.
typedef struct SereError {
  int refs;
  char* type;
  char* message;
  int64_t message_len;
  void* object;
  int64_t object_size;
} SereError;
typedef struct SereErrorFrame {
  SereError* error;
  struct SereErrorFrame* previous;
} SereErrorFrame;
static _Thread_local SereError* pendingError;
static _Thread_local SereErrorFrame* handledErrors;

static void releaseError(SereError* error) {
  if (error != NULL && --error->refs == 0) {
    free(error->type);
    free(error->message);
    free(error->object);
    free(error);
  }
}

void sere_clear_error(void) {
  releaseError(pendingError);
  pendingError = NULL;
}

void sere_raise(const char* type, const char* message, int64_t message_len) {
  SereError* error = (SereError*)calloc(1, sizeof(SereError));
  if (error == NULL)
    abort();
  error->refs = 1;
  error->type = (char*)malloc(strlen(type == NULL ? "Exception" : type) + 1);
  if (error->type == NULL)
    abort();
  memcpy(error->type,
         type == NULL ? "Exception" : type,
         strlen(type == NULL ? "Exception" : type) + 1);
  error->message_len = message != NULL && message_len > 0 ? message_len : 0;
  error->message = (char*)malloc((size_t)error->message_len + 1);
  if (error->message == NULL)
    abort();
  memcpy(error->message, message == NULL ? "" : message, (size_t)error->message_len);
  error->message[error->message_len] = 0;
  sere_clear_error();
  pendingError = error;
}

void sere_error_set_object(const void* object, int64_t size) {
  if (pendingError == NULL || size <= 0)
    return;
  pendingError->object = malloc((size_t)size);
  if (pendingError->object == NULL)
    abort();
  memcpy(pendingError->object, object, (size_t)size);
  pendingError->object_size = size;
}

void sere_error_copy_object(void* object, int64_t size) {
  if (pendingError != NULL && pendingError->object != NULL && size <= pendingError->object_size) {
    memcpy(object, pendingError->object, (size_t)size);
  }
}

int32_t sere_has_error(void) {
  return pendingError != NULL;
}

int32_t sere_error_isa(const char* name) {
  if (pendingError == NULL)
    return 0;
  if (name == NULL || name[0] == '\0')
    return 1;
  const size_t want = strlen(name);
  const char* cursor = pendingError->type;
  while (*cursor != '\0') {
    const char* start = cursor;
    while (*cursor != '\0' && *cursor != ';')
      ++cursor;
    if ((size_t)(cursor - start) == want && memcmp(start, name, want) == 0)
      return 1;
    if (*cursor == ';')
      ++cursor;
  }
  return 0;
}

const char* sere_error_type(void) {
  return pendingError == NULL ? "" : pendingError->type;
}

const char* sere_error_message(int64_t* out_len) {
  if (out_len != NULL)
    *out_len = pendingError == NULL ? 0 : pendingError->message_len;
  return pendingError == NULL ? "" : pendingError->message;
}

void sere_error_enter(void) {
  SereErrorFrame* frame = (SereErrorFrame*)malloc(sizeof(SereErrorFrame));
  if (frame == NULL)
    abort();
  frame->error = pendingError;
  frame->previous = handledErrors;
  handledErrors = frame;
  pendingError = NULL;
}

void sere_error_leave(int32_t restore) {
  SereErrorFrame* frame = handledErrors;
  if (frame == NULL)
    return;
  handledErrors = frame->previous;
  if (restore && pendingError == NULL) {
    pendingError = frame->error;
  } else {
    releaseError(frame->error);
  }
  free(frame);
}

void sere_reraise(void) {
  for (SereErrorFrame* frame = handledErrors; frame != NULL; frame = frame->previous) {
    if (frame->error != NULL) {
      ++frame->error->refs;
      sere_clear_error();
      pendingError = frame->error;
      return;
    }
  }
  const char message[] = "no active exception to reraise";
  sere_raise("RuntimeError;Exception", message, sizeof(message) - 1);
}

void sere_error_unhandled(void) {
  if (pendingError == NULL)
    return;
  const char* end = strchr(pendingError->type, ';');
  fprintf(stderr,
          "%.*s: ",
          (int)(end == NULL ? strlen(pendingError->type) : (size_t)(end - pendingError->type)),
          pendingError->type);
  fwrite(pendingError->message, 1, (size_t)pendingError->message_len, stderr);
  fputc('\n', stderr);
  exit(1);
}

static int sere_is_space(char character) {
  return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

static int sere_digit_value(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

static void sere_strip_span(const char** data, int64_t* len) {
  const char* text = *data;
  int64_t size = *len;
  while (size > 0 && sere_is_space(text[0])) {
    text += 1;
    size -= 1;
  }
  while (size > 0 && sere_is_space(text[size - 1])) {
    size -= 1;
  }
  *data = text;
  *len = size;
}

static int sere_span_eq(const char* data, int64_t len, const char* expected) {
  const size_t expected_len = strlen(expected);
  return len == (int64_t)expected_len && memcmp(data, expected, expected_len) == 0;
}

int32_t sere_parse_int(const char* data, int64_t len, int32_t bits, int32_t is_signed, int64_t* out) {
  if (data == NULL || out == NULL || bits <= 0 || bits > 64) {
    return 0;
  }
  sere_strip_span(&data, &len);
  if (len <= 0) {
    return 0;
  }
  int negative = 0;
  if (data[0] == '+' || data[0] == '-') {
    negative = data[0] == '-';
    data += 1;
    len -= 1;
  }
  if (is_signed == 0 && negative) {
    return 0;
  }
  int base = 10;
  if (len >= 2 && data[0] == '0') {
    const char prefix = data[1];
    if (prefix == 'x' || prefix == 'X') {
      base = 16;
      data += 2;
      len -= 2;
    } else if (prefix == 'b' || prefix == 'B') {
      base = 2;
      data += 2;
      len -= 2;
    } else if (prefix == 'o' || prefix == 'O') {
      base = 8;
      data += 2;
      len -= 2;
    }
  }
  uint64_t value = 0;
  int saw_digit = 0;
  int last_underscore = 0;
  for (int64_t index = 0; index < len; ++index) {
    const char character = data[index];
    if (character == '_') {
      if (saw_digit == 0 || last_underscore != 0) {
        return 0;
      }
      last_underscore = 1;
      continue;
    }
    const int digit = sere_digit_value(character);
    if (digit < 0 || digit >= base) {
      return 0;
    }
    if (value > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base) {
      return 0;
    }
    value = value * (uint64_t)base + (uint64_t)digit;
    saw_digit = 1;
    last_underscore = 0;
  }
  if (saw_digit == 0 || last_underscore != 0) {
    return 0;
  }
  if (is_signed != 0) {
    const uint64_t limit = bits == 64 ? (uint64_t)INT64_MAX + 1ull : (1ull << (bits - 1));
    if (negative) {
      if (value > limit) {
        return 0;
      }
      *out = value == limit ? (int64_t)((uint64_t)1 << 63) : -(int64_t)value;
      return 1;
    }
    if (value >= limit) {
      return 0;
    }
    *out = (int64_t)value;
    return 1;
  }
  const uint64_t umax = bits == 64 ? UINT64_MAX : ((1ull << bits) - 1ull);
  if (value > umax) {
    return 0;
  }
  *out = (int64_t)value;
  return 1;
}

int32_t sere_parse_float(const char* data, int64_t len, int32_t is_f32, double* out) {
  if (data == NULL || out == NULL) {
    return 0;
  }
  sere_strip_span(&data, &len);
  if (len <= 0 || len >= 128) {
    return 0;
  }
  char cleaned[128];
  size_t used = 0;
  int last_underscore = 0;
  int saw_digit = 0;
  for (int64_t index = 0; index < len; ++index) {
    const char character = data[index];
    if (character == '_') {
      if (saw_digit == 0 || last_underscore != 0) {
        return 0;
      }
      last_underscore = 1;
      continue;
    }
    if ((character >= '0' && character <= '9') || character == '.' || character == '+' ||
        character == '-' || character == 'e' || character == 'E' || character == 'f' ||
        character == 'F') {
      if (used + 1 >= sizeof(cleaned)) {
        return 0;
      }
      cleaned[used] = character;
      used += 1;
      last_underscore = 0;
      if (character >= '0' && character <= '9') {
        saw_digit = 1;
      }
      continue;
    }
    return 0;
  }
  if (saw_digit == 0 || last_underscore != 0 || used == 0) {
    return 0;
  }
  if (cleaned[used - 1] == 'f' || cleaned[used - 1] == 'F') {
    used -= 1;
  }
  cleaned[used] = '\0';
  char* end = NULL;
  const double value = strtod(cleaned, &end);
  if (end == NULL || end == cleaned || *end != '\0') {
    return 0;
  }
  if (is_f32 != 0) {
    *out = (double)(float)value;
  } else {
    *out = value;
  }
  return 1;
}

int32_t sere_parse_bool(const char* data, int64_t len, int32_t* out) {
  if (data == NULL || out == NULL) {
    return 0;
  }
  sere_strip_span(&data, &len);
  if (sere_span_eq(data, len, "True") || sere_span_eq(data, len, "true") ||
      sere_span_eq(data, len, "1")) {
    *out = 1;
    return 1;
  }
  if (sere_span_eq(data, len, "False") || sere_span_eq(data, len, "false") ||
      sere_span_eq(data, len, "0")) {
    *out = 0;
    return 1;
  }
  return 0;
}

int32_t sere_parse_none(const char* data, int64_t len) {
  if (data == NULL) {
    return 0;
  }
  sere_strip_span(&data, &len);
  return sere_span_eq(data, len, "None") || sere_span_eq(data, len, "none") ||
         sere_span_eq(data, len, "void") || len == 0;
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

void sere_list_insert(void* list, int64_t index, const void* item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || item == NULL) {
    return;
  }
  if (index < 0) {
    index = 0;
  }
  if (index > typed->len) {
    index = typed->len;
  }
  sere_list_push(typed, item);
  if (index >= typed->len - 1) {
    return;
  }
  memmove((char*)typed->data + (size_t)((index + 1) * typed->stride),
          (char*)typed->data + (size_t)(index * typed->stride),
          (size_t)((typed->len - 1 - index) * typed->stride));
  memcpy((char*)typed->data + (size_t)(index * typed->stride), item, (size_t)typed->stride);
}

void sere_list_pop(void* list, void* out_item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || typed->len <= 0) {
    if (out_item != NULL && typed != NULL) {
      memset(out_item, 0, (size_t)typed->stride);
    }
    return;
  }
  typed->len -= 1;
  if (out_item != NULL) {
    memcpy(out_item, (char*)typed->data + (size_t)(typed->len * typed->stride),
           (size_t)typed->stride);
  }
}

void sere_list_pop_at(void* list, int64_t index, void* out_item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || index < 0 || index >= typed->len) {
    if (out_item != NULL && typed != NULL) {
      memset(out_item, 0, (size_t)typed->stride);
    }
    return;
  }
  if (out_item != NULL) {
    memcpy(out_item, (char*)typed->data + (size_t)(index * typed->stride), (size_t)typed->stride);
  }
  sere_list_remove(list, index);
}

int32_t sere_list_remove_value(void* list, const void* item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || item == NULL) {
    return 0;
  }
  for (int64_t index = 0; index < typed->len; ++index) {
    if (memcmp((char*)typed->data + (size_t)(index * typed->stride), item,
               (size_t)typed->stride) == 0) {
      sere_list_remove(list, index);
      return 1;
    }
  }
  return 0;
}

int64_t sere_list_index_of(void* list, const void* item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || item == NULL) {
    return -1;
  }
  for (int64_t index = 0; index < typed->len; ++index) {
    if (memcmp((char*)typed->data + (size_t)(index * typed->stride), item,
               (size_t)typed->stride) == 0) {
      return index;
    }
  }
  return -1;
}

int64_t sere_list_count(void* list, const void* item) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || item == NULL) {
    return 0;
  }
  int64_t count = 0;
  for (int64_t index = 0; index < typed->len; ++index) {
    if (memcmp((char*)typed->data + (size_t)(index * typed->stride), item,
               (size_t)typed->stride) == 0) {
      count += 1;
    }
  }
  return count;
}

void sere_list_clear(void* list) {
  SereList* typed = (SereList*)list;
  if (typed != NULL) {
    typed->len = 0;
  }
}

void* sere_list_copy(void* list) {
  SereList* typed = (SereList*)list;
  if (typed == NULL) {
    return sere_list_new(1);
  }
  return sere_list_concat(typed, NULL);
}

void sere_list_reverse(void* list) {
  SereList* typed = (SereList*)list;
  if (typed == NULL || typed->data == NULL || typed->len <= 1) {
    return;
  }
  char* tmp = (char*)malloc((size_t)typed->stride);
  if (tmp == NULL) {
    return;
  }
  for (int64_t i = 0, j = typed->len - 1; i < j; ++i, --j) {
    char* a = (char*)typed->data + (size_t)(i * typed->stride);
    char* b = (char*)typed->data + (size_t)(j * typed->stride);
    memcpy(tmp, a, (size_t)typed->stride);
    memcpy(a, b, (size_t)typed->stride);
    memcpy(b, tmp, (size_t)typed->stride);
  }
  free(tmp);
}

void sere_list_extend(void* list, void* other) {
  SereList* extra = (SereList*)other;
  if (extra == NULL) {
    return;
  }
  for (int64_t index = 0; index < extra->len; ++index) {
    sere_list_push(list, (char*)extra->data + (size_t)(index * extra->stride));
  }
}

int32_t sere_dict_has(void* dict, const void* key) {
  return sere_dict_get(dict, key, NULL);
}

void sere_dict_clear(void* dict) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL || typed->state == NULL) {
    return;
  }
  memset(typed->state, 0, (size_t)typed->cap);
  typed->len = 0;
}

void* sere_dict_copy(void* dict) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL) {
    return sere_dict_new(1, 1, 0);
  }
  void* out = sere_dict_new(typed->key_stride, typed->val_stride, typed->key_kind);
  for (int64_t position = 0; position < typed->len; ++position) {
    const int64_t index = typed->order[position];
    if (typed->state != NULL && typed->state[index] == 1) {
      sere_dict_set(out, (char*)typed->keys + (size_t)(index * typed->key_stride),
                    (char*)typed->vals + (size_t)(index * typed->val_stride));
    }
  }
  return out;
}

void* sere_dict_keys(void* dict) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL) {
    return sere_list_new(1);
  }
  void* list = sere_list_new(typed->key_stride);
  for (int64_t position = 0; position < typed->len; ++position) {
    const int64_t index = typed->order[position];
    if (typed->state != NULL && typed->state[index] == 1) {
      sere_list_push(list, (char*)typed->keys + (size_t)(index * typed->key_stride));
    }
  }
  return list;
}

void* sere_dict_values(void* dict) {
  SereDict* typed = (SereDict*)dict;
  if (typed == NULL) {
    return sere_list_new(1);
  }
  void* list = sere_list_new(typed->val_stride);
  for (int64_t position = 0; position < typed->len; ++position) {
    const int64_t index = typed->order[position];
    if (typed->state != NULL && typed->state[index] == 1) {
      sere_list_push(list, (char*)typed->vals + (size_t)(index * typed->val_stride));
    }
  }
  return list;
}

int32_t sere_dict_pop(void* dict, const void* key, void* out_value) {
  if (!sere_dict_get(dict, key, out_value)) {
    return 0;
  }
  sere_dict_del(dict, key);
  return 1;
}
