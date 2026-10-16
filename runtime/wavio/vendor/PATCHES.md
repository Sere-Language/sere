# Local miniaudio patches

## Node scheduling range overlap

`ma_node_read_pcm_frames()` calls `ma_node_get_state_by_time_range()` before
its existing logic to silence the prefix/suffix around a scheduled start/stop.
In 0.11.23 that predicate returns stopped whenever the scheduled start is after
the block beginning, or the stop is before the block ending. Consequently,
partial-block scheduling never reaches the prefix/suffix logic and delays or
advances the event by a block.

The local fix returns stopped only when the scheduled interval has no overlap
with the requested interval. A zero-length query (used by
`ma_node_get_state_by_time`) retains its original point-in-time semantics.
No other upstream implementation code is modified.

`tests/WavioTest.cpp` schedules a decoded voice 100 frames into a 4800-frame
offline block and checks the silent prefix, active remainder, and stereo pan.
Keep this regression when updating the dependency; remove this patch if an
upstream release fixes the range predicate.
