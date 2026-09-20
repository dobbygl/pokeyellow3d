# Generate an adapted copy; never modify the fetched runtime or generated game C.
find_package(SDL2 REQUIRED)
get_target_property(runtime_dir gbrt SOURCE_DIR)
file(READ "${runtime_dir}/src/platform_sdl.cpp" platform_source)
function(pallet_hook before after)
    string(FIND "${platform_source}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pallet 3D: runtime integration point changed: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" platform_source "${platform_source}")
    set(platform_source "${platform_source}" PARENT_SCOPE)
endfunction()
pallet_hook("#include \"shader_pipeline.h\"" "#include \"shader_pipeline.h\"\n#include \"pallet3d.h\"")
pallet_hook("SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);" "SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);")
pallet_hook("    ImGui::NewFrame();" "    ImGui::NewFrame();\n    pallet3d_draw(g_registered_ctx, draw_w, draw_h, g_show_menu);")
pallet_hook("    const uint8_t joyp = ctx ? ctx->io[0x00] : 0xFF;" "    if (pallet3d_event(event, g_show_menu)) return true;\n    const uint8_t joyp = ctx ? ctx->io[0x00] : 0xFF;")
pallet_hook("    /* Tear down GL-owned objects before the context goes away. */" "    pallet3d_shutdown();\n    /* Tear down GL-owned objects before the context goes away. */")
pallet_hook("    SDL_GL_SwapWindow(g_window);" "    pallet3d_capture(draw_w, draw_h);\n    SDL_GL_SwapWindow(g_window);")
# The 3D view clears and covers the whole drawable. Avoid uploading/drawing a
# discarded 2D frame, while retaining the original path on first initialization,
# F2, dialogue, transitions, interiors, battles or an initialization failure.
pallet_hook("    static uint32_t s_upload_buf[GB_FRAMEBUFFER_SIZE];"
    "    const bool pallet_covers_frame = pallet3d_covers_frame(g_registered_ctx);\n    if (!pallet_covers_frame) {\n    static uint32_t s_upload_buf[GB_FRAMEBUFFER_SIZE];")
pallet_hook("    g_last_timing.upload_ms = sdl_now_ms() - upload_start_ms;"
    "    }\n    g_last_timing.upload_ms = sdl_now_ms() - upload_start_ms;")
pallet_hook("    if (g_shader_pipeline) {\n        const int active_shader = gb_shader_pipeline_active(g_shader_pipeline);"
    "    if (g_shader_pipeline && !pallet_covers_frame) {\n        const int active_shader = gb_shader_pipeline_active(g_shader_pipeline);")
# Relative controls have their own active-low d-pad channel. Scripts remain intact.
pallet_hook("static uint8_t g_script_joypad_dpad = 0xFF;"
    "static uint8_t g_script_joypad_dpad = 0xFF;\nstatic uint8_t g_pallet_joypad_dpad = 0xFF;\nstatic bool g_pallet_relative = false;")
pallet_hook([=[static void update_effective_joypad_state(void) {
    rebuild_manual_joypad_state_from_bindings();]=]
[=[void pallet3d_set_input_mask(uint8_t mask, bool relative) {
    if (relative != g_pallet_relative) {
        for (int action = 0; action < GB_INPUT_ACTION_COUNT; action++)
            for (int slot = 0; slot < 2; slot++)
                for (auto key : {SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D})
                    if (binding_matches_scancode(g_keyboard_bindings[action][slot], key))
                        g_keyboard_binding_pressed[action][slot] = false;
    }
    g_pallet_relative = relative;
    g_pallet_joypad_dpad = mask;
}
static void update_effective_joypad_state(void) {
    pallet3d_poll_controls(g_registered_ctx, g_show_menu);
    rebuild_manual_joypad_state_from_bindings();]=])
pallet_hook("g_joypad_dpad = g_manual_joypad_dpad & g_script_joypad_dpad;"
    "g_joypad_dpad = g_manual_joypad_dpad & g_script_joypad_dpad & g_pallet_joypad_dpad;")
pallet_hook("g_input_record_dpad = g_manual_joypad_dpad;"
    "g_input_record_dpad = g_manual_joypad_dpad & g_pallet_joypad_dpad;")
pallet_hook("g_input_record_dpad == g_manual_joypad_dpad &&"
    "g_input_record_dpad == (g_manual_joypad_dpad & g_pallet_joypad_dpad) &&")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pallet-runtime")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pallet-runtime/platform_sdl.cpp" CONTENT "${platform_source}" @ONLY)
get_target_property(runtime_sources gbrt SOURCES)
set(pallet_runtime_sources)
foreach(source IN LISTS runtime_sources)
    if(source STREQUAL "src/platform_sdl.cpp")
        list(APPEND pallet_runtime_sources "${CMAKE_CURRENT_BINARY_DIR}/pallet-runtime/platform_sdl.cpp")
    else()
        get_filename_component(full_source "${source}" ABSOLUTE BASE_DIR "${runtime_dir}")
        list(APPEND pallet_runtime_sources "${full_source}")
    endif()
endforeach()
set_property(TARGET gbrt PROPERTY SOURCES "${pallet_runtime_sources}")
target_sources(gbrt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet3d.cpp")
target_include_directories(gbrt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# The property was originally set in runtime/ scope; the generated copy is GLES2 too.
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/pallet-runtime/platform_sdl.cpp" PROPERTIES COMPILE_DEFINITIONS "IMGUI_IMPL_OPENGL_ES2")
