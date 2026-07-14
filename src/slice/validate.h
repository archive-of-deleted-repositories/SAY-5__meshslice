#ifndef MESHSLICE_VALIDATE_H
#define MESHSLICE_VALIDATE_H

#include <cstddef>
#include <string>

#include "mesh/mesh.h"

namespace meshslice {

// Structural health report for a triangle mesh. Edges are identified by welded
// vertex positions, so raw STL soup (duplicated vertices) validates the same
// as an indexed mesh.
struct ValidationReport {
    size_t non_manifold_edges = 0;    // edges shared by three or more triangles.
    size_t boundary_edges = 0;        // edges owned by exactly one triangle.
    size_t boundary_loops = 0;        // connected loops of boundary edges (holes).
    size_t degenerate_triangles = 0;  // zero-area or repeated-vertex triangles.
    // Triangles whose winding disagrees with the consistent outward
    // orientation of their surface component (what fix_normals would flip).
    size_t inverted_normals = 0;

    bool watertight() const {
        return boundary_edges == 0 && non_manifold_edges == 0;
    }
    bool clean() const {
        return watertight() && degenerate_triangles == 0 && inverted_normals == 0;
    }
};

ValidationReport validate_mesh(const Mesh& mesh);

// Reorient triangles so every surface component is consistently wound with
// outward-facing normals (positive enclosed volume). Returns the number of
// triangles that were flipped.
size_t fix_normals(Mesh& mesh);

// Human-readable multi-line rendering used by the validate CLI subcommand.
std::string format_report(const ValidationReport& r);

}  // namespace meshslice

#endif  // MESHSLICE_VALIDATE_H
