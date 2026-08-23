/// @file sere_mod.c
/// Native module object model used by Sere_DefineFunction and Sere_List.

#include "sere/api/sere_mod.h"
#include "sere_rt.h"

#include <stdlib.h>
#include <string.h>

#define SERE_MOD_MAX_FUNCS 64

struct Sere_Object {
  int32_t kind;
  int32_t refs;
  int64_t i;
  double f;
  void* p;
  int64_t extra;
};

struct Sere_List {
  Sere_Object header;
  void* items;
};

typedef struct {
  const char* name;
  Sere_CFunction function;
  int32_t nargs;
} SereFuncEntry;

static SereFuncEntry g_funcs[SERE_MOD_MAX_FUNCS];
static int32_t g_func_count = 0;

static Sere_Object* makeObject(int32_t kind) {
  Sere_Object* object = (Sere_Object*)calloc(1, sizeof(Sere_Object));
  if (object == NULL) {
    return NULL;
  }
  object->kind = kind;
  object->refs = 1;
  return object;
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

Sere_Object* Sere_Float_FromF64(double value) {
  Sere_Object* object = makeObject(Sere_KindF64);
  if (object != NULL) {
    object->f = value;
  }
  return object;
}

Sere_Object* Sere_Str_FromCString(const char* text) {
  Sere_Object* object = makeObject(Sere_KindStr);
  if (object == NULL) {
    return NULL;
  }
  const char* source = text == NULL ? "" : text;
  const size_t length = strlen(source);
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    free(object);
    return NULL;
  }
  memcpy(copy, source, length + 1);
  object->p = copy;
  object->extra = (int64_t)length;
  return object;
}

Sere_Object* Sere_Ptr_FromVoid(void* pointer) {
  Sere_Object* object = makeObject(Sere_KindPtr);
  if (object != NULL) {
    object->p = pointer;
  }
  return object;
}

int32_t Sere_Kind(const Sere_Object* object) {
  return object == NULL ? Sere_KindNone : object->kind;
}

int32_t Sere_Long_AsI32(const Sere_Object* object) {
  return object == NULL ? 0 : (int32_t)object->i;
}

int64_t Sere_Long_AsI64(const Sere_Object* object) {
  return object == NULL ? 0 : object->i;
}

double Sere_Float_AsF64(const Sere_Object* object) {
  return object == NULL ? 0.0 : object->f;
}

const char* Sere_Str_AsCString(const Sere_Object* object) {
  if (object == NULL || object->p == NULL) {
    return "";
  }
  return (const char*)object->p;
}

void* Sere_Ptr_AsVoid(const Sere_Object* object) {
  return object == NULL ? NULL : object->p;
}

Sere_List* Sere_List_New(void) {
  Sere_List* list = (Sere_List*)calloc(1, sizeof(Sere_List));
  if (list == NULL) {
    return NULL;
  }
  list->header.kind = Sere_KindList;
  list->header.refs = 1;
  list->items = sere_list_new((int64_t)sizeof(Sere_Object*));
  return list;
}

int32_t Sere_List_Append(Sere_List* list, Sere_Object* item) {
  if (list == NULL || list->items == NULL) {
    return 0;
  }
  sere_list_push(list->items, &item);
  return 1;
}

int64_t Sere_List_Len(const Sere_List* list) {
  return list == NULL ? 0 : sere_list_len(list->items);
}

Sere_Object* Sere_List_Get(Sere_List* list, int64_t index) {
  if (list == NULL || list->items == NULL) {
    return NULL;
  }
  Sere_Object** slot = (Sere_Object**)sere_list_item(list->items, index);
  return slot == NULL ? NULL : *slot;
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

void Sere_DefineFunction(const char* name, Sere_CFunction function, int32_t nargs) {
  if (name == NULL || function == NULL || g_func_count >= SERE_MOD_MAX_FUNCS) {
    return;
  }
  g_funcs[g_func_count].name = name;
  g_funcs[g_func_count].function = function;
  g_funcs[g_func_count].nargs = nargs;
  ++g_func_count;
}

Sere_CFunction Sere_FindFunction(const char* name) {
  if (name == NULL) {
    return NULL;
  }
  for (int32_t index = 0; index < g_func_count; ++index) {
    if (strcmp(g_funcs[index].name, name) == 0) {
      return g_funcs[index].function;
    }
  }
  return NULL;
}

int32_t Sere_CallFunction(const char* name, Sere_Object* const* args, int32_t nargs,
                          Sere_Object** result) {
  Sere_CFunction function = Sere_FindFunction(name);
  if (function == NULL) {
    return 0;
  }
  Sere_Object* value = function(args, nargs);
  if (result != NULL) {
    *result = value;
  }
  return 1;
}

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
  if (object->kind == Sere_KindStr && object->p != NULL) {
    free(object->p);
  }
  free(object);
}
