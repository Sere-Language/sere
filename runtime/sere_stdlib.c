/// @file sere_stdlib.c
/// Numeric, linear-algebra, ML, and byte-buffer helpers for the Sere stdlib.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#endif

#include "sere_rt.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const int64_t kF64Stride = (int64_t)sizeof(double);
static const int64_t kU8Stride = 1;
static const double kEps = 1e-15;

static SereList* asList(void* list) { return (SereList*)list; }

static SereList* f64List(void* list) {
  SereList* typed = asList(list);
  if (typed == NULL || typed->stride != kF64Stride) {
    return NULL;
  }
  return typed;
}

static SereList* u8List(void* list) {
  SereList* typed = asList(list);
  if (typed == NULL || typed->stride != kU8Stride) {
    return NULL;
  }
  return typed;
}

static double* f64Data(SereList* list) {
  return list == NULL ? NULL : (double*)list->data;
}

static uint8_t* u8Data(SereList* list) {
  return list == NULL ? NULL : (uint8_t*)list->data;
}

static SereList* f64Alloc(int64_t length) {
  return (SereList*)sere_array_new(kF64Stride, length);
}

static double f64At(const SereList* list, int64_t index) {
  if (list == NULL || list->data == NULL || index < 0 || index >= list->len) {
    return 0.0;
  }
  return f64Data((SereList*)list)[index];
}

static int64_t minLen(const SereList* left, const SereList* right) {
  if (left == NULL || right == NULL) {
    return 0;
  }
  return left->len < right->len ? left->len : right->len;
}

static void outStr(const char* data, int64_t len, const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = data == NULL ? "" : data;
  }
  if (out_len != NULL) {
    *out_len = data == NULL ? 0 : len;
  }
}

static void emptyOut(const char** out_data, int64_t* out_len) { outStr("", 0, out_data, out_len); }

typedef double (*UnaryFn)(double);
typedef double (*BinaryFn)(double, double);

static void* mapUnary(void* values, UnaryFn fn) {
  SereList* src = f64List(values);
  if (src == NULL) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(src->len);
  double* dst = f64Data(out);
  const double* srcData = f64Data(src);
  if (dst == NULL || srcData == NULL) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = fn(srcData[i]);
  }
  return out;
}

static void* zipBinary(void* left, void* right, BinaryFn fn) {
  SereList* a = f64List(left);
  SereList* b = f64List(right);
  const int64_t n = minLen(a, b);
  SereList* out = f64Alloc(n);
  double* dst = f64Data(out);
  if (dst == NULL || a == NULL || b == NULL) {
    return out;
  }
  const double* ad = f64Data(a);
  const double* bd = f64Data(b);
  for (int64_t i = 0; i < n; ++i) {
    dst[i] = fn(ad[i], bd[i]);
  }
  return out;
}

static double addF64(double left, double right) { return left + right; }
static double subF64(double left, double right) { return left - right; }
static double mulF64(double left, double right) { return left * right; }
static double divF64(double left, double right) { return right == 0.0 ? 0.0 : left / right; }
static double reluF64(double value) { return value > 0.0 ? value : 0.0; }
static double sigmoidF64(double value) { return 1.0 / (1.0 + exp(-value)); }

double sere_math_tan(double value) { return tan(value); }
double sere_math_asin(double value) { return asin(value); }
double sere_math_acos(double value) { return acos(value); }
double sere_math_atan(double value) { return atan(value); }
double sere_math_atan2(double y, double x) { return atan2(y, x); }
double sere_math_sinh(double value) { return sinh(value); }
double sere_math_cosh(double value) { return cosh(value); }
double sere_math_tanh(double value) { return tanh(value); }
double sere_math_exp(double value) { return exp(value); }
double sere_math_expm1(double value) { return expm1(value); }
double sere_math_log(double value) { return log(value); }
double sere_math_log2(double value) { return log2(value); }
double sere_math_log10(double value) { return log10(value); }
double sere_math_log1p(double value) { return log1p(value); }
double sere_math_hypot(double x, double y) { return hypot(x, y); }
double sere_math_fmod(double x, double y) { return fmod(x, y); }
double sere_math_copysign(double mag, double sign) { return copysign(mag, sign); }
double sere_math_round(double value) { return round(value); }
double sere_math_trunc(double value) { return trunc(value); }
double sere_math_fmin(double left, double right) { return fmin(left, right); }
double sere_math_fmax(double left, double right) { return fmax(left, right); }

