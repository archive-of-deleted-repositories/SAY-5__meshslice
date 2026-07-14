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

Plus two whole-mesh commands:

- `slice` cut the mesh into uniform Z layers and stitch each plane-mesh
  intersection into print contours, reporting closure, perimeter, and signed
  area per contour. Layers are processed in parallel: OpenMP when the toolchain
  has it, the in-tree thread pool otherwise.
- `validate` structural health report: non-manifold edges, open boundary loops
  (holes), degenerate triangles, and inverted windings. `--fix-normals`
  reorients every surface component consistently outward.

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
- **NUMA-aware scheduling.** Optional worker pinning and partition-to-node
  assignment, degrading cleanly to the naive scheduler on a single-node box. See
  `docs/numa.md`.
- **Out-of-core streaming.** A streaming loader partitions on the fly and never
  holds the whole mesh, so peak memory is bounded regardless of input size. See
  `docs/out-of-core.md`.

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
./build/meshslice bbox_stats examples/sphere.stl --numa     # NUMA-aware
./build/meshslice voxel_count big.stl --stream              # out-of-core
./build/meshslice slice examples/sphere.stl --layer-height 0.1 --format csv
./build/meshslice validate examples/cube.stl --fix-normals
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
