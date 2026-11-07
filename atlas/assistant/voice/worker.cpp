// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include "Transcribe.hpp"

using namespace Slic3r::AtlasVoice;

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3)
        return 2;
    HANDLE mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, argv[1]);
    if (!mapping)
        return 2;
    auto* data = static_cast<Exchange*>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(Exchange)));
    CloseHandle(mapping);
    if (!data)
        return 2;
    int result = 4;
    try {
        const auto path = std::filesystem::path(argv[2]).u8string();
        result = transcribe(*data, reinterpret_cast<const char*>(path.c_str()));
    }
    catch (...) { /* The parent reports worker failures without exposing local paths. */ }
    UnmapViewOfFile(data);
    return result;
}
