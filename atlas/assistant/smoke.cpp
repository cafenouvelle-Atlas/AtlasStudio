// SPDX-License-Identifier: AGPL-3.0-only
#include "Generate.hpp"
#include <llama.h>
#include <cstring>
#include <iostream>
#include <string>

using namespace Slic3r::GUI::AtlasAssistant;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    ChatExchange exchange {};
    if (generate("", exchange) != Result::prompt) return 1;
    exchange.protocol_version = version;
    exchange.count = 1;
    auto& message = exchange.messages[0];
    message.role = Role::user;
    const char* prompt = "What is 2 + 2? Reply with just the number.";
    message.bytes = static_cast<std::uint32_t>(std::strlen(prompt));
    std::memcpy(message.text, prompt, message.bytes);
    llama_backend_init();
    const auto result = generate(argv[1], exchange);
    llama_backend_free();
    if (result != Result::ok || exchange.reply_bytes == 0 || exchange.reply_bytes > max_reply) {
        std::cerr << "Local generation failed: " << static_cast<unsigned>(result) << '\n';
        return 1;
    }
    const std::string reply(exchange.reply, exchange.reply_bytes);
    std::cout << reply << '\n';
    return reply.find('4') == std::string::npos;
}
