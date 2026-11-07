// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "slic3r/GUI/AtlasAssistant/VoiceProtocol.hpp"

namespace Slic3r::AtlasVoice {
int transcribe(Exchange& data, const char* model_path);
}
