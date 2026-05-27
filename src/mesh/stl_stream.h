#ifndef MESHSLICE_STL_STREAM_H
#define MESHSLICE_STL_STREAM_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <string>

#include "mesh/types.h"

namespace meshslice {

// One streamed triangle: three corner positions, no shared vertex array. The
// streaming reader yields these one record at a time so the whole mesh is never
// materialized in memory.
struct StreamTriangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;

    Vec3 centroid() const {
        return (a + b + c) * (1.0f / 3.0f);
    }
};

// Stream a binary STL from an istream, invoking `sink` once per triangle. Reads
// in fixed-size chunks (default 64 KiB worth of triangle records) so peak input
// memory is bounded by the chunk size regardless of file size. Returns the number
// of triangles streamed. Throws StlError on a truncated header or count.
uint64_t stream_binary_stl(std::istream& in, const std::function<void(const StreamTriangle&)>& sink,
                           size_t chunk_triangles = 1024);

// Same, reading from a file path.
uint64_t stream_binary_stl_file(const std::string& path,
                                const std::function<void(const StreamTriangle&)>& sink,
                                size_t chunk_triangles = 1024);

}  // namespace meshslice

#endif  // MESHSLICE_STL_STREAM_H
