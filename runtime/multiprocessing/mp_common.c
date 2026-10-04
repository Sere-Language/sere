/// @file mp_common.c
/// Platform-neutral half of the multiprocessing runtime: the wire codec, the
/// spawnable-target registry, child bootstrap, the child registry, and the
/// public `sere_mp_*` entry points declared in `sere_rt.h`.
///
/// The wire format is a tagged, self-describing encoding of the values a
/// `Process`, `Queue` or `Pipe` can carry. Every value records its Sere type
/// name so the receiving process rebuilds a boxed `Any` whose `typeof` matches
/// the sender's, and casts keep working across the process boundary.

// Must precede every system header: the UCRT marks `getenv` deprecated unless
// this is defined before <stdlib.h> is first pulled in.
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "mp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static char gLastError[512];

void mpSetLastError(const char* message) {
  if (message == NULL) {
    gLastError[0] = '\0';
    return;
  }
  size_t n = strlen(message);
  if (n >= sizeof(gLastError)) {
    n = sizeof(gLastError) - 1;
  }
  memcpy(gLastError, message, n);
  gLastError[n] = '\0';
}

const char* mpLastError(void) { return gLastError; }

static void* mpAlloc(int64_t size) { return sere_alloc((uint64_t)(size <= 0 ? 1 : size)); }

/* ------------------------------------------------------------------ */
/* Byte buffer and reader                                              */
/* ------------------------------------------------------------------ */

typedef struct {
  uint8_t* data;
  int64_t len;
  int64_t cap;
} MpBuf;

static int32_t bufReserve(MpBuf* buf, int64_t extra) {
  if (buf->len + extra <= buf->cap) {
    return 1;
  }
  int64_t cap = buf->cap == 0 ? 64 : buf->cap;
  while (cap < buf->len + extra) {
    cap *= 2;
  }
  uint8_t* grown = (uint8_t*)realloc(buf->data, (size_t)cap);
  if (grown == NULL) {
    return 0;
  }
  buf->data = grown;
  buf->cap = cap;
  return 1;
}

static int32_t bufPut(MpBuf* buf, const void* data, int64_t len) {
  if (len <= 0) {
    return 1;
  }
  if (!bufReserve(buf, len)) {
    return 0;
  }
  memcpy(buf->data + buf->len, data, (size_t)len);
  buf->len += len;
  return 1;
}

static int32_t bufPutU8(MpBuf* buf, uint8_t value) { return bufPut(buf, &value, 1); }

static int32_t bufPutVarint(MpBuf* buf, uint64_t value) {
  uint8_t scratch[10];
  int64_t n = 0;
  do {
    uint8_t byte = (uint8_t)(value & 0x7f);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    scratch[n++] = byte;
  } while (value != 0);
  return bufPut(buf, scratch, n);
}

typedef struct {
  const uint8_t* data;
  int64_t len;
  int64_t pos;
} MpReader;

static int32_t readerU8(MpReader* reader, uint8_t* out) {
  if (reader->pos + 1 > reader->len) {
    return 0;
  }
  *out = reader->data[reader->pos++];
  return 1;
}

static int32_t readerVarint(MpReader* reader, uint64_t* out) {
  uint64_t value = 0;
  int shift = 0;
  for (;;) {
    uint8_t byte = 0;
    if (!readerU8(reader, &byte)) {
      return 0;
    }
    value |= (uint64_t)(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      break;
    }
    shift += 7;
    if (shift > 63) {
      return 0;
    }
  }
  *out = value;
  return 1;
}

static int32_t readerBytes(MpReader* reader, void* out, int64_t len) {
  if (len < 0 || reader->pos + len > reader->len) {
    return 0;
  }
  if (len > 0) {
    memcpy(out, reader->data + reader->pos, (size_t)len);
  }
  reader->pos += len;
  return 1;
}

/* ------------------------------------------------------------------ */
/* Type names                                                          */
/* ------------------------------------------------------------------ */

/// Interned type-name strings. The boxed `Any` the codec returns points at
/// these, so they must never move or be freed.
typedef struct {
  char** items;
  int64_t count;
  int64_t cap;
} MpNamePool;

static MpNamePool gNames;

static const char* internName(const char* text, int64_t len) {
  if (text == NULL) {
    return NULL;
  }
  char* owned = (char*)malloc((size_t)(len < 0 ? (int64_t)strlen(text) : len) + 1);
  if (owned == NULL) {
    return NULL;
  }
  const int64_t n = len < 0 ? (int64_t)strlen(text) : len;
  memcpy(owned, text, (size_t)n);
  owned[n] = '\0';
  if (gNames.count == gNames.cap) {
    const int64_t cap = gNames.cap == 0 ? 32 : gNames.cap * 2;
    char** grown = (char**)realloc(gNames.items, (size_t)cap * sizeof(char*));
    if (grown == NULL) {
      free(owned);
      return NULL;
    }
    gNames.items = grown;
    gNames.cap = cap;
  }
  gNames.items[gNames.count++] = owned;
  return owned;
}

enum {
  MpKindAny = 0,
  MpKindI8 = 1,
  MpKindI16 = 2,
  MpKindI32 = 3,
  MpKindI64 = 4,
  MpKindU8 = 5,
  MpKindU16 = 6,
  MpKindU32 = 7,
  MpKindU64 = 8,
  MpKindF32 = 9,
  MpKindF64 = 10,
  MpKindBool = 11,
  MpKindStr = 12,
};

static int64_t kindSize(int32_t kind) {
  switch (kind) {
  case MpKindBool:
  case MpKindI8:
  case MpKindU8:
    return 1;
  case MpKindI16:
  case MpKindU16:
    return 2;
  case MpKindI32:
  case MpKindU32:
  case MpKindF32:
    return 4;
  case MpKindI64:
  case MpKindU64:
  case MpKindF64:
    return 8;
  case MpKindStr:
    return (int64_t)sizeof(SereStr);
  case MpKindAny:
  default:
    return (int64_t)sizeof(MpAny);
  }
}

/// The scalar kind a bare type name denotes, or -1 when it is not a scalar.
static int32_t kindOfScalarName(const char* name, int64_t len) {
  struct Entry {
    const char* name;
    int32_t kind;
  };
  static const struct Entry kTable[] = {
      {"bool", MpKindBool}, {"i8", MpKindI8},   {"i16", MpKindI16}, {"i32", MpKindI32},
      {"i64", MpKindI64},   {"u8", MpKindU8},   {"u16", MpKindU16}, {"u32", MpKindU32},
      {"u64", MpKindU64},   {"byte", MpKindU8}, {"f32", MpKindF32}, {"f64", MpKindF64},
      {"str", MpKindStr},
  };
  for (size_t index = 0; index < sizeof(kTable) / sizeof(kTable[0]); ++index) {
    if (strlen(kTable[index].name) == (size_t)len &&
        memcmp(kTable[index].name, name, (size_t)len) == 0) {
      return kTable[index].kind;
    }
  }
  return -1;
}

/// True when `name` starts with `prefix` and ends with `]`.
static int32_t genericArity(const char* name, const char* prefix, const char** innerStart,
                            const char** innerEnd) {
  const size_t plen = strlen(prefix);
  const size_t total = strlen(name);
  if (total < plen + 2 || strncmp(name, prefix, plen) != 0 || name[total - 1] != ']' ||
      name[plen] != '[') {
    return 0;
  }
  *innerStart = name + plen + 1;
  *innerEnd = name + total - 1;
  return 1;
}

