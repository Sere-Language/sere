/// @file sere_mod.c
/// Native module object model behind `Sere_DefineFunction` and everything else
/// in `sere/api/sere_mod.h`.
///
/// The boxed representation is separate from the value layout generated code
/// uses: a `Sere_Object` is a tagged word plus a reference count, and the
/// containers keep references to their contents, so a module can build a value
/// graph and hand pieces of it to other modules without copying.

#include "sere/api/sere_mod.h"

#include "sere_rt.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERE_MOD_MAX_ENTRIES 256
#define SERE_MOD_MAX_TYPES 64
#define SERE_DICT_INITIAL_CAPACITY 16
#define SERE_DICT_LOAD_FACTOR 4
#define SERE_REPR_SLOTS 4
#define SERE_REPR_INITIAL 256

/// One named value in a boxed object, in insertion order.
typedef struct Sere_Attr {
  char* name;
  Sere_Object* value;
  struct Sere_Attr* next;
} Sere_Attr;

/// One bucket entry of a boxed dict.
typedef struct Sere_DictEntry {
  Sere_Object* key;
  Sere_Object* value;
  struct Sere_DictEntry* next;
} Sere_DictEntry;

struct Sere_Object {
  int32_t kind;
  int32_t refs;
  int64_t i;
  double f;
  void* p;
  int64_t extra;
  Sere_Attr* attrs;
  Sere_Finalizer finalizer;
  char* name;
};

struct Sere_List {
  Sere_Object header;
  void* items;
};

struct Sere_Dict {
  Sere_Object header;
  Sere_DictEntry** buckets;
  int64_t capacity;
  int64_t count;
};

/// Entry kinds `Sere_RegisteredKind` reports.
enum SereEntryKind {
  SereEntryFunction = 0,
  SereEntryMethod = 1,
  SereEntryConstant = 2,
};

typedef struct {
  const char* name;
  Sere_CFunction function;
  int32_t nargs;
  int32_t kind;
  const char* type_name;
  Sere_Object* value;
} SereEntry;

static SereEntry g_entries[SERE_MOD_MAX_ENTRIES];
static int32_t g_entry_count = 0;
static const char* g_types[SERE_MOD_MAX_TYPES];
static int32_t g_type_count = 0;
static char* g_module_name = NULL;

/// Growable text buffer used by `Sere_Repr` and `Sere_Str_Format`.
typedef struct {
  char* data;
  size_t length;
  size_t capacity;
} SereText;

static Sere_Object* makeObject(int32_t kind) {
  Sere_Object* object = (Sere_Object*)calloc(1, sizeof(Sere_Object));
  if (object == NULL) {
    return NULL;
  }
  object->kind = kind;
  object->refs = 1;
  return object;
}

static char* copyText(const char* text, size_t length) {
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    return NULL;
  }
  if (length > 0 && text != NULL) {
    memcpy(copy, text, length);
  }
  copy[length] = '\0';
  return copy;
}

