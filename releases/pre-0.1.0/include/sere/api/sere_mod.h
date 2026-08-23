/// @file sere_mod.h
/// C/C++ extension API for native Sere modules.
///
/// Typical module:
///   static Sere_Object* add(Sere_Object* const* args, int32_t nargs) { ... }
///   extern "C" void sere_mod_init(void) { Sere_DefineFunction("add", add, 2); }
///
/// Link the compiled library with: sere src/main.sere --link libs/native.lib

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Sere_Object Sere_Object;
typedef struct Sere_List Sere_List;

enum Sere_Kind {
  Sere_KindNone = 0,
  Sere_KindBool = 1,
  Sere_KindI32 = 2,
  Sere_KindI64 = 3,
  Sere_KindF64 = 4,
  Sere_KindStr = 5,
  Sere_KindList = 6,
  Sere_KindPtr = 7,
};

typedef Sere_Object* (*Sere_CFunction)(Sere_Object* const* args, int32_t nargs);

Sere_Object* Sere_None_New(void);
Sere_Object* Sere_Bool_FromI32(int32_t value);
Sere_Object* Sere_Long_FromI32(int32_t value);
Sere_Object* Sere_Long_FromI64(int64_t value);
Sere_Object* Sere_Float_FromF64(double value);
Sere_Object* Sere_Str_FromCString(const char* text);
Sere_Object* Sere_Ptr_FromVoid(void* pointer);

int32_t Sere_Kind(const Sere_Object* object);
int32_t Sere_Long_AsI32(const Sere_Object* object);
int64_t Sere_Long_AsI64(const Sere_Object* object);
double Sere_Float_AsF64(const Sere_Object* object);
const char* Sere_Str_AsCString(const Sere_Object* object);
void* Sere_Ptr_AsVoid(const Sere_Object* object);

Sere_List* Sere_List_New(void);
int32_t Sere_List_Append(Sere_List* list, Sere_Object* item);
int64_t Sere_List_Len(const Sere_List* list);
Sere_Object* Sere_List_Get(Sere_List* list, int64_t index);
Sere_Object* Sere_List_AsObject(Sere_List* list);
Sere_List* Sere_List_FromObject(Sere_Object* object);

void Sere_DefineFunction(const char* name, Sere_CFunction function, int32_t nargs);
Sere_CFunction Sere_FindFunction(const char* name);
int32_t Sere_CallFunction(const char* name, Sere_Object* const* args, int32_t nargs,
                          Sere_Object** result);

void Sere_IncRef(Sere_Object* object);
void Sere_DecRef(Sere_Object* object);

void sere_mod_init(void);

#ifdef __cplusplus
}
#endif
