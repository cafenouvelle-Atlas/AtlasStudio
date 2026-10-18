// Atlas Studio official-source Centauri Carbon offline profile smoke test. AGPL-3.0.
#include <catch_main.hpp>
#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/utils.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#include <cstdio>
// Diagnostic only: preserve failure handling while showing the native fault stack.
static LONG CALLBACK atlas_crash_trace(EXCEPTION_POINTERS* exception)
{
    if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);
    void* frames[64];
    const auto count = CaptureStackBackTrace(0, 64, frames, nullptr);
    std::fprintf(stderr, "Atlas native access violation at %p\n", exception->ExceptionRecord->ExceptionAddress);
    for (USHORT i = 0; i < count; ++i) {
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, reinterpret_cast<DWORD64>(frames[i]), &displacement, symbol))
            std::fprintf(stderr, "  %s + 0x%llx\n", symbol->Name, static_cast<unsigned long long>(displacement));
        else
            std::fprintf(stderr, "  %p\n", frames[i]);
    }
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

TEST_CASE("Centauri Carbon presets load, slice and export a 20mm cube", "[atlas][centauri]")
{
    using namespace Slic3r;
#ifdef _WIN32
    AddVectoredExceptionHandler(1, atlas_crash_trace);
#endif
    set_logging_level(5);
    namespace fs = std::filesystem;
    const fs::path root(ATLAS_SOURCE_ROOT);
    const fs::path profiles = root / "resources/profiles/Elegoo";
    const fs::path logs = root / "atlas-logs/centauri-smoke";
    fs::create_directories(logs);
    set_resources_dir((root / "resources").generic_string());
    set_data_dir(logs.generic_string());
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    for (const char* relative : {
        "machine/ATLAS - ELEGOO Centauri Carbon 0.4 nozzle.json",
        "process/ATLAS - 0.20mm Standard @Centauri Carbon.json",
        "filament/ATLAS - Elegoo PLA @Centauri Carbon.json"}) {
        DynamicPrintConfig loaded;
        std::map<std::string, std::string> metadata;
        std::string reason;
        const std::string path = (profiles / relative).generic_string();
        INFO(path);
        REQUIRE(fs::exists(path));
        auto substitutions = loaded.load_from_json(path, ForwardCompatibilitySubstitutionRule::Disable, metadata, reason);
        INFO(reason);
        REQUIRE(reason.empty());
        REQUIRE(substitutions.empty());
        config.apply(loaded);
    }
    REQUIRE(config.opt_string("printer_model") == "Elegoo Centauri Carbon");
    REQUIRE(config.option<ConfigOptionFloats>("nozzle_diameter")->values == std::vector<double>{0.4});
    Model model;
    auto* object = model.add_object();
    object->name = "Atlas 20mm cube";
    object->add_volume(make_cube(20., 20., 20.));
    object->add_instance();
    object->ensure_on_bed();
    Print print;
    print.auto_assign_extruders(object);
    print.apply(model, config);
    auto validation = print.validate();
    INFO(validation.string);
    REQUIRE(validation.string.empty());
    print.set_status_silent();
    REQUIRE_NOTHROW(print.process());
    const fs::path output = logs / "centauri-20mm-cube.gcode";
    GCodeProcessorResult result;
    REQUIRE_NOTHROW(print.export_gcode(output.generic_string(), &result, nullptr));
    REQUIRE(fs::file_size(output) > 1000);
    std::ifstream input(output);
    const std::string gcode((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    REQUIRE(gcode.find("M729") != std::string::npos);
    REQUIRE(gcode.find("M104 S0") != std::string::npos);
}
