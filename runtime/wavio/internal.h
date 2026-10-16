#pragma once
#include "vendor/miniaudio.h"
#include "wavio.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct WavioBuffer {
  float* data;
  int64_t frames;
  int32_t channels;
  int32_t rate;
};
int32_t wavio_result(ma_result result);
char* wavio_path(const char* data, int64_t length);
int wavio_format_valid(int32_t channels, int32_t rate);
