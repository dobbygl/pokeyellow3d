# Plan: runtime extension API and CI on GitHub Actions

Date: 2026-09-20. Status: partial implementation; goal paused for commit and push. Develops points 1 and 2
of `PLAN_MEJORAS.md`.

## Goal and scope

Two chained outcomes. First, that the 3D layer integrates with gb-recompiled
through a declared, versioned interface, instead of rewriting the SDL
frontend at configure time. Second, that every push and every pull request to
`github.com/dobbygl/pokeyellow3d` builds on Linux and runs a test
suite that does not need the ROM, with binaries published on every tag.

Out of scope: changing the game's or the renderer's behavior, including the
ROM or any derivative of it in the repository or in CI, and journey tests
with savestates, which remain local.

## Verified starting point

- `cmake/Pallet3D.cmake` applies 14 textual replacements to the downloaded
  runtime's `runtime/src/platform_sdl.cpp` and compiles the resulting copy
  instead of the original. Each replacement aborts configuration if its
  anchor disappears. The points touched are: header inclusion, depth buffer
  size, drawing after `ImGui::NewFrame`, event before the gamepad, shutdown
  before destroying GL, capture before `SDL_GL_SwapWindow`, skipping the
  framebuffer upload and draw when the 3D layer covers the frame at three
  points, and a dedicated d-pad channel with WASD key release at five
  points, including input recording.
- `CMakeLists.txt` fetches gb-recompiled via `FetchContent` at the revision
  pinned in `GBRT_REF`. The runtime links SDL2, CURL, Threads, and OpenGL or
  GLES2, and its CMake already distinguishes Linux, Windows, and macOS.
- The game's generated C sources are in the repository, 13 files with the
  largest around 700 KB, so the build does not need rgbds or pret.
- Current CTest tests: `firstperson` runs without the ROM. `pallet_state`,
  `kanto_rom`, `kanto_geometry`, `interior`, `interior_audit`, and
  `battle_state` are only registered if `build/roms/pokeyellow.gbc` exists.
  `pallet_render_smoke` is excluded from `all` and requires the ROM and
  savestates.
- `pallet_render_smoke` already initializes the runtime headless with
  `SDL_VIDEODRIVER=offscreen` and `SDL_AUDIODRIVER=dummy` over the real
  OpenGL backend, and `pallet3d_preview` draws a map without actors or a
  save.
- `.gitignore` excludes `build/`, ROMs, and saves. `.github/` does not exist.
- The `kanto::Rom` reader validates ranges and throws exceptions with
  diagnostics; the header, tileset, ledge, and collision-pair structures are
  described in `src/kanto_rom.h` with their offsets, which makes it possible
  to generate a synthetic image that satisfies them.

## Architecture decisions

1. The interface lives in the runtime, not in this repository. It is
   defined in a new `runtime/include/gb_presentation.h` with a callback
   structure and a `GB_PRESENTATION_API_VERSION` constant. This repository
   only registers its implementation.
2. Three-step path: fork `dobbygl/gb-recompiled` with the interface and a
   tag; this repository points to that tag; pull request to the original
   project. Once accepted, `GBRT_REF` switches to the original tag. The
   repository does not depend on the pull request being accepted in order to
   work.
3. The interface covers exactly what the patches do today, without
   expanding it: GL attributes before creating the context, per-frame
   drawing that returns whether it covers the framebuffer, an event with the
   option to consume it, shutdown, capture before the buffer swap, and an
   external d-pad channel with a call to release the keys of a set of
   actions. Input recording includes the external channel, as it does now.
4. Explicit version compatibility: `Pallet3D.cmake` checks the header
   constant and fails with a message if it does not match. All textual
   anchors disappear.
5. Tests without the ROM use a synthetic image built in memory by
   `tests/synthetic_rom.h`, with the minimal tables the reader and the
   classifiers expect, and a test `GBContext` with its own WRAM, VRAM, I/O,
   and framebuffer. It never contains data copied from the real ROM.
6. Tests that require the ROM keep their conditional registration and run
   locally or on a dedicated runner. CI lists them as skipped, not failed.
7. A single `ci.yml` workflow with an OS matrix, and a `release.yml`
   workflow per tag. No secrets.

## Phase 1: presentation interface in a runtime fork

Work:

- Create the `dobbygl/gb-recompiled` fork from the currently pinned
  revision.
- Add `gb_presentation.h`:
  - `GBPresentationHooks` with `gl_attributes()`, `frame(ctx, w, h, menu_open) -> bool covers`,
    `event(const SDL_Event*, menu_open) -> bool consumed`, `before_swap(w, h)`,
    `shutdown()`, and `input_poll(ctx, menu_open)`.
  - `gb_platform_set_presentation(const GBPresentationHooks*)`.
  - `gb_platform_set_external_dpad(uint8_t mask)` and
    `gb_platform_release_keys(const SDL_Scancode*, size_t)`.
  - Integer `GB_PRESENTATION_API_VERSION`.