static char* copyCString(const char* text) {
  return copyText(text == NULL ? "" : text, text == NULL ? 0 : strlen(text));
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static int32_t isIntegerKind(int32_t kind) {
  return kind == Sere_KindBool || kind == Sere_KindI32 || kind == Sere_KindI64;
}

static uint64_t hashBytes(const void* data, int64_t length) {
  const unsigned char* bytes = (const unsigned char*)data;
  uint64_t hash = 1469598103934665603ULL;
  for (int64_t index = 0; index < length; ++index) {
    hash ^= (uint64_t)bytes[index];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static uint64_t objectHash(const Sere_Object* object) {
  if (object == NULL) {
    return 0;
  }
  switch (object->kind) {
  case Sere_KindNone:
    return 0;
  case Sere_KindBool:
  case Sere_KindI32:
  case Sere_KindI64:
    return (uint64_t)object->i * 1099511628211ULL;
  case Sere_KindF64: {
    uint64_t bits = 0;
    memcpy(&bits, &object->f, sizeof(bits));
    return bits;
  }
  case Sere_KindStr:
  case Sere_KindBytes:
    return hashBytes(object->p, object->extra);
  default:
    return (uint64_t)(uintptr_t)object;
  }
}

static int32_t objectsEqual(const Sere_Object* left, const Sere_Object* right) {
  if (left == right) {
    return 1;
  }
  if (left == NULL || right == NULL) {
    return 0;
  }
  if (isIntegerKind(left->kind) && isIntegerKind(right->kind)) {
    return left->i == right->i;
  }
  if (left->kind != right->kind) {
    return 0;
  }
  switch (left->kind) {
  case Sere_KindNone:
    return 1;
  case Sere_KindF64:
    return left->f == right->f;
  case Sere_KindStr:
  case Sere_KindBytes:
    if (left->extra != right->extra) {
      return 0;
    }
    return left->extra == 0 || memcmp(left->p, right->p, (size_t)left->extra) == 0;
  default:
    return 0;
  }
}

static const char* kindName(int32_t kind) {
  switch (kind) {
  case Sere_KindNone:
    return "None";
  case Sere_KindBool:
    return "bool";
  case Sere_KindI32:
    return "i32";
  case Sere_KindI64:
    return "i64";
  case Sere_KindF64:
    return "f64";
  case Sere_KindStr:
    return "str";
  case Sere_KindList:
    return "list";
  case Sere_KindPtr:
    return "ptr";
  case Sere_KindDict:
    return "dict";
  case Sere_KindBytes:
    return "bytes";
  case Sere_KindFunction:
    return "function";
  default:
    return "object";
  }
}

static int32_t objectTruthy(const Sere_Object* object) {
  if (object == NULL) {
    return 0;
  }
  switch (object->kind) {
  case Sere_KindNone:
    return 0;
  case Sere_KindBool:
  case Sere_KindI32:
  case Sere_KindI64:
    return object->i != 0;
  case Sere_KindF64:
    return object->f != 0.0;
  case Sere_KindStr:
  case Sere_KindBytes:
    return object->extra != 0;
  case Sere_KindList:
    return Sere_List_Len((Sere_List*)object) != 0;
  case Sere_KindDict:
    return Sere_Dict_Len((Sere_Dict*)object) != 0;
  default:
    return 1;
  }
}

static void textInit(SereText* text) {
  text->data = NULL;
  text->length = 0;
  text->capacity = 0;
}

static void textRelease(SereText* text) {
  free(text->data);
  text->data = NULL;
  text->length = 0;
  text->capacity = 0;
}

static int32_t textReserve(SereText* text, size_t extra) {
  if (text->length + extra + 1 <= text->capacity) {
    return 1;
  }
  size_t wanted = text->capacity == 0 ? SERE_REPR_INITIAL : text->capacity * 2;
  while (wanted < text->length + extra + 1) {
    wanted *= 2;
  }
  char* grown = (char*)realloc(text->data, wanted);
  if (grown == NULL) {
    return 0;
  }
  text->data = grown;
  text->capacity = wanted;
  return 1;
}

static int32_t textAppend(SereText* text, const char* data, size_t length) {
  if (!textReserve(text, length)) {
    return 0;
  }
  memcpy(text->data + text->length, data, length);
  text->length += length;
  text->data[text->length] = '\0';
  return 1;
}

static int32_t textAppendCString(SereText* text, const char* data) {
  return textAppend(text, data == NULL ? "" : data, data == NULL ? 0 : strlen(data));
}

/// Appends `value` to `text` the way `repr` renders it. `depth` bounds the
/// recursion so a value that contains itself still terminates.
static int32_t textAppendValue(SereText* text, const Sere_Object* value, int32_t depth) {
  if (depth > 8) {
    return textAppendCString(text, "...");
  }
  if (value == NULL) {
    return textAppendCString(text, "None");
  }
  char buffer[64];
  switch (value->kind) {
  case Sere_KindNone:
    return textAppendCString(text, "None");
  case Sere_KindBool:
    return textAppendCString(text, value->i != 0 ? "True" : "False");
  case Sere_KindI32:
  case Sere_KindI64: {
    const int written = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->i);
    return textAppend(text, buffer, written > 0 ? (size_t)written : 0);
  }
  case Sere_KindF64: {
    const int written = snprintf(buffer, sizeof(buffer), "%g", value->f);
    return textAppend(text, buffer, written > 0 ? (size_t)written : 0);
  }
  case Sere_KindStr: {
    if (!textAppendCString(text, "'")) {
      return 0;
    }
    const char* data = (const char*)value->p;
    for (int64_t index = 0; index < value->extra; ++index) {
      const char character = data == NULL ? '\0' : data[index];
      if (character == '\'') {
        if (!textAppendCString(text, "\\'")) {
          return 0;
        }
      } else if (character == '\\') {
        if (!textAppendCString(text, "\\\\")) {
          return 0;
        }
      } else if (character == '\n') {
        if (!textAppendCString(text, "\\n")) {
          return 0;
        }
      } else if (!textAppend(text, &character, 1)) {
        return 0;
      }
    }
    return textAppendCString(text, "'");
  }
  case Sere_KindBytes: {
    const unsigned char* data = (const unsigned char*)value->p;
    if (!textAppendCString(text, "b\"")) {
      return 0;
    }
    for (int64_t index = 0; index < value->extra; ++index) {
      const unsigned char byte = data == NULL ? 0 : data[index];
      const int written =
          snprintf(buffer, sizeof(buffer), byte >= 32 && byte < 127 ? "%c" : "\\x%02x", byte);
      if (!textAppend(text, buffer, written > 0 ? (size_t)written : 0)) {
        return 0;
      }
    }
    return textAppendCString(text, "\"");
  }
  case Sere_KindList: {
    Sere_List* list = (Sere_List*)value;
    if (!textAppendCString(text, "[")) {
      return 0;
    }
    const int64_t length = Sere_List_Len(list);
    for (int64_t index = 0; index < length; ++index) {
      if (index > 0 && !textAppendCString(text, ", ")) {
        return 0;
      }
      if (!textAppendValue(text, Sere_List_Get(list, index), depth + 1)) {
        return 0;
      }
    }
    return textAppendCString(text, "]");
  }
  case Sere_KindDict: {
    Sere_Dict* dict = (Sere_Dict*)value;
    if (!textAppendCString(text, "{")) {
      return 0;
    }
    int64_t index = 0;
    for (int64_t slot = 0; slot < dict->capacity; ++slot) {
      for (const Sere_DictEntry* entry = dict->buckets[slot]; entry != NULL; entry = entry->next) {
        if (index > 0 && !textAppendCString(text, ", ")) {
          return 0;
        }
        if (!textAppendValue(text, entry->key, depth + 1) || !textAppendCString(text, ": ") ||
            !textAppendValue(text, entry->value, depth + 1)) {
          return 0;
        }
        ++index;
      }
    }
    return textAppendCString(text, "}");
  }
  case Sere_KindPtr:
  case Sere_KindFunction: {
    const int written =
        snprintf(buffer, sizeof(buffer), "<%s %p>", kindName(value->kind), value->p);
    return textAppend(text, buffer, written > 0 ? (size_t)written : 0);
  }
  default:
    return textAppendCString(text, kindName(value->kind));
  }
}

/// Renders into one of the rotating buffers `Sere_Repr` and friends return.
static const char* renderRotating(const Sere_Object* value) {
  static char* slots[SERE_REPR_SLOTS];
  static int32_t next = 0;
  SereText text;
  textInit(&text);
  if (!textAppendValue(&text, value, 0)) {
    textRelease(&text);
    return "";
  }
  const int32_t slot = next;
  next = (next + 1) % SERE_REPR_SLOTS;
  free(slots[slot]);
  slots[slot] = text.data;
  return slots[slot];
}

Sere_Object* Sere_None_New(void) { return makeObject(Sere_KindNone); }

Sere_Object* Sere_Bool_FromI32(int32_t value) {
  Sere_Object* object = makeObject(Sere_KindBool);
  if (object != NULL) {
    object->i = value ? 1 : 0;
  }
  return object;
}

Sere_Object* Sere_Long_FromI32(int32_t value) {
  Sere_Object* object = makeObject(Sere_KindI32);
  if (object != NULL) {
    object->i = value;
  }
  return object;
}

Sere_Object* Sere_Long_FromI64(int64_t value) {
  Sere_Object* object = makeObject(Sere_KindI64);
  if (object != NULL) {
    object->i = value;
  }
  return object;
}

Sere_Object* Sere_Long_FromU64(uint64_t value) {
  Sere_Object* object = makeObject(Sere_KindI64);
  if (object != NULL) {
    object->i = (int64_t)value;
  }
  return object;
}

Sere_Object* Sere_Float_FromF64(double value) {
  Sere_Object* object = makeObject(Sere_KindF64);
  if (object != NULL) {
    object->f = value;
  }
  return object;
}

Sere_Object* Sere_Str_FromCString(const char* text) {
  const char* source = text == NULL ? "" : text;
  return Sere_Str_FromData(source, (int64_t)strlen(source));
}

Sere_Object* Sere_Str_FromData(const char* data, int64_t length) {
  if (length < 0) {
    return NULL;
  }
  Sere_Object* object = makeObject(Sere_KindStr);
  if (object == NULL) {
    return NULL;
  }
  char* copy = copyText(data, (size_t)length);
  if (copy == NULL) {
    free(object);
    return NULL;
  }
  object->p = copy;
  object->extra = length;
  return object;
}

Sere_Object* Sere_Bytes_New(int64_t length) {
  if (length < 0) {
    return NULL;
  }
  Sere_Object* object = makeObject(Sere_KindBytes);
  if (object == NULL) {
    return NULL;
  }
  unsigned char* buffer = (unsigned char*)calloc((size_t)length + 1, 1);
  if (buffer == NULL) {
    free(object);
    return NULL;
  }
  object->p = buffer;
  object->extra = length;
  return object;
}

Sere_Object* Sere_Bytes_FromData(const void* data, int64_t length) {
  Sere_Object* object = Sere_Bytes_New(length);
  if (object != NULL && length > 0 && data != NULL) {
    memcpy(object->p, data, (size_t)length);
  }
  return object;
}

Sere_Object* Sere_Ptr_FromVoid(void* pointer) {
  Sere_Object* object = makeObject(Sere_KindPtr);
  if (object != NULL) {
    object->p = pointer;
  }
  return object;
}

Sere_Object* Sere_Ptr_FromVoidOwned(void* pointer, Sere_Finalizer finalizer) {
  Sere_Object* object = Sere_Ptr_FromVoid(pointer);
  if (object != NULL) {
    object->finalizer = finalizer;
  }
  return object;
}

/// Builds the callable box shared by both function constructors.
static Sere_Object* makeFunction(const char* name, Sere_CFunction function, int32_t nargs) {
  if (function == NULL) {
    return NULL;
  }
  Sere_Object* object = makeObject(Sere_KindFunction);
  if (object == NULL) {
    return NULL;
  }
  object->name = copyCString(name == NULL ? "" : name);
  object->p = (void*)(uintptr_t)function;
  object->extra = nargs;
  return object;
}

Sere_Object* Sere_Function_FromCFunction(const char* name, Sere_CFunction function, int32_t nargs) {
  return makeFunction(name, function, nargs);
}

Sere_Object* Sere_Function_FromName(const char* name) {
  if (name == NULL) {
    return NULL;
  }
  for (int32_t index = 0; index < g_entry_count; ++index) {
    if (g_entries[index].kind != SereEntryConstant && strcmp(g_entries[index].name, name) == 0) {
      return makeFunction(g_entries[index].name, g_entries[index].function, g_entries[index].nargs);
    }
  }
  return NULL;
}

int32_t Sere_Kind(const Sere_Object* object) {
  return object == NULL ? Sere_KindNone : object->kind;
}

int32_t Sere_IsNone(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindNone;
}
int32_t Sere_IsBool(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindBool;
}
int32_t Sere_IsLong(const Sere_Object* object) {
  return isIntegerKind(Sere_Kind(object)) && !Sere_IsBool(object);
}
int32_t Sere_IsFloat(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindF64;
}
int32_t Sere_IsNumber(const Sere_Object* object) {
  const int32_t kind = Sere_Kind(object);
  return isIntegerKind(kind) || kind == Sere_KindF64;
}
int32_t Sere_IsStr(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindStr;
}
int32_t Sere_IsBytes(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindBytes;
}
int32_t Sere_IsList(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindList;
}
int32_t Sere_IsDict(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindDict;
}
int32_t Sere_IsPtr(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindPtr;
}
int32_t Sere_IsCallable(const Sere_Object* object) {
  return Sere_Kind(object) == Sere_KindFunction;
}

const char* Sere_TypeName(const Sere_Object* object) {
  return kindName(Sere_Kind(object));
}

int32_t Sere_Truthy(const Sere_Object* object) {
  return objectTruthy(object);
}

int32_t Sere_Long_AsI32(const Sere_Object* object) {
  return object == NULL ? 0 : (int32_t)object->i;
}

int64_t Sere_Long_AsI64(const Sere_Object* object) {
  return object == NULL ? 0 : object->i;
}

int64_t Sere_Number_AsI64(const Sere_Object* object) {
  if (object == NULL) {
    return 0;
  }
  return object->kind == Sere_KindF64 ? (int64_t)object->f : object->i;
}

double Sere_Float_AsF64(const Sere_Object* object) {
  if (object == NULL) {
    return 0.0;
  }
  return object->kind == Sere_KindF64 ? object->f : (double)object->i;
}

double Sere_Number_AsF64(const Sere_Object* object) {
  return Sere_Float_AsF64(object);
}

const char* Sere_Str_AsCString(const Sere_Object* object) {
  if (object == NULL || object->kind != Sere_KindStr || object->p == NULL) {
    return "";
  }
  return (const char*)object->p;
}

const char* Sere_Str_Data(const Sere_Object* object, int64_t* out_length) {
  if (out_length != NULL) {
    *out_length = 0;
  }
  if (object == NULL || object->kind != Sere_KindStr || object->p == NULL) {
    return NULL;
  }
  if (out_length != NULL) {
    *out_length = object->extra;
  }
  return (const char*)object->p;
}

int64_t Sere_Str_Len(const Sere_Object* object) {
  return object == NULL || object->kind != Sere_KindStr ? 0 : object->extra;
}

void* Sere_Bytes_Data(const Sere_Object* object) {
  return object == NULL || object->kind != Sere_KindBytes ? NULL : object->p;
}

int64_t Sere_Bytes_Len(const Sere_Object* object) {
  return object == NULL || object->kind != Sere_KindBytes ? 0 : object->extra;
}

void* Sere_Ptr_AsVoid(const Sere_Object* object) {
  return object == NULL || object->kind != Sere_KindPtr ? NULL : object->p;
}

int32_t Sere_Equal(const Sere_Object* left, const Sere_Object* right) {
  return objectsEqual(left, right);
}

int64_t Sere_Hash(const Sere_Object* object) {
  return (int64_t)objectHash(object);
}

int32_t Sere_Compare(const Sere_Object* left, const Sere_Object* right, int32_t* out_order) {
  if (out_order == NULL || left == NULL || right == NULL) {
    return 0;
  }
  if (Sere_IsNumber(left) && Sere_IsNumber(right)) {
    const double leftValue = Sere_Number_AsF64(left);
    const double rightValue = Sere_Number_AsF64(right);
    *out_order = leftValue < rightValue ? -1 : (leftValue > rightValue ? 1 : 0);
    return 1;
  }
  if (Sere_IsStr(left) && Sere_IsStr(right)) {
    const int32_t compared = sere_str_cmp(Sere_Str_AsCString(left),
                                          Sere_Str_Len(left),
                                          Sere_Str_AsCString(right),
                                          Sere_Str_Len(right));
    *out_order = compared < 0 ? -1 : (compared > 0 ? 1 : 0);
    return 1;
  }
  return 0;
}

/// Allocates the wrapper around a list's backing store.
static Sere_List* makeListShell(void) {
  Sere_List* list = (Sere_List*)calloc(1, sizeof(Sere_List));
  if (list == NULL) {
    return NULL;
  }
  list->header.kind = Sere_KindList;
  list->header.refs = 1;
  list->items = sere_list_new((int64_t)sizeof(Sere_Object*));
  if (list->items == NULL) {
    free(list);
    return NULL;
  }
  return list;
}

/// Frees a backing store this API created. Every list here comes from
/// `sere_list_new`, so it is malloc-backed rather than collector-backed.
static void freeListStore(void* store) {
  SereList* typed = (SereList*)store;
  if (typed == NULL) {
    return;
  }
  free(typed->data);
  free(typed);
}

/// Releases the reference a list holds on every item.
static void releaseListItems(Sere_List* list) {
  if (list == NULL || list->items == NULL) {
    return;
  }
  const int64_t length = sere_list_len(list->items);
  for (int64_t index = 0; index < length; ++index) {
    Sere_Object** slot = (Sere_Object**)sere_list_item(list->items, index);
    if (slot != NULL && *slot != NULL) {
      Sere_DecRef(*slot);
    }
  }
}

/// Wraps a backing store the runtime produced, taking a reference to each item
/// it already contains.
static Sere_List* wrapListStore(void* store) {
  if (store == NULL) {
    return NULL;
  }
  Sere_List* list = (Sere_List*)calloc(1, sizeof(Sere_List));
  if (list == NULL) {
    freeListStore(store);
    return NULL;
  }
  list->header.kind = Sere_KindList;
  list->header.refs = 1;
  list->items = store;
  const int64_t length = sere_list_len(store);
  for (int64_t index = 0; index < length; ++index) {
    Sere_Object** slot = (Sere_Object**)sere_list_item(store, index);
    if (slot != NULL && *slot != NULL) {
      Sere_IncRef(*slot);
    }
  }
  return list;
}

Sere_List* Sere_List_New(void) {
  return makeListShell();
}

Sere_List* Sere_List_NewWithCapacity(int64_t capacity) {
  Sere_List* list = makeListShell();
  if (list == NULL || capacity <= 0) {
    return list;
  }
  // A zeroed array of the wanted length reserves the capacity; truncating it
  // back to zero keeps the allocation, so the pushes that follow never grow.
  void* store = sere_array_new((int64_t)sizeof(Sere_Object*), capacity);
  if (store == NULL) {
    return list;
  }
  freeListStore(list->items);
  sere_list_clear(store);
  list->items = store;
  return list;
}

int32_t Sere_List_Append(Sere_List* list, Sere_Object* item) {
  if (list == NULL || list->items == NULL) {
    return 0;
  }
  Sere_IncRef(item);
  sere_list_push(list->items, &item);
  return 1;
}

Sere_Object* Sere_List_AppendPtr(Sere_List* list, void* pointer) {
  Sere_Object* box = Sere_Ptr_FromVoid(pointer);
  if (box == NULL || !Sere_List_Append(list, box)) {
    Sere_DecRef(box);
    return NULL;
  }
  // The list owns a reference of its own, so the caller starts with one too.
  return box;
}

int64_t Sere_List_Len(const Sere_List* list) {
  return list == NULL ? 0 : sere_list_len(list->items);
}

Sere_Object* Sere_List_Get(Sere_List* list, int64_t index) {
  if (list == NULL || list->items == NULL) {
    return NULL;
  }
  // The runtime accessor reports a bad index by raising, so the range is
  // checked here where the API promises NULL.
  const int64_t length = sere_list_len(list->items);
  if (index < 0) {
    index += length;
  }
  if (index < 0 || index >= length) {
    return NULL;
  }
  Sere_Object** slot = (Sere_Object**)sere_list_item(list->items, index);
  return slot == NULL ? NULL : *slot;
}

int32_t Sere_List_Set(Sere_List* list, int64_t index, Sere_Object* item) {
  if (list == NULL || list->items == NULL) {
    return 0;
  }
  const int64_t length = sere_list_len(list->items);
  if (index < 0) {
    index += length;
  }
  if (index < 0 || index >= length) {
    return 0;
  }
  Sere_Object** slot = (Sere_Object**)sere_list_item(list->items, index);
  if (slot == NULL) {
    return 0;
  }
  Sere_Object* previous = *slot;
  Sere_IncRef(item);
  *slot = item;
  Sere_DecRef(previous);
  return 1;
}

int32_t Sere_List_Insert(Sere_List* list, int64_t index, Sere_Object* item) {
  if (list == NULL || list->items == NULL) {
    return 0;
  }
  const int64_t length = sere_list_len(list->items);
  if (index < 0 || index > length) {
    return 0;
  }
  Sere_IncRef(item);
  sere_list_insert(list->items, index, &item);
  return 1;
}

int32_t Sere_List_RemoveAt(Sere_List* list, int64_t index) {
  Sere_Object* item = Sere_List_Get(list, index);
  if (item == NULL) {
    return 0;
  }
  int64_t resolved = index < 0 ? index + sere_list_len(list->items) : index;
  sere_list_remove(list->items, resolved);
  Sere_DecRef(item);
  return 1;
}

Sere_Object* Sere_List_Pop(Sere_List* list, int64_t index) {
  if (list == NULL || list->items == NULL || sere_list_len(list->items) == 0) {
    return NULL;
  }
  int64_t resolved = index < 0 ? sere_list_len(list->items) - 1 : index;
  Sere_Object* item = Sere_List_Get(list, resolved);
  if (item == NULL) {
    return NULL;
  }
  Sere_IncRef(item);
  Sere_Object* popped = NULL;
  sere_list_pop_at(list->items, resolved, &popped);
  // The container's reference goes away; the caller keeps the one added above.
  Sere_DecRef(item);
  return item;
}

void Sere_List_Clear(Sere_List* list) {
  if (list == NULL || list->items == NULL) {
    return;
  }
  releaseListItems(list);
  sere_list_clear(list->items);
}

void Sere_List_Reverse(Sere_List* list) {
  if (list != NULL) {
    sere_list_reverse(list->items);
  }
}

int64_t Sere_List_IndexOf(Sere_List* list, Sere_Object* item) {
  if (list == NULL || list->items == NULL || item == NULL) {
    return -1;
  }
  const int64_t length = sere_list_len(list->items);
  for (int64_t index = 0; index < length; ++index) {
    if (objectsEqual(Sere_List_Get(list, index), item)) {
      return index;
    }
  }
  return -1;
}

int64_t Sere_List_Count(Sere_List* list, Sere_Object* item) {
  if (list == NULL || list->items == NULL || item == NULL) {
    return 0;
  }
  int64_t count = 0;
  const int64_t length = sere_list_len(list->items);
  for (int64_t index = 0; index < length; ++index) {
    if (objectsEqual(Sere_List_Get(list, index), item)) {
      ++count;
    }
  }
  return count;
}

int32_t Sere_List_Contains(Sere_List* list, Sere_Object* item) {
  return Sere_List_IndexOf(list, item) >= 0 ? 1 : 0;
}

int32_t Sere_List_Extend(Sere_List* list, Sere_List* other) {
  if (list == NULL || other == NULL) {
    return 0;
  }
  const int64_t length = Sere_List_Len(other);
  for (int64_t index = 0; index < length; ++index) {
    Sere_List_Append(list, Sere_List_Get(other, index));
  }
  return 1;
}

Sere_List* Sere_List_Slice(Sere_List* list, int64_t start, int64_t end) {
  if (list == NULL || list->items == NULL) {
    return NULL;
  }
  return wrapListStore(
      sere_list_slice(list->items, start, end, start >= 0 ? 1 : 0, end >= 0 ? 1 : 0));
}

Sere_List* Sere_List_Copy(Sere_List* list) {
  if (list == NULL || list->items == NULL) {
    return NULL;
  }
  return wrapListStore(sere_list_copy(list->items));
}

Sere_Object* Sere_List_AsObject(Sere_List* list) {
  return list == NULL ? NULL : &list->header;
}

Sere_List* Sere_List_FromObject(Sere_Object* object) {
  if (object == NULL || object->kind != Sere_KindList) {
    return NULL;
  }
  return (Sere_List*)object;
}

// ---------------------------------------------------------------------------
// Dicts
// ---------------------------------------------------------------------------

static Sere_Dict* makeDictShell(int64_t capacity) {
  Sere_Dict* dict = (Sere_Dict*)calloc(1, sizeof(Sere_Dict));
  if (dict == NULL) {
    return NULL;
  }
  Sere_DictEntry** buckets = (Sere_DictEntry**)calloc((size_t)capacity, sizeof(Sere_DictEntry*));
  if (buckets == NULL) {
    free(dict);
    return NULL;
  }
  dict->header.kind = Sere_KindDict;
  dict->header.refs = 1;
  dict->buckets = buckets;
  dict->capacity = capacity;
  return dict;
}

static Sere_DictEntry* dictFind(const Sere_Dict* dict, const Sere_Object* key) {
  if (dict == NULL || key == NULL) {
    return NULL;
  }
  const int64_t slot = (int64_t)(objectHash(key) % (uint64_t)dict->capacity);
  for (Sere_DictEntry* entry = dict->buckets[slot]; entry != NULL; entry = entry->next) {
    if (objectsEqual(entry->key, key)) {
      return entry;
    }
  }
  return NULL;
}

/// Rehashes into a wider bucket array once the chains get long.
static void dictGrow(Sere_Dict* dict) {
  if (dict == NULL || dict->count < dict->capacity * SERE_DICT_LOAD_FACTOR) {
    return;
  }
  const int64_t capacity = dict->capacity * 2;
  Sere_DictEntry** buckets = (Sere_DictEntry**)calloc((size_t)capacity, sizeof(Sere_DictEntry*));
  if (buckets == NULL) {
    return;
  }
  for (int64_t slot = 0; slot < dict->capacity; ++slot) {
    Sere_DictEntry* entry = dict->buckets[slot];
    while (entry != NULL) {
      Sere_DictEntry* next = entry->next;
      const int64_t target = (int64_t)(objectHash(entry->key) % (uint64_t)capacity);
      entry->next = buckets[target];
      buckets[target] = entry;
      entry = next;
    }
  }
  free(dict->buckets);
  dict->buckets = buckets;
  dict->capacity = capacity;
}

static void dictReleaseEntries(Sere_Dict* dict) {
  if (dict == NULL) {
    return;
  }
  for (int64_t slot = 0; slot < dict->capacity; ++slot) {
    Sere_DictEntry* entry = dict->buckets[slot];
    while (entry != NULL) {
      Sere_DictEntry* next = entry->next;
      Sere_DecRef(entry->key);
      Sere_DecRef(entry->value);
      free(entry);
      entry = next;
    }
    dict->buckets[slot] = NULL;
  }
  dict->count = 0;
}

Sere_Dict* Sere_Dict_New(void) {
  return makeDictShell(SERE_DICT_INITIAL_CAPACITY);
}

int32_t Sere_Dict_Set(Sere_Dict* dict, Sere_Object* key, Sere_Object* value) {
  if (dict == NULL || key == NULL) {
    return 0;
  }
  Sere_DictEntry* found = dictFind(dict, key);
  if (found != NULL) {
    Sere_Object* previous = found->value;
    Sere_IncRef(value);
    found->value = value;
    Sere_DecRef(previous);
    return 1;
  }
  const int64_t slot = (int64_t)(objectHash(key) % (uint64_t)dict->capacity);
  Sere_DictEntry* entry = (Sere_DictEntry*)calloc(1, sizeof(Sere_DictEntry));
  if (entry == NULL) {
    return 0;
  }
  Sere_IncRef(key);
  Sere_IncRef(value);
  entry->key = key;
  entry->value = value;
  entry->next = dict->buckets[slot];
  dict->buckets[slot] = entry;
  ++dict->count;
  dictGrow(dict);
  return 1;
}

Sere_Object* Sere_Dict_Get(Sere_Dict* dict, Sere_Object* key) {
  Sere_DictEntry* entry = dictFind(dict, key);
  return entry == NULL ? NULL : entry->value;
}

int32_t Sere_Dict_Has(Sere_Dict* dict, Sere_Object* key) {
  return dictFind(dict, key) == NULL ? 0 : 1;
}

int32_t Sere_Dict_Del(Sere_Dict* dict, Sere_Object* key) {
  if (dict == NULL || key == NULL) {
    return 0;
  }
  const int64_t slot = (int64_t)(objectHash(key) % (uint64_t)dict->capacity);
  Sere_DictEntry** link = &dict->buckets[slot];
  while (*link != NULL) {
    Sere_DictEntry* entry = *link;
    if (objectsEqual(entry->key, key)) {
      *link = entry->next;
      Sere_DecRef(entry->key);
      Sere_DecRef(entry->value);
      free(entry);
      --dict->count;
      return 1;
    }
    link = &entry->next;
  }
  return 0;
}

int64_t Sere_Dict_Len(const Sere_Dict* dict) {
  return dict == NULL ? 0 : dict->count;
}

void Sere_Dict_Clear(Sere_Dict* dict) {
  dictReleaseEntries(dict);
}

Sere_Dict* Sere_Dict_Copy(Sere_Dict* dict) {
  if (dict == NULL) {
    return NULL;
  }
  Sere_Dict* copy = makeDictShell(
      dict->capacity < SERE_DICT_INITIAL_CAPACITY ? SERE_DICT_INITIAL_CAPACITY : dict->capacity);
  if (copy == NULL) {
    return NULL;
  }
  for (int64_t slot = 0; slot < dict->capacity; ++slot) {
    for (Sere_DictEntry* entry = dict->buckets[slot]; entry != NULL; entry = entry->next) {
      Sere_Dict_Set(copy, entry->key, entry->value);
    }
  }
  return copy;
}

Sere_List* Sere_Dict_Keys(Sere_Dict* dict) {
  if (dict == NULL) {
    return NULL;
  }
  Sere_List* keys = Sere_List_NewWithCapacity(dict->count);
  if (keys == NULL) {
    return NULL;
  }
  for (int64_t slot = 0; slot < dict->capacity; ++slot) {
    for (Sere_DictEntry* entry = dict->buckets[slot]; entry != NULL; entry = entry->next) {
      Sere_List_Append(keys, entry->key);
    }
  }
  return keys;
}

Sere_List* Sere_Dict_Values(Sere_Dict* dict) {
  if (dict == NULL) {
    return NULL;
  }
  Sere_List* values = Sere_List_NewWithCapacity(dict->count);
  if (values == NULL) {
    return NULL;
  }
  for (int64_t slot = 0; slot < dict->capacity; ++slot) {
    for (Sere_DictEntry* entry = dict->buckets[slot]; entry != NULL; entry = entry->next) {
      Sere_List_Append(values, entry->value);
    }
  }
  return values;
}

Sere_Object* Sere_Dict_AsObject(Sere_Dict* dict) {
  return dict == NULL ? NULL : &dict->header;
}

Sere_Dict* Sere_Dict_FromObject(Sere_Object* object) {
  if (object == NULL || object->kind != Sere_KindDict) {
    return NULL;
  }
  return (Sere_Dict*)object;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

/// Index of the entry registered under `name`, or -1.
static int32_t findEntry(const char* name, int32_t kind, const char* type_name) {
  if (name == NULL) {
    return -1;
  }
  for (int32_t index = 0; index < g_entry_count; ++index) {
    const SereEntry* entry = &g_entries[index];
    if (entry->kind != kind || strcmp(entry->name, name) != 0) {
      continue;
    }
    if (kind != SereEntryMethod) {
      return index;
    }
    if (entry->type_name != NULL && type_name != NULL && strcmp(entry->type_name, type_name) == 0) {
      return index;
    }
  }
  return -1;
}

static int32_t addEntry(const char* name,
                        Sere_CFunction function,
                        int32_t nargs,
                        int32_t kind,
                        const char* type_name,
                        Sere_Object* value) {
  if (name == NULL || g_entry_count >= SERE_MOD_MAX_ENTRIES) {
    return 0;
  }
  SereEntry* entry = &g_entries[g_entry_count];
  entry->name = name;
  entry->function = function;
  entry->nargs = nargs;
  entry->kind = kind;
  entry->type_name = type_name;
  entry->value = value;
  ++g_entry_count;
  return 1;
}

void Sere_DefineFunction(const char* name, Sere_CFunction function, int32_t nargs) {
  if (function == NULL) {
    return;
  }
  (void)addEntry(name, function, nargs, SereEntryFunction, NULL, NULL);
}

void Sere_DefineFunctionVariadic(const char* name, Sere_CFunction function) {
  Sere_DefineFunction(name, function, -1);
}

void Sere_DefineMethod(const char* type_name,
                       const char* name,
                       Sere_CFunction function,
                       int32_t nargs) {
  if (function == NULL || name == NULL || type_name == NULL) {
    return;
  }
  (void)addEntry(name, function, nargs, SereEntryMethod, type_name, NULL);
}

void Sere_DefineType(const char* name) {
  if (name == NULL || g_type_count >= SERE_MOD_MAX_TYPES || Sere_TypeDefined(name)) {
    return;
  }
  g_types[g_type_count] = copyCString(name);
  if (g_types[g_type_count] != NULL) {
    ++g_type_count;
  }
}

void Sere_DefineConstant(const char* name, Sere_Object* value) {
  if (name == NULL || findEntry(name, SereEntryConstant, NULL) >= 0) {
    return;
  }
  if (!addEntry(name, NULL, 0, SereEntryConstant, NULL, value)) {
    return;
  }
  Sere_IncRef(value);
}

Sere_CFunction Sere_FindFunction(const char* name) {
  const int32_t index = findEntry(name, SereEntryFunction, NULL);
  return index < 0 ? NULL : g_entries[index].function;
}

Sere_CFunction Sere_FindMethod(const char* type_name, const char* name) {
  const int32_t index = findEntry(name, SereEntryMethod, type_name);
  return index < 0 ? NULL : g_entries[index].function;
}

int32_t Sere_FunctionArity(const char* name, int32_t* out_nargs) {
  const int32_t index = findEntry(name, SereEntryFunction, NULL);
  if (index < 0) {
    return 0;
  }
  if (out_nargs != NULL) {
    *out_nargs = g_entries[index].nargs;
  }
  return 1;
}

int32_t Sere_TypeDefined(const char* name) {
  if (name == NULL) {
    return 0;
  }
  for (int32_t index = 0; index < g_type_count; ++index) {
    if (strcmp(g_types[index], name) == 0) {
      return 1;
    }
  }
  return 0;
}

Sere_Object* Sere_FindConstant(const char* name) {
  const int32_t index = findEntry(name, SereEntryConstant, NULL);
  return index < 0 ? NULL : g_entries[index].value;
}

int32_t Sere_RegisteredCount(void) {
  return g_entry_count;
}

const char* Sere_RegisteredName(int32_t index) {
  return index < 0 || index >= g_entry_count ? NULL : g_entries[index].name;
}

int32_t Sere_RegisteredKind(int32_t index) {
  return index < 0 || index >= g_entry_count ? -1 : g_entries[index].kind;
}

void Sere_SetModuleName(const char* name) {
  free(g_module_name);
  g_module_name = copyCString(name);
}

const char* Sere_ModuleName(void) {
  return g_module_name == NULL ? "" : g_module_name;
}

/// Stores an owned value under an owned key name, releasing the temporaries.
static void dictPutOwned(Sere_Dict* dict, const char* key, Sere_Object* value) {
  if (dict == NULL || value == NULL) {
    Sere_DecRef(value);
    return;
  }
  Sere_Object* keyBox = Sere_Str_FromCString(key);
  if (keyBox == NULL) {
    Sere_DecRef(value);
    return;
  }
  (void)Sere_Dict_Set(dict, keyBox, value);
  Sere_DecRef(keyBox);
  Sere_DecRef(value);
}

Sere_Dict* Sere_ModuleInfo(void) {
  Sere_Dict* info = Sere_Dict_New();
  if (info == NULL) {
    return NULL;
  }
  dictPutOwned(info, "name", Sere_Str_FromCString(Sere_ModuleName()));
  Sere_List* functions = Sere_List_NewWithCapacity(g_entry_count);
  Sere_List* constants = Sere_List_NewWithCapacity(g_entry_count);
  Sere_List* types = Sere_List_NewWithCapacity(g_type_count);
  for (int32_t index = 0; index < g_entry_count; ++index) {
    const SereEntry* entry = &g_entries[index];
    if (entry->kind == SereEntryConstant) {
      if (constants != NULL) {
        Sere_List_Append(constants, Sere_Str_FromCString(entry->name));
        Sere_DecRef(Sere_List_Get(constants, Sere_List_Len(constants) - 1));
      }
    } else if (functions != NULL) {
      Sere_List_Append(functions, Sere_Str_FromCString(entry->name));
      Sere_DecRef(Sere_List_Get(functions, Sere_List_Len(functions) - 1));
    }
  }
  for (int32_t index = 0; index < g_type_count && types != NULL; ++index) {
    Sere_List_Append(types, Sere_Str_FromCString(g_types[index]));
    Sere_DecRef(Sere_List_Get(types, Sere_List_Len(types) - 1));
  }
  dictPutOwned(info, "functions", Sere_List_AsObject(functions));
  dictPutOwned(info, "types", Sere_List_AsObject(types));
  dictPutOwned(info, "constants", Sere_List_AsObject(constants));
  return info;
}

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------

static Sere_Attr* findAttr(const Sere_Object* object, const char* name) {
  if (object == NULL || name == NULL) {
    return NULL;
  }
  for (Sere_Attr* attr = object->attrs; attr != NULL; attr = attr->next) {
    if (strcmp(attr->name, name) == 0) {
      return attr;
    }
  }
  return NULL;
}

int32_t Sere_SetAttr(Sere_Object* object, const char* name, Sere_Object* value) {
  if (object == NULL || name == NULL) {
    return 0;
  }
  Sere_Attr* found = findAttr(object, name);
  if (found != NULL) {
    Sere_Object* previous = found->value;
    Sere_IncRef(value);
    found->value = value;
    Sere_DecRef(previous);
    return 1;
  }
  Sere_Attr* attr = (Sere_Attr*)calloc(1, sizeof(Sere_Attr));
  if (attr == NULL) {
    return 0;
  }
  attr->name = copyCString(name);
  if (attr->name == NULL) {
    free(attr);
    return 0;
  }
  Sere_IncRef(value);
  attr->value = value;
  // Appending keeps the walk in the order the attributes were set.
  Sere_Attr** link = &object->attrs;
  while (*link != NULL) {
    link = &(*link)->next;
  }
  *link = attr;
  return 1;
}

Sere_Object* Sere_GetAttr(Sere_Object* object, const char* name) {
  Sere_Attr* attr = findAttr(object, name);
  return attr == NULL ? NULL : attr->value;
}

int32_t Sere_HasAttr(Sere_Object* object, const char* name) {
  return findAttr(object, name) == NULL ? 0 : 1;
}

int32_t Sere_DelAttr(Sere_Object* object, const char* name) {
  if (object == NULL || name == NULL) {
    return 0;
  }
  Sere_Attr** link = &object->attrs;
  while (*link != NULL) {
    Sere_Attr* attr = *link;
    if (strcmp(attr->name, name) == 0) {
      *link = attr->next;
      free(attr->name);
      if (attr->value != object) {
        Sere_DecRef(attr->value);
      }
      free(attr);
      return 1;
    }
    link = &attr->next;
  }
  return 0;
}

void Sere_ClearAttrs(Sere_Object* object) {
  if (object == NULL) {
    return;
  }
  Sere_Attr* attr = object->attrs;
  object->attrs = NULL;
  while (attr != NULL) {
    Sere_Attr* next = attr->next;
    free(attr->name);
    if (attr->value != object) {
      Sere_DecRef(attr->value);
    }
    free(attr);
    attr = next;
  }
}

int64_t Sere_AttrCount(const Sere_Object* object) {
  int64_t count = 0;
  if (object == NULL) {
    return 0;
  }
  for (const Sere_Attr* attr = object->attrs; attr != NULL; attr = attr->next) {
    ++count;
  }
  return count;
}

const char* Sere_AttrNameAt(const Sere_Object* object, int64_t index) {
  if (object == NULL || index < 0) {
    return NULL;
  }
  int64_t position = 0;
  for (const Sere_Attr* attr = object->attrs; attr != NULL; attr = attr->next, ++position) {
    if (position == index) {
      return attr->name;
    }
  }
  return NULL;
}

Sere_Dict* Sere_AttrsAsDict(Sere_Object* object) {
  if (object == NULL) {
    return NULL;
  }
  Sere_Dict* dict = Sere_Dict_New();
  if (dict == NULL) {
    return NULL;
  }
  for (Sere_Attr* attr = object->attrs; attr != NULL; attr = attr->next) {
    Sere_Object* key = Sere_Str_FromCString(attr->name);
    if (key == NULL) {
      continue;
    }
    (void)Sere_Dict_Set(dict, key, attr->value);
    Sere_DecRef(key);
  }
  return dict;
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

int64_t Sere_List_ForEach(Sere_List* list, Sere_IterFn callback, void* user) {
  if (list == NULL) {
    return -1;
  }
  if (callback == NULL) {
    return Sere_List_Len(list);
  }
  const int64_t length = Sere_List_Len(list);
  int64_t visited = 0;
  for (int64_t index = 0; index < length; ++index) {
    ++visited;
    if (callback(Sere_List_Get(list, index), index, user) == 0) {
      break;
    }
  }
  return visited;
}

int64_t Sere_ForEach(Sere_Object* object, Sere_IterFn callback, void* user) {
  if (object == NULL) {
    return -1;
  }
  switch (object->kind) {
  case Sere_KindList:
    return Sere_List_ForEach((Sere_List*)object, callback, user);
  case Sere_KindDict: {
    Sere_Dict* dict = (Sere_Dict*)object;
    if (callback == NULL) {
      return dict->count;
    }
    int64_t visited = 0;
    for (int64_t slot = 0; slot < dict->capacity; ++slot) {
      for (Sere_DictEntry* entry = dict->buckets[slot]; entry != NULL; entry = entry->next) {
        ++visited;
        if (callback(entry->value, visited - 1, user) == 0) {
          return visited;
        }
      }
    }
    return visited;
  }
  case Sere_KindBytes: {
    if (callback == NULL) {
      return object->extra;
    }
    const unsigned char* data = (const unsigned char*)object->p;
    int64_t visited = 0;
    for (int64_t index = 0; index < object->extra; ++index) {
      Sere_Object* byte = Sere_Long_FromI32(data == NULL ? 0 : (int32_t)data[index]);
      ++visited;
      const int32_t keep = callback(byte, index, user);
      Sere_DecRef(byte);
      if (keep == 0) {
        break;
      }
    }
    return visited;
  }
  case Sere_KindStr: {
    if (callback == NULL) {
      return object->extra;
    }
    int64_t visited = 0;
    for (int64_t index = 0; index < object->extra; ++index) {
      const char* data = NULL;
      int64_t length = 0;
      sere_str_index(Sere_Str_AsCString(object), object->extra, index, &data, &length);
      Sere_Object* character = Sere_Str_FromData(data, length);
      ++visited;
      const int32_t keep = callback(character, index, user);
      Sere_DecRef(character);
      if (keep == 0) {
        break;
      }
    }
    return visited;
  }
  default:
    return -1;
  }
}

int64_t Sere_ForEachAttr(Sere_Object* object, Sere_IterFn callback, void* user) {
  if (object == NULL) {
    return -1;
  }
  if (callback == NULL) {
    return Sere_AttrCount(object);
  }
  int64_t visited = 0;
  for (Sere_Attr* attr = object->attrs; attr != NULL; attr = attr->next) {
    ++visited;
    if (callback(attr->value, visited - 1, user) == 0) {
      break;
    }
  }
  return visited;
}

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

/// Builds a string from a runtime (data, length) pair.
static Sere_Object* stringFromResult(const char* data, int64_t length) {
  if (data == NULL) {
    return Sere_Str_FromData("", 0);
  }
  return Sere_Str_FromData(data, length);
}

Sere_Object* Sere_Str_Concat(Sere_Object* left, Sere_Object* right) {
  if (!Sere_IsStr(left) || !Sere_IsStr(right)) {
    return NULL;
  }
  int64_t length = 0;
  const char* data = sere_str_concat_data(Sere_Str_AsCString(left),
                                          Sere_Str_Len(left),
                                          Sere_Str_AsCString(right),
                                          Sere_Str_Len(right),
                                          &length);
  return stringFromResult(data, length);
}

Sere_Object* Sere_Str_Slice(Sere_Object* value, int64_t start, int64_t end) {
  if (!Sere_IsStr(value)) {
    return NULL;
  }
  const char* data = NULL;
  int64_t length = 0;
  sere_str_slice(Sere_Str_AsCString(value),
                 Sere_Str_Len(value),
                 start,
                 end,
                 start >= 0 ? 1 : 0,
                 end >= 0 ? 1 : 0,
                 &data,
                 &length);
  return stringFromResult(data, length);
}

Sere_Object* Sere_Str_Upper(Sere_Object* value) {
  if (!Sere_IsStr(value)) {
    return NULL;
  }
  const char* data = NULL;
  int64_t length = 0;
  sere_string_upper(Sere_Str_AsCString(value), Sere_Str_Len(value), &data, &length);
  return stringFromResult(data, length);
}

Sere_Object* Sere_Str_Lower(Sere_Object* value) {
  if (!Sere_IsStr(value)) {
    return NULL;
  }
  const char* data = NULL;
  int64_t length = 0;
  sere_string_lower(Sere_Str_AsCString(value), Sere_Str_Len(value), &data, &length);
  return stringFromResult(data, length);
}

Sere_Object* Sere_Str_Strip(Sere_Object* value) {
  if (!Sere_IsStr(value)) {
    return NULL;
  }
  const char* data = NULL;
  int64_t length = 0;
  sere_string_strip(Sere_Str_AsCString(value), Sere_Str_Len(value), &data, &length);
  return stringFromResult(data, length);
}

Sere_Object* Sere_Str_Repeat(Sere_Object* value, int64_t count) {
  if (!Sere_IsStr(value)) {
    return NULL;
  }
  const char* data = NULL;
  int64_t length = 0;
  sere_string_repeat(Sere_Str_AsCString(value), Sere_Str_Len(value), count, &data, &length);
  return stringFromResult(data, length);
}

Sere_Object* Sere_Str_Replace(Sere_Object* value, const char* from, const char* to) {
  if (!Sere_IsStr(value) || from == NULL || to == NULL) {
    return NULL;
  }
  const char* data = NULL;
  int64_t length = 0;
  sere_string_replace(Sere_Str_AsCString(value),
                      Sere_Str_Len(value),
                      from,
                      (int64_t)strlen(from),
                      to,
                      (int64_t)strlen(to),
                      &data,
                      &length);
  return stringFromResult(data, length);
}

int32_t Sere_Str_Contains(Sere_Object* haystack, Sere_Object* needle) {
  if (!Sere_IsStr(haystack) || !Sere_IsStr(needle)) {
    return 0;
  }
  return sere_str_contains(Sere_Str_AsCString(haystack),
                           Sere_Str_Len(haystack),
                           Sere_Str_AsCString(needle),
                           Sere_Str_Len(needle));
}

int32_t Sere_Str_StartsWith(Sere_Object* value, const char* prefix) {
  if (!Sere_IsStr(value) || prefix == NULL) {
    return 0;
  }
  return sere_string_starts_with(
      Sere_Str_AsCString(value), Sere_Str_Len(value), prefix, (int64_t)strlen(prefix));
}

int32_t Sere_Str_EndsWith(Sere_Object* value, const char* suffix) {
  if (!Sere_IsStr(value) || suffix == NULL) {
    return 0;
  }
  return sere_string_ends_with(
      Sere_Str_AsCString(value), Sere_Str_Len(value), suffix, (int64_t)strlen(suffix));
}

Sere_Object* Sere_Str_Format(const char* format, ...) {
  if (format == NULL) {
    return NULL;
  }
  va_list measure;
  va_start(measure, format);
  const int wanted = vsnprintf(NULL, 0, format, measure);
  va_end(measure);
  if (wanted < 0) {
    return NULL;
  }
  char* buffer = (char*)malloc((size_t)wanted + 1);
  if (buffer == NULL) {
    return NULL;
  }
  va_list render;
  va_start(render, format);
  (void)vsnprintf(buffer, (size_t)wanted + 1, format, render);
  va_end(render);
  Sere_Object* result = Sere_Str_FromData(buffer, (int64_t)wanted);
  free(buffer);
  return result;
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

/// Scratch space for the generated type chain and the rendered message. A
/// pending error is a single slot anyway, so one buffer per purpose is enough.
static char g_chainBuffer[256];
static char g_messageBuffer[1024];

void Sere_Raise(const char* type_chain, const char* message) {
  const char* chain = type_chain == NULL ? "Exception" : type_chain;
  const char* text = message == NULL ? "" : message;
  sere_raise(chain, text, (int64_t)strlen(text));
}

void Sere_RaiseSimple(const char* type_name, const char* message) {
  if (type_name == NULL || strcmp(type_name, "Exception") == 0) {
    Sere_Raise("Exception", message);
    return;
  }
  (void)snprintf(g_chainBuffer, sizeof(g_chainBuffer), "%s;Exception", type_name);
  Sere_Raise(g_chainBuffer, message);
}

int32_t Sere_HasError(void) {
  return sere_has_error();
}

int32_t Sere_ErrorIs(const char* type_name) {
  return type_name == NULL ? 0 : sere_error_isa(type_name);
}

const char* Sere_ErrorType(void) {
  const char* type = sere_error_type();
  return type == NULL ? "" : type;
}

const char* Sere_ErrorMessage(void) {
  int64_t length = 0;
  const char* message = sere_error_message(&length);
  if (message == NULL || length <= 0) {
    return "";
  }
  size_t copied = (size_t)length;
  if (copied > sizeof(g_messageBuffer) - 1) {
    copied = sizeof(g_messageBuffer) - 1;
  }
  memcpy(g_messageBuffer, message, copied);
  g_messageBuffer[copied] = '\0';
  return g_messageBuffer;
}

void Sere_ClearError(void) {
  sere_clear_error();
}

int32_t Sere_CheckArity(const char* function, int32_t nargs, int32_t min_args, int32_t max_args) {
  if (nargs >= min_args && (max_args < 0 || nargs <= max_args)) {
    return 1;
  }
  const char* name = function == NULL ? "function" : function;
  char text[192];
  if (max_args < 0) {
    (void)snprintf(text,
                   sizeof(text),
                   "%s() takes at least %d arguments (%d given)",
                   name,
                   (int)min_args,
                   (int)nargs);
  } else if (min_args == max_args) {
    (void)snprintf(
        text, sizeof(text), "%s() takes %d arguments (%d given)", name, (int)min_args, (int)nargs);
  } else {
    (void)snprintf(text,
                   sizeof(text),
                   "%s() takes between %d and %d arguments (%d given)",
                   name,
                   (int)min_args,
                   (int)max_args,
                   (int)nargs);
  }
  Sere_RaiseSimple("TypeError", text);
  return 0;
}

int32_t Sere_CheckNumber(const char* function, const char* argument, Sere_Object* value) {
  if (Sere_IsNumber(value)) {
    return 1;
  }
  char text[192];
  (void)snprintf(text,
                 sizeof(text),
                 "%s() argument '%s' must be a number, got %s",
                 function == NULL ? "function" : function,
                 argument == NULL ? "value" : argument,
                 Sere_TypeName(value));
  Sere_RaiseSimple("TypeError", text);
  return 0;
}

// ---------------------------------------------------------------------------
// Calling back into Sere
// ---------------------------------------------------------------------------

const char* Sere_StatusName(int32_t status) {
  switch (status) {
  case Sere_StatusOk:
    return "ok";
  case Sere_StatusNotFound:
    return "not-found";
  case Sere_StatusError:
    return "error";
  case Sere_StatusTypeError:
    return "type-error";
  case Sere_StatusOutOfRange:
    return "out-of-range";
  case Sere_StatusNoMemory:
    return "out-of-memory";
  case Sere_StatusBadArgument:
    return "bad-argument";
  default:
    return "unknown";
  }
}

static int32_t invoke(Sere_CFunction function,
                      const char* name,
                      Sere_Object* const* args,
                      int32_t nargs,
                      Sere_Object** result) {
  Sere_Object* value = function(args, nargs);
  if (value == NULL) {
    if (!Sere_HasError()) {
      char text[192];
      (void)snprintf(
          text, sizeof(text), "%s() returned no value", name == NULL ? "function" : name);
      Sere_RaiseSimple("RuntimeError", text);
    }
    return Sere_StatusError;
  }
  if (result == NULL) {
    Sere_DecRef(value);
    return Sere_StatusOk;
  }
  *result = value;
  return Sere_StatusOk;
}

int32_t Sere_CallFunction(const char* name, Sere_Object* const* args, int32_t nargs,
                          Sere_Object** result) {
  return Sere_CallFunctionEx(name, args, nargs, result) == Sere_StatusOk ? 1 : 0;
}

int32_t Sere_CallFunctionEx(const char* name,
                            Sere_Object* const* args,
                            int32_t nargs,
                            Sere_Object** result) {
  const int32_t index = findEntry(name, SereEntryFunction, NULL);
  if (index < 0) {
    return Sere_StatusNotFound;
  }
  return invoke(g_entries[index].function, g_entries[index].name, args, nargs, result);
}

int32_t Sere_CallMethod(const char* type_name,
                        const char* name,
                        Sere_Object* const* args,
                        int32_t nargs,
                        Sere_Object** result) {
  const int32_t index = findEntry(name, SereEntryMethod, type_name);
  if (index < 0) {
    return 0;
  }
  return invoke(g_entries[index].function, g_entries[index].name, args, nargs, result) ==
                 Sere_StatusOk
             ? 1
             : 0;
}

int32_t Sere_CallObject(Sere_Object* callable,
                        Sere_Object* const* args,
                        int32_t nargs,
                        Sere_Object** result) {
  return Sere_CallObjectEx(callable, args, nargs, result) == Sere_StatusOk ? 1 : 0;
}

int32_t Sere_CallObjectEx(Sere_Object* callable,
                          Sere_Object* const* args,
                          int32_t nargs,
                          Sere_Object** result) {
  if (callable == NULL || callable->kind != Sere_KindFunction) {
    return Sere_StatusBadArgument;
  }
  const Sere_CFunction function = (Sere_CFunction)(uintptr_t)callable->p;
  if (function == NULL) {
    return Sere_StatusBadArgument;
  }
  if (callable->extra >= 0 && nargs != callable->extra) {
    (void)Sere_CheckArity(callable->name, nargs, callable->extra, callable->extra);
    return Sere_StatusTypeError;
  }
  return invoke(function, callable->name, args, nargs, result);
}

// ---------------------------------------------------------------------------
// Output and rendering
// ---------------------------------------------------------------------------

void Sere_Print(const char* text) {
  if (text != NULL) {
    sere_write(text, (int64_t)strlen(text));
  }
}

void Sere_PrintLn(const char* text) {
  Sere_Print(text);
  sere_write_nl();
}

void Sere_EPrint(const char* text) {
  if (text != NULL) {
    sere_io_eprint(text, (int64_t)strlen(text));
  }
  sere_io_eprint_nl();
}

void Sere_PrintObject(Sere_Object* object) {
  const char* text = renderRotating(object);
  sere_write(text, (int64_t)strlen(text));
  sere_write_nl();
}

const char* Sere_Repr(Sere_Object* object) {
  return renderRotating(object);
}

// ---------------------------------------------------------------------------
// Argument helpers
// ---------------------------------------------------------------------------

Sere_Object* Sere_Arg(Sere_Object* const* args, int32_t nargs, int32_t index) {
  if (args == NULL || index < 0 || index >= nargs) {
    return NULL;
  }
  return args[index];
}

int32_t Sere_ArgI32(Sere_Object* const* args, int32_t nargs, int32_t index, int32_t fallback) {
  Sere_Object* value = Sere_Arg(args, nargs, index);
  return value == NULL ? fallback : (int32_t)Sere_Number_AsI64(value);
}

int64_t Sere_ArgI64(Sere_Object* const* args, int32_t nargs, int32_t index, int64_t fallback) {
  Sere_Object* value = Sere_Arg(args, nargs, index);
  return value == NULL ? fallback : Sere_Number_AsI64(value);
}

double Sere_ArgF64(Sere_Object* const* args, int32_t nargs, int32_t index, double fallback) {
  Sere_Object* value = Sere_Arg(args, nargs, index);
  return value == NULL ? fallback : Sere_Number_AsF64(value);
}

int32_t Sere_ArgBool(Sere_Object* const* args, int32_t nargs, int32_t index, int32_t fallback) {
  Sere_Object* value = Sere_Arg(args, nargs, index);
  return value == NULL ? fallback : objectTruthy(value);
}

const char*
Sere_ArgCString(Sere_Object* const* args, int32_t nargs, int32_t index, const char* fallback) {
  Sere_Object* value = Sere_Arg(args, nargs, index);
  return Sere_IsStr(value) ? Sere_Str_AsCString(value) : fallback;
}

void* Sere_ArgVoid(Sere_Object* const* args, int32_t nargs, int32_t index, void* fallback) {
  Sere_Object* value = Sere_Arg(args, nargs, index);
  return Sere_IsPtr(value) ? Sere_Ptr_AsVoid(value) : fallback;
}

// ---------------------------------------------------------------------------
// References
// ---------------------------------------------------------------------------

void Sere_IncRef(Sere_Object* object) {
  if (object != NULL) {
    ++object->refs;
  }
}

void Sere_DecRef(Sere_Object* object) {
  if (object == NULL) {
    return;
  }
  --object->refs;
  if (object->refs > 0) {
    return;
  }
  // Attributes go first: releasing a value can release another object, and the
  // list must not still name the object being freed.
  Sere_Attr* attr = object->attrs;
  object->attrs = NULL;
  while (attr != NULL) {
    Sere_Attr* next = attr->next;
    free(attr->name);
    if (attr->value != object) {
      Sere_DecRef(attr->value);
    }
    free(attr);
    attr = next;
  }
  switch (object->kind) {
  case Sere_KindStr:
  case Sere_KindBytes:
    free(object->p);
    break;
  case Sere_KindPtr:
    if (object->finalizer != NULL) {
      object->finalizer(object->p);
    }
    break;
  case Sere_KindFunction:
    free(object->name);
    break;
  case Sere_KindList: {
    Sere_List* list = (Sere_List*)object;
    releaseListItems(list);
    freeListStore(list->items);
    break;
  }
  case Sere_KindDict: {
    Sere_Dict* dict = (Sere_Dict*)object;
    dictReleaseEntries(dict);
    free(dict->buckets);
    break;
  }
  default:
    break;
  }
  free(object);
}

void Sere_StoreResult(Sere_Object* object, Sere_Object** slot) {
  if (slot != NULL) {
    *slot = object;
  }
}

void Sere_ReleaseList(Sere_List* list) {
  if (list != NULL) {
    Sere_DecRef(&list->header);
  }
}

void Sere_ReleaseDict(Sere_Dict* dict) {
  if (dict != NULL) {
    Sere_DecRef(&dict->header);
  }
}
