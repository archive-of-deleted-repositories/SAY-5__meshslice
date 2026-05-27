# Partitioning

meshslice partitions a mesh with a regular spatial grid laid over its axis-aligned
bounding box. The grid has `divisions` cells per axis (a maximum of
`divisions^3` cells). Only non-empty cells become work units.

## Centroid ownership

Each triangle is assigned to exactly one cell: the cell that contains the
triangle's centroid. A triangle whose three vertices straddle a cell boundary is
still owned by a single cell, the one holding the centroid. This is a deliberate
choice that keeps the partition a true disjoint cover:

- every triangle is assigned exactly once,
- no triangle appears in two units (no overlap),
- the union of all units equals the input (complete coverage).

The alternative (assigning a triangle to every cell it overlaps) would duplicate
boundary triangles and break the disjoint-cover invariant that the operators and
the deterministic merge rely on.

## Boundary handling

A centroid can land exactly on a cell boundary or just outside the AABB due to
floating-point rounding. The partitioner clamps the computed cell index into
`[0, divisions)` on each axis, so boundary and out-of-range centroids resolve
deterministically to the nearest in-range cell. Degenerate axes (a flat mesh with
zero extent on one axis) collapse to a single cell on that axis.

## Determinism of the partition itself

Cells are keyed by a linear `(i, j, k)` index and grouped with an ordered map, so
the produced work-unit indices are stable run to run. That stable index is what
the merger uses as the canonical order. The invariants above are checked by
`tests/unit/test_grid.cpp` and stress-tested over random triangle soups in
`tests/unit/test_property.cpp`.
