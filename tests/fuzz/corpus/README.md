# Fuzz and determinism corpus

## Fuzzer seeds (`tests/fuzz/corpus/*`)

Seed inputs for the libFuzzer STL harness:

- `seed_cube.stl` a valid 12-triangle binary STL (the example cube).
- `header_only.bin` a valid 84-byte header declaring zero triangles.
- `truncated.bin` a 40-byte file (shorter than the 84-byte header) that must be
  rejected as `StlError`, never crash.
- `empty` a zero-byte file, also rejected.

The harness asserts the parser never crashes on arbitrary input: malformed
buffers, truncated headers, and bogus triangle counts surface as `StlError`.

## Determinism corpus

The determinism guarantee is verified structurally rather than against a frozen
hash, so it holds across platforms: for a fixed mesh and configuration the
canonical-order output hash must be identical across thread counts {1, 2, 4, 8}
and across in-flight caps. See `tests/unit/test_determinism.cpp`. The reference
hashes below are recorded for an x86-64 Linux build (little-endian IEEE-754,
`make_blob(3, 12345)`, divisions 8, voxel-res 32, ratio 0.5):

```
bbox_stats   6b063b126c872cc3
voxel_count  34731a81bd6d0937
normals      94cd4ba6b32aec3d
decimate     2d47251616610662
```
