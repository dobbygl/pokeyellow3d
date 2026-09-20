# Plan: ten improvements after completing the 3D design

Date: 2026-09-20. Status: proposed. Ordered by priority; each point is
independent except where a dependency is noted.

## Starting point

The `PLAN_KANTO_3D.md`, `PLAN_PRIMERA_PERSONA.md`,
`PLAN_INTERIORES_COMBATES.md`, `PLAN_MENUS_TITULO_TRANSICIONES.md`, and
`PLAN_POKEDEX_PC.md` plans define the complete 3D presentation. This
document covers what comes next. Facts verified on 2026-09-20:

- Repository `github.com/dobbygl/pokeyellow3d`, 12 commits, no `.github/`.
- The runtime integration consists of 14 textual patches in
  `cmake/Pallet3D.cmake` over `platform_sdl.cpp`, with gb-recompiled pinned
  to a specific revision via `GBRT_REF`.
- Of the CTest tests, only `firstperson` runs without the ROM; the other six
  and all `pallet_render_smoke` journeys require the ROM and private
  savestates.
- The world is drawn with colored boxes and 8-pixel tiles; 2,960 tiles
  across 104 interiors have no art classification.
- The tileset reader already extracts the animation flag, but water and
  flowers are drawn static. NPCs outside the original screen are not
  animated.
- The runtime bundles SDL2, OpenGL ES 2, ImGui, controller support via
  `SDL_GameController`, serial transfer with callbacks, and a launcher with
  a game table verified by SHA-256.
- The game exposes the current music track in `wMapMusicSoundID`,
  `wNewSoundID`, and `wLastMusicSoundID`.

## Summary

| # | Improvement | Payoff | Effort | Depends on |
| --- | --- | --- | --- | --- |
| 1 | Stable extension API in gb-recompiled | Removes the structural risk | Medium | — |
| 2 | CI on GitHub Actions and tests without the ROM | Regressions and contributions | Medium | 1 |
| 3 | Data-driven art pass and lighting | The biggest visual leap | High | 2 |
| 4 | Presentational day/night cycle | Atmosphere at low cost | Low | 3 |
| 5 | Animated world | Removes the diorama feel | Medium | 2 |
| 6 | Web build and gamepad | Massive reach | Medium | 1, 2 |
| 7 | Red, Blue, and Spanish Yellow | Triples the audience | High | 2 |
| 8 | Optional enhanced audio | Immersion | Medium | 1 |
| 9 | Photo mode and event cameras | Shareability | Low | 3 |
| 10 | Cable link between instances | Trades and battles | Medium | 1 |

Points 1 and 2 are detailed in `PLAN_API_CI.md`.

## 1. Stable extension API in gb-recompiled

What: replace the 14 textual patches with a runtime presentation interface,
with frame, event, shutdown, capture, framebuffer coverage, and controller
mask callbacks. First in an own fork, then as a contribution to
`GB-Recomp/gb-recompiled`.

Why: any change to the SDL frontend breaks the build; the pinned revision
prevents adopting runtime improvements; other recompiled games cannot reuse
the 3D layer.

Acceptance criteria:

- [ ] `cmake/Pallet3D.cmake` contains no textual replacement of the runtime.
- [ ] The 3D layer builds against a tagged runtime version that declares the API version.
- [ ] The existing regression suites pass with no behavior changes.

## 2. CI on GitHub Actions and tests without the ROM

What: a continuous integration workflow that builds on Linux (Windows and
macOS jobs defined but disabled until the runtime is portable),
runs the tests that do not require the ROM, and publishes binaries on every
tag. A synthetic ROM generated in code feeds the reader, the classifiers,
view selection, and a headless render.

Why: today there is no automatic safety net and no outside contributor can
verify a contribution.

Acceptance criteria:

- [x] Every push and every pull request builds and passes CTest on Linux; Windows and macOS jobs stay disabled.
- [ ] At minimum, the ROM reader, the terrain and interior classifiers, `view()`, and the renderer in preview mode are tested without the ROM.
- [ ] Tests that require the ROM remain separated and documented for local execution.

## 3. Data-driven art pass and lighting