/// Splits a generic argument list at the top level (respecting nesting).
static int32_t splitTypeArgs(const char* start, const char* end, const char** pieces,
                             int32_t maxPieces, int32_t* outCount) {
  int32_t count = 0;
  int32_t depth = 0;
  const char* pieceStart = start;
  for (const char* cursor = start; cursor <= end; ++cursor) {
    if (cursor == end || (*cursor == ',' && depth == 0)) {
      if (count >= maxPieces) {
        return 0;
      }
      pieces[count++] = pieceStart;
      pieceStart = cursor + 1;
      continue;
    }
    if (*cursor == '[' || *cursor == '(') {
      ++depth;
    } else if (*cursor == ']' || *cursor == ')') {
      --depth;
    }
  }
  // `pieces` holds start pointers; convert to NUL-terminated copies by the
  // caller using the returned count and the bounds below.
  (void)pieceStart;
  *outCount = count;
  return 1;
}

static char* sliceDup(const char* start, const char* end) {
  const int64_t n = (int64_t)(end - start);
  char* out = (char*)malloc((size_t)n + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, start, (size_t)n);
  out[n] = '\0';
  return out;
}

/* ------------------------------------------------------------------ */
/* Encoding                                                            */
/* ------------------------------------------------------------------ */

static int32_t encodeValue(MpBuf* buf, MpAny value, int32_t depth);

static int32_t encodeScalar(MpBuf* buf, int32_t kind, const void* data) {
  switch (kind) {
  case MpKindBool:
    return bufPutU8(buf, (*(const uint8_t*)data) != 0 ? 0x02 : 0x01);
  case MpKindI8:
  case MpKindU8:
    return bufPutU8(buf, 0x03) && bufPut(buf, data, 1);
  case MpKindI16:
    return bufPutU8(buf, 0x04) && bufPut(buf, data, 2);
  case MpKindI32:
    return bufPutU8(buf, 0x05) && bufPut(buf, data, 4);
  case MpKindI64:
    return bufPutU8(buf, 0x06) && bufPut(buf, data, 8);
  case MpKindU16:
    return bufPutU8(buf, 0x07) && bufPut(buf, data, 2);
  case MpKindU32:
    return bufPutU8(buf, 0x08) && bufPut(buf, data, 4);
  case MpKindU64:
    return bufPutU8(buf, 0x09) && bufPut(buf, data, 8);
  case MpKindF32:
    return bufPutU8(buf, 0x0a) && bufPut(buf, data, 4);
  case MpKindF64:
    return bufPutU8(buf, 0x0b) && bufPut(buf, data, 8);
  case MpKindStr: {
    const SereStr* text = (const SereStr*)data;
    const int64_t len = text == NULL || text->len < 0 ? 0 : text->len;
    return bufPutU8(buf, 0x0c) && bufPutVarint(buf, (uint64_t)len) &&
           bufPut(buf, text == NULL ? "" : text->data, len);
  }
  default:
    return 0;
  }
}

/// Encodes `list[T]`. `data` points at a slot holding the `SereList*`.
static int32_t encodeList(MpBuf* buf, const char* name, int32_t elemKind, const void* data) {
  const SereList* list = data == NULL ? NULL : *(const SereList* const*)data;
  const int64_t nameLen = (int64_t)strlen(name);
  if (!bufPutU8(buf, 0x0d) || !bufPutVarint(buf, (uint64_t)nameLen) ||
      !bufPut(buf, name, nameLen) || !bufPutU8(buf, (uint8_t)elemKind)) {
    return 0;
  }
  const int64_t count = list == NULL || list->len < 0 ? 0 : list->len;
  if (!bufPutVarint(buf, (uint64_t)count)) {
    return 0;
  }
  const int64_t stride = list == NULL || list->stride <= 0 ? 1 : list->stride;
  const uint8_t* base = list == NULL ? NULL : (const uint8_t*)list->data;
  for (int64_t index = 0; index < count; ++index) {
    const void* item = base + index * stride;
    if (elemKind == MpKindAny) {
      if (!encodeValue(buf, *(const MpAny*)item, 0)) {
        return 0;
      }
    } else if (!encodeScalar(buf, elemKind, item)) {
      return 0;
    }
  }
  return 1;
}

/// Encodes `dict[str, V]`. `data` points at a slot holding the `SereDict*`.
static int32_t encodeDict(MpBuf* buf, const char* name, int32_t valKind, const void* data) {
  void* dict = data == NULL ? NULL : *(void* const*)data;
  const int64_t nameLen = (int64_t)strlen(name);
  if (!bufPutU8(buf, 0x0e) || !bufPutVarint(buf, (uint64_t)nameLen) ||
      !bufPut(buf, name, nameLen) || !bufPutU8(buf, (uint8_t)valKind)) {
    return 0;
  }
  void* keys = sere_dict_keys(dict);
  void* values = sere_dict_values(dict);
  const SereList* keyList = (const SereList*)keys;
  const SereList* valList = (const SereList*)values;
  const int64_t count = keyList == NULL ? 0 : keyList->len;
  if (!bufPutVarint(buf, (uint64_t)count)) {
    return 0;
  }
  for (int64_t index = 0; index < count; ++index) {
    const SereStr* key = (const SereStr*)((const uint8_t*)keyList->data + index * keyList->stride);
    const int64_t keyLen = key->len < 0 ? 0 : key->len;
    if (!bufPutVarint(buf, (uint64_t)keyLen) || !bufPut(buf, key->data, keyLen)) {
      return 0;
    }
    const void* item = (const uint8_t*)valList->data + index * valList->stride;
    if (valKind == MpKindAny) {
      if (!encodeValue(buf, *(const MpAny*)item, 0)) {
        return 0;
      }
    } else if (!encodeScalar(buf, valKind, item)) {
      return 0;
    }
  }
  return 1;
}

