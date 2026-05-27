#include "mesh/obj_loader.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace meshslice {

namespace {

// Parse one face token "a", "a/b", "a//c", or "a/b/c" into a 1-based vertex index.
long parse_face_index(const std::string& token) {
    const size_t slash = token.find('/');
    const std::string idx = (slash == std::string::npos) ? token : token.substr(0, slash);
    if (idx.empty()) {
        throw ObjError("empty face vertex index");
    }
    return std::strtol(idx.c_str(), nullptr, 10);
}

uint32_t resolve_index(long raw, size_t vertex_count) {
    long resolved;
    if (raw > 0) {
        resolved = raw - 1;  // OBJ indices are 1-based.
    } else if (raw < 0) {
        resolved = static_cast<long>(vertex_count) + raw;  // negative = relative.
    } else {
        throw ObjError("face index 0 is invalid");
    }
    if (resolved < 0 || resolved >= static_cast<long>(vertex_count)) {
        throw ObjError("face index out of range");
    }
    return static_cast<uint32_t>(resolved);
}

}  // namespace

Mesh parse_obj(const std::string& text) {
    Mesh mesh;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "v") {
            float x = 0, y = 0, z = 0;
            if (!(ls >> x >> y >> z)) {
                throw ObjError("malformed vertex line");
            }
            mesh.vertices.push_back(Vec3(x, y, z));
        } else if (tag == "f") {
            std::vector<uint32_t> idx;
            std::string tok;
            while (ls >> tok) {
                idx.push_back(resolve_index(parse_face_index(tok), mesh.vertices.size()));
            }
            if (idx.size() < 3) {
                throw ObjError("face with fewer than 3 vertices");
            }
            // Fan triangulation for polygons.
            for (size_t i = 1; i + 1 < idx.size(); ++i) {
                Triangle tri;
                tri.v[0] = idx[0];
                tri.v[1] = idx[i];
                tri.v[2] = idx[i + 1];
                mesh.triangles.push_back(tri);
            }
        }
        // All other directives (vn, vt, g, o, s, mtllib, usemtl) are ignored.
    }
    return mesh;
}

Mesh load_obj(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        throw ObjError("cannot open file: " + path);
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parse_obj(ss.str());
}

}  // namespace meshslice
