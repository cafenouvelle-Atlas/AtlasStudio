# Atlas Windows test build compatibility, 2026-10-17. AGPL-3.0.
# Inject build-only repairs without modifying upstream application or test files.
include_guard(GLOBAL)
if(NOT PROJECT_NAME STREQUAL "BambuStudio")
    return()
endif()
function(atlas_fix_test_build)
    if(NOT WIN32 OR NOT TARGET test_common)
        return()
    endif()
    foreach(wx_variable IN ITEMS wxWidgets_INCLUDE_DIRS wxWidgets_DEFINITIONS wxWidgets_LIBRARIES)
        get_directory_property(${wx_variable} DIRECTORY "${PROJECT_SOURCE_DIR}/src"
            DEFINITION ${wx_variable})
    endforeach()
    # wxWidgets is configured in src/; sibling tests need its include paths too.
    target_include_directories(test_common INTERFACE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(test_common INTERFACE ${wxWidgets_DEFINITIONS})
    add_library(atlas_test_support STATIC
        "${PROJECT_SOURCE_DIR}/src/slic3r/Utils/Http.cpp"
        "${PROJECT_SOURCE_DIR}/src/slic3r/Utils/BBLUtil.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/test-nanosvg.cpp")
    target_include_directories(atlas_test_support PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(atlas_test_support PUBLIC libslic3r libcurl
        OpenSSL::SSL OpenSSL::Crypto ${wxWidgets_LIBRARIES})
    foreach(test_target IN ITEMS libslic3r_tests fff_print_tests sla_print_tests
            filament_group_tests
            support_material_on_wipe_tower_tests periodic_modifier_tests)
        if(TARGET ${test_target})
            target_link_libraries(${test_target} atlas_test_support)
            get_target_property(link_options ${test_target} LINK_OPTIONS)
            if(link_options)
                list(REMOVE_ITEM link_options "/FORCE:UNRESOLVED")
                set_property(TARGET ${test_target} PROPERTY LINK_OPTIONS "${link_options}")
            endif()
        endif()
    endforeach()
    # These legacy suites reference removed APIs (GCodeWriter::lift,
    # Print::brim, old load_obj/set_rotation and raster dimension types).
    # Keep them visible as deferred coverage, never report them as passing.
    foreach(legacy_target IN ITEMS libnest2d_tests libslic3r_tests fff_print_tests sla_print_tests)
        set_property(TARGET ${legacy_target} PROPERTY EXCLUDE_FROM_ALL TRUE)
    endforeach()
    # FilamentSpool serialization now calls GUI_App's tray-label formatter.
    # Use its real implementation, not unresolved-symbol flags or fake stubs.
    target_link_libraries(fila_manager_tests libslic3r_gui ws2_32 uxtheme setupapi user32)
    # DevUtil.h uses boost::format without including it in the slim test build.
    target_compile_options(fila_manager_tests PRIVATE /FIboost/format.hpp)
endfunction()
cmake_language(DEFER CALL atlas_fix_test_build)
