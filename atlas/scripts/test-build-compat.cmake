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
            fila_manager_tests filament_group_tests
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
    # The libnest2d callback now receives both item and placed item group.
    # Generate a corrected test copy; keep the checked-in fixture untouched.
    set(original "${PROJECT_SOURCE_DIR}/tests/libnest2d/libnest2d_tests_main.cpp")
    file(READ "${original}" test_source)
    set(old_signature "pconfig.object_function = [](const Item &item) -> double")
    string(FIND "${test_source}" "${old_signature}" signature_position)
    if(signature_position EQUAL -1)
        message(FATAL_ERROR "Upstream libnest2d test changed: review Atlas compatibility patch")
    endif()
    string(REPLACE "${old_signature}"
        "pconfig.object_function = [](const Item &item, const auto &) -> double"
        test_source "${test_source}")
    set(generated "${CMAKE_BINARY_DIR}/atlas-generated/libnest2d_tests_main.cpp")
    file(WRITE "${generated}" "${test_source}")
    get_target_property(test_sources libnest2d_tests SOURCES)
    list(REMOVE_ITEM test_sources "libnest2d_tests_main.cpp")
    set_property(TARGET libnest2d_tests PROPERTY SOURCES "${test_sources};${generated}")
    target_include_directories(libnest2d_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/libnest2d")
    # Raster dimension types moved from RasterBase into the sla namespace.
    # Repair copies of the legacy SLA fixtures, without editing upstream files.
    get_target_property(sla_sources sla_print_tests SOURCES)
    set(generated_sla_sources "")
    foreach(sla_source IN LISTS sla_sources)
        set(original_sla "${PROJECT_SOURCE_DIR}/tests/sla_print/${sla_source}")
        file(READ "${original_sla}" sla_contents)
        string(REPLACE "RasterBase::PixelDim" "PixelDim" sla_contents "${sla_contents}")
        string(REPLACE "RasterBase::Resolution" "Resolution" sla_contents "${sla_contents}")
        set(generated_sla "${CMAKE_BINARY_DIR}/atlas-generated/sla_print/${sla_source}")
        file(WRITE "${generated_sla}" "${sla_contents}")
        list(APPEND generated_sla_sources "${generated_sla}")
    endforeach()
    set_property(TARGET sla_print_tests PROPERTY SOURCES "${generated_sla_sources}")
    target_include_directories(sla_print_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/sla_print")
endfunction()
cmake_language(DEFER CALL atlas_fix_test_build)
