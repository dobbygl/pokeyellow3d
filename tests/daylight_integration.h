#pragma once

namespace daylight_qa {
inline FILE *snapshots = nullptr;
inline bool recording = false;
inline size_t replay_frames = 0;
inline void observe(GBContext *ctx, int) {
    QaWalk run{ctx};
    auto state = world_animation_qa::snapshot(run, "logs/engine-current.state");
    uint64_t size = uint64_t(state.size());
    run.require(size > 200000 && size < 1048576 && ++replay_frames < 12000,
                "bounded complete engine snapshot stream");
    if (recording) {
        run.require(std::fwrite(&size, sizeof(size), 1, snapshots) == 1 &&
                        std::fwrite(state.data(), 1, state.size(), snapshots) == state.size(),
                    "record complete baseline engine frame");
    } else {
        uint64_t expected_size = 0;
        run.require(std::fread(&expected_size, sizeof(expected_size), 1, snapshots) == 1 &&
                        expected_size == size,
                    "matching snapshot frame and byte count");
        std::vector<uint8_t> expected(state.size());
        run.require(std::fread(expected.data(), 1, expected.size(), snapshots) == expected.size() &&
                        expected == state,
                    "all engine bytes, scripts, encounters and RNG match disabled lighting");
    }
}
inline int replay(GBContext *ctx, bool fp, int hour) {
    QaWalk run{ctx};
    recording = hour < 0;
    replay_frames = 0;
    run.require(pallet3d_daylight(recording
                                      ? daynight::Settings{}
                                      : daynight::Settings{daynight::Mode::Fixed, double(hour)}),
                "select independent replay lighting");
    if (fp)
        special_transition_qa::key(ctx, SDL_SCANCODE_F3);
    // Each variant runs in a fresh process. A streaming lossless baseline
    // avoids keeping thousands of full snapshots in RAM or uncompressed on
    // disk. Every decompressed byte is compared; no hash substitutes for it.
    snapshots =
        popen(recording ? "gzip -1c > ../engine-baseline.gz" : "gzip -dc ../engine-baseline.gz",
              recording ? "w" : "r");
    run.require(snapshots, "private lossless snapshot stream");
    qa_frame_observer = observe;
    int result = battle_menus(ctx);
    qa_frame_observer = nullptr;
    if (!recording)
        run.require(std::fgetc(snapshots) == EOF, "exact same number of engine frames");
    run.require(pclose(snapshots) == 0, "lossless snapshot stream completed");
    snapshots = nullptr;
    run.require(replay_frames > 1000 && !run.read(pallet::Battle),
                "real wild encounter, menu scripts and escape completed");
    world_animation_qa::snapshot(run, "logs/engine-final.state");
    std::printf(
        "PASS: %zu complete engine frames %s; original encounter and scripts, hour=%d camera=%s\n",
        replay_frames, recording ? "recorded" : "byte-identical", hour, fp ? "fp" : "ortho");
    return result;
}
inline int captures(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.input(nullptr);
    run.wait(60);
    run.require(pallet::view(ctx) == pallet::View::Overworld, "live exterior lighting fixture");
    if (fp)
        special_transition_qa::key(ctx, SDL_SCANCODE_F3);
    for (int i = 0; i < 90; ++i)
        presentation_qa::render(ctx);
    const auto style = pallet3d_menu_style();
    const bool artistic = pallet3d_artistic();
    pallet3d_load_preferences("logs/lighting.cfg");
    // A new preferences file defaults to integrated. Keep the style selected
    // by this QA run while exercising lighting persistence in isolation.
    run.require(pallet3d_menu_style(style, true), "persist selected QA menu style");
    run.require(pallet3d_artistic(artistic, true), "persist selected QA artistic setting");
    run.require(pallet3d_daylight({}, true), "save disabled setting separately from cartridge");
    auto disabled = presentation_qa::render(ctx);
    capture_surface("logs/disabled-before.ppm");
    auto state = world_animation_qa::snapshot(run, "logs/before.state");
    for (int hour : {6, 12, 18, 0}) {
        run.require(pallet3d_daylight({daynight::Mode::Fixed, double(hour)}, true),
                    "persist fixed lighting");
        pallet3d_load_preferences("logs/lighting.cfg");
        run.require(pallet3d_menu_style() == style, "lighting reload preserves selected QA style");
        run.require(pallet3d_artistic() == artistic,
                    "lighting reload preserves selected QA artistic setting");
        auto image = presentation_qa::render(ctx);
        auto light = pallet3d_daylight_frame();
        run.require(light.enabled && light.hour == hour && image != disabled,
                    "selected lighting reaches actual GPU output");
        capture_surface(("logs/hour-" + std::to_string(hour) + ".ppm").c_str());
        run.require(world_animation_qa::snapshot(run, "logs/after.state") == state,
                    "clock and persistence do not modify any engine state");
    }
    run.require(pallet3d_daylight({}), "disable lighting after time changes");
    run.require(presentation_qa::render(ctx) == disabled,
                "same frozen scene returns to exact disabled pixels");
    capture_surface("logs/disabled-after.ppm");
    run.require(pallet3d_daylight({daynight::Mode::Automatic, 12}, true),
                "persist automatic local time");
    presentation_qa::render(ctx);
    run.require(pallet3d_daylight_frame().enabled &&
                    std::abs(pallet3d_daylight_frame().hour -
                             daynight::local_hour(std::time(nullptr))) < .01,
                "automatic renderer follows host local clock");
    run.require(pallet3d_daylight({daynight::Mode::Fixed, 18.5}, true),
                "leave a fixed setting for fresh-process reload");
    special_transition_qa::key(ctx, SDL_SCANCODE_ESCAPE);
    presentation_qa::render(ctx);
    capture_surface("logs/settings.ppm");
    run.require(pallet3d_input_mask() == 255, "lighting settings release relative controls");
    special_transition_qa::key(ctx, SDL_SCANCODE_ESCAPE);
    run.require(world_animation_qa::snapshot(run, "logs/after.state") == state,
                "opening lighting controls leaves the original machine untouched");
    run.require(pallet3d_menu_style() == style, "lighting controls preserve selected QA style");
    std::printf("[DAYLIGHT] menu style=%s\n",
                style == ui_preferences::Style::Classic ? "classic" : "integrated");
    std::puts(
        "PASS: four hours and local clock, exact disabled restoration, settings and memory guards");
    return 0;
}
inline int reload(GBContext *ctx, const char *path) {
    const auto style = pallet3d_menu_style();
    pallet3d_load_preferences(path);
    auto settings = pallet3d_daylight_settings();
    QaWalk run{ctx};
    run.require(settings.mode == daynight::Mode::Fixed && settings.hour == 18.5,
                "fresh process restores separately persisted fixed time");
    run.require(pallet3d_menu_style() == style, "fresh process restores selected QA menu style");
    std::puts("PASS: fresh-process lighting preference reload");
    return 0;
}
} // namespace daylight_qa
