# Out-of-core streaming mode

The streaming mode processes meshes larger than RAM. It reads the STL in chunks,
partitions on the fly, and never holds the whole mesh in memory. Peak memory is
bounded regardless of input size.

## How it works

A `TriangleSource` drives the engine: it invokes a sink once per triangle, in a
stable order, and may be replayed. Backed by an STL file, each pass re-opens the
file and reads it in fixed-size chunks (`stream_binary_stl`), so peak input memory
is one chunk, not the whole file. Backed by an on-the-fly generator, no bytes are
stored at all.

The engine makes two passes:

1. **AABB pass.** Stream every triangle to compute the global bounding box. State
   is a handful of scalars.
2. **Fold pass.** Stream again, assigning each triangle to its grid cell by
   centroid and folding it into that cell's aggregate (a `CellBbox` for
   `bbox_stats`, or directly into the global occupied-voxel set for
   `voxel_count`). Peak state is one aggregate per occupied cell (at most
   `divisions^3`) or the distinct-voxel set (at most `resolution^3`), never one
   record per triangle.

Only the foldable operators (`bbox_stats`, `voxel_count`) run in streaming mode,
because their per-partition state is a fixed-size aggregate. `decimate` and
`normals` need the full per-unit triangle set and are in-core only; the engine
rejects them with an explicit error.

## Memory bound

Peak working set is `occupied_cells * sizeof(aggregate)` (plus one chunk of input
and one triangle), independent of the triangle count. A 2 GB-equivalent in-core
mesh (~60M triangles at 36 bytes each) would need roughly 2 GB if materialized.
Streaming it with a `16^3` grid holds at most 4096 small aggregates, a few
hundred kilobytes. `tests/unit/test_streaming.cpp` streams 60M generated
triangles under a 256 MB cap and asserts process peak RSS stays under it.

## Chunk-boundary triangle handling

Triangles are whole 50-byte records in the binary STL. The chunk reader always
consumes complete records: a chunk is `chunk_triangles * 50` bytes, so a triangle
is never split across a chunk boundary. Each triangle is assigned to exactly one
cell by its centroid, identically to the in-core partitioner, so there is no
double counting at chunk boundaries and the cell assignment is independent of how
the stream is chunked. The result is bit-identical to the in-core pipeline on the
same mesh, which the tests verify by comparing output hashes.

## Determinism across modes

Because the streaming engine uses the same centroid-to-cell rule, the same
canonical cell-key merge order, and the same aggregate math as the in-core
pipeline, `run_streaming` and `run_pipeline` produce identical output hashes for
`bbox_stats` and `voxel_count`. Streaming is a memory optimization, not a
different algorithm.