double sere_math_clamp(double value, double low, double high) {
  return fmin(fmax(value, low), high);
}

double sere_math_lerp(double from, double to, double t) { return from + (to - from) * t; }
double sere_math_radians(double degrees) { return degrees * 0.017453292519943295; }
double sere_math_degrees(double radians) { return radians * 57.29577951308232; }
double sere_math_pi(void) { return 3.14159265358979323846; }
double sere_math_tau(void) { return 6.28318530717958647692; }
double sere_math_e(void) { return 2.71828182845904523536; }
double sere_math_inf(void) { return INFINITY; }
double sere_math_nan(void) { return NAN; }
int32_t sere_math_isnan(double value) { return isnan(value) ? 1 : 0; }
int32_t sere_math_isinf(double value) { return isinf(value) ? 1 : 0; }
int32_t sere_math_isfinite(double value) { return isfinite(value) ? 1 : 0; }

void* sere_f64_full(int64_t length, double fill) {
  SereList* out = f64Alloc(length < 0 ? 0 : length);
  double* data = f64Data(out);
  if (data == NULL) {
    return out;
  }
  for (int64_t i = 0; i < out->len; ++i) {
    data[i] = fill;
  }
  return out;
}

void* sere_f64_copy(void* values) {
  SereList* src = f64List(values);
  if (src == NULL) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(src->len);
  if (out->data != NULL && src->data != NULL) {
    memcpy(out->data, src->data, (size_t)src->len * sizeof(double));
  }
  return out;
}

void* sere_f64_reverse(void* values) {
  SereList* src = f64List(values);
  if (src == NULL) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(src->len);
  const double* ad = f64Data(src);
  double* dst = f64Data(out);
  if (ad == NULL || dst == NULL) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = ad[src->len - 1 - i];
  }
  return out;
}

void* sere_f64_concat(void* left, void* right) {
  SereList* a = f64List(left);
  SereList* b = f64List(right);
  const int64_t an = a == NULL ? 0 : a->len;
  const int64_t bn = b == NULL ? 0 : b->len;
  SereList* out = f64Alloc(an + bn);
  double* dst = f64Data(out);
  if (dst == NULL) {
    return out;
  }
  if (an > 0 && a->data != NULL) {
    memcpy(dst, a->data, (size_t)an * sizeof(double));
  }
  if (bn > 0 && b->data != NULL) {
    memcpy(dst + an, b->data, (size_t)bn * sizeof(double));
  }
  return out;
}

void* sere_f64_add(void* left, void* right) { return zipBinary(left, right, addF64); }
void* sere_f64_sub(void* left, void* right) { return zipBinary(left, right, subF64); }
void* sere_f64_mul(void* left, void* right) { return zipBinary(left, right, mulF64); }
void* sere_f64_div(void* left, void* right) { return zipBinary(left, right, divF64); }

void* sere_f64_scale(void* values, double factor) {
  SereList* src = f64List(values);
  if (src == NULL) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(src->len);
  const double* ad = f64Data(src);
  double* dst = f64Data(out);
  if (ad == NULL || dst == NULL) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = ad[i] * factor;
  }
  return out;
}

void* sere_f64_abs(void* values) { return mapUnary(values, fabs); }

void* sere_f64_clip(void* values, double low, double high) {
  SereList* src = f64List(values);
  if (src == NULL) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(src->len);
  const double* ad = f64Data(src);
  double* dst = f64Data(out);
  if (ad == NULL || dst == NULL) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = sere_math_clamp(ad[i], low, high);
  }
  return out;
}

void* sere_f64_linspace(double start, double stop, int64_t count) {
  if (count <= 0) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(count);
  double* dst = f64Data(out);
  if (dst == NULL) {
    return out;
  }
  if (count == 1) {
    dst[0] = start;
    return out;
  }
  const double step = (stop - start) / (double)(count - 1);
  for (int64_t i = 0; i < count; ++i) {
    dst[i] = start + step * (double)i;
  }
  return out;
}

double sere_f64_sum(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->data == NULL) {
    return 0.0;
  }
  const double* ad = f64Data(src);
  double total = 0.0;
  for (int64_t i = 0; i < src->len; ++i) {
    total += ad[i];
  }
  return total;
}

double sere_f64_mean(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len == 0) {
    return 0.0;
  }
  return sere_f64_sum(values) / (double)src->len;
}

