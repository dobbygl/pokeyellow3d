#include "shadow_projection.h"
#include <cstdio>
#include <stdexcept>

namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::array<double, 4> clip(const firstperson::Matrix &m, sunlight::Point p) {
    std::array<double, 4> result{};
    for (int row = 0; row < 4; ++row)
        result[row] = m[row] * p[0] + m[4 + row] * p[1] + m[8 + row] * p[2] + m[12 + row];
    return result;
}
bool visible(const firstperson::Matrix &m, sunlight::Point p) {
    auto c = clip(m, p);
    return c[3] > 0 && std::abs(c[0]) <= c[3] && std::abs(c[1]) <= c[3] && std::abs(c[2]) <= c[3];
}
} // namespace
int main() {
    try {
        sunlight::Bounds resident;
        resident.add({-40, -.8, -80});
        resident.add({40, 6, 30});
        firstperson::Camera eye;
        eye.x = 3;
        eye.z = 2;
        for (auto camera :
             {firstperson::orthographic(3, 2, -.32f, .08f, .09f), eye.perspective(4.f / 3)}) {
            for (sunlight::Point sun :
                 {sunlight::Point{1, .15, .25}, {0, 1, .25}, {-1, .15, .25}}) {
                auto fitted = sunlight::fit(resident, camera, sun);
                check(fitted.valid, "valid camera and resident map must fit");
                size_t count = 0;
                // Independent oracle: a dense world-space lattice, accepted by
                // direct camera clip coordinates rather than the plane solver.
                for (int x = -40; x <= 40; ++x)
                    for (int z = -80; z <= 30; ++z)
                        for (double y : {0., 1., 3., 6.}) {
                            sunlight::Point p{double(x), y, double(z)};
                            if (!visible(camera, p))
                                continue;
                            ++count;
                            check(visible(fitted.matrix, p), "a visible receiver was cropped");
                            // Rays parallel to the sun keep the same shadow UV;
                            // casters towards the sun must have smaller depth.
                            auto receiver = clip(fitted.matrix, p);
                            for (int axis = 0; axis < 3; ++axis)
                                p[axis] += sun[axis] * .5;
                            auto caster = clip(fitted.matrix, p);
                            check(std::abs(receiver[0] - caster[0]) < 1e-6 &&
                                      std::abs(receiver[1] - caster[1]) < 1e-6 &&
                                      caster[2] < receiver[2],
                                  "sun ray must preserve UV and order depth towards the sun");
                        }
                check(count > 100, "the oracle must exercise visible receivers");
            }
        }
        auto narrow = firstperson::orthographic(0, 0, 0, 1, 1);
        check(sunlight::fit(resident, narrow, {0, 1, 0}).valid,
              "vertical sun and window without resident corners remain valid");
        auto outside = firstperson::orthographic(1000, 1000, 0, 1, 1);
        check(!sunlight::fit(resident, outside, {0, 1, 0}).valid,
              "empty camera intersection must not create a shadow pass");
        check(!sunlight::fit({}, narrow, {0, 1, 0}).valid, "missing geometry must fail closed");
        check(!sunlight::fit(resident, narrow, {0, 0, 0}).valid, "missing sun must fail closed");
        narrow[0] = NAN;
        check(!sunlight::fit(resident, narrow, {0, 1, 0}).valid, "nonfinite camera rejected");
        std::puts("PASS: cropped receivers, offscreen sun rays, depth order and invalid inputs");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
