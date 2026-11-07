// Atlas AI Creator, 2026-11-06. SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PrimitiveRequest.hpp"
#include "libslic3r/TriangleMesh.hpp"

namespace Slic3r::AtlasAICreator {
TriangleMesh generate(const PrimitiveRequest& request);
}