double sere_f64_var(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len <= 1 || src->data == NULL) {
    return 0.0;
  }
  const double mean = sere_f64_mean(values);
  const double* ad = f64Data(src);
  double total = 0.0;
  for (int64_t i = 0; i < src->len; ++i) {
    const double d = ad[i] - mean;
    total += d * d;
  }
  return total / (double)src->len;
}

double sere_f64_min(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len == 0 || src->data == NULL) {
    return 0.0;
  }
  const double* ad = f64Data(src);
  double best = ad[0];
  for (int64_t i = 1; i < src->len; ++i) {
    if (ad[i] < best) {
      best = ad[i];
    }
  }
  return best;
}

double sere_f64_max(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len == 0 || src->data == NULL) {
    return 0.0;
  }
  const double* ad = f64Data(src);
  double best = ad[0];
  for (int64_t i = 1; i < src->len; ++i) {
    if (ad[i] > best) {
      best = ad[i];
    }
  }
  return best;
}

int64_t sere_f64_argmin(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len == 0 || src->data == NULL) {
    return 0;
  }
  const double* ad = f64Data(src);
  int64_t best = 0;
  for (int64_t i = 1; i < src->len; ++i) {
    if (ad[i] < ad[best]) {
      best = i;
    }
  }
  return best;
}

int64_t sere_f64_argmax(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len == 0 || src->data == NULL) {
    return 0;
  }
  const double* ad = f64Data(src);
  int64_t best = 0;
  for (int64_t i = 1; i < src->len; ++i) {
    if (ad[i] > ad[best]) {
      best = i;
    }
  }
  return best;
}

double sere_f64_dot(void* left, void* right) {
  SereList* a = f64List(left);
  SereList* b = f64List(right);
  const int64_t n = minLen(a, b);
  if (n == 0 || a->data == NULL || b->data == NULL) {
    return 0.0;
  }
  const double* ad = f64Data(a);
  const double* bd = f64Data(b);
  double total = 0.0;
  for (int64_t i = 0; i < n; ++i) {
    total += ad[i] * bd[i];
  }
  return total;
}

void* sere_f64_outer(void* left, void* right) {
  SereList* a = f64List(left);
  SereList* b = f64List(right);
  const int64_t an = a == NULL ? 0 : a->len;
  const int64_t bn = b == NULL ? 0 : b->len;
  SereList* out = f64Alloc(an * bn);
  double* dst = f64Data(out);
  if (dst == NULL || a == NULL || b == NULL) {
    return out;
  }
  const double* ad = f64Data(a);
  const double* bd = f64Data(b);
  for (int64_t i = 0; i < an; ++i) {
    for (int64_t j = 0; j < bn; ++j) {
      dst[i * bn + j] = ad[i] * bd[j];
    }
  }
  return out;
}

void* sere_f64_normalize(void* values) {
  const double mag = sqrt(sere_f64_dot(values, values));
  if (mag < kEps) {
    return sere_f64_copy(values);
  }
  return sere_f64_scale(values, 1.0 / mag);
}

void* sere_f64_cross3(void* left, void* right) {
  SereList* out = f64Alloc(3);
  double* dst = f64Data(out);
  if (dst == NULL) {
    return out;
  }
  SereList* a = f64List(left);
  SereList* b = f64List(right);
  const double ax = f64At(a, 0);
  const double ay = f64At(a, 1);
  const double az = f64At(a, 2);
  const double bx = f64At(b, 0);
  const double by = f64At(b, 1);
  const double bz = f64At(b, 2);
  dst[0] = ay * bz - az * by;
  dst[1] = az * bx - ax * bz;
  dst[2] = ax * by - ay * bx;
  return out;
}

void* sere_mat_identity(int64_t n) {
  if (n <= 0) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(n * n);
  double* dst = f64Data(out);
  if (dst == NULL) {
    return out;
  }
  for (int64_t i = 0; i < n; ++i) {
    dst[i * n + i] = 1.0;
  }
  return out;
}

void* sere_mat_mul(void* left, int64_t left_rows, int64_t left_cols, void* right,
                   int64_t right_rows, int64_t right_cols) {
  if (left_cols != right_rows || left_rows <= 0 || right_cols <= 0) {
    return f64Alloc(0);
  }
  SereList* a = f64List(left);
  SereList* b = f64List(right);
  SereList* out = f64Alloc(left_rows * right_cols);
  double* dst = f64Data(out);
  if (dst == NULL || a == NULL || b == NULL) {
    return out;
  }
  for (int64_t r = 0; r < left_rows; ++r) {
    for (int64_t c = 0; c < right_cols; ++c) {
      double sum = 0.0;
      for (int64_t k = 0; k < left_cols; ++k) {
        sum += f64At(a, r * left_cols + k) * f64At(b, k * right_cols + c);
      }
      dst[r * right_cols + c] = sum;
    }
  }
  return out;
}

