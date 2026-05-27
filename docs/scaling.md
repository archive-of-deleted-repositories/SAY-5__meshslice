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

`make bench-regress` re-runs the benchmark and compares throughput against the
committed baseline JSON. If any operator at any thread count is more than 30%
slower than its baseline, the gate fails. This catches accidental performance
regressions in the partitioner, pool, or operators.

## Honest caveats

Measured efficiency depends heavily on the host: core count, turbo behavior,
memory bandwidth, and how oversubscribed the machine is. On a small shared CI
runner the absolute numbers are modest and 8-thread efficiency can fall well below
1.0 because the box has fewer than 8 physical cores. The committed baseline is
therefore a local reference; the gate measures drift against that reference rather
than asserting an absolute speedup.
