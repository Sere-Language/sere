/// @file sere_mod.h
/// C/C++ extension API for native Sere modules.
///
/// A native module is a static or shared library that exports boxed functions
/// and registers them from `sere_mod_init`, which the generated `main` calls
/// before any Sere global runs.
///
/// Typical module:
///   static Sere_Object* add(Sere_Object* const* args, int32_t nargs) {
///     if (!Sere_CheckArity("add", nargs, 2, 2)) return NULL;
///     return Sere_Long_FromI64(Sere_ArgI64(args, nargs, 0, 0) +
///                              Sere_ArgI64(args, nargs, 1, 0));
///   }
///   SERE_MOD_EXPORT void sere_mod_init(void) { Sere_DefineFunction("add", add, 2); }
///
/// Link the compiled library with: sere src/main.sere --link libs/native.lib
///
/// Every function here is defined by the Sere runtime, so a module needs
/// nothing but this header and the runtime library the compiler links with it.
///
/// Ownership: a constructor or an accessor that *builds* a value returns a new
/// reference. Hand it to Sere and stop using it, or call `Sere_DecRef` to drop
/// it. An accessor that reaches a value *through* a container, such as
/// `Sere_List_Get` or `Sere_Dict_Get`, returns a borrowed reference the
/// container still owns.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Marks a symbol as exported, which shared-library modules need:
///   SERE_MOD_EXPORT void sere_mod_init(void) { ... }
#if defined(_WIN32)
#  define SERE_MOD_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#  define SERE_MOD_EXPORT __attribute__((visibility("default")))
#else
#  define SERE_MOD_EXPORT
#endif

typedef struct Sere_Object Sere_Object;
typedef struct Sere_List Sere_List;
typedef struct Sere_Dict Sere_Dict;

/// Kinds `Sere_Kind` reports. The numeric values are the ABI contract shared
/// with the runtime; boxed kinds only ever appear through this API.
enum Sere_Kind {
  Sere_KindNone = 0,
  Sere_KindBool = 1,
  Sere_KindI32 = 2,
  Sere_KindI64 = 3,
  Sere_KindF64 = 4,
  Sere_KindStr = 5,
  Sere_KindList = 6,
  Sere_KindPtr = 7,
  Sere_KindDict = 8,
  Sere_KindBytes = 9,
  Sere_KindFunction = 10,
};

/// Status codes for the `*Ex` entry points. Zero is success, so the common
/// `if (Sere_CallFunctionEx(...) != Sere_StatusOk)` reads naturally.
enum Sere_Status {
  Sere_StatusOk = 0,
  Sere_StatusNotFound = 1,
  Sere_StatusError = 2,
  Sere_StatusTypeError = 3,
  Sere_StatusOutOfRange = 4,
  Sere_StatusNoMemory = 5,
  Sere_StatusBadArgument = 6,
};

typedef Sere_Object* (*Sere_CFunction)(Sere_Object* const* args, int32_t nargs);

/// Runs when the last reference to a pointer box is released. The runtime does
/// not free the pointer itself; the finalizer decides how.
typedef void (*Sere_Finalizer)(void* pointer);

/// Called by `Sere_ForEach` for each element. Return 0 to stop the walk.
typedef int32_t (*Sere_IterFn)(Sere_Object* item, int64_t index, void* user);

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Sere_Object* Sere_None_New(void);
Sere_Object* Sere_Bool_FromI32(int32_t value);
Sere_Object* Sere_Long_FromI32(int32_t value);
Sere_Object* Sere_Long_FromI64(int64_t value);
Sere_Object* Sere_Long_FromU64(uint64_t value);
Sere_Object* Sere_Float_FromF64(double value);
Sere_Object* Sere_Str_FromCString(const char* text);
Sere_Object* Sere_Str_FromData(const char* data, int64_t length);
Sere_Object* Sere_Bytes_New(int64_t length);
Sere_Object* Sere_Bytes_FromData(const void* data, int64_t length);
Sere_Object* Sere_Ptr_FromVoid(void* pointer);
/// A pointer box that runs `finalizer` when its last reference is released,
/// which is how a C-owned resource rides along with a Sere value.
Sere_Object* Sere_Ptr_FromVoidOwned(void* pointer, Sere_Finalizer finalizer);
/// A callable box around a C function, for building callback tables.
Sere_Object* Sere_Function_FromCFunction(const char* name,
                                         Sere_CFunction function,
                                         int32_t nargs);
