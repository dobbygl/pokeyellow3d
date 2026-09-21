# Plan: ten improvements after completing the 3D design

Date: 2026-09-20. Updated 2026-09-21: points 5 and 4 are completed, verified and released in v0.2.0, after the prerequisite work recorded in the other plans. The integrated objective still awaits the original runtime CI gate in `PLAN_API_CI.md`. Other improvements retain their individual status below.

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

Points 1 and 2 are detailed in `PLAN_API_CI.md`. Their implementation criteria below are verified by its phases 1–4 execution records, including the Linux `v0.1.0` release. The upstream contribution and full Windows portability in phase 5 remain pending.

## 1. Stable extension API in gb-recompiled

What: replace the 14 textual patches with a runtime presentation interface,
with frame, event, shutdown, capture, framebuffer coverage, and controller
mask callbacks. First in an own fork, then as a contribution to
`GB-Recomp/gb-recompiled`.

Why: any change to the SDL frontend breaks the build; the pinned revision
prevents adopting runtime improvements; other recompiled games cannot reuse
the 3D layer.

Acceptance criteria:

- [x] `cmake/Pallet3D.cmake` contains no textual replacement of the runtime.
- [x] The 3D layer builds against a tagged runtime version that declares the API version.
- [x] The existing regression suites pass with no behavior changes.

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
- [x] At minimum, the ROM reader, the terrain and interior classifiers, `view()`, and the renderer in preview mode are tested without the ROM.
- [x] Tests that require the ROM remain separated and documented for local execution.

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

- [x] Dawn, noon, dusk, and night are reviewed in Pallet Town, Route 1, and Viridian City.
- [x] An application setting allows fixing the time or disabling the cycle.
- [x] Encounters, scripts, and RNG do not change with the time of day.

### Phase 4A: presentational clock and lighting

Work:

- Convert local system time into a continuous sun direction and palette for
  sky, fog, ambient/direct light and emissive window surfaces. Reuse the
  existing materials; completing point 3 is outside this requested scope.
- Add application settings for automatic local time, a fixed hour and disabled
  cycle. Persist presentation preferences separately from cartridge saves.
- Keep disabled mode byte-identical to the previous world rendering. Keep
  title sunset independent of the world clock. Never advance or sample RNG.

Acceptance criteria:

- [x] Unit tests cover midnight wrap, dawn/noon/dusk/night and fixed/disabled settings.
- [x] Both cameras expose the settings and preserve them across application restarts.
- [x] Sun, sky, fog and window light respond continuously; disabled mode preserves the reference captures.

### Phase 4B: visual and engine parity gate

Work:

- Capture and review dawn, noon, dusk and night in Pallet Town, Route 1 and
  Viridian City in both cameras. Record exact fixed hours and commands.
- Replay identical input with different fixed hours and disabled lighting;
  compare complete engine state, scripts, encounters and RNG. Guard WRAM,
  VRAM, cartridge RAM and framebuffer around every presentation frame.
- Run full CTest, ROM-free CTest and every `tests/*_qa.sh` suite. Compare all
  38 exterior references with the cycle disabled, and explicitly document
  intended illumination differences for the enabled captures.

Acceptance criteria:

- [x] All 24 location/time/camera captures are reviewed under `build/qa/logs/`.
- [x] Engine states and RNG are identical for the time-of-day replay variants.
- [x] All regression suites pass; capture comparisons and reproducible commands are recorded.

### 4A/4B execution record — 2026-09-21

