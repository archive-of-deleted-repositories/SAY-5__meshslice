# NUMA-aware scheduling

On a multi-socket machine, memory is attached to specific sockets (NUMA nodes).
A thread running on node 0 that touches memory homed on node 1 pays a remote
access penalty. meshslice can pin workers to nodes and steer each partition to
the node whose memory holds it, so the per-unit working set stays node-local.

## Detection

`detect_numa_topology()` parses `/sys/devices/system/node/node*/cpulist` on
Linux to enumerate nodes and their CPUs. On a non-Linux host, or when `/sys` is
unavailable, it returns a synthetic single node covering all online CPUs. The
result always has at least one node with at least one CPU, so no caller has to
handle an empty topology.

## Scheduling

When `--numa` is set (or `PipelineConfig::numa_aware`), the thread pool pins each
worker round-robin to a NUMA node via `pthread_setaffinity_np`. Partitions are
assigned to nodes round-robin by index (`assign_partitions_to_nodes`), matching
the worker pinning so a worker pulls units homed on its own node.

## Determinism is preserved

NUMA scheduling only changes where threads run and which worker pulls which unit.
It never changes the canonical merge order, so the output is bit-identical to the
naive scheduler. `tests/unit/test_numa.cpp` asserts the output hash is unchanged
with NUMA on versus off for every operator.

## Graceful degradation on a single-node box

A single-socket machine, which is what the CI runners are, reports one NUMA node.
In that case:

- partition-to-node assignment maps everything to node 0 (a no-op),
- worker pinning to the single node's CPUs adds no locality benefit, and the test
  asserts `pinned_workers() == 0` so the code path is exercised without claiming a
  win,
- the run completes with identical output and the same performance as the naive
  scheduler.

This is the honest position: the NUMA code path is exercised in CI and verified
to degrade cleanly, but the locality speedup is hardware-dependent and only shows
up on a genuine multi-socket box. The reported `numa_nodes` and
`numa_pinned_workers` make the outcome observable. To measure the win, run the
benchmark with and without `--numa` on a two-socket host and compare throughput
for the bandwidth-sensitive operators; on a one-node box the two runs are
expected to match within noise.
