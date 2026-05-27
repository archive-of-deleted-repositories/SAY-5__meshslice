#include "mesh/stl_stream.h"

#include <cstring>
#include <fstream>
#include <vector>

#include "mesh/stl_loader.h"  // for StlError

namespace meshslice {

namespace {

constexpr size_t kHeaderBytes = 80;
constexpr size_t kCountBytes = 4;
constexpr size_t kTriangleRecordBytes = 50;

float read_le_float(const uint8_t* p) {
    uint32_t bits = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                    (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    float out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

uint32_t read_le_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

Vec3 read_vertex(const uint8_t* p) {
    return Vec3(read_le_float(p), read_le_float(p + 4), read_le_float(p + 8));
}

}  // namespace

uint64_t stream_binary_stl(std::istream& in, const std::function<void(const StreamTriangle&)>& sink,
                           size_t chunk_triangles) {
    if (chunk_triangles == 0) {
        chunk_triangles = 1;
    }
    uint8_t head[kHeaderBytes + kCountBytes];
    in.read(reinterpret_cast<char*>(head), sizeof(head));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(head))) {
        throw StlError("truncated header: need at least 84 bytes");
    }
    const uint32_t count = read_le_u32(head + kHeaderBytes);

    std::vector<uint8_t> buf(chunk_triangles * kTriangleRecordBytes);
    uint64_t streamed = 0;
    while (streamed < count) {
        const uint64_t want = std::min<uint64_t>(chunk_triangles, count - streamed);
        const std::streamsize bytes = static_cast<std::streamsize>(want * kTriangleRecordBytes);
        in.read(reinterpret_cast<char*>(buf.data()), bytes);
        if (in.gcount() != bytes) {
            throw StlError("truncated triangle records: declared count exceeds stream");
        }
        for (uint64_t i = 0; i < want; ++i) {
            const uint8_t* rec = buf.data() + i * kTriangleRecordBytes;
            const uint8_t* vp = rec + 12;  // skip the face normal.
            StreamTriangle t;
            t.a = read_vertex(vp);
            t.b = read_vertex(vp + 12);
            t.c = read_vertex(vp + 24);
            sink(t);
        }
        streamed += want;
    }
    return streamed;
}

uint64_t stream_binary_stl_file(const std::string& path,
                                const std::function<void(const StreamTriangle&)>& sink,
                                size_t chunk_triangles) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw StlError("cannot open file: " + path);
    }
    return stream_binary_stl(f, sink, chunk_triangles);
}

}  // namespace meshslice