Branch `day-night-4ab`, [PR #13](https://github.com/dobbygl/pokeyellow3d/pull/13),
implementation `2ef3242674696593eec462a325814c9a18f959dd`, based on completed
5B PR #12 merged as `8ac62d4200961eb453c2baeed51c5ab57bd565a7`.
The runtime remains pinned to `00cc26dafb9a41ea9d935508e9fbe9e25b5f5a6e`.
The upstream CI blocker recorded in `PLAN_API_CI.md` remains open; this
acceptance does not close that separate requirement.

`src/daylight.h` converts local time into a continuous sun direction and
sky, fog, ambient/direct light and original-window emission. Settings append
to the runtime's existing ImGui window through the presentation callback;
no runtime or generated code changes are needed. `lighting.cfg` uses SDL's
application preference directory, separately from cartridge data. Automatic,
fixed and disabled modes persist independently; missing or invalid preferences
select disabled lighting. Interiors preserve their previous lighting and the
title retains its fixed sunset.

Validation completed against frozen game and runtime sources:

- 30/30 full CTest, 13/13 tests in the independent ROM-free directory, and
  clang-format 18.1.8 passed. No renderer test was skipped. Unit tests cover
  local time, all lighting keyframes, midnight continuity, every mode,
  preference round trips, malformed input and failed writes.
- All 20 `tests/*_qa.sh` suites passed, followed by full CTest again. This
  includes boot, title, both animation suites, daylight, PC details, world,
  first person, interiors, battles, battle UI, crossfades, special transitions,
  menus, UI transitions, Dex list/portraits/area and PC focus/storage.
- The 24 final captures in `build/qa/daylight-1HnG07` were reviewed at 06:00,
  12:00, 18:00 and 00:00 in Pallet, Route 1 and Viridian, in both cameras.
  Dawn/dusk have warm horizons and different facade illumination; night
  retains legible terrain with warm original window panes. Both settings
  screens were reviewed. All six scenarios reload the fixed preference in a
  fresh process and restore the disabled image byte for byte.
- Each camera replays 3,933 frames from the same fixture and input, disabled
  and at each of the four hours. All 31,464 complete frame-state comparisons
  are byte-identical, including scripts, a natural encounter, battle menus,
  escape, RNG and cartridge RAM. Lossless gzip streams keep the complete
  baseline bounded on disk; every decoded byte is compared, not just hashes.
  Per-presentation guards protect WRAM, VRAM, cartridge RAM and framebuffer.
- The new title guard tests four world hours against an identical frozen
  title image in New Game, Continue and idle/intro-return scenarios.
- All 38 original exterior references are byte-identical with the cycle
  disabled. Of 2,077 captures compared with 5B, 2,043 are exact, including all
  215 interior views, 374 tile/world animation captures and 486 battle/UI
  captures. All 34 differences were visually reviewed: four settings screens
  include the new controls; 29 captures differ in host-timed intermediate
  fade intensity; one first-person F2-return capture differs in 112 distant
  grass pixels after the host-timed wait advances the guest. Stable endpoints
  and all fixed catalog references remain exact. The enabled lighting views
  are separate from this unchanged reference gate.
- [Implementation CI](https://github.com/dobbygl/pokeyellow3d/actions/runs/35595769175)
  passed Linux GCC, Clang, 2D-only, format and native Windows/MSVC with ANGLE.
  The documentation commit must retain green CI before merging.

Private evidence under `build/qa/logs/`: `daylight-regression-evidence.json`,
`daylight-final-regressions.log`, `daylight-final-exterior-comparison.txt`,
`daylight-source-snapshot.json`, `daylight-final-visual-review.json` and
`daylight-baseline-differences-review.json`. The review manifests hash the
actual captures and the contact pages, including `daylight-final-*-review.png`
and `daylight-baseline-*.png`. The acceptance verifier checks the complete
suite inventory, unchanged sources, code CI, reference images, replay bytes
and both visual-review records. ROMs, states and captures stay private.

Reproduction (existing private fixtures):

```sh
cmake --build build --target all pallet_render_smoke --parallel 4
ctest --test-dir build --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
tests/daylight_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state \
  build/qa/firstperson-9cB3FJ/route.state \
  build/qa/interiors-fJDbZk/city.state
bash build/qa/logs/run-daylight-final-regressions.sh
python3 build/qa/logs/check-daylight-regressions.py
```

The private driver configures/builds the independent no-ROM directory,
checks format, runs every QA suite serially, then repeats CTest and compares
the 38 exterior references. Fixture paths and commands for the focused suite
are also documented in `PALLET3D.md`.

## 5. Animated world

What: water and flower tileset animations driven by the ROM flag; grass
with wind; NPCs outside the original screen animated from their movement
state and the ROM's sprite sheets; particles when walking in grass and when
surfing.

Why: bring the ROM's tile and actor animation into the 3D presentation.

Acceptance criteria:

- [x] Water and flowers animate at the original game's cadence.
- [x] An NPC walking outside the original screen shows its walking frames.
- [x] The regression journeys keep passing with memory intact.

### Phase 5A: original tileset animation

Work:

- Verify the tileset animation flag, original water/flower frames and cadence
  against the ROM and the live VRAM output of the original engine.
- Animate private presentation textures from ROM data at the original guest
  cadence, honoring tileset flags, pause, reload and map transitions. Never
  modify guest tiles or depend on wall-clock time for original animations.

Acceptance criteria:

- [x] Water and flower frames match original VRAM over complete animation periods.
- [x] Non-animated tilesets, pauses and loads select the correct frame without stale textures.
- [x] Full CTest, ROM-free CTest and every QA suite pass; frame guards cover all guest memory.

### 5A execution record — 2026-09-21

Branch `tileset-animations-5a`, [PR #6](https://github.com/dobbygl/pokeyellow3d/pull/6).
The user explicitly selected 5A while the broader integrated goal remained
paused; 5B and the other pending milestones are not part of this change.

`src/tile_animation.h` reconstructs the water tile's rotation and the three
flower frames from ROM. It finds the exact resident phase by matching the
original VRAM, rather than estimating it from elapsed host time or a counter
that survives tileset changes. ROM animation flags select none, water, or
water+flowers. Pauses and loads need no accumulated presentation clock;
unrecognized tiles keep their original static appearance. The renderer
uploads only changed 8×8 cells in its private atlas. The catalog's fixed ROM
phase and all other geometry, actor and game behavior remain unchanged.

The original routine is `UpdateMovingBgTiles` at `00:1c75`, verified with
`pokeyellow_metadata.json`, `pokeyellow_internal.h`, and live execution.
[pret's original routine](https://github.com/pret/pokeyellow/blob/master/home/vcopy.asm)
explains the eight alternating rotation steps: water updates at counter 20,
and flag 2 copies a flower at counter 21 before resetting. The integration
oracle checks those counter transitions, direction and flower sequence
independently of the renderer's VRAM-matching implementation.

Validation completed:

- 26/26 CTest tests, 10/10 tests in the independent ROM-free build, and
  clang-format 18.1.8. The new synthetic test uses procedural bytes only.
- `tests/tile_animation_qa.sh` passed all 20 scenarios: maps 0, 51, 40, 65,
  95, 94, 59, 83, 9 and 37 in both cameras. This covers all nine animated
  tilesets and one disabled tileset. Each scenario verifies 360 engine frames,
  two full animation periods, live VRAM, actual GPU texels, frozen guest
  frames and immediate state reload. The Pallet scenarios also traverse the
  original house entrance and exit and switch to/from a fixed catalog preview.
- Evidence: `build/qa/tile-animation-mNqcoQ/`,
  `build/qa/logs/tiles-5a-animation-summary.json`,
  `tiles-5a-phases-review.png` and `tiles-5a-cameras-review.png`.
  Every presented frame is guarded against WRAM, VRAM, SRAM and framebuffer writes.
- All 38 exterior catalog images in `build/qa/kanto-39A2jj/logs/catalog`
  exactly match `build/qa/kanto-hvoPvo/logs/catalog`.
- Of the 215 interior journey/catalog images, 210 are byte-identical to the
  deterministic pre-5A reference `interiors-3jiH2O`, including all 179 catalog
  views. The five intentional changes are `lab-return[-fp].ppm`,
  `pallet-return[-fp].ppm` and `town-route1.ppm`: reviewed differences affect
  only animated water/flowers. Counts and highlighted comparisons are in
  `tiles-5a-interior-comparison.json` and `tiles-5a-intentional-differences.png`
  under `build/qa/logs/`.

The complete 15-suite regression passed with exit 0, followed by 26/26 CTest
and an exact 38-image exterior comparison. Suites: tile animation, PC details,
world, first person, interiors, battles, battle UI, crossfades, menus,
transitions, Dex list, all 151 Dex portraits, Dex areas, PC focus and PC storage.
`tiles-5a-regressions.log`, `tiles-5a-acceptance.json`,
`tiles-5a-exterior-comparison.txt` and the individual suite logs record the
results under `build/qa/logs/`. The PR's Linux GCC, Clang, 2D-only and format
checks also passed for the implementation commit. The final documentation
commit must keep those checks green before merge.

Reproducible commands (private fixtures remain local):

```sh
cmake --build build --target all pallet_render_smoke --parallel 4
ctest --test-dir build --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
tests/tile_animation_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state
bash build/qa/logs/run-tiles-5a-regressions.sh
```

The driver configures/builds the independent no-ROM directory, checks format,
runs every `tests/*_qa.sh`, repeats full CTest, and compares the 38 exterior
reference images. ROMs, states and captures remain ignored in `build/qa/`.

### Phase 5B: distant actors, wind and particles

Work:

- Derive NPC direction and walking frame from live movement state and decode
  the corresponding ROM sprite sheet even outside the original LCD bounds.
- Animate grass with deterministic presentation-only wind. Add bounded grass
  and surf particle pools triggered by observed movement, without invoking
  game RNG, changing state or adding movement to stationary actors.
- Reset transient effects on loads/map changes and freeze them while paused.

Acceptance criteria:

- [x] A walking NPC outside the LCD shows the ROM walking frames matching its live state; stationary actors stay stationary.
- [x] Reviewed grass wind, walking particles and surf particles work in both cameras, with bounded memory.
- [x] Identical input yields identical engine state with effects enabled or disabled, including RNG and cartridge RAM.
- [x] Full CTest, ROM-free CTest and every QA suite pass. All 38 exterior reference captures remain byte-identical at their fixed reference animation phase; separate frame sequences prove the animation, without relaxing the reference gate.

### 5B execution record — 2026-09-21

Branch `world-animations-5b`, [PR #12](https://github.com/dobbygl/pokeyellow3d/pull/12),
implementation `7b48b43dd8e0698b851416673bef5748bc570812`, based on API/Windows PR #11 merged as
`f1ab11a08f9e1d0768ac0f6653820f134624cd61`. The upstream CI requirement is
still externally blocked as recorded in `PLAN_API_CI.md`; the integrated
objective explicitly authorizes continuing with the next point in this case.

The implementation reads NPC movement counters and the matching ROM's 82
sprite-sheet entries (05:42a9 through 05:43f0). It reconstructs off-LCD
frames, including flips, and derives partial steps from the remaining walk
counter. The original engine freezes ordinary offscreen NPCs: no synthetic
movement is added to them. The private test additionally requests an original
scripted walk, which can continue outside the LCD, and validates actual GPU
pixels there. The matching recompiled routine's `LD DE,$cc5b` at 01:4ea2
verifies the script buffer alias; generated code remains unchanged.

Wind changes only grass-tip vertices. Grass and surf trails use a fixed
96-particle pool, observed movement and guest cycles; a private integer mixer
varies particles without accessing game RNG. Pause freezes effects, while
loads and map changes reset them. Catalog rendering selects phase zero.

Validation completed against the frozen implementation and runtime sources:

- 29/29 full CTest and 12/12 tests in the independent ROM-free build,
  with no skipped renderer test; clang-format 18.1.8 passed.
- All 19 `tests/*_qa.sh` suites passed, followed by another full CTest.
  The inventory includes boot, title, tile/world animation, world, first
  person, interiors, battles, battle UI, crossfade, special transitions,
  menus, transitions, Dex list/portraits/area and PC details/focus/storage.
- The six focused scenarios in `build/qa/world-animation-GGh8J7` verify
  all four original walking frames against resident VRAM and actual GPU
  pixels. The scripted off-LCD walk covers four phases and 31 positions
  in each camera. Stationary/frozen actor checks use live counters.
- Grass and surf replays in both cameras compare every byte of 300 complete
  serialized engine snapshots per scenario: 1,200 matching pairs with
  effects on/off, including CPU, cartridge RAM, WRAM, VRAM, OAM, HRAM, I/O,
  PPU and APU state. Per-frame guards also reject presentation writes to
  WRAM, VRAM, cartridge RAM or the original framebuffer. Pause, immediate
  reload, real map crossings and the bounded pool pass their checks.
- Reviewed separate NPC, stationary-wind, walking-grass and surf sequences
  in both cameras. The fixed pool holds at most 96 particles; this journey
  reaches 21 grass and 15 surf particles. Review sheets and their input
  manifests are `5b-final-*-review.png` and `5b-final-visual-review.json`
  under `build/qa/logs/`.
- All 38 exterior references in `build/qa/kanto-HdtWld/logs/catalog` match
  `build/qa/kanto-hvoPvo/logs/catalog` byte for byte. All 179 interior catalog
  views also match. Across 1,895 prior journey/catalog captures, 1,695 match
  exactly and 200 differ. Reviewed comparisons show animated grass in live
  or retained world scenes and host-timed fade opacity in transitional
  boot/title/crossfade frames, preserving scene geometry and UI content.
  These extra journey comparisons do not relax the fixed-reference gate.
  `5b-broad-visual-review.json` records the reviewed samples explicitly.
- [Implementation CI](https://github.com/dobbygl/pokeyellow3d/actions/runs/35591205727)
  passed Linux GCC, Clang, 2D-only, format and native Windows MSVC. Windows
  ran all 12 ROM-free tests, including ANGLE rendering; macOS remains disabled.
  The documentation commit must also pass CI before merge.

`build/qa/logs/5b-regression-evidence.json` ties results to the implementation,
runtime, input directories and reviewed image hashes. `5b-source-snapshot.json`
checks source identity; `5b-final-regressions.log` and its exit file record
the complete run. ROMs, states and captures remain private and ignored.

Reproducible commands (focused fixture paths are also in `PALLET3D.md`):

```sh
cmake --build build --target all pallet_render_smoke --parallel 4
ctest --test-dir build --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
tests/world_animation_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state \
  build/qa/firstperson-9cB3FJ/route.state \
  build/qa/kanto-Eqy7Ry/surf-active.state
bash build/qa/logs/run-5b-final-regressions.sh
python3 build/qa/logs/check-5b-regressions.py
```

The private regression driver configures/builds the independent no-ROM
directory, checks the format and full suite inventory, runs every QA script
serially and verifies all 38 exterior references. The acceptance verifier
also checks source hashes, complete replay parity, visual-review hashes and
the successful implementation CI response saved as `5b-code-ci.json`.

### Execution and validation order for points 5 and 4

The user explicitly advanced phase 5A on 2026-09-21 while the integrated
goal remained paused. This changes the order for 5A only; the other phases
and prerequisite work remain pending.

After the 5A exception above, implement 5B, 4A, then 4B after the three
prerequisite plans. Every phase
records build/test commands and output directories; unchecked criteria remain
pending until the relevant evidence is inspected. ROMs, saves and captures stay
private under `build/qa/`. Unsupported input retains the existing presentation.
Final delivery includes `build/pokeyellow3d`, updated `PALLET3D.md` and
`README.md`, reviewed captures, and green CI on `main`.

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
