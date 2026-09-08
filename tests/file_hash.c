#include "sere_rt.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  const char* name = "sere_hash_test.bin";
  unsigned char data[131077];
  for (size_t index = 0; index < sizeof(data); ++index)
    data[index] = (unsigned char)index;
  FILE* file = fopen(name, "wb");
  if (file == NULL)
    return 1;
  if (fwrite(data, 1, sizeof(data), file) != sizeof(data) || fclose(file) != 0)
    return 2;
  const int64_t expected = sere_hash_fnv1a((const char*)data, sizeof(data));
  if (sere_hash_file(name, (int64_t)strlen(name)) != expected || sere_has_error())
    return 3;
  file = fopen(name, "wb");
  if (file == NULL || fclose(file) != 0)
    return 4;
  if (sere_hash_file(name, (int64_t)strlen(name)) != sere_hash_fnv1a("", 0) || sere_has_error())
    return 5;
  if (remove(name) != 0)
    return 6;
  (void)sere_hash_file(name, (int64_t)strlen(name));
  if (!sere_error_isa("FileHashError") || !sere_error_isa("Exception"))
    return 7;
  sere_clear_error();
  (void)sere_hash_file("bad\0path", 8);
  if (!sere_error_isa("FileHashError"))
    return 8;
  sere_clear_error();
  return 0;
}