/// The boxed function registered under `name`, or NULL.
Sere_Object* Sere_Function_FromName(const char* name);

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------

int32_t Sere_Kind(const Sere_Object* object);
int32_t Sere_IsNone(const Sere_Object* object);
int32_t Sere_IsBool(const Sere_Object* object);
int32_t Sere_IsLong(const Sere_Object* object);
int32_t Sere_IsFloat(const Sere_Object* object);
int32_t Sere_IsNumber(const Sere_Object* object);
int32_t Sere_IsStr(const Sere_Object* object);
int32_t Sere_IsBytes(const Sere_Object* object);
int32_t Sere_IsList(const Sere_Object* object);
int32_t Sere_IsDict(const Sere_Object* object);
int32_t Sere_IsPtr(const Sere_Object* object);
int32_t Sere_IsCallable(const Sere_Object* object);
/// The Sere spelling of a value's type: "None", "bool", "i32", "i64", "f64",
/// "str", "bytes", "list", "dict", "ptr", or "function".
const char* Sere_TypeName(const Sere_Object* object);
/// Truthiness with Sere's rules: `None` is false, zero numbers are false, an
/// empty string, list, dict, or byte buffer is false, everything else is true.
int32_t Sere_Truthy(const Sere_Object* object);

// ---------------------------------------------------------------------------
// Extraction
// ---------------------------------------------------------------------------

int32_t Sere_Long_AsI32(const Sere_Object* object);
int64_t Sere_Long_AsI64(const Sere_Object* object);
/// Reads any numeric kind as a 64-bit integer, truncating like a cast does.
int64_t Sere_Number_AsI64(const Sere_Object* object);
double Sere_Float_AsF64(const Sere_Object* object);
/// Reads any numeric kind as a double, widening integers when needed.
double Sere_Number_AsF64(const Sere_Object* object);
/// The value's UTF-8 bytes, or "" for anything that is not a string.
const char* Sere_Str_AsCString(const Sere_Object* object);
/// The value's bytes and length, or (NULL, 0) for anything that is not a
/// string. The data is NUL-terminated even though `out_length` excludes it.
const char* Sere_Str_Data(const Sere_Object* object, int64_t* out_length);
int64_t Sere_Str_Len(const Sere_Object* object);
/// A writable view of a byte buffer, or NULL for anything else.
void* Sere_Bytes_Data(const Sere_Object* object);
int64_t Sere_Bytes_Len(const Sere_Object* object);
void* Sere_Ptr_AsVoid(const Sere_Object* object);

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

/// Sere's `==`: strings and byte buffers compare by content, the integer kinds
/// compare by value, and lists, dicts, and pointers compare by identity.
int32_t Sere_Equal(const Sere_Object* left, const Sere_Object* right);
/// A content hash consistent with `Sere_Equal`, for dict keys and caches.
int64_t Sere_Hash(const Sere_Object* object);
/// Orders two values the way `<` does, writing -1, 0, or 1 to `out_order`.
/// Reports 0 when the two values cannot be ordered.
int32_t Sere_Compare(const Sere_Object* left, const Sere_Object* right, int32_t* out_order);

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

