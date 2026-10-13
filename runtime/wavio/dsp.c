#include "internal.h"

struct WavioFilter {
  int32_t highpass, channels, rate;
  union { ma_lpf low; ma_hpf high; } filter;
};
WavioFilter* wavio_filter_create(int32_t highpass, int32_t channels, int32_t rate, double cutoff) {
  if (!wavio_format_valid(channels, rate) || !isfinite(cutoff) || cutoff <= 0 || cutoff >= rate / 2.0 || (highpass != 0 && highpass != 1)) {
    wavio_result(MA_INVALID_ARGS); return NULL;
  }
  WavioFilter* f = calloc(1, sizeof(*f));
  if (!f) { wavio_result(MA_OUT_OF_MEMORY); return NULL; }
  f->highpass = highpass; f->channels = channels; f->rate = rate;
  ma_result result;
  if (highpass) {
    ma_hpf_config config = ma_hpf_config_init(ma_format_f32, channels, rate, cutoff, 2);
    result = ma_hpf_init(&config, NULL, &f->filter.high);
  } else {
    ma_lpf_config config = ma_lpf_config_init(ma_format_f32, channels, rate, cutoff, 2);
    result = ma_lpf_init(&config, NULL, &f->filter.low);
  }
  wavio_result(result); if (result != MA_SUCCESS) { free(f); return NULL; } return f;
}
void wavio_filter_close(WavioFilter* f) {
  if (f) {
    if (f->highpass) ma_hpf_uninit(&f->filter.high, NULL); else ma_lpf_uninit(&f->filter.low, NULL);
    free(f);
  }
}
int32_t wavio_filter_process(WavioFilter* f, WavioBuffer* b) {
  if (!f || !b || f->channels != b->channels || f->rate != b->rate) return wavio_result(MA_INVALID_ARGS);
  return wavio_result(f->highpass ? ma_hpf_process_pcm_frames(&f->filter.high, b->data, b->data, b->frames) :
                                    ma_lpf_process_pcm_frames(&f->filter.low, b->data, b->data, b->frames));
}
int32_t wavio_filter_reset(WavioFilter* f) {
  if (!f) return wavio_result(MA_INVALID_ARGS);
  /* Fixed order two means exactly one biquad and no first-order stage. */
  return wavio_result(f->highpass ? ma_biquad_clear_cache(&f->filter.high.pHPF2[0].bq) : ma_lpf_clear_cache(&f->filter.low));
}
