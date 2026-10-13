# miniaudio

Vendored unmodified `miniaudio.h` from upstream tag **0.11.23**:
https://github.com/mackron/miniaudio/tree/0.11.23

Header SHA-256:
`7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6`

The adjacent upstream LICENSE offers public-domain or MIT-0 terms. Sere uses
the MIT-0 option. Preserve the license when redistributing the source.
Builds require no downloads. `../miniaudio.c` is the sole implementation TU.
Wavio uses only the built-in WAV/FLAC/MP3 decoders and WAV encoder. Merely
vendoring upstream code does not expose every upstream feature through Wavio.
