# meshslice

A C++20 parallel 3D mesh processing pipeline. meshslice loads a triangle mesh
(binary STL or a simple OBJ subset), partitions it into independent spatial work
units, processes each unit on a bounded-memory thread pool, and merges the
results in a canonical order so the output is bit-identical regardless of thread
count or scheduling. The embarrassingly-parallel operators scale near-linearly
across cores.

## What it does

Four per-unit operators:

- `decimate` edge-collapse simplification toward a target triangle ratio
- `normals` recompute per-vertex area-weighted normals
- `bbox_stats` per-partition AABB, area-weighted centroid, and surface area
- `voxel_count` count occupied voxels at a chosen grid resolution

## Design

The work is pure CPU-bound geometry. There is no networking and no I/O on the
hot path. Three properties carry the project:

- **Spatial partitioning.** A regular grid over the mesh AABB. Each triangle is
  owned by the single cell containing its centroid, so the partition is a
  disjoint cover: every triangle assigned exactly once, no overlap, complete
  coverage. See `docs/partitioning.md`.
- **Bounded memory per worker.** The pool caps the number of in-flight units, so
  peak working set is `workers * max_unit_bytes` rather than the whole mesh. See
  `docs/memory-budget.md`.
- **Deterministic merge.** Results are merged by partition index, never by
  completion order. A property test proves the output is bit-identical across
  thread counts {1, 2, 4, 8}. See `docs/determinism.md`.
- **Near-linear scaling.** The bench reports throughput and scaling efficiency at
  1..N cores. See `docs/scaling.md`.

## Build and run

```sh
make build          # cmake + build (Release)
make test           # ctest
make asan           # AddressSanitizer + UBSan run
make tsan           # ThreadSanitizer on the concurrency tests
make bench          # write bench/results/bench_local.json
make bench-regress  # fail on >30% throughput drift vs the baseline
make format-check   # clang-format dry run

./build/meshslice bbox_stats examples/cube.stl
./build/meshslice voxel_count examples/sphere.stl --threads 4 --voxel-res 64
./build/meshslice decimate examples/bunny_lowpoly.stl --ratio 0.5
```

## Layout

```
src/mesh        mesh types, STL + OBJ loaders, synthetic generators
src/partition   spatial grid partitioner and work units
src/pool        bounded in-flight thread pool
src/ops         the four operators
src/merge       canonical-order deterministic merge
src/pipeline    partition -> pool -> ops -> merge
bench           scaling benchmark + regression gate
tests           unit, e2e, libFuzzer
docs            design notes
```

## License

MIT. See `LICENSE`.
