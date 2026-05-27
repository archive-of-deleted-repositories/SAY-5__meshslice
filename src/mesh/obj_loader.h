#ifndef MESHSLICE_OBJ_LOADER_H
#define MESHSLICE_OBJ_LOADER_H

#include <stdexcept>
#include <string>

#include "mesh/mesh.h"

namespace meshslice {

class ObjError : public std::runtime_error {
   public:
    explicit ObjError(const std::string& msg) : std::runtime_error(msg) {
    }
};

// Parse a restricted OBJ subset: `v x y z` vertices and `f a b c ...` faces.
// Faces with more than 3 vertices are triangulated as a fan. Vertex references
// may be negative (relative) per the OBJ spec. Texture/normal indices in the
// `a/b/c` form are tolerated and ignored. Comments (`#`) and other directives
// are skipped.
Mesh parse_obj(const std::string& text);

Mesh load_obj(const std::string& path);

}  // namespace meshslice

#endif  // MESHSLICE_OBJ_LOADER_H
