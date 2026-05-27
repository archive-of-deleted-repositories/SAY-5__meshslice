#include "mesh/generators.h"

#include <cmath>
#include <cstdint>

namespace meshslice {

namespace {

// Small deterministic PRNG (splitmix64) so generated meshes are reproducible
// across platforms without depending on <random> engine portability.
struct SplitMix {
    uint64_t state;
    explicit SplitMix(uint64_t seed) : state(seed) {
    }
    uint64_t next() {
        uint64_t z = (state += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    float unit() {
        return static_cast<float>(next() >> 40) / static_cast<float>(1u << 24);
    }
};

void add_quad(Mesh& m, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    const uint32_t base = static_cast<uint32_t>(m.vertices.size());
    m.vertices.push_back(a);
    m.vertices.push_back(b);
    m.vertices.push_back(c);
    m.vertices.push_back(d);
    m.triangles.push_back(Triangle{{base, base + 1, base + 2}});
    m.triangles.push_back(Triangle{{base, base + 2, base + 3}});
}

}  // namespace

Mesh make_cube(float h) {
    Mesh m;
    const Vec3 p[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                       {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
    add_quad(m, p[0], p[1], p[2], p[3]);  // -z
    add_quad(m, p[5], p[4], p[7], p[6]);  // +z
    add_quad(m, p[4], p[0], p[3], p[7]);  // -x
    add_quad(m, p[1], p[5], p[6], p[2]);  // +x
    add_quad(m, p[4], p[5], p[1], p[0]);  // -y
    add_quad(m, p[3], p[2], p[6], p[7]);  // +y
    return m;
}

Mesh make_sphere(uint32_t stacks, uint32_t slices, float radius) {
    Mesh m;
    if (stacks < 2) {
        stacks = 2;
    }
    if (slices < 3) {
        slices = 3;
    }
    const float pi = 3.14159265358979323846f;
    auto vert = [&](uint32_t st, uint32_t sl) -> Vec3 {
        const float phi = pi * static_cast<float>(st) / static_cast<float>(stacks);
        const float theta =
            2.0f * pi * static_cast<float>(sl % slices) / static_cast<float>(slices);
        return {radius * std::sin(phi) * std::cos(theta), radius * std::cos(phi),
                radius * std::sin(phi) * std::sin(theta)};
    };
    for (uint32_t st = 0; st < stacks; ++st) {
        for (uint32_t sl = 0; sl < slices; ++sl) {
            add_quad(m, vert(st, sl), vert(st + 1, sl), vert(st + 1, sl + 1), vert(st, sl + 1));
        }
    }
    return m;
}

Mesh make_blob(uint32_t subdivisions, uint64_t seed) {
    Mesh m = make_sphere(8 + subdivisions * 2, 12 + subdivisions * 2, 1.0f);
    SplitMix rng(seed);
    for (Vec3& v : m.vertices) {
        const float jitter = 0.85f + 0.30f * rng.unit();
        v = v * jitter;
    }
    return m;
}

Mesh make_grid_mesh(size_t target_triangles, uint64_t seed) {
    Mesh m;
    // Two triangles per grid quad; pick an n*n grid.
    size_t quads = target_triangles / 2;
    if (quads == 0) {
        quads = 1;
    }
    size_t n = static_cast<size_t>(std::sqrt(static_cast<double>(quads)));
    if (n == 0) {
        n = 1;
    }
    SplitMix rng(seed);
    const float step = 10.0f / static_cast<float>(n);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            const float x = static_cast<float>(i) * step;
            const float y = static_cast<float>(j) * step;
            const float z = 0.5f * rng.unit();  // gentle height field for AABB spread.
            add_quad(m, Vec3(x, y, z), Vec3(x + step, y, z), Vec3(x + step, y + step, z),
                     Vec3(x, y + step, z));
        }
    }
    return m;
}

}  // namespace meshslice
