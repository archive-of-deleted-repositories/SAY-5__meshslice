#include "slice/validate.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace meshslice {

namespace {

// Weld vertices by exact position so STL soup and indexed meshes produce the
// same connectivity. Returns one welded id per original vertex.
std::vector<uint32_t> weld(const Mesh& mesh) {
    std::map<std::tuple<float, float, float>, uint32_t> ids;
    std::vector<uint32_t> welded(mesh.vertex_count());
    for (size_t i = 0; i < mesh.vertex_count(); ++i) {
        const Vec3& p = mesh.vertices[i];
        const auto key = std::make_tuple(p.x, p.y, p.z);
        const auto it = ids.find(key);
        if (it == ids.end()) {
            const uint32_t id = static_cast<uint32_t>(ids.size());
            ids.emplace(key, id);
            welded[i] = id;
        } else {
            welded[i] = it->second;
        }
    }
    return welded;
}

uint64_t edge_key(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | b;
}

struct EdgeUse {
    uint32_t tri;
    bool forward;  // true when the triangle traverses the edge min -> max.
};

struct Connectivity {
    std::vector<uint32_t> welded_of_tri_corner;  // 3 per triangle.
    std::unordered_map<uint64_t, std::vector<EdgeUse>> edges;
    std::vector<bool> degenerate;
    size_t degenerate_count = 0;
};

Connectivity build_connectivity(const Mesh& mesh) {
    Connectivity conn;
    const std::vector<uint32_t> welded = weld(mesh);
    const size_t nt = mesh.triangle_count();
    conn.welded_of_tri_corner.resize(nt * 3);
    conn.degenerate.assign(nt, false);
    for (size_t t = 0; t < nt; ++t) {
        uint32_t w[3];
        for (int k = 0; k < 3; ++k) {
            w[k] = welded[mesh.triangles[t].v[k]];
            conn.welded_of_tri_corner[t * 3 + k] = w[k];
        }
        const auto c = mesh.corners(t);
        const bool repeated = w[0] == w[1] || w[1] == w[2] || w[2] == w[0];
        if (repeated || triangle_area(c[0], c[1], c[2]) <= 0.0f) {
            conn.degenerate[t] = true;
            ++conn.degenerate_count;
            continue;  // degenerate triangles carry no orientation information.
        }
        for (int k = 0; k < 3; ++k) {
            const uint32_t a = w[k];
            const uint32_t b = w[(k + 1) % 3];
            conn.edges[edge_key(a, b)].push_back(EdgeUse{static_cast<uint32_t>(t), a < b});
        }
    }
    return conn;
}

// Count connected loops among boundary edges by union-find over welded ids.
size_t count_boundary_loops(const Connectivity& conn) {
    std::unordered_map<uint32_t, uint32_t> parent;
    std::function<uint32_t(uint32_t)> find = [&](uint32_t x) {
        auto it = parent.find(x);
        if (it == parent.end()) {
            parent[x] = x;
            return x;
        }
        while (it->second != x) {
            x = it->second;
            it = parent.find(x);
        }
        return x;
    };
    size_t boundary = 0;
    for (const auto& [key, uses] : conn.edges) {
        if (uses.size() != 1) {
            continue;
        }
        ++boundary;
        const uint32_t a = static_cast<uint32_t>(key >> 32);
        const uint32_t b = static_cast<uint32_t>(key & 0xffffffffull);
        const uint32_t ra = find(a);
        const uint32_t rb = find(b);
        if (ra != rb) {
            parent[ra] = rb;
        }
    }
    if (boundary == 0) {
        return 0;
    }
    // Loops = boundary vertices' components.
    std::vector<uint32_t> roots;
    for (const auto& [key, uses] : conn.edges) {
        if (uses.size() != 1) {
            continue;
        }
        roots.push_back(find(static_cast<uint32_t>(key >> 32)));
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
    return roots.size();
}

// Propagate a consistent orientation over each surface component. Fills
// `flip[t]` with whether triangle t disagrees with its component's seed, and
// returns the component id per triangle (SIZE_MAX for degenerate).
struct Orientation {
    std::vector<bool> flip;
    std::vector<size_t> component;
    size_t components = 0;
};

Orientation orient(const Mesh& mesh, const Connectivity& conn) {
    const size_t nt = mesh.triangle_count();
    Orientation o;
    o.flip.assign(nt, false);
    o.component.assign(nt, SIZE_MAX);

    // Adjacency: for every 2-manifold edge, the two incident triangles agree
    // when they traverse the shared edge in opposite directions.
    std::vector<std::vector<std::pair<uint32_t, bool>>> adj(nt);
    for (const auto& [key, uses] : conn.edges) {
        if (uses.size() != 2) {
            continue;  // boundary or non-manifold: no orientation constraint.
        }
        const bool agree = uses[0].forward != uses[1].forward;
        adj[uses[0].tri].push_back({uses[1].tri, agree});
        adj[uses[1].tri].push_back({uses[0].tri, agree});
    }

    for (size_t seed = 0; seed < nt; ++seed) {
        if (o.component[seed] != SIZE_MAX || conn.degenerate[seed]) {
            continue;
        }
        const size_t comp = o.components++;
        std::queue<uint32_t> q;
        o.component[seed] = comp;
        q.push(static_cast<uint32_t>(seed));
        while (!q.empty()) {
            const uint32_t t = q.front();
            q.pop();
            for (const auto& [n, agree] : adj[t]) {
                if (o.component[n] != SIZE_MAX) {
                    continue;
                }
                o.component[n] = comp;
                o.flip[n] = agree ? o.flip[t] : !o.flip[t];
                q.push(n);
            }
        }
    }
    return o;
}

// Signed volume contribution per component with the propagated orientation
// applied; negative volume means the component faces inward overall.
std::vector<double> component_volumes(const Mesh& mesh, const Orientation& o) {
    std::vector<double> vol(o.components, 0.0);
    for (size_t t = 0; t < mesh.triangle_count(); ++t) {
        if (o.component[t] == SIZE_MAX) {
            continue;
        }
        auto c = mesh.corners(t);
        if (o.flip[t]) {
            std::swap(c[1], c[2]);
        }
        const Vec3 x = cross(c[1], c[2]);
        vol[o.component[t]] += static_cast<double>(dot(c[0], x)) / 6.0;
    }
    return vol;
}

}  // namespace

ValidationReport validate_mesh(const Mesh& mesh) {
    ValidationReport r;
    const Connectivity conn = build_connectivity(mesh);
    r.degenerate_triangles = conn.degenerate_count;
    for (const auto& [key, uses] : conn.edges) {
        if (uses.size() == 1) {
            ++r.boundary_edges;
        } else if (uses.size() > 2) {
            ++r.non_manifold_edges;
        }
    }
    r.boundary_loops = count_boundary_loops(conn);

    const Orientation o = orient(mesh, conn);
    const std::vector<double> vol = component_volumes(mesh, o);
    for (size_t t = 0; t < mesh.triangle_count(); ++t) {
        if (o.component[t] == SIZE_MAX) {
            continue;
        }
        // A triangle is inverted when its final outward orientation requires a
        // flip: propagated flip, then a whole-component flip when the volume
        // came out negative.
        const bool comp_flip = vol[o.component[t]] < 0.0;
        if (o.flip[t] != comp_flip) {
            ++r.inverted_normals;
        }
    }
    return r;
}

size_t fix_normals(Mesh& mesh) {
    const Connectivity conn = build_connectivity(mesh);
    const Orientation o = orient(mesh, conn);
    const std::vector<double> vol = component_volumes(mesh, o);
    size_t flipped = 0;
    for (size_t t = 0; t < mesh.triangle_count(); ++t) {
        if (o.component[t] == SIZE_MAX) {
            continue;
        }
        const bool comp_flip = vol[o.component[t]] < 0.0;
        if (o.flip[t] != comp_flip) {
            std::swap(mesh.triangles[t].v[1], mesh.triangles[t].v[2]);
            ++flipped;
        }
    }
    return flipped;
}

std::string format_report(const ValidationReport& r) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "non_manifold_edges=%zu\n"
                  "boundary_edges=%zu\n"
                  "boundary_loops=%zu\n"
                  "degenerate_triangles=%zu\n"
                  "inverted_normals=%zu\n"
                  "watertight=%s\n"
                  "clean=%s\n",
                  r.non_manifold_edges, r.boundary_edges, r.boundary_loops, r.degenerate_triangles,
                  r.inverted_normals, r.watertight() ? "yes" : "no", r.clean() ? "yes" : "no");
    return buf;
}

}  // namespace meshslice
