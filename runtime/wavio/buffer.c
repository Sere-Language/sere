#include "internal.h"

static _Thread_local int32_t lastError;
int32_t wavio_result(ma_result result) { lastError = result; return result; }
int32_t wavio_last_error(void) { return lastError; }
const char* wavio_error_message(int32_t code) { return ma_result_description(code); }
int wavio_format_valid(int32_t channels, int32_t rate) {
  return channels > 0 && channels <= 32 && rate >= 8000 && rate <= 384000;
}
char* wavio_path(const char* data, int64_t length) {
  if (!data || length <= 0 || (uint64_t)length >= SIZE_MAX || memchr(data, 0, (size_t)length)) {
    wavio_result(MA_INVALID_ARGS); return NULL;
  }
  char* path = malloc((size_t)length + 1);
  if (!path) { wavio_result(MA_OUT_OF_MEMORY); return NULL; }
  memcpy(path, data, (size_t)length); path[length] = 0; return path;
}
WavioBuffer* wavio_buffer_create(int64_t frames, int32_t channels, int32_t rate) {
  if (!wavio_format_valid(channels, rate) || frames < 0 ||
      (uint64_t)frames > SIZE_MAX / sizeof(float) / (uint32_t)channels) {
    wavio_result(MA_INVALID_ARGS); return NULL;
  }
  WavioBuffer* b = calloc(1, sizeof(*b));
  if (b) b->data = calloc(frames ? (size_t)frames * channels : 1, sizeof(float));
  if (!b || !b->data) { free(b); wavio_result(MA_OUT_OF_MEMORY); return NULL; }
  b->frames = frames; b->channels = channels; b->rate = rate;
  wavio_result(MA_SUCCESS); return b;
}
void wavio_buffer_close(WavioBuffer* b) { if (b) { free(b->data); free(b); } }
int64_t wavio_buffer_frames(WavioBuffer* b) { return b ? b->frames : 0; }
int32_t wavio_buffer_channels(WavioBuffer* b) { return b ? b->channels : 0; }
int32_t wavio_buffer_rate(WavioBuffer* b) { return b ? b->rate : 0; }
double wavio_buffer_get(WavioBuffer* b, int64_t sample) {
  if (!b || sample < 0 || sample >= b->frames * b->channels) {
    wavio_result(MA_OUT_OF_RANGE); return 0;
  }
  wavio_result(MA_SUCCESS); return b->data[sample];
}
int32_t wavio_buffer_set(WavioBuffer* b, int64_t sample, double value) {
  if (!b || sample < 0 || sample >= b->frames * b->channels || !isfinite(value) || fabs(value) > FLT_MAX)
    return wavio_result(MA_INVALID_ARGS);
  b->data[sample] = (float)value; return wavio_result(MA_SUCCESS);
}
int32_t wavio_buffer_fill(WavioBuffer* b, double value) {
  if (!b || !isfinite(value) || fabs(value) > FLT_MAX) return wavio_result(MA_INVALID_ARGS);
  for (int64_t i = 0; i < b->frames * b->channels; ++i) b->data[i] = (float)value;
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_buffer_gain(WavioBuffer* b, double gain) {
  if (!b || !isfinite(gain) || fabs(gain) > FLT_MAX) return wavio_result(MA_INVALID_ARGS);
  if (wavio_buffer_peak(b) * fabs(gain) > FLT_MAX) return wavio_result(MA_OUT_OF_RANGE);
  for (int64_t i = 0; i < b->frames * b->channels; ++i) b->data[i] = (float)(b->data[i] * gain);
  return wavio_result(MA_SUCCESS);
}
double wavio_buffer_peak(WavioBuffer* b) {
  double peak = 0;
  if (!b) { wavio_result(MA_INVALID_ARGS); return 0; }
  for (int64_t i = 0; i < b->frames * b->channels; ++i) peak = fmax(peak, fabs(b->data[i]));
  wavio_result(MA_SUCCESS); return peak;
}
double wavio_buffer_rms(WavioBuffer* b) {
  if (!b) { wavio_result(MA_INVALID_ARGS); return 0; }
  double sum = 0;
  for (int64_t i = 0; i < b->frames * b->channels; ++i) sum += (double)b->data[i] * b->data[i];
  wavio_result(MA_SUCCESS); return b->frames ? sqrt(sum / (double)(b->frames * b->channels)) : 0;
}
int32_t wavio_buffer_normalize(WavioBuffer* b, double peak) {
  if (!b || !isfinite(peak) || peak < 0 || peak > 1) return wavio_result(MA_INVALID_ARGS);
  double old = wavio_buffer_peak(b);
  return old ? wavio_buffer_gain(b, peak / old) : wavio_result(MA_SUCCESS);
}
int32_t wavio_buffer_reverse(WavioBuffer* b) {
  if (!b) return wavio_result(MA_INVALID_ARGS);
  for (int64_t i = 0; i < b->frames / 2; ++i)
    for (int32_t c = 0; c < b->channels; ++c) {
      int64_t left = i * b->channels + c, right = (b->frames - i - 1) * b->channels + c;
      float t = b->data[left]; b->data[left] = b->data[right]; b->data[right] = t;
    }
  return wavio_result(MA_SUCCESS);
}
WavioBuffer* wavio_buffer_slice(WavioBuffer* b, int64_t first, int64_t count) {
  if (!b || first < 0 || count < 0 || first > b->frames || count > b->frames - first) {
    wavio_result(MA_INVALID_ARGS); return NULL;
  }
  WavioBuffer* out = wavio_buffer_create(count, b->channels, b->rate);
  if (out) memcpy(out->data, b->data + first * b->channels, (size_t)count * b->channels * sizeof(float));
  return out;
}
WavioBuffer* wavio_buffer_convert(WavioBuffer* b, int32_t channels, int32_t rate) {
  if (!b || !wavio_format_valid(channels, rate)) { wavio_result(MA_INVALID_ARGS); return NULL; }
  ma_uint64 count = ma_convert_frames(NULL, 0, ma_format_f32, channels, rate,
                                     b->data, b->frames, ma_format_f32, b->channels, b->rate);
  if (count > INT64_MAX) { wavio_result(MA_TOO_BIG); return NULL; }
  WavioBuffer* out = wavio_buffer_create((int64_t)count, channels, rate);
  if (out && count) {
    ma_uint64 written = ma_convert_frames(out->data, count, ma_format_f32, channels, rate,
                                         b->data, b->frames, ma_format_f32, b->channels, b->rate);
    if (!written) { wavio_buffer_close(out); wavio_result(MA_ERROR); return NULL; }
    out->frames = (int64_t)written;
  }
  return out;
}
int32_t wavio_buffer_mix(WavioBuffer* dest, WavioBuffer* source, int64_t offset, double gain) {
  if (!dest || !source || dest->channels != source->channels || dest->rate != source->rate ||
      offset < 0 || offset > dest->frames || source->frames > dest->frames - offset ||
      !isfinite(gain) || fabs(gain) > FLT_MAX) return wavio_result(MA_INVALID_ARGS);
  if (wavio_buffer_peak(dest) + wavio_buffer_peak(source) * fabs(gain) > FLT_MAX)
    return wavio_result(MA_OUT_OF_RANGE);
  /* Reverse traversal permits self-mixing without corrupting unread samples. */
  for (int64_t i = source->frames * source->channels; i-- > 0;)
    dest->data[offset * dest->channels + i] += (float)(source->data[i] * gain);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_buffer_tone(WavioBuffer* b, int32_t waveform, double hz, double amplitude, double phase) {
  if (!b || waveform < 0 || waveform > 3 || !isfinite(hz) || hz <= 0 || hz >= b->rate / 2.0 ||
      !isfinite(amplitude) || fabs(amplitude) > 1 || !isfinite(phase)) return wavio_result(MA_INVALID_ARGS);
  phase -= floor(phase);
  for (int64_t i = 0; i < b->frames; ++i) {
    double v = waveform == 0 ? sin(6.283185307179586 * phase) :
               waveform == 1 ? (phase < 0.5 ? 1 : -1) :
               waveform == 2 ? 1 - 4 * fabs(phase - 0.5) : 2 * phase - 1;
    for (int32_t c = 0; c < b->channels; ++c) b->data[i * b->channels + c] = (float)(v * amplitude);
    phase += hz / b->rate; phase -= floor(phase);
  }
  return wavio_result(MA_SUCCESS);
}
