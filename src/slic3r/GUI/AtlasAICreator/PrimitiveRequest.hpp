// Atlas AI Creator, 2026-11-06. SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cmath>
#include <stdexcept>

namespace Slic3r::AtlasAICreator {

enum class Primitive { Box, Cylinder, Sphere };

// UI-independent request, in millimetres. X is diameter for round primitives.
struct PrimitiveRequest {
    Primitive shape = Primitive::Box;
    double x = 20.0;
    double y = 20.0;
    double z = 20.0;
};

inline void validate(const PrimitiveRequest& request)
{
    auto dimension = [](double value) {
        if (!std::isfinite(value) || value < 0.1 || value > 1000.0)
            throw std::invalid_argument("Dimensions must be between 0.1 and 1000 mm.");
    };
    dimension(request.x);
    switch (request.shape) {
    case Primitive::Box: dimension(request.y); dimension(request.z); break;
    case Primitive::Cylinder: dimension(request.z); break;
    case Primitive::Sphere: break;
    default: throw std::invalid_argument("Unknown primitive type.");
    }
}

} // namespace Slic3r::AtlasAICreator
