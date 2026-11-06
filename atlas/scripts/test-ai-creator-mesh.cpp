// SPDX-License-Identifier: AGPL-3.0-only
#include <catch_main.hpp>
#include "slic3r/GUI/AtlasAICreator/PrimitiveGenerator.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Format/STL.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/utils.hpp"
#include <filesystem>
#include <boost/filesystem.hpp>
#include <map>

TEST_CASE("Creator primitives are bounded, closed solids with correct dimensions", "[atlas][creator]")
{
    using namespace Slic3r;
    using namespace Slic3r::AtlasAICreator;
    for (auto shape : {Primitive::Box, Primitive::Cylinder, Primitive::Sphere}) {
        auto mesh = generate({shape, 20, 30, 40});
        REQUIRE_FALSE(mesh.empty());
        REQUIRE(mesh.its.indices.size() <= 5000);
        const auto size = mesh.bounding_box().size();
        REQUIRE(size.x() == Approx(20).margin(0.01));
        REQUIRE(size.y() == Approx(shape == Primitive::Box ? 30 : 20).margin(0.01));
        REQUIRE(size.z() == Approx(shape == Primitive::Sphere ? 20 : 40).margin(0.01));
        const double volume = shape == Primitive::Box ? 24000.0 :
            shape == Primitive::Cylinder ? PI * 100.0 * 40.0 : 4.0 / 3.0 * PI * 1000.0;
        REQUIRE(mesh.volume() == Approx(volume).epsilon(0.02));
        // Every undirected edge must belong to exactly two oppositely wound faces.
        std::map<std::pair<int, int>, std::pair<int, int>> edges;
        for (const auto& face : mesh.its.indices) {
            for (int i = 0; i < 3; ++i) {
                const int a = face[i], b = face[(i + 1) % 3];
                auto& edge = edges[{std::min(a, b), std::max(a, b)}];
                ++edge.first;
                edge.second += a < b ? 1 : -1;
            }
        }
        for (const auto& entry : edges) {
            REQUIRE(entry.second.first == 2);
            REQUIRE(entry.second.second == 0);
        }
        struct TemporarySTL {
            boost::filesystem::path path = boost::filesystem::temp_directory_path() /
                boost::filesystem::unique_path("atlas-creator-%%%%-%%%%.stl");
            ~TemporarySTL() { boost::system::error_code ec; boost::filesystem::remove(path, ec); }
        } file;
        REQUIRE(store_stl(file.path.string().c_str(), &mesh, true));
        REQUIRE(boost::filesystem::file_size(file.path) > 84);
        Model reloaded;
        REQUIRE(load_stl(file.path.string().c_str(), &reloaded));
        REQUIRE(reloaded.objects.size() == 1);
        // load_stl creates geometry without placed instances; compare the imported solid.
        REQUIRE(reloaded.objects.front()->raw_mesh().volume() == Approx(mesh.volume()).epsilon(0.001));
    }
}

TEST_CASE("Creator STL round trips slice and export with the Centauri profile", "[atlas][creator]")
{
    using namespace Slic3r;
    using namespace Slic3r::AtlasAICreator;
    namespace fs = std::filesystem;
    const fs::path root(ATLAS_SOURCE_ROOT);
    const auto output = root / "atlas-logs/creator-smoke";
    fs::create_directories(output);
    set_resources_dir((root / "resources").generic_string());
    set_data_dir(output.generic_string());
    DynamicPrintConfig config;
    config.apply(DynamicPrintConfig::full_print_config());
    for (const char* relative : {
        "machine/ATLAS - ELEGOO Centauri Carbon 0.4 nozzle.json",
        "process/ATLAS - 0.20mm Standard @Centauri Carbon.json",
        "filament/ATLAS - Elegoo PLA @Centauri Carbon.json"}) {
        DynamicPrintConfig loaded;
        std::map<std::string, std::string> metadata;
        std::string reason;
        const auto path = root / "resources/profiles/Elegoo" / relative;
        INFO(path.string());
        REQUIRE(fs::exists(path));
        const auto substitutions = loaded.load_from_json(path.generic_string(),
            ForwardCompatibilitySubstitutionRule::Disable, metadata, reason);
        INFO(reason);
        REQUIRE(reason.empty());
        REQUIRE(substitutions.empty());
        config.apply(loaded);
    }
    for (auto shape : {Primitive::Box, Primitive::Cylinder, Primitive::Sphere}) {
        const auto stem = std::to_string(static_cast<int>(shape));
        auto mesh = generate({shape, 20, 20, 20});
        const auto stl = output / (stem + ".stl");
        REQUIRE(store_stl(stl.string().c_str(), &mesh, true));
        Model model;
        REQUIRE(load_stl(stl.string().c_str(), &model));
        REQUIRE(model.objects.size() == 1);
        auto* object = model.objects.front();
        if (object->instances.empty()) object->add_instance();
        object->ensure_on_bed();
        Print print;
        print.auto_assign_extruders(object);
        print.apply(model, config);
        const auto validation = print.validate();
        INFO(validation.string);
        REQUIRE(validation.string.empty());
        print.set_status_silent();
        REQUIRE_NOTHROW(print.process());
        GCodeProcessorResult result;
        const auto gcode = output / (stem + ".gcode");
        REQUIRE_NOTHROW(print.export_gcode(gcode.generic_string(), &result, nullptr));
        REQUIRE(fs::file_size(gcode) > 1000);
    }
}
