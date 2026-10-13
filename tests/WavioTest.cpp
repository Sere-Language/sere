#include "wavio/wavio.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "line %d: %s (native: %s)\n", __LINE__, #condition, \
               wavio_error_message(wavio_last_error())); std::exit(1); } } while (0)
static bool near(double a, double b, double tolerance = 1e-6) { return std::abs(a - b) <= tolerance; }

int main(int argc, char** argv) {
  const char* path = "wavio-roundtrip.wav";
  auto* b = wavio_buffer_create(4800, 2, 48000);
  CHECK(b);
  CHECK(!wavio_buffer_create(-1, 2, 48000));
  CHECK(!wavio_buffer_create(INT64_MAX, 32, 48000));
  CHECK(wavio_buffer_set(b, -1, 0) < 0);
  CHECK(wavio_buffer_set(b, 9600, 0) < 0);
  CHECK(wavio_buffer_set(b, 0, INFINITY) < 0);
  CHECK(wavio_buffer_tone(b, 0, 1000, 0.5, 0) == 0);
  CHECK(near(wavio_buffer_peak(b), 0.5));
  CHECK(near(wavio_buffer_rms(b), 0.5 / std::sqrt(2.0)));
  CHECK(wavio_buffer_normalize(b, 0.25) == 0);
  CHECK(near(wavio_buffer_peak(b), 0.25));
  auto* copy = wavio_buffer_slice(b, 0, 4800);
  CHECK(copy);
  CHECK(wavio_buffer_reverse(copy) == 0);
  CHECK(near(wavio_buffer_get(copy, 0), wavio_buffer_get(b, 9598)));
  CHECK(wavio_buffer_reverse(copy) == 0);
  CHECK(wavio_buffer_mix(copy, b, 0, -1) == 0);
  CHECK(wavio_buffer_peak(copy) == 0);
  CHECK(wavio_buffer_mix(copy, b, 1, 1) < 0);
  auto* converted = wavio_buffer_convert(b, 1, 24000);
  CHECK(converted);
  CHECK(std::abs(wavio_buffer_frames(converted) - 2400) <= 2);
  CHECK(wavio_buffer_channels(converted) == 1);
  CHECK(wavio_buffer_rms(converted) > 0.1);
  wavio_buffer_close(converted);

  for (int format = 1; format <= 5; ++format) {
    auto* writer = wavio_writer_open(path, std::strlen(path), 2, 48000, format);
    CHECK(writer);
    CHECK(wavio_writer_write(writer, b) == 4800);
    CHECK(wavio_writer_close(writer) == 0);
    auto* reader = wavio_reader_open(path, std::strlen(path));
    CHECK(reader);
    CHECK(wavio_reader_channels(reader) == 2);
    CHECK(wavio_reader_rate(reader) == 48000);
    CHECK(wavio_reader_length(reader) == 4800);
    CHECK(wavio_reader_read(reader, copy) == 4800);
    double tolerance = format == 1 ? 0.009 : format == 2 ? 0.00004 : 0.000001;
    for (int i = 0; i < 9600; ++i) CHECK(near(wavio_buffer_get(b, i), wavio_buffer_get(copy, i), tolerance));
    CHECK(wavio_reader_read(reader, copy) == 0);
    CHECK(wavio_buffer_peak(copy) == 0);
    CHECK(wavio_reader_seek(reader, 4700) == 0);
    CHECK(wavio_reader_read(reader, copy) == 100);
    CHECK(wavio_buffer_get(copy, 200) == 0);
    wavio_reader_close(reader);
  }
  CHECK(!wavio_reader_open("missing-wavio-file", 18));
  CHECK(!wavio_writer_open(path, std::strlen(path), 2, 48000, 6));
  CHECK(!wavio_reader_open("bad\0path", 8));

  auto* low = wavio_filter_create(0, 2, 48000, 500);
  auto* high = wavio_filter_create(1, 2, 48000, 500);
  CHECK(low && high);
  CHECK(wavio_buffer_tone(copy, 0, 8000, 1, 0) == 0);
  CHECK(wavio_filter_process(low, copy) == 0);
  CHECK(wavio_buffer_rms(copy) < 0.02);
  CHECK(wavio_filter_reset(low) == 0);
  CHECK(wavio_buffer_fill(copy, 1) == 0);
  CHECK(wavio_filter_process(high, copy) == 0);
  CHECK(std::abs(wavio_buffer_get(copy, 9599)) < 1e-5);
  CHECK(wavio_filter_reset(high) == 0);
  wavio_filter_close(low); wavio_filter_close(high);

  for (int round = 0; round < 3; ++round) {
    auto* context = wavio_context_create(48000, 2, 2, -1);
    CHECK(context);
    CHECK(wavio_context_rate(context) == 48000);
    CHECK(wavio_device_count(context, 0) > 0);
    CHECK(std::strlen(wavio_device_name(context, 0, 0)) > 0);
    auto* sound = wavio_sound_open(context, path, std::strlen(path), 0);
    auto* second = wavio_sound_open(context, path, std::strlen(path), 0);
    auto* bus = wavio_bus_create(context);
    CHECK(sound && second && bus);
    CHECK(wavio_sound_route(second, bus) == 0);
    CHECK(wavio_bus_volume(bus, 0.5) == 0);
    CHECK(wavio_sound_play(sound) == 0);
    CHECK(wavio_sound_play(second) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
    CHECK(wavio_buffer_peak(copy) > 0.3);
    CHECK(wavio_context_clock(context) == 4800);
    CHECK(wavio_sound_stop(sound) == 0);
    CHECK(wavio_sound_stop(second) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
    if (wavio_buffer_peak(copy) != 0) {
      int last = 0;
      for (int i = 0; i < 9600; ++i) if (wavio_buffer_get(copy, i) != 0) last = i;
      std::printf("stop residual peak=%f last sample=%d\n", wavio_buffer_peak(copy), last);
    }
    CHECK(wavio_buffer_peak(copy) == 0);
    CHECK(wavio_sound_loop(sound, 1) == 0);
    CHECK(wavio_sound_play(sound) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
    CHECK(wavio_sound_playing(sound));
    CHECK(wavio_buffer_peak(copy) > 0.2);
    CHECK(wavio_sound_pause(sound) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
    CHECK(wavio_buffer_peak(copy) == 0);
    CHECK(wavio_sound_play(sound) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
    CHECK(wavio_buffer_peak(copy) > 0.2);
    wavio_context_close(context); // Children must retain the engine and bus.
    wavio_bus_close(bus);
    CHECK(wavio_sound_stop(sound) == 0);
    wavio_sound_close(sound); wavio_sound_close(second);
  }

  // Real null-device callbacks, worker streaming, and capture overflow.
  auto* context = wavio_context_create(48000, 2, 1, -1);
  CHECK(context);
  auto* stream = wavio_sound_open(context, path, std::strlen(path), 1);
  CHECK(stream);
  CHECK(wavio_sound_loop(stream, 1) == 0);
  CHECK(wavio_sound_play(stream) == 0);
  auto* capture = wavio_capture_open(context, -1, 2, 48000, 256, 0);
  CHECK(capture);
  CHECK(wavio_capture_start(capture) == 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  CHECK(wavio_capture_stop(capture) == 0);
  CHECK(wavio_capture_dropped(capture) > 0);
  CHECK(wavio_capture_read(capture, copy) > 0);
  CHECK(wavio_buffer_peak(copy) == 0); // The explicit null input is silence.
  CHECK(wavio_sound_position(stream) >= 0);
  wavio_capture_close(capture); wavio_sound_close(stream); wavio_context_close(context);

  // Opt-in hardware probe does not record or play non-silent audio.
  if (argc > 1 && std::strcmp(argv[1], "--hardware") == 0) {
    context = wavio_context_create(48000, 2, 0, -1);
    CHECK(context);
    std::printf("hardware backend=%d outputs=%d inputs=%d\n", wavio_context_backend(context),
                wavio_device_count(context, 0), wavio_device_count(context, 1));
    for (int i = 0; i < wavio_device_count(context, 0); ++i)
      std::printf("output %d: %s\n", i, wavio_device_name(context, 0, i));
    wavio_context_close(context);
  }
  wavio_buffer_close(b); wavio_buffer_close(copy);
  CHECK(std::remove(path) == 0);
  std::puts("Wavio native checks passed");
  return 0;
}