void* sere_mat_transpose(void* values, int64_t rows, int64_t cols) {
  SereList* src = f64List(values);
  if (src == NULL || rows <= 0 || cols <= 0) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(rows * cols);
  double* dst = f64Data(out);
  if (dst == NULL) {
    return out;
  }
  for (int64_t r = 0; r < rows; ++r) {
    for (int64_t c = 0; c < cols; ++c) {
      dst[c * rows + r] = f64At(src, r * cols + c);
    }
  }
  return out;
}

double sere_mat_det2(void* values) {
  SereList* m = f64List(values);
  return f64At(m, 0) * f64At(m, 3) - f64At(m, 1) * f64At(m, 2);
}

double sere_mat_det3(void* values) {
  SereList* m = f64List(values);
  const double a = f64At(m, 0);
  const double b = f64At(m, 1);
  const double c = f64At(m, 2);
  const double d = f64At(m, 3);
  const double e = f64At(m, 4);
  const double f = f64At(m, 5);
  const double g = f64At(m, 6);
  const double h = f64At(m, 7);
  const double i = f64At(m, 8);
  return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

static void mat4Load(const SereList* src, double* dst) {
  for (int i = 0; i < 16; ++i) {
    dst[i] = f64At(src, i);
  }
}

static SereList* mat4Store(const double* src) {
  SereList* out = f64Alloc(16);
  double* dst = f64Data(out);
  if (dst != NULL) {
    memcpy(dst, src, 16 * sizeof(double));
  }
  return out;
}

void* sere_mat4_identity(void) { return sere_mat_identity(4); }

void* sere_mat4_mul(void* left, void* right) { return sere_mat_mul(left, 4, 4, right, 4, 4); }

void* sere_mat4_transpose(void* values) { return sere_mat_transpose(values, 4, 4); }

void* sere_mat4_inverse(void* values) {
  double m[16];
  double inv[16];
  mat4Load(f64List(values), m);
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
           m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
           m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
           m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
           m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
           m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
           m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
           m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
           m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
           m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
           m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (fabs(det) < kEps) {
    return sere_mat4_identity();
  }
  const double scale = 1.0 / det;
  for (int i = 0; i < 16; ++i) {
    inv[i] *= scale;
  }
  return mat4Store(inv);
}

void* sere_mat4_translate(double x, double y, double z) {
  SereList* out = (SereList*)sere_mat4_identity();
  double* d = f64Data(out);
  if (d != NULL) {
    d[3] = x;
    d[7] = y;
    d[11] = z;
  }
  return out;
}

void* sere_mat4_scale(double x, double y, double z) {
  SereList* out = (SereList*)sere_mat4_identity();
  double* d = f64Data(out);
  if (d != NULL) {
    d[0] = x;
    d[5] = y;
    d[10] = z;
  }
  return out;
}

void* sere_mat4_rotate_x(double radians) {
  SereList* out = (SereList*)sere_mat4_identity();
  double* d = f64Data(out);
  if (d != NULL) {
    const double c = cos(radians);
    const double s = sin(radians);
    d[5] = c;
    d[6] = -s;
    d[9] = s;
    d[10] = c;
  }
  return out;
}

void* sere_mat4_rotate_y(double radians) {
  SereList* out = (SereList*)sere_mat4_identity();
  double* d = f64Data(out);
  if (d != NULL) {
    const double c = cos(radians);
    const double s = sin(radians);
    d[0] = c;
    d[2] = s;
    d[8] = -s;
    d[10] = c;
  }
  return out;
}

void* sere_mat4_rotate_z(double radians) {
  SereList* out = (SereList*)sere_mat4_identity();
  double* d = f64Data(out);
  if (d != NULL) {
    const double c = cos(radians);
    const double s = sin(radians);
    d[0] = c;
    d[1] = -s;
    d[4] = s;
    d[5] = c;
  }
  return out;
}

void* sere_mat4_perspective(double fov_y, double aspect, double near_z, double far_z) {
  SereList* out = f64Alloc(16);
  double* d = f64Data(out);
  if (d == NULL || aspect == 0.0 || fabs(near_z - far_z) < kEps) {
    return out;
  }
  const double f = 1.0 / tan(fov_y * 0.5);
  d[0] = f / aspect;
  d[5] = f;
  d[10] = (far_z + near_z) / (near_z - far_z);
  d[11] = (2.0 * far_z * near_z) / (near_z - far_z);
  d[14] = -1.0;
  return out;
}

void* sere_mat4_ortho(double left, double right, double bottom, double top, double near_z,
                      double far_z) {
  SereList* out = (SereList*)sere_mat4_identity();
  double* d = f64Data(out);
  const double dx = right - left;
  const double dy = top - bottom;
  const double dz = far_z - near_z;
  if (d == NULL || fabs(dx) < kEps || fabs(dy) < kEps || fabs(dz) < kEps) {
    return out;
  }
  d[0] = 2.0 / dx;
  d[5] = 2.0 / dy;
  d[10] = -2.0 / dz;
  d[3] = -(right + left) / dx;
  d[7] = -(top + bottom) / dy;
  d[11] = -(far_z + near_z) / dz;
  return out;
}

static void vec3Norm(double* v) {
  const double mag = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (mag < kEps) {
    return;
  }
  v[0] /= mag;
  v[1] /= mag;
  v[2] /= mag;
}

void* sere_mat4_look_at(void* eye, void* center, void* up) {
  SereList* e = f64List(eye);
  SereList* c = f64List(center);
  SereList* u = f64List(up);
  double f[3] = {f64At(c, 0) - f64At(e, 0), f64At(c, 1) - f64At(e, 1), f64At(c, 2) - f64At(e, 2)};
  double upv[3] = {f64At(u, 0), f64At(u, 1), f64At(u, 2)};
  vec3Norm(f);
  vec3Norm(upv);
  double s[3] = {f[1] * upv[2] - f[2] * upv[1], f[2] * upv[0] - f[0] * upv[2],
                 f[0] * upv[1] - f[1] * upv[0]};
  vec3Norm(s);
  double u2[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
  double m[16] = {0};
  m[0] = s[0];
  m[1] = s[1];
  m[2] = s[2];
  m[4] = u2[0];
  m[5] = u2[1];
  m[6] = u2[2];
  m[8] = -f[0];
  m[9] = -f[1];
  m[10] = -f[2];
  m[3] = -(s[0] * f64At(e, 0) + s[1] * f64At(e, 1) + s[2] * f64At(e, 2));
  m[7] = -(u2[0] * f64At(e, 0) + u2[1] * f64At(e, 1) + u2[2] * f64At(e, 2));
  m[11] = f[0] * f64At(e, 0) + f[1] * f64At(e, 1) + f[2] * f64At(e, 2);
  m[15] = 1.0;
  return mat4Store(m);
}

void* sere_mat4_transform_point(void* matrix, double x, double y, double z) {
  SereList* m = f64List(matrix);
  SereList* out = f64Alloc(3);
  double* d = f64Data(out);
  if (d == NULL) {
    return out;
  }
  const double w = f64At(m, 12) * x + f64At(m, 13) * y + f64At(m, 14) * z + f64At(m, 15);
  const double inv = fabs(w) < kEps ? 1.0 : 1.0 / w;
  d[0] = (f64At(m, 0) * x + f64At(m, 1) * y + f64At(m, 2) * z + f64At(m, 3)) * inv;
  d[1] = (f64At(m, 4) * x + f64At(m, 5) * y + f64At(m, 6) * z + f64At(m, 7)) * inv;
  d[2] = (f64At(m, 8) * x + f64At(m, 9) * y + f64At(m, 10) * z + f64At(m, 11)) * inv;
  return out;
}

void* sere_mat4_transform_dir(void* matrix, double x, double y, double z) {
  SereList* m = f64List(matrix);
  SereList* out = f64Alloc(3);
  double* d = f64Data(out);
  if (d == NULL) {
    return out;
  }
  d[0] = f64At(m, 0) * x + f64At(m, 1) * y + f64At(m, 2) * z;
  d[1] = f64At(m, 4) * x + f64At(m, 5) * y + f64At(m, 6) * z;
  d[2] = f64At(m, 8) * x + f64At(m, 9) * y + f64At(m, 10) * z;
  return out;
}

void* sere_ml_relu(void* values) { return mapUnary(values, reluF64); }

void* sere_ml_leaky_relu(void* values, double alpha) {
  SereList* src = f64List(values);
  if (src == NULL) {
    return f64Alloc(0);
  }
  SereList* out = f64Alloc(src->len);
  const double* ad = f64Data(src);
  double* dst = f64Data(out);
  if (ad == NULL || dst == NULL) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = ad[i] > 0.0 ? ad[i] : ad[i] * alpha;
  }
  return out;
}

void* sere_ml_sigmoid(void* values) { return mapUnary(values, sigmoidF64); }
void* sere_ml_tanh(void* values) { return mapUnary(values, tanh); }

void* sere_ml_softmax(void* values) {
  SereList* src = f64List(values);
  if (src == NULL || src->len == 0) {
    return f64Alloc(0);
  }
  const double maxv = sere_f64_max(values);
  SereList* out = f64Alloc(src->len);
  const double* ad = f64Data(src);
  double* dst = f64Data(out);
  if (ad == NULL || dst == NULL) {
    return out;
  }
  double sum = 0.0;
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = exp(ad[i] - maxv);
    sum += dst[i];
  }
  if (sum == 0.0) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] /= sum;
  }
  return out;
}

