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
./build/meshslice slice examples/sphere.stl --adaptive --min-layer-height 0.02 \
    --max-layer-height 0.2 --export contours.json
./build/meshslice validate examples/cube.stl --fix-normals
```

## Slicing scaling

`slice_bench` slices a dense generated sphere (294,912 triangles, 300 layers)
at 1/2/4/8 worker threads. Layers are independent, so slicing scales like the
per-unit operators. Measured on an Apple M-series laptop (thread pool backend,
best of two passes after warm-up):

| threads | time (s) | speedup | efficiency |
|---------|----------|---------|------------|
| 1 | 0.228 | 1.00x | 100% |
| 2 | 0.127 | 1.80x | 90% |
| 4 | 0.061 | 3.73x | 93% |
| 8 | 0.040 | 5.65x | 71% |

The output is byte-identical regardless of thread count: layers land in
per-index slots, contours stitch in segment-index order, and shared-edge
crossings interpolate from a canonically ordered edge, so a determinism test
can compare serialized output across {1, 2, 4, 8} threads byte for byte.

## Layout

```
src/mesh        mesh types, STL + OBJ loaders, synthetic generators
src/partition   spatial grid partitioner and work units
src/pool        bounded in-flight thread pool
src/ops         the four operators
src/merge       canonical-order deterministic merge
src/slice       layer slicer, adaptive planner, validation, contour io
src/pipeline    partition -> pool -> ops -> merge
bench           scaling benchmark + regression gate + slice bench
tests           unit, e2e, libFuzzer
docs            design notes
```

## Releases

- **v1.0.0** parallel mesh pipeline: STL/OBJ loaders, spatial partitioning,
  bounded-memory thread pool, four operators, deterministic merge, NUMA-aware
  scheduling, and out-of-core streaming.
- **v2.0.0** layer slicer: uniform Z layers, plane-mesh intersection, contour
  stitching with closure/perimeter/signed area, OpenMP with thread pool
  fallback, and the `slice` CLI subcommand.
- **v3.0.0** validation: non-manifold edges, boundary loops, degenerate
  triangles, inverted windings, slice warnings on suspect input, and
  `--fix-normals` consistent outward reorientation.
- **v4.0.0** adaptive layers bounded by min/max heights (thin where the
  surface is flat, thick on walls) and `--export` contour JSON with an exact
  round-trip loader.
- **v5.0.0** slicing scaling bench with the speedup table above and
  byte-identical determinism tests across thread counts.

## License

MIT. See `LICENSE`.
