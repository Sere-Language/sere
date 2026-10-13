#include "internal.h"

struct WavioReader { ma_decoder decoder; };
struct WavioWriter { ma_encoder encoder; ma_default_vfs vfs; ma_vfs_file file; ma_result failure; };
static ma_result writeBytes(ma_encoder* encoder, const void* data, size_t size, size_t* written) {
  WavioWriter* w = encoder->pUserData;
  ma_result r = ma_vfs_write(&w->vfs, w->file, data, size, written);
  if (r != MA_SUCCESS || *written != size) w->failure = r != MA_SUCCESS ? r : MA_IO_ERROR;
  return w->failure;
}
static ma_result seekBytes(ma_encoder* encoder, ma_int64 offset, ma_seek_origin origin) {
  WavioWriter* w = encoder->pUserData;
  ma_result r = ma_vfs_seek(&w->vfs, w->file, offset, origin);
  if (r != MA_SUCCESS) w->failure = r;
  return w->failure;
}
WavioReader* wavio_reader_open(const char* data, int64_t length) {
  char* path = wavio_path(data, length);
  if (!path) return NULL;
  WavioReader* r = calloc(1, sizeof(*r));
  ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
  ma_result result = r ? ma_decoder_init_file(path, &config, &r->decoder) : MA_OUT_OF_MEMORY;
  free(path); wavio_result(result);
  if (result != MA_SUCCESS) { free(r); return NULL; }
  return r;
}
void wavio_reader_close(WavioReader* r) { if (r) { ma_decoder_uninit(&r->decoder); free(r); } }
int64_t wavio_reader_length(WavioReader* r) {
  ma_uint64 length = 0;
  ma_result result = r ? ma_decoder_get_length_in_pcm_frames(&r->decoder, &length) : MA_INVALID_ARGS;
  wavio_result(result); return result == MA_SUCCESS ? (int64_t)length : result;
}
int32_t wavio_reader_channels(WavioReader* r) { return r ? (int32_t)r->decoder.outputChannels : 0; }
int32_t wavio_reader_rate(WavioReader* r) { return r ? (int32_t)r->decoder.outputSampleRate : 0; }
int32_t wavio_reader_seek(WavioReader* r, int64_t frame) {
  if (!r || frame < 0) return wavio_result(MA_INVALID_ARGS);
  return wavio_result(ma_decoder_seek_to_pcm_frame(&r->decoder, (ma_uint64)frame));
}
int64_t wavio_reader_read(WavioReader* r, WavioBuffer* b) {
  if (!r || !b || b->channels != wavio_reader_channels(r) || b->rate != wavio_reader_rate(r))
    return wavio_result(MA_INVALID_ARGS);
  ma_uint64 count = 0;
  ma_result result = ma_decoder_read_pcm_frames(&r->decoder, b->data, (ma_uint64)b->frames, &count);
  memset(b->data + count * b->channels, 0, (size_t)(b->frames - count) * b->channels * sizeof(float));
  wavio_result(result == MA_AT_END ? MA_SUCCESS : result);
  return result == MA_SUCCESS || result == MA_AT_END ? (int64_t)count : result;
}
WavioWriter* wavio_writer_open(const char* data, int64_t length, int32_t channels, int32_t rate, int32_t format) {
  if (!wavio_format_valid(channels, rate) || format < 1 || format > 5) {
    wavio_result(MA_INVALID_ARGS); return NULL;
  }
  char* path = wavio_path(data, length);
  if (!path) return NULL;
  WavioWriter* w = calloc(1, sizeof(*w));
  ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, (ma_format)format, channels, rate);
  ma_result result = w ? ma_default_vfs_init(&w->vfs, NULL) : MA_OUT_OF_MEMORY;
  if (result == MA_SUCCESS) result = ma_vfs_open(&w->vfs, path, MA_OPEN_MODE_WRITE, &w->file);
  if (result == MA_SUCCESS) result = ma_encoder_init(writeBytes, seekBytes, w, &config, &w->encoder);
  free(path); wavio_result(result);
  if (result != MA_SUCCESS) { if (w && w->file) ma_vfs_close(&w->vfs, w->file); free(w); return NULL; }
  return w;
}
int64_t wavio_writer_write(WavioWriter* w, WavioBuffer* b) {
  if (!w || !b || b->channels != (int32_t)w->encoder.config.channels || b->rate != (int32_t)w->encoder.config.sampleRate)
    return wavio_result(MA_INVALID_ARGS);
  /* Fixed stack conversion blocks bound memory use regardless of file size. */
  unsigned char pcm[4096 * sizeof(float)];
  ma_uint64 total = 0;
  ma_uint32 chunk = 4096 / (ma_uint32)b->channels;
  while (total < (ma_uint64)b->frames) {
    ma_uint64 n = (ma_uint64)b->frames - total, written = 0;
    if (n > chunk) n = chunk;
    ma_pcm_convert(pcm, w->encoder.config.format, b->data + total * b->channels,
                   ma_format_f32, n * b->channels, ma_dither_mode_none);
    ma_result result = ma_encoder_write_pcm_frames(&w->encoder, pcm, n, &written);
    if (result != MA_SUCCESS || written != n) return wavio_result(result != MA_SUCCESS ? result : MA_IO_ERROR);
    total += written;
  }
  wavio_result(MA_SUCCESS); return (int64_t)total;
}
int32_t wavio_writer_close(WavioWriter* w) {
  if (!w) return wavio_result(MA_SUCCESS);
  ma_encoder_uninit(&w->encoder);
  ma_result result = ma_vfs_close(&w->vfs, w->file);
  if (w->failure != MA_SUCCESS) result = w->failure;
  free(w); return wavio_result(result);
}