void* sere_ml_log_softmax(void* values) {
  SereList* sm = (SereList*)sere_ml_softmax(values);
  double* dst = f64Data(sm);
  if (dst == NULL) {
    return sm;
  }
  for (int64_t i = 0; i < sm->len; ++i) {
    dst[i] = log(dst[i] < kEps ? kEps : dst[i]);
  }
  return sm;
}

double sere_ml_mse(void* pred, void* target) {
  SereList* a = f64List(pred);
  SereList* b = f64List(target);
  const int64_t n = minLen(a, b);
  if (n == 0) {
    return 0.0;
  }
  double total = 0.0;
  for (int64_t i = 0; i < n; ++i) {
    const double d = f64At(a, i) - f64At(b, i);
    total += d * d;
  }
  return total / (double)n;
}

double sere_ml_mae(void* pred, void* target) {
  SereList* a = f64List(pred);
  SereList* b = f64List(target);
  const int64_t n = minLen(a, b);
  if (n == 0) {
    return 0.0;
  }
  double total = 0.0;
  for (int64_t i = 0; i < n; ++i) {
    total += fabs(f64At(a, i) - f64At(b, i));
  }
  return total / (double)n;
}

double sere_ml_bce(void* pred, void* target) {
  SereList* a = f64List(pred);
  SereList* b = f64List(target);
  const int64_t n = minLen(a, b);
  if (n == 0) {
    return 0.0;
  }
  double total = 0.0;
  for (int64_t i = 0; i < n; ++i) {
    const double p = sere_math_clamp(f64At(a, i), kEps, 1.0 - kEps);
    const double t = f64At(b, i);
    total += -(t * log(p) + (1.0 - t) * log(1.0 - p));
  }
  return total / (double)n;
}

