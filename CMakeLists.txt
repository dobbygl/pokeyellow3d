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
    set(GBRT_REF "6581880fce60e6f139901a5942fc984e9c1db8ab" CACHE STRING "git ref of GB-Recomp/gb-recompiled to use")
    FetchContent_Declare(gb_recompiled
        GIT_REPOSITORY https://github.com/GB-Recomp/gb-recompiled.git
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
        add_executable(kanto_rom_audit tests/kanto_rom_audit.cpp)
        target_include_directories(kanto_rom_audit PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}" "${gb_recompiled_SOURCE_DIR}/runtime/include")
        if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME interior COMMAND interior_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME interior_audit COMMAND interior_audit "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
            add_test(NAME battle_state COMMAND battle_state_test "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
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
        set_target_properties(pokeyellow_launcher PROPERTIES OUTPUT_NAME pokeyellow3d)
    else()
        set_target_properties(pokeyellow_launcher PROPERTIES OUTPUT_NAME pokeyellow)
    endif()
endif()
