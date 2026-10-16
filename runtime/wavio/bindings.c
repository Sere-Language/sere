#include "../sere_rt.h"
#include "wavio.h"

#include <string.h>

/* Returned Sere strings own GC memory, never borrowed context/device memory. */
static void stringResult(const char* value, const char** data, int64_t* length) {
  size_t n = strlen(value);
  char* copy = sere_gc_alloc(n + 1);
  if (!copy) {
    *data = "";
    *length = 0;
    return;
  }
  memcpy(copy, value, n + 1);
  *data = copy;
  *length = (int64_t)n;
}
void sere_wavio_error_message(int32_t code, const char** data, int64_t* length) {
  stringResult(wavio_error_message(code), data, length);
}
void sere_wavio_device_name(
    WavioContext* c, int32_t capture, int32_t index, const char** data, int64_t* length) {
  stringResult(wavio_device_name(c, capture, index), data, length);
}
