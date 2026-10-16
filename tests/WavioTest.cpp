#include "wavio/wavio.h"
#include "wavio/vendor/miniaudio.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#define CHECK(condition)                                                                           \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::fprintf(stderr,                                                                         \
                   "line %d: %s (native: %s)\n",                                                   \
                   __LINE__,                                                                       \
                   #condition,                                                                     \
                   wavio_error_message(wavio_last_error()));                                       \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)
static bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) <= tolerance;
}

int main(int argc, char** argv) {
  // Pin the vendored scheduling range patch, including point queries.
  {
    ma_node_graph graph;
    auto config = ma_node_graph_config_init(2);
    CHECK(ma_node_graph_init(&config, nullptr, &graph) == MA_SUCCESS);
    auto* node = ma_node_graph_get_endpoint(&graph);
    CHECK(ma_node_set_state_time(node, ma_node_state_started, 100) == MA_SUCCESS);
    CHECK(ma_node_set_state_time(node, ma_node_state_stopped, 200) == MA_SUCCESS);
    CHECK(ma_node_get_state_by_time_range(node, 0, 100) == ma_node_state_stopped);
    CHECK(ma_node_get_state_by_time_range(node, 0, 101) == ma_node_state_started);
    CHECK(ma_node_get_state_by_time_range(node, 150, 250) == ma_node_state_started);
    CHECK(ma_node_get_state_by_time_range(node, 200, 250) == ma_node_state_stopped);
    CHECK(ma_node_get_state_by_time(node, 99) == ma_node_state_stopped);
    CHECK(ma_node_get_state_by_time(node, 100) == ma_node_state_started);
    CHECK(ma_node_get_state_by_time(node, 199) == ma_node_state_started);
    CHECK(ma_node_get_state_by_time(node, 200) == ma_node_state_stopped);
    ma_node_graph_uninit(&graph, nullptr);
  }
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
    double tolerance = format == 1 ? 0.009 : format == 2 ? 0.00004 : format == 5 ? 0 : 0.00000013;
    for (int i = 0; i < 9600; ++i)
      CHECK(near(wavio_buffer_get(b, i), wavio_buffer_get(copy, i), tolerance));
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
  {
    FILE* invalid = std::fopen("wavio-invalid.bin", "wb");
    CHECK(invalid);
    CHECK(std::fwrite("not an audio file", 1, 17, invalid) == 17);
    CHECK(std::fclose(invalid) == 0);
    CHECK(!wavio_reader_open("wavio-invalid.bin", 17));
    CHECK(std::remove("wavio-invalid.bin") == 0);
  }

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
  wavio_filter_close(low);
  wavio_filter_close(high);

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
    // The bus resampler can retain one interpolated stereo frame after stop.
    for (int i = 2; i < 9600; ++i)
      CHECK(wavio_buffer_get(copy, i) == 0);
    CHECK(wavio_context_render(context, copy) == 0);
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
    wavio_sound_close(sound);
    wavio_sound_close(second);
  }

  {
    auto* scheduled = wavio_context_create(48000, 2, 2, -1);
    auto* voice = wavio_sound_open(scheduled, path, std::strlen(path), 0);
    CHECK(scheduled && voice);
    CHECK(wavio_sound_pan(voice, -1) == 0);
    CHECK(wavio_sound_schedule(voice, 100) == 0);
    CHECK(wavio_context_render(scheduled, copy) == 0);
    for (int i = 0; i < 200; ++i)
      CHECK(wavio_buffer_get(copy, i) == 0);
    for (int i = 1; i < 9600; i += 2)
      CHECK(wavio_buffer_get(copy, i) == 0);
    CHECK(wavio_buffer_peak(copy) > 0.2);
    wavio_sound_close(voice);
    wavio_context_close(scheduled);
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
  wavio_capture_close(capture);
  wavio_sound_close(stream);
  wavio_context_close(context);

  if (argc > 1 && std::strcmp(argv[1], "--benchmark") == 0) {
    auto* block = wavio_buffer_create(256, 2, 48000);
    CHECK(block);
    for (int count : {1, 32, 128, 256}) {
      auto* engine = wavio_context_create(48000, 2, 2, -1);
      CHECK(engine);
      std::vector<WavioSound*> voices;
      for (int i = 0; i < count; ++i) {
        auto* voice = wavio_sound_open(engine, path, std::strlen(path), 0);
        CHECK(voice);
        CHECK(wavio_sound_loop(voice, 1) == 0);
        CHECK(wavio_sound_volume(voice, 1.0 / count) == 0);
        CHECK(wavio_sound_play(voice) == 0);
        voices.push_back(voice);
      }
      double totalUs = 0, peakUs = 0;
      for (int iteration = 0; iteration < 210; ++iteration) {
        auto start = std::chrono::steady_clock::now();
        CHECK(wavio_context_render(engine, block) == 0);
        double us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                .count();
        if (iteration >= 10) {
          totalUs += us;
          peakUs = std::fmax(peakUs, us);
        }
      }
      CHECK(wavio_buffer_peak(block) > 0.1);
      std::printf("%d voices: 256-frame block mean %.2f us, max %.2f us, budget 5333.33 us\n",
                  count,
                  totalUs / 200,
                  peakUs);
      for (auto* voice : voices)
        wavio_sound_close(voice);
      wavio_context_close(engine);
    }
    wavio_buffer_close(block);
  }

  // Opt-in hardware probe briefly plays a quiet tone and drains default input.
  if (argc > 1 && std::strcmp(argv[1], "--hardware") == 0) {
    context = wavio_context_create(48000, 2, 0, -1);
    CHECK(context);
    std::printf("hardware backend=%d outputs=%d inputs=%d\n",
                wavio_context_backend(context),
                wavio_device_count(context, 0),
                wavio_device_count(context, 1));
    for (int i = 0; i < wavio_device_count(context, 0); ++i)
      std::printf("output %d: %s\n", i, wavio_device_name(context, 0, i));
    auto* hardwareSound = wavio_sound_open(context, path, std::strlen(path), 1);
    CHECK(hardwareSound);
    CHECK(wavio_sound_volume(hardwareSound, 0.1) == 0);
    CHECK(wavio_sound_loop(hardwareSound, 1) == 0);
    CHECK(wavio_sound_play(hardwareSound) == 0);
    auto* hardwareCapture = wavio_capture_open(context, -1, 2, 48000, 48000, 0);
    if (hardwareCapture)
      CHECK(wavio_capture_start(hardwareCapture) == 0);
    auto* loopback = wavio_capture_open(context, -1, 2, 48000, 48000, 1);
    if (loopback)
      CHECK(wavio_capture_start(loopback) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    CHECK(wavio_context_clock(context) > 0);
    CHECK(wavio_sound_position(hardwareSound) > 0);
    CHECK(wavio_sound_stop(hardwareSound) == 0);
    if (hardwareCapture) {
      CHECK(wavio_capture_stop(hardwareCapture) == 0);
      auto captured = wavio_capture_read(hardwareCapture, copy);
      CHECK(captured > 0);
      std::printf("hardware input: %lld frames, peak %.6f, dropped %lld\n",
                  static_cast<long long>(captured),
                  wavio_buffer_peak(copy),
                  static_cast<long long>(wavio_capture_dropped(hardwareCapture)));
      wavio_capture_close(hardwareCapture);
    } else {
      std::puts("hardware capture unavailable; not verified");
    }
    if (loopback) {
      CHECK(wavio_capture_stop(loopback) == 0);
      CHECK(wavio_capture_read(loopback, copy) > 0);
      std::printf("hardware loopback peak: %.6f\n", wavio_buffer_peak(copy));
      CHECK(wavio_buffer_peak(copy) > 0);
      wavio_capture_close(loopback);
    } else {
      std::puts("hardware loopback unavailable; not verified");
    }
    wavio_sound_close(hardwareSound);
    wavio_context_close(context);
  }
  wavio_buffer_close(b);
  wavio_buffer_close(copy);
  CHECK(std::remove(path) == 0);
  std::puts("Wavio native checks passed");
  return 0;
}
