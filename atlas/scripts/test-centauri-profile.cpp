// Atlas Studio official-source Centauri Carbon offline profile smoke test. AGPL-3.0.
#include <catch_main.hpp>
#include "libslic3r/Print.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/utils.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

TEST_CASE("Centauri Carbon presets load, slice and export a 20mm cube", "[atlas][centauri]")
{
    using namespace Slic3r;
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
    REQUIRE_NOTHROW(print.export_gcode(output.generic_string(), nullptr, nullptr));
    REQUIRE(fs::file_size(output) > 1000);
    std::ifstream input(output);
    const std::string gcode((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    REQUIRE(gcode.find("M729") != std::string::npos);
    REQUIRE(gcode.find("M104 S0") != std::string::npos);
}
