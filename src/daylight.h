#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <fstream>
#include <locale>
#include <string>

// Pure presentation inputs. This module has no access to the emulated machine.
namespace daynight {
constexpr double Tau = 6.283185307179586;
enum class Mode { Disabled, Automatic, Fixed };
struct Settings {
    Mode mode = Mode::Disabled;
    double hour = 12;
};
using RGB = std::array<float, 3>;
struct Light {
    bool enabled = false;
    double hour = 12;
    RGB sun{}, horizon{}, zenith{}, fog{}, ambient{}, direct{};
    float windows = 0;
};
inline double wrap(double hour) {
    if (!std::isfinite(hour))
        return 12;
    double result = std::fmod(hour, 24);
    return result < 0 ? result + 24 : result;
}
inline RGB mix(RGB a, RGB b, float t) {
    for (size_t i = 0; i < a.size(); ++i)
        a[i] += (b[i] - a[i]) * t;
    return a;
}
inline Light sample(Settings settings, double local_hour) {
    Light out;
    if (settings.mode != Mode::Automatic && settings.mode != Mode::Fixed)
        return out;
    out.enabled = true;
    out.hour = wrap(settings.mode == Mode::Fixed ? settings.hour : local_hour);
    struct Key {
        double hour;
        RGB horizon, zenith, ambient, direct;
        float windows;
    };
    constexpr Key night{0, {.12f, .17f, .27f}, {.025f, .045f, .11f}, {.27f, .34f, .49f}, {0, 0, 0},
                        1};
    constexpr Key dawn{
        6, {.94f, .57f, .38f}, {.22f, .30f, .49f}, {.54f, .45f, .47f}, {.52f, .28f, .16f}, .45f};
    constexpr Key noon{
        12, {.72f, .84f, .85f}, {.20f, .44f, .66f}, {.69f, .74f, .78f}, {.40f, .37f, .29f}, 0};
    constexpr Key dusk{
        18, {.96f, .51f, .29f}, {.24f, .22f, .42f}, {.53f, .40f, .43f}, {.59f, .26f, .12f}, .5f};
    const Key keys[]{night,
                     {4.5, night.horizon, night.zenith, night.ambient, night.direct, 1},
                     dawn,
                     {9, noon.horizon, noon.zenith, noon.ambient, noon.direct, 0},
                     noon,
                     {15, noon.horizon, noon.zenith, noon.ambient, noon.direct, 0},
                     dusk,
                     {21, night.horizon, night.zenith, night.ambient, night.direct, 1},
                     {24, night.horizon, night.zenith, night.ambient, night.direct, 1}};
    for (size_t i = 1; i < std::size(keys); ++i) {
        if (out.hour > keys[i].hour)
            continue;
        const auto &a = keys[i - 1], &b = keys[i];
        float t = float((out.hour - a.hour) / (b.hour - a.hour));
        t = t * t * (3 - 2 * t);
        out.horizon = mix(a.horizon, b.horizon, t);
        out.zenith = mix(a.zenith, b.zenith, t);
        out.fog = out.horizon;
        out.ambient = mix(a.ambient, b.ambient, t);
        out.direct = mix(a.direct, b.direct, t);
        out.windows = a.windows + (b.windows - a.windows) * t;
        break;
    }
    double angle = (out.hour - 6) * Tau / 24;
    constexpr double length = 1.0307764064044151; // length of (cos, sin, .25)
    out.sun = {float(std::cos(angle) / length), float(std::sin(angle) / length),
               float(.25 / length)};
    return out;
}
inline double local_hour(std::time_t time) {
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &time))
        return 12;
#else
    if (!localtime_r(&time, &local))
        return 12;
#endif
    return local.tm_hour + local.tm_min / 60.0 + local.tm_sec / 3600.0;
}
inline Settings load(const std::string &path) {
    Settings fallback;
    std::ifstream input(path);
    input.imbue(std::locale::classic());
    std::string header, mode, extra;
    int version = 0;
    double hour = 0;
    if (!(input >> header >> version >> mode >> hour) || header != "pokeyellow3d-lighting" ||
        version != 1 || !std::isfinite(hour) || hour < 0 || hour >= 24 || (input >> extra))
        return fallback;
    if (mode == "disabled")
        return {Mode::Disabled, hour};
    if (mode == "automatic")
        return {Mode::Automatic, hour};
    if (mode == "fixed")
        return {Mode::Fixed, hour};
    return fallback;
}
inline bool save(const std::string &path, Settings settings) {
    if (settings.mode != Mode::Disabled && settings.mode != Mode::Automatic &&
        settings.mode != Mode::Fixed)
        return false;
    std::ofstream output(path, std::ios::trunc);
    output.imbue(std::locale::classic());
    output.precision(17);
    output << "pokeyellow3d-lighting 1\n"
           << (settings.mode == Mode::Disabled    ? "disabled"
               : settings.mode == Mode::Automatic ? "automatic"
                                                  : "fixed")
           << ' ' << wrap(settings.hour) << '\n';
    output.close();
    return bool(output);
}
} // namespace daynight
