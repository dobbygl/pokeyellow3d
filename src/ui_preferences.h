#pragma once
#include "daylight.h"

namespace ui_preferences {
enum class Style { Classic, Integrated };
struct Settings {
    daynight::Settings lighting;
    Style style = Style::Integrated;
};
inline Settings load(const std::string &path) {
    Settings fallback;
    std::ifstream input(path);
    input.imbue(std::locale::classic());
    std::string header, mode, style, extra;
    int version = 0;
    if (!(input >> header >> version) || header != "pokeyellow3d-lighting")
        return fallback;
    if (version == 1)
        return {daynight::load(path), Style::Integrated};
    Settings result;
    if (version != 2 || !(input >> mode >> result.lighting.hour >> style) || (input >> extra) ||
        !std::isfinite(result.lighting.hour) || result.lighting.hour < 0 ||
        result.lighting.hour >= 24)
        return fallback;
    if (mode == "disabled")
        result.lighting.mode = daynight::Mode::Disabled;
    else if (mode == "automatic")
        result.lighting.mode = daynight::Mode::Automatic;
    else if (mode == "fixed")
        result.lighting.mode = daynight::Mode::Fixed;
    else
        return fallback;
    if (style == "classic")
        result.style = Style::Classic;
    else if (style != "integrated")
        return fallback;
    return result;
}
inline bool save(const std::string &path, Settings settings) {
    using daynight::Mode;
    if ((settings.lighting.mode != Mode::Disabled && settings.lighting.mode != Mode::Automatic &&
         settings.lighting.mode != Mode::Fixed) ||
        (settings.style != Style::Classic && settings.style != Style::Integrated))
        return false;
    std::ofstream output(path, std::ios::trunc);
    output.imbue(std::locale::classic());
    output.precision(17);
    output << "pokeyellow3d-lighting 2\n"
           << (settings.lighting.mode == Mode::Disabled    ? "disabled"
               : settings.lighting.mode == Mode::Automatic ? "automatic"
                                                           : "fixed")
           << ' ' << daynight::wrap(settings.lighting.hour) << '\n'
           << (settings.style == Style::Classic ? "classic" : "integrated") << '\n';
    output.close();
    return bool(output);
}
} // namespace ui_preferences
