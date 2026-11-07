// SPDX-License-Identifier: AGPL-3.0-only
#include "Transcribe.hpp"
#include <fstream>
#include <iostream>
#include <string>

using namespace Slic3r::AtlasVoice;

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    Exchange exchange {};
    if (transcribe(exchange, "") == 0) return 1;
    std::ifstream pcm(argv[2], std::ios::binary);
    pcm.read(reinterpret_cast<char*>(exchange.pcm), sizeof(exchange.pcm));
    if (!pcm.eof() || pcm.bad() || pcm.gcount() % sizeof(exchange.pcm[0])) return 2;
    exchange.protocol_version = version;
    exchange.samples = static_cast<std::uint32_t>(pcm.gcount() / sizeof(exchange.pcm[0]));
    if (transcribe(exchange, argv[1]) != 0 || !valid_text(exchange.text_bytes)) return 1;
    const std::string text(exchange.text, exchange.text_bytes);
    std::cout << text << '\n';
    return text.find("country") == std::string::npos;
}