static int32_t encodeValue(MpBuf* buf, MpAny value, int32_t depth) {
  if (depth > 64) {
    mpSetLastError("value nesting is too deep to serialize");
    return 0;
  }
  if (value.name == NULL || value.data == NULL || strcmp(value.name, "None") == 0) {
    return bufPutU8(buf, 0x00);
  }
  const int64_t nameLen = (int64_t)strlen(value.name);
  const int32_t scalar = kindOfScalarName(value.name, nameLen);
  if (scalar >= 0) {
    return encodeScalar(buf, scalar, value.data);
  }
  const char* innerStart = NULL;
  const char* innerEnd = NULL;
  if (genericArity(value.name, "list", &innerStart, &innerEnd) ||
      genericArity(value.name, "array", &innerStart, &innerEnd)) {
    const char* pieces[1];
    int32_t count = 0;
    if (!splitTypeArgs(innerStart, innerEnd, pieces, 1, &count) || count != 1) {
      mpSetLastError("cannot serialize list with an unsupported element type");
      return 0;
    }
    char* elemName = sliceDup(innerStart, innerEnd);
    if (elemName == NULL) {
      return 0;
    }
    int32_t elemKind = kindOfScalarName(elemName, (int64_t)strlen(elemName));
    if (strcmp(elemName, "Any") == 0) {
      elemKind = MpKindAny;
    }
    free(elemName);
    if (elemKind < 0) {
      mpSetLastError("cannot serialize a list whose element type is not a scalar, str or Any");
      return 0;
    }
    return encodeList(buf, value.name, elemKind, value.data);
  }
  if (genericArity(value.name, "dict", &innerStart, &innerEnd)) {
    char* keyName = sliceDup(innerStart, innerEnd);
    const char* comma = NULL;
    int32_t depth2 = 0;
    for (const char* cursor = innerStart; cursor < innerEnd; ++cursor) {
      if (*cursor == '[') {
        ++depth2;
      } else if (*cursor == ']') {
        --depth2;
      } else if (*cursor == ',' && depth2 == 0) {
        comma = cursor;
        break;
      }
    }
    if (keyName == NULL || comma == NULL) {
      free(keyName);
      mpSetLastError("cannot serialize this dict");
      return 0;
    }
    free(keyName);
    if ((int64_t)(comma - innerStart) != 3 || strncmp(innerStart, "str", 3) != 0) {
      mpSetLastError("only dict[str, ...] values can cross a process boundary");
      return 0;
    }
    char* valName = sliceDup(comma + 1, innerEnd);
    if (valName == NULL) {
      return 0;
    }
    int32_t valKind = kindOfScalarName(valName, (int64_t)strlen(valName));
    if (strcmp(valName, "Any") == 0) {
      valKind = MpKindAny;
    }
    free(valName);
    if (valKind < 0) {
      mpSetLastError("dict values must be scalars, str or Any");
      return 0;
    }
    return encodeDict(buf, value.name, valKind, value.data);
  }
  mpSetLastError("cannot serialize this value; only scalars, str, list and dict cross processes");
  return 0;
}

/* ------------------------------------------------------------------ */
/* Decoding                                                            */
/* ------------------------------------------------------------------ */

static int32_t decodeValue(MpReader* reader, MpAny* out, int32_t depth);

static void* allocScalar(int32_t kind, const void* bytes, int64_t size) {
  void* payload = mpAlloc(size);
  if (payload == NULL) {
    return NULL;
  }
  memcpy(payload, bytes, (size_t)size);
  (void)kind;
  return payload;
}

static int32_t decodeValue(MpReader* reader, MpAny* out, int32_t depth) {
  if (depth > 64) {
    mpSetLastError("value nesting is too deep");
    return 0;
  }
  uint8_t tag = 0;
  if (!readerU8(reader, &tag)) {
    return 0;
  }
  out->name = NULL;
  out->data = NULL;
  switch (tag) {
  case 0x00:
    return 1; // None
  case 0x01:
  case 0x02: {
    uint8_t flag = tag == 0x02 ? 1 : 0;
    out->name = internName("bool", -1);
    out->data = allocScalar(MpKindBool, &flag, 1);
    return out->data != NULL;
  }
  case 0x03: {
    int64_t size = 1;
    uint8_t scratch[1];
    if (!readerBytes(reader, scratch, 1)) {
      return 0;
    }
    out->name = internName("i8", -1);
    out->data = allocScalar(MpKindI8, scratch, size);
    return out->data != NULL;
  }
  case 0x04: {
    uint8_t scratch[2];
    if (!readerBytes(reader, scratch, 2)) {
      return 0;
    }
    out->name = internName("i16", -1);
    out->data = allocScalar(MpKindI16, scratch, 2);
    return out->data != NULL;
  }
  case 0x05: {
    uint8_t scratch[4];
    if (!readerBytes(reader, scratch, 4)) {
      return 0;
    }
    out->name = internName("i32", -1);
    out->data = allocScalar(MpKindI32, scratch, 4);
    return out->data != NULL;
  }
  case 0x06: {
    uint8_t scratch[8];
    if (!readerBytes(reader, scratch, 8)) {
      return 0;
    }
    out->name = internName("i64", -1);
    out->data = allocScalar(MpKindI64, scratch, 8);
    return out->data != NULL;
  }
  case 0x07: {
    uint8_t scratch[2];
    if (!readerBytes(reader, scratch, 2)) {
      return 0;
    }
    out->name = internName("u16", -1);
    out->data = allocScalar(MpKindU16, scratch, 2);
    return out->data != NULL;
  }
  case 0x08: {
    uint8_t scratch[4];
    if (!readerBytes(reader, scratch, 4)) {
      return 0;
    }
    out->name = internName("u32", -1);
    out->data = allocScalar(MpKindU32, scratch, 4);
    return out->data != NULL;
  }
  case 0x09: {
    uint8_t scratch[8];
    if (!readerBytes(reader, scratch, 8)) {
      return 0;
    }
    out->name = internName("u64", -1);
    out->data = allocScalar(MpKindU64, scratch, 8);
    return out->data != NULL;
  }
  case 0x0a: {
    uint8_t scratch[4];
    if (!readerBytes(reader, scratch, 4)) {
      return 0;
    }
    out->name = internName("f32", -1);
    out->data = allocScalar(MpKindF32, scratch, 4);
    return out->data != NULL;
  }
  case 0x0b: {
    uint8_t scratch[8];
    if (!readerBytes(reader, scratch, 8)) {
      return 0;
    }
    out->name = internName("f64", -1);
    out->data = allocScalar(MpKindF64, scratch, 8);
    return out->data != NULL;
  }
  case 0x0c: {
    uint64_t len = 0;
    if (!readerVarint(reader, &len)) {
      return 0;
    }
    if (reader->pos + (int64_t)len > reader->len) {
      return 0;
    }
    SereStr* text = (SereStr*)mpAlloc((int64_t)sizeof(SereStr));
    if (text == NULL) {
      return 0;
    }
    char* bytes = (char*)mpAlloc((int64_t)len + 1);
    if (bytes == NULL) {
      return 0;
    }
    if (!readerBytes(reader, bytes, (int64_t)len)) {
      return 0;
    }
    bytes[len] = '\0';
    text->data = bytes;
    text->len = (int64_t)len;
    out->name = internName("str", -1);
    out->data = text;
    return 1;
  }
  case 0x0d: {
    uint64_t nameLen = 0;
    if (!readerVarint(reader, &nameLen) || reader->pos + (int64_t)nameLen > reader->len) {
      return 0;
    }
    char* name = sliceDup((const char*)reader->data + reader->pos, (const char*)reader->data +
                                                                       reader->pos +
                                                                       (int64_t)nameLen);
    reader->pos += (int64_t)nameLen;
    uint8_t elemKind = 0;
    uint64_t count = 0;
    if (name == NULL || !readerU8(reader, &elemKind) || !readerVarint(reader, &count)) {
      free(name);
      return 0;
    }
    const int64_t stride = kindSize(elemKind);
    void* list = sere_list_new(stride);
    for (uint64_t index = 0; index < count; ++index) {
      if (elemKind == MpKindAny) {
        MpAny item;
        item.name = NULL;
        item.data = NULL;
        if (!decodeValue(reader, &item, depth + 1)) {
          free(name);
          return 0;
        }
        sere_list_push(list, &item);
      } else {
        uint8_t scratch[16];
        if (!readerBytes(reader, scratch, stride)) {
          free(name);
          return 0;
        }
        sere_list_push(list, scratch);
      }
    }
    void** slot = (void**)mpAlloc((int64_t)sizeof(void*));
    if (slot == NULL) {
      free(name);
      return 0;
    }
    *slot = list;
    out->name = internName(name, -1);
    out->data = slot;
    free(name);
    return 1;
  }
  case 0x0e: {
    uint64_t nameLen = 0;
    if (!readerVarint(reader, &nameLen) || reader->pos + (int64_t)nameLen > reader->len) {
      return 0;
    }
    char* name = sliceDup((const char*)reader->data + reader->pos, (const char*)reader->data +
                                                                       reader->pos +
                                                                       (int64_t)nameLen);
    reader->pos += (int64_t)nameLen;
    uint8_t valKind = 0;
    uint64_t count = 0;
    if (name == NULL || !readerU8(reader, &valKind) || !readerVarint(reader, &count)) {
      free(name);
      return 0;
    }
    const int64_t valStride = kindSize(valKind);
    void* dict = sere_dict_new((int64_t)sizeof(SereStr), valStride, 3);
    for (uint64_t index = 0; index < count; ++index) {
      uint64_t keyLen = 0;
      if (!readerVarint(reader, &keyLen) || reader->pos + (int64_t)keyLen > reader->len) {
        free(name);
        return 0;
      }
      SereStr key;
      key.data = (const char*)reader->data + reader->pos;
      key.len = (int64_t)keyLen;
      reader->pos += (int64_t)keyLen;
      if (valKind == MpKindAny) {
        MpAny item;
        item.name = NULL;
        item.data = NULL;
        if (!decodeValue(reader, &item, depth + 1)) {
          free(name);
          return 0;
        }
        sere_dict_set(dict, &key, &item);
      } else {
        uint8_t scratch[16];
        if (!readerBytes(reader, scratch, valStride)) {
          free(name);
          return 0;
        }
        sere_dict_set(dict, &key, scratch);
      }
    }
    void** slot = (void**)mpAlloc((int64_t)sizeof(void*));
    if (slot == NULL) {
      free(name);
      return 0;
    }
    *slot = dict;
    out->name = internName(name, -1);
    out->data = slot;
    free(name);
    return 1;
  }
  default:
    mpSetLastError("the byte stream is not a valid multiprocessing payload");
    return 0;
  }
}

