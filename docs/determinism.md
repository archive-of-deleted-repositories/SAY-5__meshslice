# Determinism

The headline guarantee: for a fixed input and configuration, meshslice produces
bit-identical output regardless of the number of worker threads or how the
scheduler interleaves them.

## How it is achieved

1. **Stable partition order.** The partitioner assigns each work unit a stable
   index derived from its grid cell, independent of thread activity.
2. **No shared mutable state in operators.** Each unit is processed in isolation
   and writes its result into a pre-sized slot `results[unit.index]`. Workers
   never touch each other's slots, so there are no data races and no
   order-dependent accumulation inside the parallel phase.
3. **Canonical-order merge.** The merger consumes `results` in ascending
   partition index, never in completion order. Floating-point addition is not
   associative, so fixing the summation order is what makes the result
   bit-stable. Per-vertex normal contributions are accumulated through ordered
   maps for the same reason.

## Proof

`tests/unit/test_determinism.cpp` hashes the canonical-order merged output (FNV-1a
over the exact byte image) and asserts the hash is identical across thread counts
{1, 2, 4, 8} for all four operators. It also varies the in-flight cap
independently of the worker count so completion order changes while thread count
is held fixed, and confirms the hash is unchanged.

## Limits

Determinism holds for a fixed `divisions` and operator configuration. Changing the
grid resolution changes the partition and therefore the summation order, which can
change low bits of floating-point results; that is expected and documented rather
than hidden.
