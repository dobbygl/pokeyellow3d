#include "ui_preferences.h"
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
int main() {
    using namespace ui_preferences;
    auto directory = std::filesystem::temp_directory_path() /
                     ("pokeyellow3d-preferences-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    auto path = (directory / "lighting.cfg").string();
    check(load(path).style == Style::Integrated && load(path).artistic,
          "new installations default to integrated and artistic");
    for (const char *mode : {"disabled", "automatic", "fixed"}) {
        std::ofstream(path) << "pokeyellow3d-lighting 1\n" << mode << " 18.25\n";
        auto old = daynight::load(path);
        auto migrated = load(path);
        check(migrated.lighting.mode == old.mode && migrated.lighting.hour == old.hour &&
                  migrated.style == Style::Integrated && migrated.artistic,
              "v1 lighting is unchanged; the new style defaults to integrated");
        for (auto style : {Style::Classic, Style::Integrated}) {
            std::ofstream(path) << "pokeyellow3d-lighting 2\n"
                                << mode << " 18.25\n"
                                << (style == Style::Classic ? "classic" : "integrated") << '\n';
            auto v2 = load(path);
            check(v2.style == style && v2.artistic && v2.lighting.mode == old.mode &&
                      v2.lighting.hour == old.hour,
                  "v2 migrates both settings and enables artistic presentation");
            for (bool artistic : {false, true}) {
                check(save(path, {old, style, artistic}), "write v3 preferences");
                auto restored = load(path);
                check(restored.style == style && restored.artistic == artistic &&
                          restored.lighting.mode == old.mode && restored.lighting.hour == old.hour,
                      "restart preserves three independent preferences");
            }
        }
    }
    for (const char *bad :
         {"", "pokeyellow3d-lighting 3 fixed 12 classic", "pokeyellow3d-lighting 2 fixed 12",
          "pokeyellow3d-lighting 2 fixed 12 typo", "pokeyellow3d-lighting 2 fixed 12 classic extra",
          "pokeyellow3d-lighting 2 fixed 24 classic", "pokeyellow3d-lighting 2 fixed nan classic",
          "pokeyellow3d-lighting 2 unknown 12 classic",
          "pokeyellow3d-lighting 3 fixed 12 classic art-typo",
          "pokeyellow3d-lighting 3 fixed 12 classic art-off extra"}) {
        std::ofstream(path) << bad;
        auto fallback = load(path);
        check(fallback.style == Style::Integrated && fallback.artistic &&
                  fallback.lighting.mode == daynight::Mode::Disabled,
              "malformed preferences never partially apply");
    }
    check(!save((directory / "missing" / "lighting.cfg").string(), {}), "write failure reported");
    check(!save(path, {{}, Style(7)}), "invalid style cannot be persisted");
    std::filesystem::remove_all(directory);
    std::puts("PASS: v1/v2 migration, v3 persistence, default and invalid preferences");
}