double sere_ml_linear(void* inputs, void* weights, double bias) {
  return sere_f64_dot(inputs, weights) + bias;
}

void* sere_bytes_alloc(int64_t length) { return sere_array_new(kU8Stride, length < 0 ? 0 : length); }

void* sere_bytes_copy(void* buffer) {
  SereList* src = u8List(buffer);
  if (src == NULL) {
    return sere_bytes_alloc(0);
  }
  SereList* out = (SereList*)sere_bytes_alloc(src->len);
  if (out->data != NULL && src->data != NULL) {
    memcpy(out->data, src->data, (size_t)src->len);
  }
  return out;
}

void* sere_bytes_concat(void* left, void* right) {
  SereList* a = u8List(left);
  SereList* b = u8List(right);
  const int64_t an = a == NULL ? 0 : a->len;
  const int64_t bn = b == NULL ? 0 : b->len;
  SereList* out = (SereList*)sere_bytes_alloc(an + bn);
  uint8_t* dst = u8Data(out);
  if (dst == NULL) {
    return out;
  }
  if (an > 0 && a->data != NULL) {
    memcpy(dst, a->data, (size_t)an);
  }
  if (bn > 0 && b->data != NULL) {
    memcpy(dst + an, b->data, (size_t)bn);
  }
  return out;
}

void sere_bytes_fill(void* buffer, uint8_t value) {
  SereList* src = u8List(buffer);
  if (src == NULL || src->data == NULL) {
    return;
  }
  memset(src->data, value, (size_t)src->len);
}

void sere_bytes_set(void* buffer, int64_t index, uint8_t value) {
  memcpy(sere_list_item(buffer, index), &value, 1);
}

uint8_t sere_bytes_get(void* buffer, int64_t index) {
  uint8_t value = 0;
  memcpy(&value, sere_list_item(buffer, index), 1);
  return value;
}