Sere_Object* Sere_Str_Concat(Sere_Object* left, Sere_Object* right);
Sere_Object* Sere_Str_Slice(Sere_Object* value, int64_t start, int64_t end);
Sere_Object* Sere_Str_Upper(Sere_Object* value);
Sere_Object* Sere_Str_Lower(Sere_Object* value);
Sere_Object* Sere_Str_Strip(Sere_Object* value);
Sere_Object* Sere_Str_Repeat(Sere_Object* value, int64_t count);
Sere_Object* Sere_Str_Replace(Sere_Object* value, const char* from, const char* to);
int32_t Sere_Str_Contains(Sere_Object* haystack, Sere_Object* needle);
int32_t Sere_Str_StartsWith(Sere_Object* value, const char* prefix);
int32_t Sere_Str_EndsWith(Sere_Object* value, const char* suffix);
/// Builds a string the way C's `snprintf` would, covering the usual printf
/// specifiers, and returns NULL when the conversion fails.
Sere_Object* Sere_Str_Format(const char* format, ...);

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

Sere_List* Sere_List_New(void);
Sere_List* Sere_List_NewWithCapacity(int64_t capacity);
int32_t Sere_List_Append(Sere_List* list, Sere_Object* item);
/// Boxes `pointer`, appends it, and returns the box. The list holds its own
/// reference, so the caller may either keep the box or `Sere_DecRef` it.
Sere_Object* Sere_List_AppendPtr(Sere_List* list, void* pointer);
int64_t Sere_List_Len(const Sere_List* list);
/// A borrowed item, or NULL when the index is out of range.
Sere_Object* Sere_List_Get(Sere_List* list, int64_t index);
int32_t Sere_List_Set(Sere_List* list, int64_t index, Sere_Object* item);
int32_t Sere_List_Insert(Sere_List* list, int64_t index, Sere_Object* item);
int32_t Sere_List_RemoveAt(Sere_List* list, int64_t index);
/// Removes the item at `index`, or the last one when `index` is negative, and
/// returns it as a new reference. NULL when the list is empty.
Sere_Object* Sere_List_Pop(Sere_List* list, int64_t index);
void Sere_List_Clear(Sere_List* list);
void Sere_List_Reverse(Sere_List* list);
/// Index of the first item equal to `item`, or -1.
int64_t Sere_List_IndexOf(Sere_List* list, Sere_Object* item);
int64_t Sere_List_Count(Sere_List* list, Sere_Object* item);
int32_t Sere_List_Contains(Sere_List* list, Sere_Object* item);
/// Appends every element of `other`.
int32_t Sere_List_Extend(Sere_List* list, Sere_List* other);
/// A new list holding `list[start:end]`. A negative bound means "not given",
/// so `Sere_List_Slice(list, 0, -1)` copies the whole list.
Sere_List* Sere_List_Slice(Sere_List* list, int64_t start, int64_t end);
Sere_List* Sere_List_Copy(Sere_List* list);
Sere_Object* Sere_List_AsObject(Sere_List* list);
/// Borrows the list inside a boxed value, or NULL when the value is not one.
Sere_List* Sere_List_FromObject(Sere_Object* object);

// ---------------------------------------------------------------------------
// Dicts
// ---------------------------------------------------------------------------

/// Keys are compared with `Sere_Equal`, so integers, strings, and byte buffers
/// key by value and lists, dicts, and pointers key by identity. Iteration order
/// is unspecified.
Sere_Dict* Sere_Dict_New(void);
int32_t Sere_Dict_Set(Sere_Dict* dict, Sere_Object* key, Sere_Object* value);
/// A borrowed value, or NULL when the key is absent. Use `Sere_Dict_Has` to
/// tell a missing key from a key stored with `None`.
Sere_Object* Sere_Dict_Get(Sere_Dict* dict, Sere_Object* key);
int32_t Sere_Dict_Has(Sere_Dict* dict, Sere_Object* key);
int32_t Sere_Dict_Del(Sere_Dict* dict, Sere_Object* key);
int64_t Sere_Dict_Len(const Sere_Dict* dict);
void Sere_Dict_Clear(Sere_Dict* dict);
Sere_Dict* Sere_Dict_Copy(Sere_Dict* dict);
Sere_List* Sere_Dict_Keys(Sere_Dict* dict);
Sere_List* Sere_Dict_Values(Sere_Dict* dict);
Sere_Object* Sere_Dict_AsObject(Sere_Dict* dict);
Sere_Dict* Sere_Dict_FromObject(Sere_Object* object);

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------

