// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "slic3r/GUI/AtlasAssistant/Protocol.hpp"
#include <string>

namespace Slic3r::GUI::AtlasAssistant {
Result generate(const std::string& model_path, ChatExchange& chat);
}