What: a per-tile-family pipeline that supports optional models and textures
with fallback to the original tile; directional light, shadow-map shadows,
ambient occlusion, and per-family materials. Complete the classification of
the 2,960 pending tiles.

Why: it's the improvement anyone notices in the first screenshot. The
architecture already separates ROM data from art touch-ups; only the art is
missing.

Acceptance criteria:

- [ ] No tile in the 38 outdoor maps or the 179 interiors remains unclassified.
- [ ] Shadows and lighting active in both cameras without exceeding twice the current presentation time.
- [ ] A missing art asset never produces a gap: it always falls back to the original tile.

## 4. Presentational day/night cycle

What: system time converted into sun position, sky color, fog, and window
light at night. Presentation only; the game does not change.

Why: the first generation has no clock and the static ambience feels flat.
With the lighting from point 3, the cost is low.

Acceptance criteria:

- [ ] Dawn, noon, dusk, and night are reviewed in Pallet Town, Route 1, and Viridian City.
- [ ] An application setting allows fixing the time or disabling the cycle.
- [ ] Encounters, scripts, and RNG do not change with the time of day.

## 5. Animated world

What: water and flower tileset animations driven by the ROM flag; grass
with wind; NPCs outside the original screen animated from their movement
state and the ROM's sprite sheets; particles when walking in grass and when
surfing.

Why: today the world is a diorama with actors frozen at a distance.

Acceptance criteria:

- [ ] Water and flowers animate at the original game's cadence.
- [ ] An NPC walking outside the original screen shows its walking frames.
- [ ] The regression journeys keep passing with memory intact.

## 6. Web build and gamepad

What: an Emscripten build, with the ROM supplied by the user in the browser
and verified by hash; gamepad mapping for relative movement, camera, and
shortcuts.

Why: SDL2, OpenGL ES 2, and ImGui are the natural combination for
WebAssembly. Playing without installing multiplies the reach.

Acceptance criteria:

- [ ] The web version starts, loads a local ROM, and reaches the outdoor world in 3D.
- [ ] A gamepad navigates Pallet Town, talks to an NPC, and battles without a keyboard.
- [ ] CI publishes the web version on every tag.

## 7. Red, Blue, and Spanish Yellow

What: an address table per ROM variant, verified against each pret
project's symbols, and automatic selection by hash in the launcher.

Why: the 3D layer only understands the English Yellow ROM. The other three
variants share the same structure and multiply the audience.

Acceptance criteria:

- [ ] Each variant passes the ROM audit and the Pallet Town–to–Viridian City journey.
- [ ] No address is shared between variants without recorded verification.
- [ ] The launcher identifies the variant by hash and rejects unknown ROMs.

## 8. Optional enhanced audio

What: current-track detection via WRAM, a user-supplied music pack with
crossfade, and positional 3D effects. Original audio by default and no
asset included in the repository.

Why: the immersion of the 3D contrasts with the original chip audio, and
the current track is already readable.

Acceptance criteria:

- [ ] Changing maps or entering a battle changes the pack's track with a fade.
- [ ] Without a pack, the audio is identical to the current one.
- [ ] Footstep and door effects are positioned relative to the camera.

## 9. Photo mode and event cameras

What: a free camera with the game paused, guided cameras for the Oak and
Team Rocket events detected by script, and PNG and GIF capture from the
application.

Why: it's cheap, it never writes to memory, and shared screenshots are the
best possible growth for the project.

Acceptance criteria:

- [ ] Photo mode does not advance the game or alter WRAM.
- [ ] Oak's entrance on Route 1 has a guided camera and returns to the normal camera when it ends.
- [ ] Screenshots are saved next to the save with a name based on map and date.

## 10. Cable link between instances

What: local or network socket transport for the runtime's serial transfer,
with the Cable Club screens composited over the 3D.

Why: original trades and battles are the biggest remaining nostalgia value,
and the 3D battle scene already exists.

Acceptance criteria:

- [ ] Two instances trade a Pokémon and both saves remain consistent.
- [ ] A full link battle is presented in 2D, as defined by the battle plan, with no desync.
- [ ] Disconnection is handled with the game's original message.