int32_t sere_bytes_eq(void* left, void* right) {
  SereList* a = u8List(left);
  SereList* b = u8List(right);
  if (a == NULL || b == NULL || a->len != b->len) {
    return 0;
  }
  if (a->len == 0) {
    return 1;
  }
  if (a->data == NULL || b->data == NULL) {
    return 0;
  }
  return memcmp(a->data, b->data, (size_t)a->len) == 0 ? 1 : 0;
}

int64_t sere_bytes_find(void* hay, void* needle) {
  SereList* h = u8List(hay);
  SereList* n = u8List(needle);
  if (h == NULL || n == NULL || n->len == 0 || n->len > h->len || h->data == NULL ||
      n->data == NULL) {
    return n != NULL && n->len == 0 ? 0 : -1;
  }
  const uint8_t* hd = u8Data(h);
  const uint8_t* nd = u8Data(n);
  const int64_t last = h->len - n->len;
  for (int64_t i = 0; i <= last; ++i) {
    if (memcmp(hd + i, nd, (size_t)n->len) == 0) {
      return i;
    }
  }
  return -1;
}

void* sere_bytes_from_str(const char* data, int64_t len) {
  if (len < 0) {
    len = 0;
  }
  SereList* out = (SereList*)sere_bytes_alloc(len);
  if (out->data != NULL && data != NULL && len > 0) {
    memcpy(out->data, data, (size_t)len);
  }
  return out;
}

void sere_bytes_to_str(void* buffer, const char** out_data, int64_t* out_len) {
  SereList* src = u8List(buffer);
  if (src == NULL || src->len <= 0) {
    emptyOut(out_data, out_len);
    return;
  }
  char* copy = (char*)malloc((size_t)src->len + 1);
  if (copy == NULL) {
    emptyOut(out_data, out_len);
    return;
  }
  if (src->data != NULL) {
    memcpy(copy, src->data, (size_t)src->len);
  }
  copy[src->len] = '\0';
  outStr(copy, src->len, out_data, out_len);
}

void sere_bytes_hex(void* buffer, const char** out_data, int64_t* out_len) {
  static const char kHex[] = "0123456789abcdef";
  SereList* src = u8List(buffer);
  if (src == NULL || src->len <= 0 || src->data == NULL) {
    emptyOut(out_data, out_len);
    return;
  }
  const int64_t n = src->len * 2;
  char* copy = (char*)malloc((size_t)n + 1);
  if (copy == NULL) {
    emptyOut(out_data, out_len);
    return;
  }
  const uint8_t* bytes = u8Data(src);
  for (int64_t i = 0; i < src->len; ++i) {
    copy[i * 2] = kHex[bytes[i] >> 4];
    copy[i * 2 + 1] = kHex[bytes[i] & 0x0f];
  }
  copy[n] = '\0';
  outStr(copy, n, out_data, out_len);
}

