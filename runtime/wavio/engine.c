#include "internal.h"

#include <stdatomic.h>

struct WavioContext {
  ma_context context;
  ma_engine engine;
  int refs;
  int mode;
  ma_device_info* outputs;
  ma_device_info* inputs;
  ma_uint32 outputCount, inputCount;
};
struct WavioBus {
  ma_sound_group group;
  WavioContext* context;
  int refs;
};
struct WavioSound {
  ma_sound sound;
  WavioContext* context;
  WavioBus* bus;
};
struct WavioCapture {
  ma_device device;
  ma_pcm_rb ring;
  WavioContext* context;
  _Atomic uint64_t dropped;
};
static void retain(WavioContext* c) {
  ++c->refs;
}
void wavio_context_close(WavioContext* c) {
  if (c && --c->refs == 0) {
    ma_engine_uninit(&c->engine);
    ma_context_uninit(&c->context);
    free(c);
  }
}
WavioContext* wavio_context_create(int32_t rate, int32_t channels, int32_t mode, int32_t device) {
  if (!wavio_format_valid(channels, rate) || mode < 0 || mode > 2 || device < -1) {
    wavio_result(MA_INVALID_ARGS);
    return NULL;
  }
  WavioContext* c = calloc(1, sizeof(*c));
  if (!c) {
    wavio_result(MA_OUT_OF_MEMORY);
    return NULL;
  }
  /* Explicit list prevents miniaudio's automatic null-backend fallback. */
  ma_backend hardware[] = {ma_backend_wasapi,
                           ma_backend_dsound,
                           ma_backend_winmm,
                           ma_backend_coreaudio,
                           ma_backend_pulseaudio,
                           ma_backend_alsa,
                           ma_backend_jack,
                           ma_backend_sndio,
                           ma_backend_audio4,
                           ma_backend_oss,
                           ma_backend_aaudio,
                           ma_backend_opensl,
                           ma_backend_webaudio};
  ma_backend nullBackend = ma_backend_null;
  ma_result result = ma_context_init(mode ? &nullBackend : hardware,
                                     mode ? 1 : sizeof(hardware) / sizeof(hardware[0]),
                                     NULL,
                                     &c->context);
  if (result != MA_SUCCESS) {
    free(c);
    wavio_result(result);
    return NULL;
  }
  result =
      ma_context_get_devices(&c->context, &c->outputs, &c->outputCount, &c->inputs, &c->inputCount);
  if (result != MA_SUCCESS || (device >= 0 && (ma_uint32)device >= c->outputCount)) {
    ma_context_uninit(&c->context);
    free(c);
    wavio_result(result != MA_SUCCESS ? result : MA_OUT_OF_RANGE);
    return NULL;
  }
  ma_engine_config config = ma_engine_config_init();
  config.pContext = &c->context;
  config.channels = channels;
  config.sampleRate = rate;
  config.noDevice = mode == 2;
  config.defaultVolumeSmoothTimeInPCMFrames = 64;
  if (device >= 0)
    config.pPlaybackDeviceID = &c->outputs[device].id;
  result = ma_engine_init(&config, &c->engine);
  if (result != MA_SUCCESS) {
    ma_context_uninit(&c->context);
    free(c);
    wavio_result(result);
    return NULL;
  }
  c->refs = 1;
  c->mode = mode;
  wavio_result(MA_SUCCESS);
  return c;
}
int32_t wavio_context_rate(WavioContext* c) {
  return c ? (int32_t)ma_engine_get_sample_rate(&c->engine) : 0;
}
int32_t wavio_context_channels(WavioContext* c) {
  return c ? (int32_t)ma_engine_get_channels(&c->engine) : 0;
}
int32_t wavio_context_backend(WavioContext* c) {
  return c ? (int32_t)c->context.backend : -1;
}
int64_t wavio_context_clock(WavioContext* c) {
  return c ? (int64_t)ma_engine_get_time_in_pcm_frames(&c->engine) : 0;
}
int32_t wavio_context_render(WavioContext* c, WavioBuffer* b) {
  if (!c || !b || c->mode != 2 || b->channels != wavio_context_channels(c) ||
      b->rate != wavio_context_rate(c))
    return wavio_result(MA_INVALID_ARGS);
  return wavio_result(ma_engine_read_pcm_frames(&c->engine, b->data, b->frames, NULL));
}
int32_t wavio_device_count(WavioContext* c, int32_t capture) {
  if (!c || (capture != 0 && capture != 1))
    return wavio_result(MA_INVALID_ARGS);
  return (int32_t)(capture ? c->inputCount : c->outputCount);
}
static ma_device_info* deviceInfo(WavioContext* c, int32_t capture, int32_t index) {
  int32_t count = wavio_device_count(c, capture);
  if (index < 0 || index >= count) {
    wavio_result(MA_OUT_OF_RANGE);
    return NULL;
  }
  return capture ? &c->inputs[index] : &c->outputs[index];
}
const char* wavio_device_name(WavioContext* c, int32_t capture, int32_t index) {
  ma_device_info* info = deviceInfo(c, capture, index);
  return info ? info->name : "";
}
int32_t wavio_device_default(WavioContext* c, int32_t capture, int32_t index) {
  ma_device_info* info = deviceInfo(c, capture, index);
  return info ? (int32_t)info->isDefault : 0;
}
WavioSound* wavio_sound_open(WavioContext* c, const char* data, int64_t length, int32_t stream) {
  if (!c || (stream != 0 && stream != 1)) {
    wavio_result(MA_INVALID_ARGS);
    return NULL;
  }
  char* path = wavio_path(data, length);
  if (!path)
    return NULL;
  WavioSound* s = calloc(1, sizeof(*s));
  ma_uint32 flags =
      MA_SOUND_FLAG_NO_SPATIALIZATION | (stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE);
  ma_result result = s ? ma_sound_init_from_file(&c->engine, path, flags, NULL, NULL, &s->sound)
                       : MA_OUT_OF_MEMORY;
  free(path);
  wavio_result(result);
  if (result != MA_SUCCESS) {
    free(s);
    return NULL;
  }
  s->context = c;
  retain(c);
  return s;
}
void wavio_sound_close(WavioSound* s) {
  if (s) {
    ma_sound_uninit(&s->sound);
    wavio_bus_close(s->bus);
    wavio_context_close(s->context);
    free(s);
  }
}
int32_t wavio_sound_play(WavioSound* s) {
  return wavio_result(s ? ma_sound_start(&s->sound) : MA_INVALID_ARGS);
}
int32_t wavio_sound_pause(WavioSound* s) {
  return wavio_result(s ? ma_sound_stop(&s->sound) : MA_INVALID_ARGS);
}
int32_t wavio_sound_stop(WavioSound* s) {
  ma_result result = wavio_sound_pause(s);
  return result == MA_SUCCESS ? wavio_sound_seek(s, 0) : result;
}
int32_t wavio_sound_seek(WavioSound* s, int64_t frame) {
  if (!s || frame < 0)
    return wavio_result(MA_INVALID_ARGS);
  return wavio_result(ma_sound_seek_to_pcm_frame(&s->sound, frame));
}
int32_t wavio_sound_playing(WavioSound* s) {
  return s ? (int32_t)ma_sound_is_playing(&s->sound) : 0;
}
int64_t wavio_sound_position(WavioSound* s) {
  ma_uint64 cursor = 0;
  ma_result r = s ? ma_sound_get_cursor_in_pcm_frames(&s->sound, &cursor) : MA_INVALID_ARGS;
  wavio_result(r);
  return r == MA_SUCCESS ? (int64_t)cursor : r;
}
int64_t wavio_sound_length(WavioSound* s) {
  ma_uint64 length = 0;
  ma_result r = s ? ma_sound_get_length_in_pcm_frames(&s->sound, &length) : MA_INVALID_ARGS;
  wavio_result(r);
  return r == MA_SUCCESS ? (int64_t)length : r;
}
int32_t wavio_sound_volume(WavioSound* s, double gain) {
  if (!s || !isfinite(gain) || gain < 0 || gain > 16)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_volume(&s->sound, (float)gain);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_sound_pan(WavioSound* s, double pan) {
  if (!s || !isfinite(pan) || fabs(pan) > 1)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_pan(&s->sound, (float)pan);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_sound_pitch(WavioSound* s, double pitch) {
  if (!s || !isfinite(pitch) || pitch < 0.125 || pitch > 8)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_pitch(&s->sound, (float)pitch);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_sound_loop(WavioSound* s, int32_t loop) {
  if (!s)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_looping(&s->sound, loop != 0);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_sound_fade(WavioSound* s, double from, double to, int64_t frames) {
  if (!s || !isfinite(from) || !isfinite(to) || from < 0 || from > 16 || to < 0 || to > 16 ||
      frames < 0)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_fade_in_pcm_frames(&s->sound, (float)from, (float)to, frames);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_sound_schedule(WavioSound* s, int64_t frame) {
  if (!s || frame < 0)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_start_time_in_pcm_frames(&s->sound, frame);
  return wavio_sound_play(s);
}
int32_t wavio_sound_position3d(WavioSound* s, double x, double y, double z) {
  if (!s || !isfinite(x) || !isfinite(y) || !isfinite(z) || fabs(x) > FLT_MAX ||
      fabs(y) > FLT_MAX || fabs(z) > FLT_MAX)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_set_spatialization_enabled(&s->sound, MA_TRUE);
  ma_sound_set_position(&s->sound, (float)x, (float)y, (float)z);
  return wavio_result(MA_SUCCESS);
}
WavioBus* wavio_bus_create(WavioContext* c) {
  if (!c) {
    wavio_result(MA_INVALID_ARGS);
    return NULL;
  }
  WavioBus* b = calloc(1, sizeof(*b));
  ma_result r =
      b ? ma_sound_group_init(&c->engine, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &b->group)
        : MA_OUT_OF_MEMORY;
  wavio_result(r);
  if (r != MA_SUCCESS) {
    free(b);
    return NULL;
  }
  b->context = c;
  b->refs = 1;
  retain(c);
  return b;
}
void wavio_bus_close(WavioBus* b) {
  if (b && --b->refs == 0) {
    ma_sound_group_uninit(&b->group);
    wavio_context_close(b->context);
    free(b);
  }
}
int32_t wavio_bus_volume(WavioBus* b, double gain) {
  if (!b || !isfinite(gain) || gain < 0 || gain > 16)
    return wavio_result(MA_INVALID_ARGS);
  ma_sound_group_set_volume(&b->group, (float)gain);
  return wavio_result(MA_SUCCESS);
}
int32_t wavio_sound_route(WavioSound* s, WavioBus* b) {
  if (!s || !b || s->context != b->context)
    return wavio_result(MA_INVALID_ARGS);
  ma_result r = ma_node_attach_output_bus(&s->sound, 0, &b->group, 0);
  if (r == MA_SUCCESS) {
    ++b->refs;
    wavio_bus_close(s->bus);
    s->bus = b;
  }
  return wavio_result(r);
}
static void captureCallback(ma_device* device, void* output, const void* input, ma_uint32 frames) {
  WavioCapture* c = device->pUserData;
  (void)output;
  ma_uint32 total = 0;
  while (input && total < frames) {
    ma_uint32 n = frames - total;
    void* dest = NULL;
    if (ma_pcm_rb_acquire_write(&c->ring, &n, &dest) != MA_SUCCESS || !n)
      break;
    memcpy(dest,
           (const float*)input + total * device->capture.channels,
           (size_t)n * device->capture.channels * sizeof(float));
    ma_pcm_rb_commit_write(&c->ring, n);
    total += n;
  }
  atomic_fetch_add_explicit(&c->dropped, frames - total, memory_order_relaxed);
}
WavioCapture* wavio_capture_open(WavioContext* context,
                                 int32_t index,
                                 int32_t channels,
                                 int32_t rate,
                                 int32_t capacity,
                                 int32_t loopback) {
  if (!context || !wavio_format_valid(channels, rate) || capacity < 1 || capacity > INT32_MAX / 2 ||
      index < -1 || (loopback != 0 && loopback != 1)) {
    wavio_result(MA_INVALID_ARGS);
    return NULL;
  }
  if (loopback && !ma_context_is_loopback_supported(&context->context)) {
    wavio_result(MA_NOT_IMPLEMENTED);
    return NULL;
  }
  ma_device_info* info = index >= 0 ? deviceInfo(context, !loopback, index) : NULL;
  if (index >= 0 && !info)
    return NULL;
  WavioCapture* c = calloc(1, sizeof(*c));
  if (!c) {
    wavio_result(MA_OUT_OF_MEMORY);
    return NULL;
  }
  atomic_init(&c->dropped, 0);
  if (!atomic_is_lock_free(&c->dropped)) {
    free(c);
    wavio_result(MA_NOT_IMPLEMENTED);
    return NULL;
  }
  ma_result r = ma_pcm_rb_init(ma_format_f32, channels, capacity, NULL, NULL, &c->ring);
  if (r != MA_SUCCESS) {
    free(c);
    wavio_result(r);
    return NULL;
  }
  ma_device_config config =
      ma_device_config_init(loopback ? ma_device_type_loopback : ma_device_type_capture);
  config.capture.format = ma_format_f32;
  config.capture.channels = channels;
  config.sampleRate = rate;
  if (info)
    config.capture.pDeviceID = &info->id;
  config.dataCallback = captureCallback;
  config.pUserData = c;
  r = ma_device_init(&context->context, &config, &c->device);
  if (r != MA_SUCCESS) {
    ma_pcm_rb_uninit(&c->ring);
    free(c);
    wavio_result(r);
    return NULL;
  }
  c->context = context;
  retain(context);
  wavio_result(MA_SUCCESS);
  return c;
}
void wavio_capture_close(WavioCapture* c) {
  if (c) {
    ma_device_uninit(&c->device);
    ma_pcm_rb_uninit(&c->ring);
    wavio_context_close(c->context);
    free(c);
  }
}
int32_t wavio_capture_start(WavioCapture* c) {
  return wavio_result(c ? ma_device_start(&c->device) : MA_INVALID_ARGS);
}
int32_t wavio_capture_stop(WavioCapture* c) {
  return wavio_result(c ? ma_device_stop(&c->device) : MA_INVALID_ARGS);
}
int64_t wavio_capture_dropped(WavioCapture* c) {
  return c ? (int64_t)atomic_load_explicit(&c->dropped, memory_order_relaxed) : 0;
}
int64_t wavio_capture_read(WavioCapture* c, WavioBuffer* b) {
  if (!c || !b || b->channels != (int32_t)c->device.capture.channels ||
      b->rate != (int32_t)c->device.sampleRate)
    return wavio_result(MA_INVALID_ARGS);
  int64_t total = 0;
  while (total < b->frames) {
    ma_uint32 n = (ma_uint32)((b->frames - total) > INT32_MAX ? INT32_MAX : b->frames - total);
    void* source = NULL;
    ma_result r = ma_pcm_rb_acquire_read(&c->ring, &n, &source);
    if (r != MA_SUCCESS)
      return wavio_result(r);
    if (!n)
      break;
    memcpy(b->data + total * b->channels, source, (size_t)n * b->channels * sizeof(float));
    ma_pcm_rb_commit_read(&c->ring, n);
    total += n;
  }
  memset(
      b->data + total * b->channels, 0, (size_t)(b->frames - total) * b->channels * sizeof(float));
  wavio_result(MA_SUCCESS);
  return total;
}