/* ------------------------------------------------------------------ */
/* Target registry                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
  char* name;
  void* fn;
} MpTarget;

static MpTarget* gTargets;
static int64_t gTargetCount;
static int64_t gTargetCap;

/// Code address of the package's wire-to-object rebuilder, registered during
/// module initialization so it exists in every process, including children.
static void* gRebuilder;

static char* dupText(const char* text) {
  if (text == NULL) {
    return NULL;
  }
  const size_t n = strlen(text);
  char* out = (char*)malloc(n + 1);
  if (out != NULL) {
    memcpy(out, text, n + 1);
  }
  return out;
}

/// A Sere function value is a pointer to a heap-allocated `{ code, env }` pair
/// (see `packCallable` in the code generator). Spawnable targets are free
/// functions, so the registry keys on the code address and rejects bound
/// callables whose `env` is non-null: a closure's captured state cannot exist
/// in a different process.
static void* unwrapTargetBox(const MpAny* target, int32_t* isBound) {
  if (isBound != NULL) {
    *isBound = 0;
  }
  if (target == NULL || target->data == NULL || target->name == NULL) {
    return NULL;
  }
  if (strstr(target->name, "->") == NULL) {
    return NULL; // not a function value
  }
  void** fat = (void**)(*(void**)target->data);
  if (fat == NULL) {
    return NULL;
  }
  if (isBound != NULL && fat[1] != NULL) {
    *isBound = 1;
  }
  return fat[0];
}

void sere_mp_register_target(const char* name, int64_t nameLen, const void* box) {
  (void)nameLen;
  int32_t isBound = 0;
  void* fn = unwrapTargetBox((const MpAny*)box, &isBound);
  if (name == NULL || fn == NULL) {
    return;
  }
  if (isBound) {
    mpSetLastError("a bound callable cannot be a spawn target; use a top-level function");
    sere_raise("PicklingError;Exception", mpLastError(), (int64_t)strlen(mpLastError()));
    return;
  }
  for (int64_t index = 0; index < gTargetCount; ++index) {
    if (strcmp(gTargets[index].name, name) == 0) {
      gTargets[index].fn = fn;
      return;
    }
  }
  if (gTargetCount == gTargetCap) {
    const int64_t cap = gTargetCap == 0 ? 16 : gTargetCap * 2;
    MpTarget* grown = (MpTarget*)realloc(gTargets, (size_t)cap * sizeof(MpTarget));
    if (grown == NULL) {
      return;
    }
    gTargets = grown;
    gTargetCap = cap;
  }
  gTargets[gTargetCount].name = dupText(name);
  gTargets[gTargetCount].fn = fn;
  gTargetCount += 1;
}

void* sere_mp_lookup_target(const char* name, int64_t nameLen) {
  (void)nameLen;
  if (name == NULL) {
    return NULL;
  }
  for (int64_t index = 0; index < gTargetCount; ++index) {
    if (strcmp(gTargets[index].name, name) == 0) {
      return gTargets[index].fn;
    }
  }
  return NULL;
}

void sere_mp_target_name(void* fn, const char** outData, int64_t* outLen) {
  const char* name = NULL;
  for (int64_t index = 0; index < gTargetCount; ++index) {
    if (gTargets[index].fn == fn) {
      name = gTargets[index].name;
      break;
    }
  }
  *outData = name == NULL ? "" : name;
  *outLen = name == NULL ? 0 : (int64_t)strlen(name);
}

/// The registered name of the function boxed in `box`, used by `Process` to
/// turn a `target=` value into the name the child will resolve.
void sere_mp_target_name_box(const void* box, const char** outData, int64_t* outLen) {
  void* fn = unwrapTargetBox((const MpAny*)box, NULL);
  if (fn == NULL) {
    *outData = "";
    *outLen = 0;
    return;
  }
  sere_mp_target_name(fn, outData, outLen);
}

int64_t sere_mp_target_count(void) { return gTargetCount; }

void sere_mp_set_rebuilder(const void* box) {
  gRebuilder = unwrapTargetBox((const MpAny*)box, NULL);
}

void sere_mp_call_target(const void* box, void* argsList) {
  int32_t isBound = 0;
  void* fn = unwrapTargetBox((const MpAny*)box, &isBound);
  if (fn == NULL) {
    sere_raise("TypeError;Exception", "the target is not a function", 25);
    return;
  }
  if (isBound) {
    sere_raise("PicklingError;Exception",
               "a bound callable cannot run as a spawn target", 43);
    return;
  }
  typedef void (*MpTargetFn)(void*);
  ((MpTargetFn)fn)(argsList);
}

/* ------------------------------------------------------------------ */
/* Child registry (for `active_children`)                              */
/* ------------------------------------------------------------------ */

#define MP_MAX_CHILDREN 4096

typedef struct {
  MpProcess* handle;
  int32_t daemon;
  int32_t reaped;
} MpChildRecord;

