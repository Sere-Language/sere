/* Wavio native ABI. All resources are opaque, explicitly owned, and must be
 * closed exactly once. NULL closes are harmless. Control calls are serialized
 * by the caller; none may be called from an audio callback. Children retain
 * contexts. Buffer processing is offline, in-place, interleaved float32.
 * Constructors return NULL on failure; status functions return 0 on success
 * and a negative miniaudio result on failure. last_error is thread-local.
 * Frame counts are per channel, sample indices address interleaved samples.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct WavioBuffer WavioBuffer;
typedef struct WavioContext WavioContext;
typedef struct WavioSound WavioSound;
typedef struct WavioReader WavioReader;
typedef struct WavioWriter WavioWriter;
typedef struct WavioCapture WavioCapture;
typedef struct WavioFilter WavioFilter;
typedef struct WavioBus WavioBus;

int32_t wavio_last_error(void);
const char* wavio_error_message(int32_t code);
WavioBuffer* wavio_buffer_create(int64_t frames, int32_t channels, int32_t rate);
void wavio_buffer_close(WavioBuffer* b);
int64_t wavio_buffer_frames(WavioBuffer* b);
int32_t wavio_buffer_channels(WavioBuffer* b);
int32_t wavio_buffer_rate(WavioBuffer* b);
double wavio_buffer_get(WavioBuffer* b, int64_t sample);
int32_t wavio_buffer_set(WavioBuffer* b, int64_t sample, double value);
int32_t wavio_buffer_fill(WavioBuffer* b, double value);
int32_t wavio_buffer_gain(WavioBuffer* b, double gain);
double wavio_buffer_peak(WavioBuffer* b);
double wavio_buffer_rms(WavioBuffer* b);
int32_t wavio_buffer_normalize(WavioBuffer* b, double peak);
int32_t wavio_buffer_reverse(WavioBuffer* b);
WavioBuffer* wavio_buffer_slice(WavioBuffer* b, int64_t first, int64_t count);
WavioBuffer* wavio_buffer_convert(WavioBuffer* b, int32_t channels, int32_t rate);
int32_t wavio_buffer_mix(WavioBuffer* dest, WavioBuffer* source, int64_t offset, double gain);
/* Waveforms: 0 sine, 1 square, 2 triangle, 3 saw. phase is in cycles. */
int32_t wavio_buffer_tone(WavioBuffer* b, int32_t waveform, double hz, double amplitude, double phase);
/* All file paths are UTF-8, with explicit byte lengths (also the Sere str ABI). */
WavioReader* wavio_reader_open(const char* path, int64_t length);
void wavio_reader_close(WavioReader* r);
int64_t wavio_reader_length(WavioReader* r);
int32_t wavio_reader_channels(WavioReader* r);
int32_t wavio_reader_rate(WavioReader* r);
int32_t wavio_reader_seek(WavioReader* r, int64_t frame);
int64_t wavio_reader_read(WavioReader* r, WavioBuffer* b);
/* WAV formats: 1 unsigned8, 2 signed16, 3 packed24, 4 signed32, 5 float32. */
WavioWriter* wavio_writer_open(const char* path, int64_t length, int32_t channels, int32_t rate, int32_t format);
int64_t wavio_writer_write(WavioWriter* w, WavioBuffer* b);
int32_t wavio_writer_close(WavioWriter* w);
/* mode: 0 hardware (no null fallback), 1 explicit null device, 2 offline.
 * device: -1 default or index from a context using the same backend. */
WavioContext* wavio_context_create(int32_t rate, int32_t channels, int32_t mode, int32_t device);
void wavio_context_close(WavioContext* c);
int32_t wavio_context_rate(WavioContext* c);
int32_t wavio_context_channels(WavioContext* c);
int32_t wavio_context_backend(WavioContext* c);
int64_t wavio_context_clock(WavioContext* c);
int32_t wavio_context_render(WavioContext* c, WavioBuffer* b);
int32_t wavio_device_count(WavioContext* c, int32_t capture);
const char* wavio_device_name(WavioContext* c, int32_t capture, int32_t index);
int32_t wavio_device_default(WavioContext* c, int32_t capture, int32_t index);
/* stream=0 decodes before returning; stream=1 uses bounded worker decoding.
 * Multiple file sounds share the engine resource manager's decoded cache. */
WavioSound* wavio_sound_open(WavioContext* c, const char* path, int64_t length, int32_t stream);
void wavio_sound_close(WavioSound* s);
int32_t wavio_sound_play(WavioSound* s);
int32_t wavio_sound_pause(WavioSound* s);
int32_t wavio_sound_stop(WavioSound* s);
int32_t wavio_sound_seek(WavioSound* s, int64_t frame);
int32_t wavio_sound_playing(WavioSound* s);
int64_t wavio_sound_position(WavioSound* s);
int64_t wavio_sound_length(WavioSound* s);
int32_t wavio_sound_volume(WavioSound* s, double gain);
int32_t wavio_sound_pan(WavioSound* s, double pan);
int32_t wavio_sound_pitch(WavioSound* s, double pitch);
int32_t wavio_sound_loop(WavioSound* s, int32_t loop);
int32_t wavio_sound_fade(WavioSound* s, double from, double to, int64_t frames);
int32_t wavio_sound_schedule(WavioSound* s, int64_t frame);
int32_t wavio_sound_position3d(WavioSound* s, double x, double y, double z);
WavioBus* wavio_bus_create(WavioContext* c);
void wavio_bus_close(WavioBus* b);
int32_t wavio_bus_volume(WavioBus* b, double gain);
int32_t wavio_sound_route(WavioSound* s, WavioBus* b);
/* Capture callback only copies into an SPSC ring. One control-thread reader.
 * capture index -1 means default. loopback is distinct from microphone input.
 * Read returns frames, 0 if empty, or negative status. Dropped frames are counted. */
WavioCapture* wavio_capture_open(WavioContext* c, int32_t device, int32_t channels, int32_t rate, int32_t capacity, int32_t loopback);
void wavio_capture_close(WavioCapture* c);
int32_t wavio_capture_start(WavioCapture* c);
int32_t wavio_capture_stop(WavioCapture* c);
int64_t wavio_capture_read(WavioCapture* c, WavioBuffer* b);
int64_t wavio_capture_dropped(WavioCapture* c);
/* Butterworth low/high pass, order 2. Processor state persists across blocks. */
WavioFilter* wavio_filter_create(int32_t highpass, int32_t channels, int32_t rate, double cutoff);
void wavio_filter_close(WavioFilter* f);
int32_t wavio_filter_process(WavioFilter* f, WavioBuffer* b);
int32_t wavio_filter_reset(WavioFilter* f);
#ifdef __cplusplus
}
#endif