- Implement the six call points and the d-pad channel in `platform_sdl.cpp`,
  replicating the current behavior of skipping the framebuffer upload and of
  input recording. Without a registered presentation, the runtime behaves
  exactly as before.
- Tag `presentation-api-v1` on the fork.

Acceptance criteria:

- [ ] The fork's runtime builds and runs a game with no registered presentation with identical behavior.
- [ ] A test presentation in the fork itself receives frame, event, shutdown, and capture in the expected order.
- [ ] The version constant is documented in the runtime's README.

## Phase 2: migrating the 3D layer to the interface

Work:

- `GBRT_REF` points to `presentation-api-v1` on the fork, with the fork's
  URL as a cache variable so it is possible to go back to the original.
- `src/pallet3d.cpp` exposes a static `GBPresentationHooks` and registers it
  from the launcher or from `pokeyellow_main`. Relative controls use
  `gb_platform_set_external_dpad` and `gb_platform_release_keys`.
- `cmake/Pallet3D.cmake` is reduced to: checking the API version, adding
  `src/pallet3d.cpp` to the executable, and setting the depth buffer size
  through the attributes callback.
- Remove the generated `pallet-runtime` directory and every reference to it
  in documentation and scripts.

Acceptance criteria:

- [ ] `cmake/Pallet3D.cmake` contains no textual-replacement calls.
- [ ] The full CTest suite and the three `world_qa.sh`, `firstperson_qa.sh`, and `interiors_qa.sh` suites pass locally.
- [ ] The 38 outdoor captures and the interior captures are identical to those of the previous version.
- [ ] Recording and replaying input with relative controls produces the same final state as before.

## Phase 3: synthetic ROM and tests without the ROM

Work:

- `tests/synthetic_rom.h`: builds a 1 MiB image with a valid cartridge
  header, `MapHeaderBanks`, `MapHeaderPointers`, tileset headers, a ledge
  table, and collision pairs at the offsets `kanto_rom.h` expects. It
  generates a small world: three connected outdoor maps with offsets, one
  interior per warp, one outdoor tileset and one interior tileset with
  procedural graphics, grass, water, a ledge, and a sign.
- `tests/synthetic_context.h`: `GBContext` with its own WRAM, VRAM, I/O,
  HRAM, and framebuffer, with helpers to place the player, write the live
  map, open a text box via border tiles, and build a battle tilemap with
  portraits.
- New tests, all registered unconditionally:
  - `rom_reader_synthetic`: catalog, connections, origins, warps, rejection
    of truncated images.
  - `terrain_synthetic`: outdoor and interior classification over the
    generated world.
  - `view_synthetic`: `Unsupported`, `Transition`, `Dialogue`, `Overworld`,
    and `Battle` from the synthetic context.
  - `battle_state_synthetic`: portrait rectangles, names, default palette,
    and decompression of a synthetic portrait if phase A1 of
    `PLAN_POKEDEX_PC.md` already exists.
  - `render_preview_synthetic`: initializes SDL headless, builds the
    synthetic world's meshes with `pallet3d_preview`, draws ten frames, and
    checks for no GL errors, intact memory, and a non-empty capture.
- Tests with the real ROM remain registered only if the ROM exists. Add a
  `rom` CTest label so they can be excluded with `-LE rom`.

Acceptance criteria:

- [ ] `ctest -LE rom` passes in a build directory without `roms/`.
- [ ] No synthetic test contains bytes copied from the real ROM; the generator is purely procedural.
- [ ] `render_preview_synthetic` passes with `SDL_VIDEODRIVER=offscreen` over software Mesa.

## Phase 4: CI workflow

Work:

- `.github/workflows/ci.yml` triggered on push and pull request:
  - Matrix: `ubuntu-24.04` with GCC and with Clang; `windows-2022` with
    MSVC and vcpkg dependencies; `macos-14` marked non-blocking until the
    runtime's GL backend is confirmed.
  - Steps: checkout, dependency installation, FetchContent `_deps` cache
    and ccache cache, configure with `-DPOKEYELLOW_3D=ON`, build with
    Ninja, `ctest -LE rom --output-on-failure`.
  - On Linux, `render_preview_synthetic` under `xvfb-run` or with
    surfaceless EGL Mesa `llvmpipe`; whichever works on the runner is the
    one that is pinned.
  - Additional build with `-DPOKEYELLOW_3D=OFF` on Linux so the 2D
    executable does not break.
  - Warnings as errors for `src/` and `tests/` with `-Wall -Wextra -Werror`;
    the runtime and the generated C are outside that rule.
  - Format check with `clang-format --dry-run` on `src/` and `tests/`.