static MpChildRecord* gChildren;
static int64_t gChildCount;
static int64_t gChildCap;

static int64_t childAdd(MpProcess* handle, int32_t daemon) {
  if (gChildCount == gChildCap) {
    const int64_t cap = gChildCap == 0 ? 16 : gChildCap * 2;
    MpChildRecord* grown = (MpChildRecord*)realloc(gChildren, (size_t)cap * sizeof(MpChildRecord));
    if (grown == NULL) {
      return -1;
    }
    gChildren = grown;
    gChildCap = cap;
  }
  const int64_t index = gChildCount++;
  gChildren[index].handle = handle;
  gChildren[index].daemon = daemon;
  gChildren[index].reaped = 0;
  return index;
}

/// Drops finished children so the registry does not grow without bound.
static void childrenReap(void) {
  for (int64_t index = 0; index < gChildCount; ++index) {
    MpChildRecord* record = &gChildren[index];
    if (record->reaped || record->handle == NULL) {
      continue;
    }
    if (record->daemon) {
      continue; // daemons stay visible until shutdown, like Python
    }
    if (mpWait(record->handle, 0) == 0) {
      record->reaped = 1;
      mpProcessRelease(record->handle);
      record->handle = NULL;
    }
  }
}

int32_t sere_mp_active_children(void) {
  childrenReap();
  int32_t live = 0;
  for (int64_t index = 0; index < gChildCount; ++index) {
    if (!gChildren[index].reaped && gChildren[index].handle != NULL &&
        mpIsAlive(gChildren[index].handle) == 1) {
      live += 1;
    }
  }
  return live;
}

void* sere_mp_child_at(int32_t index) {
  int32_t seen = 0;
  for (int64_t slot = 0; slot < gChildCount; ++slot) {
    if (gChildren[slot].reaped || gChildren[slot].handle == NULL) {
      continue;
    }
    if (mpIsAlive(gChildren[slot].handle) != 1) {
      continue;
    }
    if (seen == index) {
      return gChildren[slot].handle;
    }
    seen += 1;
  }
  return NULL;
}

void sere_mp_forget_child(void* handle) {
  for (int64_t index = 0; index < gChildCount; ++index) {
    if (gChildren[index].handle == handle) {
      gChildren[index].reaped = 1;
      gChildren[index].handle = NULL;
    }
  }
}

