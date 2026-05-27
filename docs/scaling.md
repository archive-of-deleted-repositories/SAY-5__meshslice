# Scaling

The operators are embarrassingly parallel: each work unit is processed
independently, and the only synchronization is the in-flight slot accounting plus
the final canonical merge. That structure should give near-linear speedup up to
the core count, bounded by memory bandwidth and the serial merge tail.

## The benchmark

`bench/scaling_bench.cpp` generates a synthetic mesh (~500k triangles by default),
runs each operator at 1, 2, 4, and 8 worker threads, and reports:

- **throughput** in triangles per second,
- **scaling efficiency** = `speedup / threads`, where `speedup = t1 / tN`,
- the best of three timed runs after a warm-up, to reduce noise.

Run it with `make bench`, which writes `bench/results/bench_local.json`.

## Regression gate

`make bench-regress` re-runs the benchmark and compares it against the committed
baseline JSON. The gate keys on **scaling efficiency** (`speedup / threads`)
rather than absolute throughput, because efficiency is host-independent: a slower
box has lower absolute throughput but the same parallel efficiency. It only gates
thread counts up to the physical core count and only operators whose baseline
efficiency was meaningfully parallel (> 0.4), so memory-bandwidth-bound operators
near the serial floor do not produce flaky failures. A drop of more than 30% in
efficiency fails the gate.

## Memory budget in the bench

The bench also prints the bounded-memory budget: `partitions`, `max_unit_bytes`,
the bounded budget `workers * max_unit_bytes`, and the full-mesh footprint
`partitions * max_unit_bytes`. On the ~200k-triangle synthetic mesh the bounded
budget is roughly 0.2% of the full-mesh footprint, confirming peak working set
scales with cores, not mesh size. Process peak RSS is reported via `getrusage`.

## Measured (local reference, Apple M-series, ~200k tris)

| op | t1 | t2 eff | t4 eff |
| --- | --- | --- | --- |
| decimate | 1.0M tris/s | 0.89 | 0.72 |
| voxel_count | 3.8M tris/s | 0.56 | 0.30 |
| normals | 1.2M tris/s | 0.50 | 0.26 |
| bbox_stats | 8.9M tris/s | 0.53 | 0.25 |

`decimate` is the compute-bound operator and scales close to linear at 2 threads.
The lighter operators (`bbox_stats`, `normals`) are already fast enough per
triangle that they are memory-bandwidth bound on this mesh layout, so additional
threads add little; this is expected and is why the gate excludes them.

## Honest caveats

Measured efficiency depends heavily on the host: core count, turbo behavior,
memory bandwidth, and how oversubscribed the machine is. On a small shared CI
runner the absolute numbers are modest and 8-thread efficiency can fall well below
1.0 because the box has fewer than 8 physical cores. The committed baseline is
therefore a local reference; the gate measures drift against that reference rather
than asserting an absolute speedup.