static int hexNibble(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

void* sere_bytes_from_hex(const char* data, int64_t len) {
  if (data == NULL || len <= 0) {
    return sere_bytes_alloc(0);
  }
  int64_t nibbleCount = 0;
  for (int64_t i = 0; i < len; ++i) {
    if (hexNibble(data[i]) >= 0) {
      nibbleCount += 1;
    }
  }
  SereList* out = (SereList*)sere_bytes_alloc(nibbleCount / 2);
  uint8_t* dst = u8Data(out);
  if (dst == NULL) {
    return out;
  }
  int64_t written = 0;
  int pending = -1;
  for (int64_t i = 0; i < len && written < out->len; ++i) {
    const int nibble = hexNibble(data[i]);
    if (nibble < 0) {
      continue;
    }
    if (pending < 0) {
      pending = nibble;
    } else {
      dst[written] = (uint8_t)((pending << 4) | nibble);
      written += 1;
      pending = -1;
    }
  }
  out->len = written;
  return out;
}

static void* zipBytes(void* left, void* right, int mode) {
  SereList* a = u8List(left);
  SereList* b = u8List(right);
  const int64_t n = minLen(a, b);
  SereList* out = (SereList*)sere_bytes_alloc(n);
  uint8_t* dst = u8Data(out);
  if (dst == NULL || a == NULL || b == NULL) {
    return out;
  }
  const uint8_t* ad = u8Data(a);
  const uint8_t* bd = u8Data(b);
  for (int64_t i = 0; i < n; ++i) {
    if (mode == 0) {
      dst[i] = (uint8_t)(ad[i] ^ bd[i]);
    } else if (mode == 1) {
      dst[i] = (uint8_t)(ad[i] & bd[i]);
    } else {
      dst[i] = (uint8_t)(ad[i] | bd[i]);
    }
  }
  return out;
}

void* sere_bytes_xor(void* left, void* right) { return zipBytes(left, right, 0); }
void* sere_bytes_and(void* left, void* right) { return zipBytes(left, right, 1); }
void* sere_bytes_or(void* left, void* right) { return zipBytes(left, right, 2); }

void* sere_bytes_not(void* buffer) {
  SereList* src = u8List(buffer);
  if (src == NULL) {
    return sere_bytes_alloc(0);
  }
  SereList* out = (SereList*)sere_bytes_alloc(src->len);
  const uint8_t* ad = u8Data(src);
  uint8_t* dst = u8Data(out);
  if (ad == NULL || dst == NULL) {
    return out;
  }
  for (int64_t i = 0; i < src->len; ++i) {
    dst[i] = (uint8_t)~ad[i];
  }
  return out;
}

static uint64_t readInt(void* buffer, int64_t index, int bytes, int little) {
  uint64_t value = 0;
  for (int i = 0; i < bytes; ++i) {
    const uint8_t b = sere_bytes_get(buffer, index + i);
    const int shift = little ? (i * 8) : ((bytes - 1 - i) * 8);
    value |= ((uint64_t)b) << shift;
  }
  return value;
}

static void writeInt(void* buffer, int64_t index, uint64_t value, int bytes, int little) {
  for (int i = 0; i < bytes; ++i) {
    const int shift = little ? (i * 8) : ((bytes - 1 - i) * 8);
    sere_bytes_set(buffer, index + i, (uint8_t)((value >> shift) & 0xffu));
  }
}

uint16_t sere_bytes_read_u16_le(void* buffer, int64_t index) {
  return (uint16_t)readInt(buffer, index, 2, 1);
}
uint32_t sere_bytes_read_u32_le(void* buffer, int64_t index) {
  return (uint32_t)readInt(buffer, index, 4, 1);
}
uint64_t sere_bytes_read_u64_le(void* buffer, int64_t index) { return readInt(buffer, index, 8, 1); }
uint16_t sere_bytes_read_u16_be(void* buffer, int64_t index) {
  return (uint16_t)readInt(buffer, index, 2, 0);
}
uint32_t sere_bytes_read_u32_be(void* buffer, int64_t index) {
  return (uint32_t)readInt(buffer, index, 4, 0);
}
uint64_t sere_bytes_read_u64_be(void* buffer, int64_t index) { return readInt(buffer, index, 8, 0); }

double sere_bytes_read_f32_le(void* buffer, int64_t index) {
  uint32_t bits = sere_bytes_read_u32_le(buffer, index);
  float value = 0.0f;
  memcpy(&value, &bits, sizeof(value));
  return (double)value;
}

double sere_bytes_read_f64_le(void* buffer, int64_t index) {
  uint64_t bits = sere_bytes_read_u64_le(buffer, index);
  double value = 0.0;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

void sere_bytes_write_u16_le(void* buffer, int64_t index, uint16_t value) {
  writeInt(buffer, index, value, 2, 1);
}
void sere_bytes_write_u32_le(void* buffer, int64_t index, uint32_t value) {
  writeInt(buffer, index, value, 4, 1);
}
void sere_bytes_write_u64_le(void* buffer, int64_t index, uint64_t value) {
  writeInt(buffer, index, value, 8, 1);
}
void sere_bytes_write_u16_be(void* buffer, int64_t index, uint16_t value) {
  writeInt(buffer, index, value, 2, 0);
}
void sere_bytes_write_u32_be(void* buffer, int64_t index, uint32_t value) {
  writeInt(buffer, index, value, 4, 0);
}
void sere_bytes_write_u64_be(void* buffer, int64_t index, uint64_t value) {
  writeInt(buffer, index, value, 8, 0);
}

void sere_bytes_write_f32_le(void* buffer, int64_t index, double value) {
  float narrowed = (float)value;
  uint32_t bits = 0;
  memcpy(&bits, &narrowed, sizeof(bits));
  sere_bytes_write_u32_le(buffer, index, bits);
}

void sere_bytes_write_f64_le(void* buffer, int64_t index, double value) {
  uint64_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  sere_bytes_write_u64_le(buffer, index, bits);
}