void sere_mp_shutdown(void) {
  for (int64_t index = 0; index < gChildCount; ++index) {
    MpChildRecord* record = &gChildren[index];
    if (record->reaped || record->handle == NULL) {
      continue;
    }
    if (mpIsAlive(record->handle) != 1) {
      mpProcessRelease(record->handle);
      record->handle = NULL;
      record->reaped = 1;
      continue;
    }
    if (record->daemon) {
      mpTerminate(record->handle);
    }
    (void)mpWait(record->handle, 5000);
    mpProcessRelease(record->handle);
    record->handle = NULL;
    record->reaped = 1;
  }
  for (int64_t index = 0; index < gChildCount; ++index) {
    free(gChildren[index].handle);
  }
  free(gChildren);
  gChildren = NULL;
  gChildCount = 0;
  gChildCap = 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void* sere_mp_spawn(void* payloadList, int32_t daemon) {
  const SereList* bytes = (const SereList*)payloadList;
  int64_t length = 0;
  const char* data = "";
  if (bytes != NULL && bytes->len > 0) {
    length = bytes->len * (bytes->stride <= 0 ? 1 : bytes->stride);
    data = (const char*)bytes->data;
  }
  char error[512];
  error[0] = '\0';
  MpProcess* process = mpSpawn(data, length, daemon, error, sizeof(error));
  if (process == NULL) {
    mpSetLastError(error[0] == '\0' ? "failed to start the child process" : error);
    sere_raise("ProcessError;Exception", mpLastError(), (int64_t)strlen(mpLastError()));
    return NULL;
  }
  (void)childAdd(process, daemon);
  return process;
}

int32_t sere_mp_wait(void* handle, int64_t timeoutMs) {
  return mpWait((MpProcess*)handle, timeoutMs);
}

int32_t sere_mp_is_alive(void* handle) { return mpIsAlive((MpProcess*)handle); }

int32_t sere_mp_exit_code(void* handle) { return mpExitCode((MpProcess*)handle); }

int64_t sere_mp_pid(void* handle) { return mpProcessPid((MpProcess*)handle); }

int64_t sere_mp_self_pid(void) { return mpSelfPid(); }

void sere_mp_terminate(void* handle) { mpTerminate((MpProcess*)handle); }

void sere_mp_kill(void* handle) { mpKill((MpProcess*)handle); }

void sere_mp_close(void* handle) {
  if (handle == NULL) {
    return;
  }
  sere_mp_forget_child(handle);
  mpProcessRelease((MpProcess*)handle);
}

void* sere_mp_sentinel(void* handle) { return mpSentinel((MpProcess*)handle); }

/// The waitable value of a child, as an integer so `wait` can collect a
/// `list[i64]` of handles without pointer casts in Sere code.
int64_t sere_mp_sentinel_value(void* handle) {
  return (int64_t)(intptr_t)mpSentinel((MpProcess*)handle);
}

int32_t sere_mp_cpu_count(void) { return mpCpuCount(); }

int64_t sere_mp_now_ms(void) { return mpNowMs(); }

void sere_mp_random_bytes(void* out, int64_t len) { mpRandomBytes(out, len); }

/* ---- bytes ------------------------------------------------------- */

static void* bytesFromBuffer(MpBuf* buf) {
  void* list = sere_list_new(1);
  if (list == NULL) {
    return NULL;
  }
  if (buf->len > 0) {
    for (int64_t index = 0; index < buf->len; ++index) {
      sere_list_push(list, buf->data + index);
    }
  }
  return list;
}

static int32_t bufferFromBytes(const SereList* list, MpBuf* out) {
  if (list == NULL) {
    return 1;
  }
  const int64_t stride = list->stride <= 0 ? 1 : list->stride;
  if (!bufReserve(out, list->len)) {
    return 0;
  }
  for (int64_t index = 0; index < list->len; ++index) {
    out->data[out->len++] = ((const uint8_t*)list->data)[index * stride];
  }
  return 1;
}

/* ---- pack/unpack -------------------------------------------------- */

/// Frames `targetName` plus the encoded argument list into a byte list.
void* sere_mp_pack_call(const char* name, int64_t nameLen, void* argsList) {
  MpBuf buf;
  buf.data = NULL;
  buf.len = 0;
  buf.cap = 0;
  const int64_t n = nameLen < 0 ? (int64_t)strlen(name == NULL ? "" : name) : nameLen;
  MpAny args;
  args.name = "list[Any]";
  args.data = NULL;
  void* slot = &argsList;
  args.data = slot;
  if (!bufPutVarint(&buf, (uint64_t)n) || !bufPut(&buf, name == NULL ? "" : name, n) ||
      !encodeValue(&buf, args, 0)) {
    free(buf.data);
    const char* message = mpLastError()[0] == '\0' ? "cannot serialize the arguments" : mpLastError();
    sere_raise("PicklingError;Exception", message, (int64_t)strlen(message));
    return NULL;
  }
  void* out = bytesFromBuffer(&buf);
  free(buf.data);
  return out;
}

/// Encodes a boxed value (a pointer to its 16-byte `Any`) as a byte list.
void* sere_mp_pack_any(const void* box) {
  const MpAny* value = (const MpAny*)box;
  MpBuf buf;
  buf.data = NULL;
  buf.len = 0;
  buf.cap = 0;
  if (value == NULL || !encodeValue(&buf, *value, 0)) {
    free(buf.data);
    const char* message = mpLastError()[0] == '\0' ? "cannot serialize this value" : mpLastError();
    sere_raise("PicklingError;Exception", message, (int64_t)strlen(message));
    return NULL;
  }
  void* out = bytesFromBuffer(&buf);
  free(buf.data);
  return out;
}

/// Decodes a byte list produced by `sere_mp_pack_any` into `outBox`.
void sere_mp_unpack_any_into(void* byteList, void* outBox) {
  MpAny* out = (MpAny*)outBox;
  if (out != NULL) {
    out->name = NULL;
    out->data = NULL;
  }
  MpBuf raw;
  raw.data = NULL;
  raw.len = 0;
  raw.cap = 0;
  if (!bufferFromBytes((const SereList*)byteList, &raw)) {
    free(raw.data);
    sere_raise("UnpicklingError;Exception", "out of memory", 14);
    return;
  }
  MpReader reader;
  reader.data = raw.data;
  reader.len = raw.len;
  reader.pos = 0;
  MpAny result;
  result.name = NULL;
  result.data = NULL;
  if (!decodeValue(&reader, &result, 0)) {
    free(raw.data);
    const char* message = mpLastError()[0] == '\0' ? "invalid payload" : mpLastError();
    sere_raise("UnpicklingError;Exception", message, (int64_t)strlen(message));
    return;
  }
  free(raw.data);
  if (out != NULL) {
    *out = result;
  }
}

/// Decodes a byte list produced by `sere_mp_pack_call` and returns the
/// argument list, or NULL when the payload is not a child payload.
void* sere_mp_unpack_args(void* byteList) {
  MpBuf raw;
  raw.data = NULL;
  raw.len = 0;
  raw.cap = 0;
  if (!bufferFromBytes((const SereList*)byteList, &raw)) {
    free(raw.data);
    return NULL;
  }
  MpReader reader;
  reader.data = raw.data;
  reader.len = raw.len;
  reader.pos = 0;
  uint64_t nameLen = 0;
  if (!readerVarint(&reader, &nameLen) || reader.pos + (int64_t)nameLen > reader.len) {
    free(raw.data);
    return NULL;
  }
  reader.pos += (int64_t)nameLen; // the target name is not needed here
  MpAny value;
  value.name = NULL;
  value.data = NULL;
  if (!decodeValue(&reader, &value, 0)) {
    free(raw.data);
    return NULL;
  }
  free(raw.data);
  if (value.name == NULL || value.data == NULL) {
    return NULL;
  }
  return *(void**)value.data;
}

/* ------------------------------------------------------------------ */
/* Argument normalization                                              */
/* ------------------------------------------------------------------ */

/// The canonical Sere spelling of a scalar kind, used as an element's boxed
/// type name so a child's `as u8` sees "u8" even when the parent wrote "byte".
static const char* canonicalScalarName(int32_t kind) {
  switch (kind) {
  case MpKindBool:
    return "bool";
  case MpKindI8:
    return "i8";
  case MpKindI16:
    return "i16";
  case MpKindI32:
    return "i32";
  case MpKindI64:
    return "i64";
  case MpKindU8:
    return "u8";
  case MpKindU16:
    return "u16";
  case MpKindU32:
    return "u32";
  case MpKindU64:
    return "u64";
  case MpKindF32:
    return "f32";
  case MpKindF64:
    return "f64";
  case MpKindStr:
    return "str";
  default:
    return "Any";
  }
}

/// Normalizes a boxed list (or `None`) into a fresh `list[Any]`.
///
/// A Sere literal such as `["Sere", 42]` may be inferred as `list[Any]` while
/// `["a", "b"]` is `list[str]`, and the two have different element strides.
/// Doing the re-boxing here means the Sere side never has to rely on a runtime
/// cast between `Any` and a container type, and the codec always receives a
/// buffer whose stride matches its declared type.
void* sere_mp_to_any_list(const void* box) {
  const MpAny* value = (const MpAny*)box;
  if (value == NULL || value->name == NULL || value->data == NULL ||
      strcmp(value->name, "None") == 0) {
    return sere_list_new((int64_t)sizeof(MpAny));
  }
  const char* innerStart = NULL;
  const char* innerEnd = NULL;
  if (!genericArity(value->name, "list", &innerStart, &innerEnd)) {
    mpSetLastError("expected a list of transferable values as the argument list");
    sere_raise("PicklingError;Exception", mpLastError(), (int64_t)strlen(mpLastError()));
    return NULL;
  }
  char* elementName = sliceDup(innerStart, innerEnd);
  if (elementName == NULL) {
    return NULL;
  }
  int32_t kind = strcmp(elementName, "Any") == 0
                     ? MpKindAny
                     : kindOfScalarName(elementName, (int64_t)strlen(elementName));
  free(elementName);
  if (kind < 0) {
    mpSetLastError("only lists of scalars, str, bytes or Any cross a process boundary");
    sere_raise("PicklingError;Exception", mpLastError(), (int64_t)strlen(mpLastError()));
    return NULL;
  }
  const SereList* source = *(const SereList* const*)value->data;
  void* out = sere_list_new((int64_t)sizeof(MpAny));
  if (source == NULL || source->len <= 0) {
    return out;
  }
  const int64_t stride = source->stride <= 0 ? 1 : source->stride;
  const uint8_t* base = (const uint8_t*)source->data;
  if (kind == MpKindAny) {
    for (int64_t index = 0; index < source->len; ++index) {
      sere_list_push(out, base + index * stride);
    }
    return out;
  }
  const int64_t size = kindSize(kind);
  const char* name = canonicalScalarName(kind);
  for (int64_t index = 0; index < source->len; ++index) {
    MpAny item;
    item.name = internName(name, -1);
    item.data = allocScalar(kind, base + index * stride, size);
    if (item.data == NULL) {
      continue;
    }
    sere_list_push(out, &item);
  }
  return out;
}

/* ------------------------------------------------------------------ */
/* Child bootstrap                                                     */
/* ------------------------------------------------------------------ */

static const char* kChildArgPrefix = "--sere-mp-child=";

void sere_mp_bootstrap(int32_t argc, char** argv) {
  int64_t handle = -1;
  if (!mpHandleArg(argc, argv, kChildArgPrefix, &handle)) {
    const char* fromEnv = getenv("SERE_MP_CHILD_HANDLE");
    if (fromEnv != NULL) {
      handle = (int64_t)strtoll(fromEnv, NULL, 10);
    }
  }
  if (handle < 0) {
    return; // an ordinary process
  }
  uint8_t* payload = NULL;
  int64_t payloadLen = 0;
  if (!mpReadAllFromHandle(handle, &payload, &payloadLen)) {
    fprintf(stderr, "multiprocessing: cannot read the child payload: %s\n", mpLastError());
    sere_runtime_flush();
    exit(1);
  }
  MpReader reader;
  reader.data = payload;
  reader.len = payloadLen;
  reader.pos = 0;
  uint64_t nameLen = 0;
  if (!readerVarint(&reader, &nameLen) || reader.pos + (int64_t)nameLen > reader.len) {
    fprintf(stderr, "multiprocessing: malformed child payload\n");
    free(payload);
    sere_runtime_flush();
    exit(1);
  }
  char* name = sliceDup((const char*)payload + reader.pos,
                        (const char*)payload + reader.pos + (int64_t)nameLen);
  reader.pos += (int64_t)nameLen;
  MpAny args;
  args.name = NULL;
  args.data = NULL;
  int32_t ok = name != NULL && decodeValue(&reader, &args, 0);
  free(payload);
  if (!ok) {
    fprintf(stderr, "multiprocessing: cannot decode the child arguments\n");
    free(name);
    sere_runtime_flush();
    exit(1);
  }
  void* fn = sere_mp_lookup_target(name, (int64_t)strlen(name));
  if (fn == NULL) {
    fprintf(stderr, "multiprocessing: child target '%s' is not registered (add a "
                    "multiprocessing.expose(...) declaration at the top level)\n",
            name);
    free(name);
    sere_runtime_flush();
    exit(1);
  }
  free(name);
  void* argList = args.data == NULL ? NULL : *(void**)args.data;
  if (argList == NULL) {
    argList = sere_list_new((int64_t)sizeof(MpAny));
  }
  // The package registers a rebuilder that turns reduced wire values (a lock's
  // name, a queue's handles) back into live objects before the target runs.
  if (gRebuilder != NULL) {
    typedef void* (*MpRebuildFn)(void*);
    void* rebuilt = ((MpRebuildFn)gRebuilder)(argList);
    if (rebuilt != NULL) {
      argList = rebuilt;
    }
  }
  typedef void (*MpTargetFn)(void*);
  ((MpTargetFn)fn)(argList);
  sere_error_unhandled();
  sere_mp_shutdown();
  sere_runtime_flush();
  exit(0);
}

/* ------------------------------------------------------------------ */
/* Misc                                                                */
/* ------------------------------------------------------------------ */

void sere_mp_set_error(const char* message, int64_t len) {
  (void)len;
  mpSetLastError(message);
  const char* text = mpLastError();
  sere_raise("RuntimeError;Exception", text, (int64_t)strlen(text));
}

/* ------------------------------------------------------------------ */
/* Pipes and connections                                               */
/* ------------------------------------------------------------------ */

static void raiseMp(const char* fallback) {
  const char* text = mpLastError()[0] == '\0' ? fallback : mpLastError();
  sere_raise("RuntimeError;Exception", text, (int64_t)strlen(text));
}

int32_t sere_mp_pipe_duplex(void** a, void** b) {
  MpConn* left = NULL;
  MpConn* right = NULL;
  if (!mpPipeDuplex(&left, &right)) {
    raiseMp("cannot create a pipe");
    return 0;
  }
  *a = left;
  *b = right;
  return 1;
}

int32_t sere_mp_pipe_simplex(void** readEnd, void** writeEnd) {
  MpConn* reader = NULL;
  MpConn* writer = NULL;
  if (!mpPipeSimplex(&reader, &writer)) {
    raiseMp("cannot create a pipe");
    return 0;
  }
  *readEnd = reader;
  *writeEnd = writer;
  return 1;
}

void* sere_mp_conn_from_handles(int64_t readHandle, int64_t writeHandle, int32_t ownsRead,
                                int32_t ownsWrite) {
  MpConn* conn = mpConnFromHandles(readHandle, writeHandle, ownsRead, ownsWrite);
  if (conn == NULL) {
    sere_raise("RuntimeError;Exception", "out of memory", 14);
  }
  return conn;
}

int64_t sere_mp_conn_read_handle(void* conn) { return mpConnReadHandle((MpConn*)conn); }
int64_t sere_mp_conn_write_handle(void* conn) { return mpConnWriteHandle((MpConn*)conn); }

int64_t sere_mp_conn_read(void* conn, void* buffer, int64_t capacity, int64_t timeoutMs) {
  return mpConnRead((MpConn*)conn, buffer, capacity, timeoutMs);
}

int32_t sere_mp_conn_write(void* conn, const void* data, int64_t len) {
  return mpConnWrite((MpConn*)conn, data, len);
}

int32_t sere_mp_conn_poll(void* conn, int64_t timeoutMs) {
  return mpConnPoll((MpConn*)conn, timeoutMs);
}

void sere_mp_conn_close_read(void* conn) { mpConnCloseRead((MpConn*)conn); }
void sere_mp_conn_close_write(void* conn) { mpConnCloseWrite((MpConn*)conn); }
void sere_mp_conn_close(void* conn) { mpConnClose((MpConn*)conn); }
int32_t sere_mp_conn_readable(void* conn) { return mpConnReadable((MpConn*)conn); }
int32_t sere_mp_conn_writable(void* conn) { return mpConnWritable((MpConn*)conn); }
int32_t sere_mp_conn_closed(void* conn) { return mpConnClosed((MpConn*)conn); }

/* ------------------------------------------------------------------ */
/* Shared memory                                                       */
/* ------------------------------------------------------------------ */

void* sere_mp_shm_create(const char* name, int64_t size) {
  char error[512];
  error[0] = '\0';
  MpShm* shm = mpShmCreate(name, size, error, sizeof(error));
  if (shm == NULL) {
    mpSetLastError(error[0] == '\0' ? "cannot create shared memory" : error);
    raiseMp("cannot create shared memory");
  }
  return shm;
}

void* sere_mp_shm_open(const char* name) {
  char error[512];
  error[0] = '\0';
  MpShm* shm = mpShmOpen(name, error, sizeof(error));
  if (shm == NULL) {
    mpSetLastError(error[0] == '\0' ? "cannot attach to shared memory" : error);
    raiseMp("cannot attach to shared memory");
  }
  return shm;
}

void* sere_mp_shm_ptr(void* shm) { return mpShmPtr((MpShm*)shm); }
int64_t sere_mp_shm_size(void* shm) { return mpShmSize((MpShm*)shm); }
void sere_mp_shm_close(void* shm) { mpShmClose((MpShm*)shm); }
void sere_mp_shm_unlink(void* shm) { mpShmUnlink((MpShm*)shm); }
int32_t sere_mp_shm_unlink_name(const char* name) { return mpShmUnlinkName(name); }

/* ------------------------------------------------------------------ */
/* Named semaphores and events                                         */
/* ------------------------------------------------------------------ */

void* sere_mp_sem_create(const char* name, int64_t initial, int64_t maximum) {
  char error[512];
  error[0] = '\0';
  MpSem* sem = mpSemCreate(name, initial, maximum, error, sizeof(error));
  if (sem == NULL) {
    mpSetLastError(error[0] == '\0' ? "cannot create the semaphore" : error);
    raiseMp("cannot create the semaphore");
  }
  return sem;
}

void* sere_mp_sem_open(const char* name) {
  char error[512];
  error[0] = '\0';
  MpSem* sem = mpSemOpen(name, error, sizeof(error));
  if (sem == NULL) {
    mpSetLastError(error[0] == '\0' ? "cannot open the semaphore" : error);
    raiseMp("cannot open the semaphore");
  }
  return sem;
}

int32_t sere_mp_sem_wait(void* sem, int64_t timeoutMs) {
  return mpSemWait((MpSem*)sem, timeoutMs);
}
int32_t sere_mp_sem_try_wait(void* sem) { return mpSemTryWait((MpSem*)sem); }
void sere_mp_sem_post(void* sem) { mpSemPost((MpSem*)sem); }
void sere_mp_sem_close(void* sem) { mpSemClose((MpSem*)sem); }
void sere_mp_sem_unlink(void* sem) { mpSemUnlink((MpSem*)sem); }

void* sere_mp_event_create(const char* name, int32_t manualReset) {
  char error[512];
  error[0] = '\0';
  MpEvent* event = mpEventCreate(name, manualReset, error, sizeof(error));
  if (event == NULL) {
    mpSetLastError(error[0] == '\0' ? "cannot create the event" : error);
    raiseMp("cannot create the event");
  }
  return event;
}

void* sere_mp_event_open(const char* name) {
  char error[512];
  error[0] = '\0';
  MpEvent* event = mpEventOpen(name, error, sizeof(error));
  if (event == NULL) {
    mpSetLastError(error[0] == '\0' ? "cannot open the event" : error);
    raiseMp("cannot open the event");
  }
  return event;
}

int32_t sere_mp_event_wait(void* event, int64_t timeoutMs) {
  return mpEventWait((MpEvent*)event, timeoutMs);
}
void sere_mp_event_set(void* event) { mpEventSet((MpEvent*)event); }
void sere_mp_event_clear(void* event) { mpEventClear((MpEvent*)event); }
void sere_mp_event_close(void* event) { mpEventClose((MpEvent*)event); }
void sere_mp_event_unlink(void* event) { mpEventUnlink((MpEvent*)event); }

int32_t sere_mp_wait_many(void* handleList, int64_t timeoutMs) {
  const SereList* list = (const SereList*)handleList;
  if (list == NULL || list->len <= 0) {
    return -1;
  }
  int32_t count = (int32_t)list->len;
  void* items[64];
  if (count > 64) {
    count = 64;
  }
  for (int32_t index = 0; index < count; ++index) {
    const int64_t value = *(const int64_t*)((const uint8_t*)list->data + (int64_t)index *
                                                                       list->stride);
    items[index] = (void*)(intptr_t)value;
  }
  return mpWaitMany(items, count, timeoutMs);
}

/* ------------------------------------------------------------------ */
/* Framed messages                                                     */
/* ------------------------------------------------------------------ */

/// Framing is a 4-byte little-endian length followed by the payload, so a
/// single `send` always maps onto a single `recv` regardless of how the OS
/// splits the byte stream.
int32_t sere_mp_conn_send_frame(void* conn, void* bytes) {
  const SereList* list = (const SereList*)bytes;
  const int64_t length = list == NULL || list->len < 0 ? 0 : list->len;
  uint8_t header[4];
  header[0] = (uint8_t)(length & 0xff);
  header[1] = (uint8_t)((length >> 8) & 0xff);
  header[2] = (uint8_t)((length >> 16) & 0xff);
  header[3] = (uint8_t)((length >> 24) & 0xff);
  if (!mpConnWrite((MpConn*)conn, header, 4)) {
    return 0;
  }
  if (length == 0) {
    return 1;
  }
  const int64_t stride = list->stride <= 0 ? 1 : list->stride;
  if (stride == 1) {
    return mpConnWrite((MpConn*)conn, list->data, length);
  }
  for (int64_t index = 0; index < length; ++index) {
    if (!mpConnWrite((MpConn*)conn, (const uint8_t*)list->data + index * stride, 1)) {
      return 0;
    }
  }
  return 1;
}

/// Receives one framed message. `status` reports 0 = ok, 1 = timed out,
/// 2 = end of input, 3 = error. An empty result with status 0 is a valid frame.
void* sere_mp_conn_recv_frame(void* conn, int64_t timeoutMs, int32_t* status) {
  if (status != NULL) {
    *status = 3;
  }
  if (conn == NULL) {
    return NULL;
  }
  uint8_t header[4];
  const int64_t first = mpConnRead((MpConn*)conn, header, 4, timeoutMs);
  if (first == -1) {
    if (status != NULL) {
      *status = 1;
    }
    return NULL;
  }
  if (first == 0) {
    if (status != NULL) {
      *status = 2;
    }
    return NULL;
  }
  if (first < 0) {
    return NULL;
  }
  int64_t filled = first;
  while (filled < 4) {
    const int64_t read = mpConnRead((MpConn*)conn, header + filled, 4 - filled, -1);
    if (read <= 0) {
      return NULL;
    }
    filled += read;
  }
  const int64_t length = (int64_t)(uint32_t)header[0] | ((int64_t)(uint32_t)header[1] << 8) |
                         ((int64_t)(uint32_t)header[2] << 16) |
                         ((int64_t)(uint32_t)header[3] << 24);
  void* list = sere_list_new(1);
  uint8_t chunk[4096];
  int64_t remaining = length;
  while (remaining > 0) {
    const int64_t want = remaining < (int64_t)sizeof(chunk) ? remaining : (int64_t)sizeof(chunk);
    const int64_t read = mpConnRead((MpConn*)conn, chunk, want, -1);
    if (read <= 0) {
      return NULL;
    }
    for (int64_t index = 0; index < read; ++index) {
      sere_list_push(list, chunk + index);
    }
    remaining -= read;
  }
  if (status != NULL) {
    *status = 0;
  }
  return list;
}

/* ------------------------------------------------------------------ */
/* Misc                                                                */
/* ------------------------------------------------------------------ */

void sere_mp_make_name(const char* prefix, int64_t prefixLen, const char** outData,
                       int64_t* outLen) {
  static char buffer[256];
  char raw[128];
  const int64_t n = prefixLen < 0 ? (int64_t)strlen(prefix == NULL ? "" : prefix) : prefixLen;
  const int64_t copy = n > 100 ? 100 : n;
  memcpy(raw, prefix == NULL ? "" : prefix, (size_t)copy);
  raw[copy] = '\0';
  mpMakeName(raw, buffer, sizeof(buffer));
  *outData = buffer;
  *outLen = (int64_t)strlen(buffer);
}

void sere_mp_authkey(const char** outData, int64_t* outLen) {
  static char buffer[65];
  uint8_t raw[32];
  mpRandomBytes(raw, 32);
  static const char* kHex = "0123456789abcdef";
  for (int index = 0; index < 32; ++index) {
    buffer[index * 2] = kHex[raw[index] >> 4];
    buffer[index * 2 + 1] = kHex[raw[index] & 0x0f];
  }
  buffer[64] = '\0';
  *outData = buffer;
  *outLen = 64;
}

void* sere_mp_ptr_offset(void* base, int64_t offset) {
  return (void*)((uint8_t*)base + offset);
}

void sere_mp_raise(const char* message, int64_t messageLen) {
  sere_raise("RuntimeError;Exception", message == NULL ? "" : message, messageLen);
}
