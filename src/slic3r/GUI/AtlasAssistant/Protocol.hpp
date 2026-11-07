// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>

namespace Slic3r::GUI::AtlasAssistant {
constexpr std::uint32_t version = 1;
constexpr std::uint32_t max_messages = 6;
constexpr std::uint32_t max_message = 4096;
constexpr std::uint32_t max_reply = 4096;

enum class Role : std::uint32_t { system, user, assistant };
enum class Result : std::uint32_t { pending, ok, model, context, prompt, generation };

struct Message {
    Role role;
    std::uint32_t bytes;
    char text[max_message + 1];
};
struct ChatExchange {
    std::uint32_t protocol_version, count;
    Message messages[max_messages];
    Result result;
    std::uint32_t reply_bytes;
    char reply[max_reply];
};
}
