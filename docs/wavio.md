# Wavio native audio

Wavio is an initial working audio foundation, **not the complete professional
audio subsystem in the engineering specification**. Import it with `import
wavio`; submodules are `buffer`, `codec`, `context`, `sound`, `capture`, and
`dsp`. All C implementation files, native headers, and vendored miniaudio live
under `runtime/wavio/`. The Sere package contains only Sere source.

## Current API

Every public Sere class and method has a docstring. The native ABI and common
return/ownership contracts are in `runtime/wavio/wavio.h`.

| API | Implemented behavior |
| --- | --- |
| `AudioBuffer(frames, channels=2, sample_rate=48000)` | Owned interleaved float32; get/set, fill, gain, peak/RMS, normalize, reverse, copying slice, channel/rate conversion, mix, basic generated waveforms |
| `AudioReader(path)` | Incremental WAV/FLAC/MP3 decoding, frame length/rate/channels, seek, read into caller storage |
| `AudioWriter(path, channels=2, sample_rate=48000, format=5)` | Streaming WAV encoder: U8=1, S16=2, packed S24=3, S32=4, F32=5; converts float32 in bounded blocks |
| `write(path, buffer, format=5)` | Whole-buffer WAV convenience writer |
| `AudioContext(sample_rate=48000, channels=2, mode=0, device=-1)` | Native output, snapshot input/output enumeration, device selection, engine format/clock; offline rendering |
| `Sound(context, path, stream=0)` | Independent voice; play/pause/resume/stop, seek, loop, smoothed gain, balance, resampling pitch/speed, fade, scheduled start, basic spatial positioning |
| `Bus(context)` | Native submix with gain, retained by routed voices |
| `Recorder(context, device=-1, channels=1, sample_rate=48000, capacity=48000, loopback=0)` | Selected input or explicit loopback capture, start/stop, drain preallocated SPSC ring, overflow counter |
| `Filter(cutoff, channels=2, sample_rate=48000, highpass=0)` | Stateful second-order Butterworth low/high-pass, allocation-free block processing, reset |
| `can_decode(format)`, `can_encode(format)` | Separate truthful built-in capability lists |
| `last_error()`, `error_message(code)` | Calling-thread native status and copied description |

Channels range from 1 to 32; sample rates from 8000 to 384000. Float PCM is not
automatically limited: mixing can exceed full scale. Integer encoding clips
according to miniaudio's conversion. This revision does not add dithering.
Sample indices are interleaved: frame `f`, channel `c` is `f * channels + c`.
Frame counts, offsets, seek positions, and ring capacity are **per channel**.

## Ownership, errors, and threading

Check `is_open()` immediately after construction. On failure it returns false;
`last_error()` provides a negative native status. Status-returning operations
return zero on success. Reads/writes return a frame count or negative error.
Reader EOF is zero, not an error. Short reads silence the unused buffer tail;
write only the returned frames when recording (slice a short final block).

Every resource requires explicit `close()`. Repeated close on the same Sere
object is harmless. Do not copy or manipulate its `handle` field, independently
close a borrowed handle, or call methods concurrently with close. The native
C ABI requires exactly one close per owned reference; it cannot validate
dangling pointers. There are no automatic finalizers in this initial wrapper.
Sounds, buses and recorders retain the native context; sounds also retain their
bus. Closing a context first does not invalidate its children, but its hardware
device remains open until the last child closes.

Use one application thread for resource creation, mutation, and teardown.
Do not expose Sere callbacks to the native audio thread. Buffers and offline
processors must have exclusive access while processing. Capture uses
miniaudio's single-producer/single-consumer ring with acquire/commit publication;
one native callback writes and exactly one application thread reads. Overflow
drops incoming frames and increments a lock-free relaxed atomic counter.
Device teardown joins callbacks before freeing their ring and userdata.

Decoded sounds use miniaudio's shared resource cache. Streaming voices use its
worker and bounded decoded pages instead of loading the file into RAM. Seek
and stream cancellation use the upstream resource-manager protocol. Stream
underrun telemetry is not yet exposed. DSP/filter buffers are preallocated;
capture does no file I/O, heap allocation, logging, Sere execution, or GC work.
The upstream engine graph synchronizes attachment changes using its own
atomics/spinlocks. Wavio therefore does **not** promise a wait-free or formally
bounded hard-real-time engine, especially during live topology changes.

## Devices and platform support

Mode 0 opens hardware and excludes null fallback. Mode 1 explicitly opens the
miniaudio null device (silence input, discarded output); mode 2 creates an
offline engine with no device callback. Only mode 2 permits `render()`.
Null/offline enumeration is synthetic and must never be presented as hardware.

The vendored backend supports WASAPI/DirectSound/WinMM on Windows, Core Audio
on macOS, and PulseAudio/ALSA/JACK on Linux. Selection is automatic within the
explicit hardware backend list. These platform claims describe dependency
support; they do not mean Sere was tested on all those hosts. System libraries
are loaded by miniaudio; no audio SDK or manual linker flags are needed by
ordinary users. PipeWire may be reached through a system's PulseAudio or JACK
compatibility service; Wavio does not advertise a native PipeWire backend.
ASIO is not integrated.

