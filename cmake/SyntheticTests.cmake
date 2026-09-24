# Tests built on tests/synthetic_rom.h, a procedural 1 MiB image. They are
# registered unconditionally because they never touch the cartridge. The
# cartridge-backed tests keep their own EXISTS guard in CMakeLists.txt and are
# labelled here so `ctest -LE rom` runs the portable set alone.

get_target_property(gbrt_runtime_dir gbrt SOURCE_DIR)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME art_mesh_report COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/art_mesh_report_test.py")
set_tests_properties(art_mesh_report PROPERTIES LABELS synthetic)

# Header-only state tests: kanto_rom.h, world_scene.h, pallet_state.h and
# battle_state.h, the last two of which need the runtime's gbrt.h.
foreach(synthetic_test ambient_occlusion_test exterior_geometry_test art_manifest_test rom_reader_synthetic terrain_synthetic view_synthetic battle_state_synthetic battle_phase_synthetic pc_details_test tile_animation_test world_animation_test daylight_test ui_preferences_test rom_font_test pokemon_menu_test item_menu_test dex_menu_test battle_bag_test options_menu_test trainer_card_test naming_menu_test)
    add_executable(${synthetic_test} tests/${synthetic_test}.cpp)
    target_include_directories(${synthetic_test} PRIVATE src "${gbrt_runtime_dir}/include")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${synthetic_test} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${synthetic_test} COMMAND ${synthetic_test})
    set_tests_properties(${synthetic_test} PROPERTIES LABELS synthetic)
endforeach()

# The renderer lives in the gbrt target (see cmake/Pallet3D.cmake), which also
# carries the public SDL2, OpenGL and ImGui usage requirements this test needs.
add_executable(render_preview_synthetic tests/render_preview_synthetic.cpp)
target_include_directories(render_preview_synthetic PRIVATE src)
target_link_libraries(render_preview_synthetic PRIVATE gbrt)
target_compile_definitions(render_preview_synthetic PRIVATE SDL_MAIN_HANDLED)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(render_preview_synthetic PRIVATE -Wall -Wextra)
endif()
add_test(NAME render_preview_synthetic COMMAND render_preview_synthetic)
# Exit 77 means no OpenGL ES 2 context was available (a `dummy` video driver,
# for instance); the test reports the reason and CTest records it as skipped.
if(WIN32)
    set(preview_environment "SDL_VIDEODRIVER=windows;SDL_AUDIODRIVER=dummy;SDL_OPENGL_ES_DRIVER=1")
else()
    set(preview_environment "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy")
endif()
set_tests_properties(render_preview_synthetic PROPERTIES
    LABELS synthetic
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${preview_environment}")

# Compile the production shadow resource in isolation so the negative case can
# create a genuinely incomplete framebuffer without adding game fault switches.
add_executable(shadow_map_gl_test tests/shadow_map_gl_test.cpp)
target_include_directories(shadow_map_gl_test PRIVATE src)
target_link_libraries(shadow_map_gl_test PRIVATE gbrt)
target_compile_definitions(shadow_map_gl_test PRIVATE SDL_MAIN_HANDLED)
add_test(NAME shadow_map_gl COMMAND shadow_map_gl_test)
set_tests_properties(shadow_map_gl PROPERTIES
    LABELS synthetic SKIP_RETURN_CODE 77 ENVIRONMENT "${preview_environment}")

foreach(rom_test interior interior_audit battle_state fade_state battle_transition pallet_state kanto_geometry kanto_rom)
    if(TEST ${rom_test})
        set_tests_properties(${rom_test} PROPERTIES LABELS rom)
    endif()
endforeach()

# Registered even without the private ROM: exclusion/skipping is explicit.
add_executable(art_manifest_rom tests/art_manifest_rom.cpp)
target_include_directories(art_manifest_rom PRIVATE src)
add_test(NAME art_manifest_rom COMMAND art_manifest_rom "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
set_tests_properties(art_manifest_rom PROPERTIES LABELS rom SKIP_RETURN_CODE 77)
add_executable(exterior_geometry_rom tests/exterior_geometry_rom.cpp)
target_include_directories(exterior_geometry_rom PRIVATE src)
add_test(NAME exterior_geometry_rom COMMAND exterior_geometry_rom "${CMAKE_CURRENT_BINARY_DIR}/roms/pokeyellow.gbc")
set_tests_properties(exterior_geometry_rom PROPERTIES LABELS rom SKIP_RETURN_CODE 77)
