// Atlas AI Creator, 2026-11-06. SPDX-License-Identifier: AGPL-3.0-only
#include "PrimitiveGenerator.hpp"

namespace Slic3r::AtlasAICreator {
TriangleMesh generate(const PrimitiveRequest& request)
{
    validate(request);
    // Fixed tessellation bounds CPU/RAM use; no AI runtime or network required.
    constexpr double angle = 2.0 * PI / 64.0;
    switch (request.shape) {
    case Primitive::Box: return make_cube(request.x, request.y, request.z);
    case Primitive::Cylinder: return make_cylinder(request.x / 2.0, request.z, angle);
    case Primitive::Sphere: return make_sphere(request.x / 2.0, angle);
    default: throw std::invalid_argument("Unknown primitive type.");
    }
}
} // namespace Slic3r::AtlasAICreator
