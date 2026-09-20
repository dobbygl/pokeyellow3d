cmake_minimum_required(VERSION 3.16)
project(pokeyellow C CXX)

include(CheckIPOSupported)
include(FetchContent)
option(POKEYELLOW_3D "Build the Pallet Town and Route 1 3D presentation prototype" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()
# Default to a small-binary profile: MinSizeRel + section-based dead
# stripping + post-link symbol stripping. Flip GBRECOMP_SHRINK off for
# debugging; bump GBRECOMP_GENERATED_OPT_LEVEL to 3 for max perf.
option(GBRECOMP_SHRINK "Aggressive size shrink (dead-strip + symbol strip)" ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
    if(GBRECOMP_SHRINK)
        add_compile_options(-ffunction-sections -fdata-sections)
        add_link_options(-Wl,--gc-sections)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_link_options(-s)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# gb-recompiled runtime
# When consumed by an umbrella compilation (e.g. GB-Recomp/pgbcomp), the
# parent is expected to provide `gbrt`. We detect that and skip the fetch.
# ---------------------------------------------------------------------------
if(NOT TARGET gbrt)
    set(GBRT_URL "https://github.com/dobbygl/gb-recompiled.git" CACHE STRING "Runtime repository URL")
    set(GBRT_REF "presentation-api-v1" CACHE STRING "Runtime git ref with presentation API v1")
    FetchContent_Declare(gb_recompiled
        GIT_REPOSITORY ${GBRT_URL}
        GIT_TAG ${GBRT_REF}
        GIT_SHALLOW FALSE
    )
    FetchContent_GetProperties(gb_recompiled)
    if(NOT gb_recompiled_POPULATED)
        FetchContent_Populate(gb_recompiled)
    endif()
    add_subdirectory(${gb_recompiled_SOURCE_DIR}/runtime ${CMAKE_BINARY_DIR}/_gbrt_build)
endif()

# ---------------------------------------------------------------------------
# Cart sources
# ---------------------------------------------------------------------------
set(POKEYELLOW_SOURCES
    pokeyellow_main.c
    pokeyellow.c
    pokeyellow_rom.c
    pokeyellow_dispatch_chunk_0.c
    pokeyellow_dispatch_chunk_1.c
    pokeyellow_dispatch_chunk_2.c
    pokeyellow_funcs_0.c
    pokeyellow_funcs_1.c
    pokeyellow_funcs_2.c
    pokeyellow_funcs_3.c
    pokeyellow_funcs_4.c
    pokeyellow_funcs_5.c
    pokeyellow_funcs_6.c
    pokeyellow_funcs_7.c
    pokeyellow_funcs_8.c
    pokeyellow_funcs_9.c
    pokeyellow_funcs_10.c
    pokeyellow_funcs_11.c
    pokeyellow_funcs_12.c
    pokeyellow_funcs_13.c
    pokeyellow_funcs_14.c
    pokeyellow_funcs_15.c
    pokeyellow_funcs_16.c
    pokeyellow_funcs_17.c
    pokeyellow_funcs_18.c
    pokeyellow_funcs_19.c
    pokeyellow_funcs_20.c
    pokeyellow_funcs_21.c
    pokeyellow_funcs_22.c
    pokeyellow_funcs_23.c
    pokeyellow_funcs_24.c
    pokeyellow_funcs_25.c
    pokeyellow_funcs_26.c
    pokeyellow_funcs_27.c
    pokeyellow_funcs_28.c
    pokeyellow_funcs_29.c
    pokeyellow_funcs_30.c
    pokeyellow_funcs_31.c
    pokeyellow_funcs_32.c
    pokeyellow_funcs_33.c
)

set(GBRECOMP_GENERATED_OPT_LEVEL "1" CACHE STRING "Optimization level for generated ROM source files")
set_property(CACHE GBRECOMP_GENERATED_OPT_LEVEL PROPERTY STRINGS 0 1 2 3)
set_source_files_properties(${POKEYELLOW_SOURCES} PROPERTIES COMPILE_OPTIONS "-O${GBRECOMP_GENERATED_OPT_LEVEL}")

# ---------------------------------------------------------------------------
# pokeyellow_cart: static library so umbrella compilations can link us in.
# Exposes `pokeyellow_main(argc, argv)` for the launcher's g_games[].
# ---------------------------------------------------------------------------
add_library(pokeyellow_cart STATIC ${POKEYELLOW_SOURCES})
target_include_directories(pokeyellow_cart PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pokeyellow_cart PUBLIC gbrt)

# ---------------------------------------------------------------------------
# Standalone launcher executable. Only built when this is the top-level
# CMake project.
# ---------------------------------------------------------------------------
if(CMAKE_PROJECT_NAME STREQUAL "pokeyellow")
    if(POKEYELLOW_3D)
        include(cmake/Pallet3D.cmake)
        enable_testing()
        add_executable(firstperson_test tests/firstperson_test.cpp)
        target_include_directories(firstperson_test PRIVATE src)
        add_test(NAME firstperson COMMAND firstperson_test)
        add_executable(pallet_state_test tests/pallet_state_test.cpp)
        target_include_directories(pallet_state_test PRIVATE src "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(kanto_geometry_audit tests/kanto_geometry_audit.cpp)
        target_include_directories(kanto_geometry_audit PRIVATE src)
        add_executable(interior_test tests/interior_test.cpp)
        add_executable(interior_audit tests/interior_audit.cpp)
        target_include_directories(interior_audit PRIVATE src)
        target_include_directories(interior_test PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(battle_state_test tests/battle_state_test.cpp)
        target_include_directories(battle_state_test PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(fade_state_test tests/fade_state_test.cpp)
        target_include_directories(fade_state_test PRIVATE src "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(battle_transition_test tests/battle_transition_test.cpp)
        target_include_directories(battle_transition_test PRIVATE src "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(menu_layout_test tests/menu_layout_test.cpp)
        add_executable(mon_pic_test tests/mon_pic_test.cpp)
        target_include_directories(mon_pic_test PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(dex_state_test tests/dex_state_test.cpp)
        target_include_directories(dex_state_test PRIVATE src "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(pc_state_test tests/pc_state_test.cpp)
        target_include_directories(pc_state_test PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(dex_nests_test tests/dex_nests_test.cpp)
        target_include_directories(dex_nests_test PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        add_executable(pc_storage_test tests/pc_storage_test.cpp)
        target_include_directories(pc_storage_test PRIVATE src "${gb_recompiled_SOURCE_DIR}/runtime/include")
        target_include_directories(menu_layout_test PRIVATE src)
        add_test(NAME menu_layout COMMAND menu_layout_test)
        add_executable(presentation_blend_test tests/presentation_blend_test.cpp)
        target_include_directories(presentation_blend_test PRIVATE src)
        add_test(NAME presentation_blend COMMAND presentation_blend_test)
        add_executable(kanto_rom_audit tests/kanto_rom_audit.cpp)
        target_include_directories(kanto_rom_audit PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME mon_pic COMMAND mon_pic_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME mon_pic_vram COMMAND mon_pic_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc" "${CMAKE_CURRENT_BINARY_DIR}/qa/dex/front-vram.bin")
            set_tests_properties(mon_pic mon_pic_vram PROPERTIES LABELS rom)
            set_tests_properties(mon_pic_vram PROPERTIES SKIP_RETURN_CODE 77)
            add_test(NAME dex_state COMMAND dex_state_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            set_tests_properties(dex_state PROPERTIES LABELS rom)
            add_test(NAME dex_nests COMMAND dex_nests_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME dex_nests_original COMMAND dex_nests_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc" "${CMAKE_CURRENT_BINARY_DIR}/qa/dex/areas.bin")
            set_tests_properties(dex_nests dex_nests_original PROPERTIES LABELS rom)
            set_tests_properties(dex_nests_original PROPERTIES SKIP_RETURN_CODE 77)
            add_test(NAME pc_state COMMAND pc_state_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            set_tests_properties(pc_state PROPERTIES LABELS rom)
            add_test(NAME pc_storage COMMAND pc_storage_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            set_tests_properties(pc_storage PROPERTIES LABELS rom)
            add_test(NAME pc_storage_original COMMAND pc_storage_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc" "${CMAKE_CURRENT_BINARY_DIR}/qa/pc/boxes.wram" "${CMAKE_CURRENT_BINARY_DIR}/qa/pc/boxes.sram")
            set_tests_properties(pc_storage_original PROPERTIES LABELS rom SKIP_RETURN_CODE 77)
            add_test(NAME interior COMMAND interior_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME interior_audit COMMAND interior_audit "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME battle_state COMMAND battle_state_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME fade_state COMMAND fade_state_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME battle_transition COMMAND battle_transition_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME pallet_state COMMAND pallet_state_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME kanto_geometry COMMAND kanto_geometry_audit "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME kanto_rom COMMAND kanto_rom_audit "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
        endif()
        add_executable(pallet_render_smoke EXCLUDE_FROM_ALL tests/pallet_render_smoke.cpp)
        target_include_directories(pallet_render_smoke PRIVATE src)
        target_link_libraries(pallet_render_smoke PRIVATE pokeyellow_cart gbrt)
        include(cmake/SyntheticTests.cmake)
    endif()
    add_executable(pokeyellow_launcher launcher_main.cpp)
    target_link_libraries(pokeyellow_launcher PRIVATE pokeyellow_cart gbrt)
    if(POKEYELLOW_3D)
        target_compile_definitions(pokeyellow_launcher PRIVATE POKEYELLOW_3D=1)
        set_target_properties(pokeyellow_launcher PROPERTIES OUTPUT_NAME pokeyellow3d)
    else()
        set_target_properties(pokeyellow_launcher PROPERTIES OUTPUT_NAME pokeyellow)
    endif()
endif()