- `.github/workflows/release.yml` triggered by a `v*` tag: builds on Linux
  and Windows, packages `pokeyellow3d` with the launcher, `PALLET3D.md`, and
  a run `README`, and publishes the files to the release. No ROM.
- Status badges in `README.md` and a "Contributing" section with the local
  commands and the note that `rom` tests are local.
- Branch protection: `main` requires the workflow to pass in order to
  merge.

Acceptance criteria:

- [x] A push to `main` and a test pull request show the workflow passing on Linux. Windows and macOS jobs are defined but disabled: the pinned runtime includes POSIX headers unconditionally and uses C++20 designated initializers under the project's C++17 standard; macOS lacks GLES2 headers.
- [ ] A pull request that breaks the synthetic reader fails in CI with the test flagged.
- [ ] A `v0.1.0` tag produces a release with Linux binaries (Windows once the runtime is portable).
- [ ] Total workflow time with a warm cache is under ten minutes.

## Phase 5: contribution to the original project

Work:

- Raise the runtime's Windows portability with upstream as well: the POSIX
  headers included unconditionally by `serial_link.c` and
  `network_discovery.c`, and the C++20 designated initializers in
  `platform_sdl.cpp` when the consumer pins C++17.
- Pull request to `GB-Recomp/gb-recompiled` with `gb_presentation.h`, the
  implementation, the example presentation, and the documentation. No
  reference to Pokémon anywhere in the runtime code.
- Address review feedback; if the interface changes, update the fork's tag
  and this repository in the same change.
- Once merged, `GBRT_REF` points to the original project's tag and the fork
  remains as a mirror.

Acceptance criteria:

- [ ] The pull request is open with the original project's CI passing.
- [ ] This repository builds against the original tag after the merge, or against the fork in the meantime, without textual patches in either case.

## Accepted limitations

- Journey tests, real battles, real interiors, and capture comparisons
  still need the ROM and private savestates. CI does not replace them; it
  guarantees the reader, the classifiers, view selection, and the renderer.
- macOS depends on the runtime offering a compatible GL backend; until that
  is confirmed, it does not block.
- Acceptance of the pull request does not depend on this project; the fork
  covers the interval.

## Validation and delivery

- Each phase ends with the full CTest suite locally with the ROM and with
  `ctest -LE rom` in a directory without the ROM.
- Regression captures are compared byte for byte after phase 2.
- Deliverables: tagged fork, `Pallet3D.cmake` without patches, the synthetic
  generator and its tests, two GitHub Actions workflows, a README with
  badges and a contributing section, and the pull request to the original
  project.
- Check the boxes only with recorded evidence, including links to the CI
  runs.

## Recommended order

1. Phase 3 can start now, in parallel with phase 1, because it does not
   touch the runtime integration.
2. Phase 1 and phase 2 in sequence, with the capture regression as a gate.
3. Phase 4 as soon as phase 3 has its first synthetic test; it expands as
   the others arrive.
4. Phase 5 once phase 2 closes.

## Technical references

- `cmake/Pallet3D.cmake`: inventory of the 14 current integration points.
- `build/_deps/gb_recompiled-src/runtime/src/platform_sdl.cpp`:
  `update_effective_joypad_state`, framebuffer upload, and the event loop.
- `build/_deps/gb_recompiled-src/runtime/CMakeLists.txt`: dependencies and per-OS branches.
- `tests/pallet_render_smoke.cpp`: headless initialization that reuses the synthetic test.
- [GB-Recomp/gb-recompiled](https://github.com/GB-Recomp/gb-recompiled).
- [GitHub Actions: matrices and caching](https://docs.github.com/actions).


### Paused checkpoint, 2026-09-20

- The runtime fork exists at `dobbygl/gb-recompiled`, branch
  `presentation-api`, based on upstream `6581880fce60e6f139901a5942fc984e9c1db8ab`.
  Its versioned header, frontend callbacks and procedural contract test are
  implemented and compile. Runtime execution validation is still pending;
  no API tag or upstream PR has been published.
- This game still uses its existing runtime integration. Migration to the
  API and removal of textual patches have not been performed.
- Existing synthetic tests have been audited: procedural portrait decoding
  now exercises all three modes, plane ordering, mirroring and truncation;
  render invariants include cartridge RAM. `battle_transition` is correctly
  labeled ROM-dependent. The independent ROM-free build passes 8/8 tests,
  including software GL rendering with llvmpipe. Evidence is recorded in the
  AREA checkpoint in `PLAN_POKEDEX_PC.md`.
- CI workflows already exist; the initial-state description above is
  historical. The remaining CI, test PR, release and upstream validation
  criteria are not claimed complete by this checkpoint.
