// SPDX-License-Identifier: AGPL-3.0-only
#include "Generate.hpp"
#include <llama.h>
#include <windows.h>
#include <string>

using namespace Slic3r::GUI::AtlasAssistant;

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 1;
    HANDLE mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, argv[1]);
    if (!mapping) return 1;
    auto* exchange = static_cast<ChatExchange*>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(ChatExchange)));
    if (!exchange) { CloseHandle(mapping); return 1; }
    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[2], -1, nullptr, 0, nullptr, nullptr);
    if (bytes > 1) {
        try {
            std::string path(bytes, '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[2], -1, path.data(), bytes, nullptr, nullptr);
            llama_backend_init();
            exchange->result = generate(path, *exchange);
            llama_backend_free();
        } catch (...) { exchange->result = Result::generation; }
    }
    UnmapViewOfFile(exchange);
    CloseHandle(mapping);
    return bytes > 1 ? 0 : 1;
}
