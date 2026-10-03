#include "sere/api/sere_mod.h"

extern "C" int32_t native_add(int32_t left, int32_t right) {
  return left + right;
}

static Sere_Object* native_add_obj(Sere_Object* const* args, int32_t nargs) {
  if (nargs < 2) {
    return Sere_Long_FromI32(0);
  }
  const int32_t sum = Sere_Long_AsI32(args[0]) + Sere_Long_AsI32(args[1]);
  return Sere_Long_FromI32(sum);
}

extern "C" void sere_mod_init(void) {
  Sere_DefineFunction("add", native_add_obj, 2);
}