/// Attributes are the C side of `object.field`: a name/value table any boxed
/// value can carry, walked by `Sere_ForEachAttr` and exportable as a dict.
int32_t Sere_SetAttr(Sere_Object* object, const char* name, Sere_Object* value);
/// A borrowed value, or NULL when the attribute is absent.
Sere_Object* Sere_GetAttr(Sere_Object* object, const char* name);
int32_t Sere_HasAttr(Sere_Object* object, const char* name);
int32_t Sere_DelAttr(Sere_Object* object, const char* name);
void Sere_ClearAttrs(Sere_Object* object);
int64_t Sere_AttrCount(const Sere_Object* object);
/// The name of the attribute at `index` in insertion order, or NULL.
const char* Sere_AttrNameAt(const Sere_Object* object, int64_t index);
/// The attributes as a dict, so a C-built value inspects and prints like a
/// Sere record.
Sere_Dict* Sere_AttrsAsDict(Sere_Object* object);

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

/// Walks a list, dict, byte buffer, or string (as one-character strings) and
/// calls `callback` with a borrowed item and its index. Returns the number of
/// items visited, or -1 when the value is not iterable. A callback that returns
/// 0 stops the walk.
int64_t Sere_ForEach(Sere_Object* object, Sere_IterFn callback, void* user);
int64_t Sere_List_ForEach(Sere_List* list, Sere_IterFn callback, void* user);
/// Walks the attributes in insertion order; the callback receives the value and
/// may read the matching name with `Sere_AttrNameAt`.
int64_t Sere_ForEachAttr(Sere_Object* object, Sere_IterFn callback, void* user);

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

/// Raises through the pending error state, the same path `raise` takes. The
/// chain runs from the most specific type to `Exception`, separated by `;`, as
/// in "ValueError;Exception", so both `except ValueError` and `except
/// Exception` match. Use `Sere_RaiseSimple` to have ";Exception" appended.
void Sere_Raise(const char* type_chain, const char* message);
void Sere_RaiseSimple(const char* type_name, const char* message);
int32_t Sere_HasError(void);
int32_t Sere_ErrorIs(const char* type_name);
const char* Sere_ErrorType(void);
/// The pending message as a C string in a rotating buffer, or "" when there is
/// no error. Copy it before the next error call.
const char* Sere_ErrorMessage(void);
void Sere_ClearError(void);
/// Raises a TypeError naming `function` when `nargs` falls outside
/// `[min_args, max_args]`, and reports whether the count is acceptable. Pass -1
/// for `max_args` to accept any count above `min_args`.
int32_t Sere_CheckArity(const char* function, int32_t nargs, int32_t min_args, int32_t max_args);
/// Raises a TypeError unless `value` is a number.
int32_t Sere_CheckNumber(const char* function, const char* argument, Sere_Object* value);

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void Sere_DefineFunction(const char* name, Sere_CFunction function, int32_t nargs);
/// Registers a function that inspects `nargs` itself; its arity is reported as
/// -1 so callers know not to check it.
void Sere_DefineFunctionVariadic(const char* name, Sere_CFunction function);
void Sere_DefineMethod(const char* type_name,
                       const char* name,
                       Sere_CFunction function,
                       int32_t nargs);
/// Declares a type this module owns, so `Sere_DefineMethod` can attach methods
/// to it and `Sere_TypeDefined` reports it.
void Sere_DefineType(const char* name);
void Sere_DefineConstant(const char* name, Sere_Object* value);
Sere_CFunction Sere_FindFunction(const char* name);
Sere_CFunction Sere_FindMethod(const char* type_name, const char* name);
/// Writes the registered arity of `name` and reports whether it is registered.
int32_t Sere_FunctionArity(const char* name, int32_t* out_nargs);
int32_t Sere_TypeDefined(const char* name);
Sere_Object* Sere_FindConstant(const char* name);
int32_t Sere_RegisteredCount(void);
const char* Sere_RegisteredName(int32_t index);
int32_t Sere_RegisteredKind(int32_t index);
/// Names this module so diagnostics and `Sere_ModuleInfo` can report it.
void Sere_SetModuleName(const char* name);
const char* Sere_ModuleName(void);
/// A dict describing the module: "name", "functions", "types", "constants".
Sere_Dict* Sere_ModuleInfo(void);

