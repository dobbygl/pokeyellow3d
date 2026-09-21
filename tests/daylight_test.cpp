#include "daylight.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

static void check(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
static bool near(double a, double b, double epsilon = .0001) {
    return std::abs(a - b) < epsilon;
}
int main() {
    using namespace daynight;
    check(wrap(24) == 0 && wrap(-1) == 23 && wrap(49.5) == 1.5, "midnight and negative wrap");
    check(wrap(NAN) == 12 && wrap(INFINITY) == 12, "invalid clock falls back to noon");
    check(!sample({}, 6).enabled && !sample({Mode::Disabled, 18}, 0).enabled,
          "disabled ignores both clocks");
    check(sample({Mode::Fixed, 18}, 6).hour == 18 && sample({Mode::Automatic, 18}, 6).hour == 6,
          "fixed hour and local hour are independent");
    auto dawn = sample({Mode::Fixed, 6}, 0), noon = sample({Mode::Fixed, 12}, 0),
         dusk = sample({Mode::Fixed, 18}, 0), night = sample({Mode::Fixed, 0}, 0);
    check(dawn.sun[0] > .9 && near(dawn.sun[1], 0) && noon.sun[1] > .9 && dusk.sun[0] < -.9 &&
              night.sun[1] < -.9,
          "sun rises in the east, peaks at noon, sets in the west, then below the horizon");
    check(noon.ambient[0] > night.ambient[0] && noon.direct[0] > night.direct[0] &&
              dawn.horizon[0] > dawn.horizon[2] && dusk.horizon[0] > dusk.horizon[2] &&
              night.zenith[2] > night.zenith[0],
          "daylight, warm dawn/dusk and cool night palettes");
    check(noon.windows == 0 && night.windows == 1 && dawn.windows > 0 && dusk.windows > 0,
          "windows light smoothly around twilight and at night");
    for (double boundary : {0., 4.5, 6., 9., 12., 15., 18., 21., 24.}) {
        auto a = sample({Mode::Fixed, boundary - .00001}, 0);
        auto b = sample({Mode::Fixed, boundary + .00001}, 0);
        for (size_t i = 0; i < 3; ++i)
            check(near(a.sun[i], b.sun[i]) && near(a.horizon[i], b.horizon[i]) &&
                      near(a.zenith[i], b.zenith[i]) && near(a.fog[i], b.fog[i]) &&
                      near(a.ambient[i], b.ambient[i]) && near(a.direct[i], b.direct[i]),
                  "all light channels remain continuous at keyframes and midnight");
        check(near(a.windows, b.windows), "window emission remains continuous at midnight");
    }
    for (int minute = 0; minute < 1440; ++minute) {
        auto light = sample({Mode::Fixed, minute / 60.0}, 0);
        double length = 0;
        for (float component : light.sun)
            length += component * component;
        check(near(length, 1), "sun direction stays normalized all day");
    }
    std::time_t now = std::time(nullptr);
    auto expected = *std::localtime(&now);
    check(
        near(local_hour(now), expected.tm_hour + expected.tm_min / 60.0 + expected.tm_sec / 3600.0),
        "automatic uses host local timezone, not UTC");

    auto directory = std::filesystem::temp_directory_path() /
                     ("pokeyellow3d-lighting-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    auto path = (directory / "lighting.cfg").string();
    check(load(path).mode == Mode::Disabled, "missing preferences preserve prior rendering");
    for (Mode mode : {Mode::Disabled, Mode::Automatic, Mode::Fixed}) {
        check(save(path, {mode, 23 + 59 / 60.0}), "persist each mode");
        auto restored = load(path);
        check(restored.mode == mode && near(restored.hour, 23 + 59 / 60.0, 1e-12),
              "restart restores mode and exact fixed minute");
    }
    for (const char *bad :
         {"", "pokeyellow3d-lighting 2 fixed 18", "pokeyellow3d-lighting 1 fixed",
          "pokeyellow3d-lighting 1 fixed 24", "pokeyellow3d-lighting 1 fixed -1",
          "pokeyellow3d-lighting 1 fixed nan", "pokeyellow3d-lighting 1 unknown 12",
          "pokeyellow3d-lighting 1 fixed 12 unexpected"}) {
        std::ofstream(path) << bad;
        check(load(path).mode == Mode::Disabled, "invalid preferences retain disabled fallback");
    }
    check(!save((directory / "absent" / "lighting.cfg").string(), {}),
          "failed preference write is reported");
    std::filesystem::remove_all(directory);
    std::puts(
        "PASS: local clock, continuous lighting and midnight; independent persistent settings");
}
