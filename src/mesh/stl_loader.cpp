#include "mesh/stl_loader.h"

#include <cstring>
#include <fstream>
#include <ios>

namespace meshslice {

namespace {

constexpr size_t kHeaderBytes = 80;
constexpr size_t kCountBytes = 4;
constexpr size_t kTriangleRecordBytes = 50;  // normal(12) + 3*vertex(36) + attr(2)

float read_le_float(const uint8_t* p) {
    // STL is little-endian IEEE-754. Reconstruct via memcpy after assembling the
    // 32-bit word from individual bytes so the parse is endian-independent.
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

void write_le_float(std::vector<uint8_t>& out, float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    out.push_back(static_cast<uint8_t>(bits & 0xff));
    out.push_back(static_cast<uint8_t>((bits >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((bits >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((bits >> 24) & 0xff));
}

}  // namespace

Mesh parse_binary_stl(const uint8_t* data, size_t size) {
    if (data == nullptr) {
        throw StlError("null buffer");
    }
    if (size < kHeaderBytes + kCountBytes) {
        throw StlError("truncated header: need at least 84 bytes");
    }

    const uint32_t count = read_le_u32(data + kHeaderBytes);
    const size_t need =
        kHeaderBytes + kCountBytes + static_cast<size_t>(count) * kTriangleRecordBytes;
    if (size < need) {
        throw StlError("truncated triangle records: declared count exceeds buffer");
    }

    Mesh mesh;
    mesh.vertices.reserve(static_cast<size_t>(count) * 3);
    mesh.triangles.reserve(count);

    const uint8_t* cursor = data + kHeaderBytes + kCountBytes;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* rec = cursor + static_cast<size_t>(i) * kTriangleRecordBytes;
        // Skip the 12-byte face normal; we recompute normals ourselves.
        const uint8_t* vp = rec + 12;
        Triangle tri;
        for (int c = 0; c < 3; ++c) {
            const uint8_t* base = vp + c * 12;
            Vec3 v(read_le_float(base), read_le_float(base + 4), read_le_float(base + 8));
            tri.v[c] = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(v);
        }
        mesh.triangles.push_back(tri);
    }
    return mesh;
}

Mesh load_binary_stl(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw StlError("cannot open file: " + path);
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return parse_binary_stl(buf.data(), buf.size());
}

std::vector<uint8_t> write_binary_stl(const Mesh& mesh) {
    std::vector<uint8_t> out;
    out.resize(kHeaderBytes, 0);
    const uint32_t count = static_cast<uint32_t>(mesh.triangle_count());
    out.push_back(static_cast<uint8_t>(count & 0xff));
    out.push_back(static_cast<uint8_t>((count >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((count >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((count >> 24) & 0xff));

    for (size_t i = 0; i < mesh.triangle_count(); ++i) {
        auto corners = mesh.corners(i);
        const Vec3 n = normalize(cross(corners[1] - corners[0], corners[2] - corners[0]));
        write_le_float(out, n.x);
        write_le_float(out, n.y);
        write_le_float(out, n.z);
        for (const Vec3& v : corners) {
            write_le_float(out, v.x);
            write_le_float(out, v.y);
            write_le_float(out, v.z);
        }
        out.push_back(0);
        out.push_back(0);
    }
    return out;
}

}  // namespace meshslice