// ---------------------------------------------------------------------------
// Calling back into Sere
// ---------------------------------------------------------------------------

int32_t
Sere_CallFunction(const char* name, Sere_Object* const* args, int32_t nargs, Sere_Object** result);
int32_t Sere_CallMethod(const char* type_name,
                        const char* name,
                        Sere_Object* const* args,
                        int32_t nargs,
                        Sere_Object** result);
/// Calls a value built by `Sere_Function_FromCFunction` or
/// `Sere_Function_FromName`.
int32_t Sere_CallObject(Sere_Object* callable,
                        Sere_Object* const* args,
                        int32_t nargs,
                        Sere_Object** result);
/// The reporting form: a status instead of a bare 0/1, with `*result` left
/// untouched on failure.
int32_t Sere_CallFunctionEx(const char* name,
                            Sere_Object* const* args,
                            int32_t nargs,
                            Sere_Object** result);
int32_t Sere_CallObjectEx(Sere_Object* callable,
                          Sere_Object* const* args,
                          int32_t nargs,
                          Sere_Object** result);
const char* Sere_StatusName(int32_t status);

// ---------------------------------------------------------------------------
// Output and rendering
// ---------------------------------------------------------------------------

void Sere_Print(const char* text);
void Sere_PrintLn(const char* text);
/// Writes to stderr and ends the line.
void Sere_EPrint(const char* text);
/// Prints a value the way Sere's `print` does, followed by a newline.
void Sere_PrintObject(Sere_Object* object);
/// Renders a value the way Sere's `repr` does: strings gain quotes, lists and
/// dicts recurse, and pointers render as `<ptr 0x...>`. The result lives in a
/// rotating buffer of four slots, so copy it before the fifth call.
const char* Sere_Repr(Sere_Object* object);

// ---------------------------------------------------------------------------
// Argument helpers
// ---------------------------------------------------------------------------

/// The argument at `index`, or NULL when the caller passed too few.
Sere_Object* Sere_Arg(Sere_Object* const* args, int32_t nargs, int32_t index);
int32_t Sere_ArgI32(Sere_Object* const* args, int32_t nargs, int32_t index, int32_t fallback);
int64_t Sere_ArgI64(Sere_Object* const* args, int32_t nargs, int32_t index, int64_t fallback);
double Sere_ArgF64(Sere_Object* const* args, int32_t nargs, int32_t index, double fallback);
int32_t Sere_ArgBool(Sere_Object* const* args, int32_t nargs, int32_t index, int32_t fallback);
/// The argument's bytes, or `fallback` when it is missing or not a string.
const char* Sere_ArgCString(Sere_Object* const* args,
                            int32_t nargs,
                            int32_t index,
                            const char* fallback);
void* Sere_ArgVoid(Sere_Object* const* args, int32_t nargs, int32_t index, void* fallback);

// ---------------------------------------------------------------------------
// References
// ---------------------------------------------------------------------------

void Sere_IncRef(Sere_Object* object);
void Sere_DecRef(Sere_Object* object);
/// Stores `value` through `slot`, the way a Sere out-parameter does.
void Sere_StoreResult(Sere_Object* object, Sere_Object** slot);
void Sere_ReleaseList(Sere_List* list);
void Sere_ReleaseDict(Sere_Dict* dict);

/// The entry point every native module exports. The generated `main` calls it
/// before Sere globals are created, so register everything from here.
void sere_mod_init(void);

#ifdef __cplusplus
}
#endif
