# Memory budget

meshslice bounds peak working-set memory so it scales with the number of workers,
not with the size of the mesh.

## The mechanism

The thread pool is constructed with two numbers: `workers` and `max_in_flight`.
A worker that wants the next unit must first acquire an in-flight slot; at most
`max_in_flight` units are checked out at any instant. With
`max_in_flight == workers`, each worker holds exactly one unit at a time.

A unit's working set is its triangle-index list plus whatever the operator
allocates for that unit (for example, the per-vertex normal map or the per-unit
edge list during decimation). Because only `max_in_flight` units are live, peak
memory is bounded by:

```
peak_working_set <= max_in_flight * max_unit_bytes
```

rather than `total_units * max_unit_bytes`, which is what a naive "materialize
every unit's result up front" approach would cost.

## Why this matters

For a large mesh partitioned into thousands of cells, the difference is the gap
between holding a few units' working sets and holding the entire mesh's derived
data at once. The cap turns memory from O(mesh) into O(cores).

## Verification

`tests/unit/test_memory_budget.cpp` runs the pool with caps {1, 2, 3} and asserts
the observed peak number of simultaneously-live units never exceeds the cap, using
both an instrumented counter inside the task body and the pool's own
`peak_in_flight()` high-water mark. A second test computes `max_unit_bytes` over a
large partitioned mesh and asserts the bounded budget is strictly below the
full-mesh footprint. The streaming out-of-core mode (`docs/out-of-core.md`)
extends the same bound to the loader so even the input never fully resides in
memory.