Device enumeration is a context-lifetime snapshot. Indices are local to that
snapshot and are not persistent IDs. Names are copied to GC-owned Sere strings.
`sample_rate()` and `channels()` report the **engine** format; hardware can use
another native format behind conversion. `clock()` counts mixed frames and
does not account for queued hardware latency. There is no claim of bit-perfect
output, exclusive access, hotplug recovery, or measured presentation latency.
Loopback is a separate explicit device type and fails when unsupported.

Stopping a routed voice can leave one interpolated frame in its bus resampler;
already buffered device output also cannot be recalled. Pause/stop are control
operations, not a guarantee that the physical output becomes silent instantly.

## Examples

Generate, filter, and save a tone without audio hardware:

```python
import wavio

def main() -> i32:
    samples: wavio.AudioBuffer = wavio.AudioBuffer(48000, 2, 48000)
    assert samples.is_open()
    assert samples.tone(440.0, 0.15) == 0
    lowpass: wavio.Filter = wavio.Filter(2000.0)
    assert lowpass.is_open()
    assert lowpass.process(samples) == 0
    assert wavio.write("tone.wav", samples, 3) == 0
    lowpass.close()
    samples.close()
    return 0
```

Playback and enumeration are demonstrated in `examples/wavio_play.sere`.
The headless end-to-end example/test is `tests/features/wavio_audio.sere`.
Use `Sound(context, path, 1)` for large-file streaming. A separate Sound for
each trigger permits overlapping playback while sharing decoded resources.
Use `sound.route(bus)` to create a submix; `bus.set_volume(0.0)` mutes it.
`sound.schedule(context.clock() + 48000)` schedules one engine-second ahead at
48 kHz. Predecoded sources are appropriate for precise triggering; a stream's
worker may not be ready at its scheduled frame.

Capture is deliberately a drain API: create a Recorder and a matching buffer,
start it, regularly call `read(buffer)`, process/write the returned frames on
the application thread, then stop, drain remaining frames, and close. Increasing
capacity tolerates longer application stalls at the cost of buffered latency.
Playback and capture can run concurrently, but there is not yet synchronized
full-duplex clock handling or a low-latency monitoring route.

## Build and validation

miniaudio 0.11.23 is vendored with its license and SHA-256 in
`runtime/wavio/vendor/README.md`. Normal CMake runtime builds include Wavio;
the static archive extracts audio objects only when referenced. Existing
release staging copies the runtime archive and stdlib package. Regenerate the
private ABI declarations with `python scripts/gen-wavio-bindings.py` after
editing the native header.

```powershell
. .\scripts\env.ps1
cmake --build --preset windows-clang-cl-relwithdebinfo --target sere_stage sere_test_wavio
ctest --preset windows-clang-cl-relwithdebinfo -R wavio --output-on-failure
```

Native tests exercise PCM WAV round trips for all five formats, EOF and seek,
invalid sizes/paths/parameters, buffer math, resampling, numerical filter
responses/reset, overlapping voices, bus routing, pause/resume/looping,
parent-before-child cleanup, null-device worker streaming, and capture overflow.
The Sere feature test exercises import, linking, string ABI, and wrapper calls.
`sere_test_wavio --hardware` additionally opens real output, prints enumeration,
plays a quiet 250 ms tone, and attempts to drain the default capture device.
Unavailable capture is reported explicitly. Captured samples stay in memory.
This probe cannot establish that a human heard the output.
`sere_test_wavio --benchmark` measures mean/maximum render time over 200 blocks
after warmup for 1, 32, 128 and 256 cached voices, using 256-frame stereo blocks
at 48 kHz. It measures software render time, not physical latency or dropouts.

## Remaining specification work

This patch does not complete the original seven-phase specification. Remaining
work includes an independently selectable discovery context, persistent device
IDs/capabilities, exclusive/latency/buffer configuration, device notifications,
recovery, synchronized duplex/monitoring, a first-class sound-resource/voice-pool
API with priorities and stealing, in-memory/custom streaming sources, float64
and planar public PCM, zero-copy views, resize/append, robust large-file WAV/RF64
limits, richer metadata, additional codecs and encoders (Vorbis/Opus/AIFF/AAC,
FLAC/MP3 encoding), independent time stretch/pitch shift, custom graph nodes,
cycle checks, sends, solo, latency compensation, insert DSP, advanced effects,
ADSR/wavetables/samplers, FFT/LUFS/pitch/onset analysis, transport/MIDI, complete
listener/velocity/cone control, HRTF/ambisonics, stream dropout diagnostics,
SIMD/allocation instrumentation, sanitizer coverage and cross-platform hardware
qualification. No empty modules or success-returning stubs stand in for these.
