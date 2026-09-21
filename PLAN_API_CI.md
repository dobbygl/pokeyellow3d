# Plan: runtime extension API and CI on GitHub Actions

Date: 2026-09-20. Status: phases 1–4 verified; phase 5 fork/Windows validation complete, original CI externally blocked; goal active. Develops points 1 and 2
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

- [x] The fork's runtime builds and runs a game with no registered presentation with identical behavior.
- [x] A test presentation in the fork itself receives frame, event, shutdown, and capture in the expected order.
- [x] The version constant is documented in the runtime's README.

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
- Remove the generated SDL source directory and every reference to it
  in documentation and scripts.

Acceptance criteria:

- [x] `cmake/Pallet3D.cmake` contains no textual-replacement calls.
- [x] The full CTest suite and the three `world_qa.sh`, `firstperson_qa.sh`, and `interiors_qa.sh` suites pass locally.
- [x] The 38 outdoor captures and the interior captures are identical to those of the previous version.
- [x] Recording and replaying input with relative controls produces the same final state as before.

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

- [x] `ctest -LE rom` passes in a build directory without `roms/`.
- [x] No synthetic test contains bytes copied from the real ROM; the generator is purely procedural.
- [x] `render_preview_synthetic` passes with `SDL_VIDEODRIVER=offscreen` over software Mesa.

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
- [x] A pull request that breaks the synthetic reader fails in CI with the test flagged.
- [x] A `v0.1.0` tag produces a release with Linux binaries (Windows once the runtime is portable).
- [x] Total workflow time with a warm cache is under ten minutes.

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
- [x] This repository builds against the original tag after the merge, or against the fork in the meantime, without textual patches in either case.

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


### Phase 3 acceptance audit and phase 1 validation, 2026-09-20

Phase 3 is closed using `build/qa/logs/dex-a3-no-rom.log` (8/8, independent
build directory without ROMs) and `dex-a3-software-render.log` (llvmpipe,
30 preview frames plus three live frames). `tests/synthetic_rom.h` generates
its graphics, map blocks and records procedurally; game-format addresses and
IDs describe the protocol, not copied ROM assets. The procedural portrait
bitstream covers all blend modes and mirroring. The same change passed all
thirteen real-ROM QA suites and CTest 24/24, recorded in `PLAN_POKEDEX_PC.md`.

The fork's `presentation-api-v1` tag adds `begin_frame` (skip prediction) and
`state_loaded` (successful explicit loads) alongside the planned callbacks;
these preserve existing integration behavior. Its contract CTest passes,
including per-frame memory guards, failed-render recovery with letterboxing
and perturbed GL state, recording, key release and savestate notifications.
A procedural generated cartridge was regenerated and built against both
runtimes; six captures from frames 1/30/60 are identical. Separate 480x320
GL surface probes match SHA-256
`8efb730749d0374bcdb00086127579af355c02e9c8042adba9f84cd20489b5dc`.
Private fork logs: `logs/presentation-test.log`,
`logs/presentation-game-comparison.txt`, `logs/presentation-surface-comparison.txt`.

```sh
# From the gb-recompiled fork, no commercial ROM required:
cmake -S . -B build -G Ninja -DGBRT_PRESENTATION_TESTS=ON
cmake --build build --target gb_presentation_test gbrecomp --parallel 4
ctest --test-dir build -R presentation_api --output-on-failure
```

Phase 1/2 acceptance remains open until the game's post-migration regression
gate completes. The last published game commit passed Linux CI:
https://github.com/dobbygl/pokeyellow3d/actions/runs/35523876849 .


### API migration: initial local checks, 2026-09-20

`cmake/Pallet3D.cmake` now only verifies API version 1 and adds the renderer
and adapter. `src/pallet_presentation.cpp` registers from the launcher and
smoke harness, preserving depth attributes, composition, input and load hooks.
The obsolete generated SDL copies have been removed. CI/release ref extraction
accepts the version tag as well as commit hashes.

- Build and CTest pass: **24/24** (`build/qa/logs/api-ctest.log`).
- Separate ROM-free build: **8/8** (`api-no-rom.log`).
- Complete first-person QA, including the new cycle-anchored replay: **PASS**
  (`build/qa/firstperson-Bbkf91/`). Replay checks CPU registers/cycles, WRAM,
  VRAM, ERAM, OAM, HRAM, I/O and the guest framebuffer.
- The same harness was linked against the archived, unmodified pre-API runtime
  library from the previous ROM-free build. Both the recording and its start
  and end savestates are byte-identical to the API version; replay passes on
  both. Evidence: `build/qa/logs/api-input-comparison.txt`,
  `api-before-controls.log`, `api-before-replay.log`.
- The complete post-migration gate is running via
  `bash build/qa/logs/run-api-regressions.sh`; phase 1/2 checkboxes remain open
  until its results and capture comparisons have been audited.


### CI positive and negative evidence, 2026-09-20

- Positive migration PR: https://github.com/dobbygl/pokeyellow3d/pull/1 .
  Run https://github.com/dobbygl/pokeyellow3d/actions/runs/35524960932 passes
  GCC, Clang and 2D, fetching the public runtime tag. Linux job durations:
  GCC 67 seconds, Clang 48 seconds, 2D 157 seconds. Windows/macOS remain
  explicitly disabled; their status is not presented as a pass.
- Negative reader PR: https://github.com/dobbygl/pokeyellow3d/pull/2 .
  Its isolated one-line mutation accepted oversized images. Run
  https://github.com/dobbygl/pokeyellow3d/actions/runs/35525023315 fails
  `rom_reader_synthetic` in both Linux compilers with
  `FAIL: an oversized image must be refused`; the other seven tests pass.
  The PR is closed, unmerged. Main and the integration branch keep the valid
  reader. Logs: `build/qa/logs/ci-negative-reader.log`.
- Formatting, warnings-as-errors, portability and release remain pending
  before the complete CI/delivery work can close.


Main branch protection is now enabled with strict, required GitHub Actions
checks `Linux (gcc)`, `Linux (clang)` and `Linux (2D, POKEYELLOW_3D=OFF)`, also
for administrators. Force pushes and deletion are disabled. Existing reviews
were not required and no human-review gate was added. The API response is
retained in `build/qa/logs/main-protection-after.json`.


The second attempt of the positive CI run passes with restored FetchContent
and ccache entries. It ran from 17:18:43 to 17:21:24 UTC (**2m41s total**),
below ten minutes. Evidence: `build/qa/logs/api-ci-warm.json` and
`api-ci-warm.log`; GitHub run 35524960932, attempt 2. Formatting and
warnings-as-errors still remain before phase 4 can close.


### Closure of phases 1–2, 2026-09-20

The complete API regression batch exits 0 (`api-regressions.exit`): all
thirteen `tests/*_qa.sh` suites pass, followed by CTest **24/24**. The separate
build without ROMs passes **8/8**. Logs are under `build/qa/logs/api-*.log`.

- All **38 exterior** captures match the pre-API AREA checkpoint exactly:
  `kanto-OW0YIo` versus `kanto-pv52hf`; report `api-exterior-comparison.txt`.
- All **179 interior catalog** captures match directly:
  `interiors-NG0KR3` versus `interiors-lPijAF`; report
  `api-interior-catalog-comparison.txt`.
- The normal-clock journey captures again have five differences, exactly the
  five already identified before migration: `pallet-return`, `lab-return`,
  `mother-dialogue-fp`, `town-route1`, `mart-2f-camera`. Their real-time camera
  easing prevents treating independent wall-clock runs as exact references.
- For the complete comparison, the same current smoke harness was linked
  against the archived pre-API library and the API library, using a **QA-only**
  linker wrapper for `ImGui::NewFrame` that fixes `DeltaTime` to 1/60 second.
  It does not change guest cycles, input, SDL time or the production binaries.
  Both complete interior journeys pass. All **215 captures**, including the
  five above, are now byte-identical: `interiors-pymvpu` versus
  `interiors-Zo43uK`; report `api-fixed-interior-comparison.txt`.
  No reference image was replaced to obtain this result.
- Recording/replay parity is independently proven by identical recordings
  and complete start/end savestate files, as recorded above.
- The runtime tag resolves to commit
  `813f689a42111112fdeb337f9f4fdb919a31bcc0`. Its contract test, no-extension
  generated-game comparison and actual GL surface comparison pass.

Reproducible local commands (private fixtures and archived libraries retained):

```sh
bash build/qa/logs/run-api-regressions.sh
bash build/qa/logs/run-api-fixed-interiors.sh
ctest --test-dir build --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
```

The fixed-time wrapper is `build/qa/api-reference/fixed_imgui_time.cpp`; both
helpers use `-Wl,--wrap=_ZN5ImGui8NewFrameEv`. The pre-API helper links
`libgbrt-before.a` and a no-op registration shim; the API helper links the
current runtime and performs real API registration. Both link the same
`pallet_render_smoke.cpp.o` and cartridge archive. This isolates the integration
change while preserving per-frame memory checks in the journeys.

The local build uses the clean fork worktree at the tag's exact commit;
GitHub CI independently fetched and built the public tag with no source
override. Upstream remains at the original pinned revision. Windows work is
isolated on `windows-portability`; only the C++17 initializer issue has been
fixed and checked on Linux so far. POSIX networking/directory access and the
Windows GLES backend still need work and actual Windows validation.


### CI hardening in progress, 2026-09-20

The API migration is merged as `d727fa66a40d4a967e390bd9248e5e1bbd0ceeeb`;
main's Linux CI passes at
https://github.com/dobbygl/pokeyellow3d/actions/runs/35526424594 .

- Added a pinned clang-format 18.1.8 check and formatted only `src/` and
  `tests/`. Generated cartridge C and runtime sources are untouched.
- `cmake/ProjectWarnings.cmake` applies strict warnings to the two owned
  renderer/adapter sources and test targets. Actual Ninja commands verify
  **23 owned translation units** have `-Wall -Wextra -Werror`; **71 other
  units** do not. Evidence: `build/qa/logs/ci-hardening-warning-scope.txt`.
  CMake minimum is now 3.18 for source properties in another target directory.
- Removed an unused parameter name and value-initialized a QA actor before
  its checked lookup. The GCC build now passes with warnings as errors.
- The 2D job now caches ccache too. Format validation passes locally.
- Local CTest passes **24/24**, and the separate no-ROM build passes
  **8/8**. Logs: `build/qa/logs/ci-ctest.log` and
  `build/qa/logs/ci-no-rom.log`.
- The full post-hardening graphics gate is still running via
  `bash build/qa/logs/run-ci-regressions.sh` at this checkpoint; its final
  result and remote CI are not yet verified. The phase remains open.


### CI hardening validated, 2026-09-20

PR https://github.com/dobbygl/pokeyellow3d/pull/3 passes GCC, Clang, 2D and
clang-format at run https://github.com/dobbygl/pokeyellow3d/actions/runs/35527339148 .
The required checks on `main` now include `clang-format`, preserving the three
existing checks, strict up-to-date status and administrator enforcement.
The Linux matrix checks the CTest JUnit result to reject a skipped renderer
or any renderer other than software Mesa; the release workflow does the same.

- The complete local gate exits **0**, all **13** `tests/*_qa.sh` suites pass,
  and final CTest is **24/24**. The independent ROM-free build is **8/8**.
  Evidence: `ci-regressions.exit`, `ci-regressions-summary.txt`,
  `ci-ctest-final.log` and `ci-no-rom.log`, all under `build/qa/logs/`.
- All **38 exterior** captures match `kanto-pv52hf` versus `kanto-hvoPvo`;
  all **179 interior catalog** captures match `interiors-lPijAF` versus
  `interiors-K77PW7`. Reports: `ci-world-comparison.txt` and
  `ci-interiors-comparison.txt`.
- The supplementary fixed-ImGui-time journey also passes. All **215**
  interior captures match the preserved API baseline `interiors-Zo43uK`
  versus `interiors-lwsDEA`, including moving-camera captures. Report:
  `ci-fixed-interior-comparison.txt`. The old baseline binary and images
  were not overwritten; the new helper is `pallet-fixed-ci`.
- Only two source changes beyond formatting were necessary: the unused
  parameter name and the initialized QA actor documented above. Both Linux
  compilers pass strict warnings; generated C and external runtime flags
  remain unchanged.

Reproduce with the retained private fixtures:

```sh
bash build/qa/logs/run-ci-regressions.sh
bash build/qa/logs/run-ci-fixed-interiors.sh
```

The second script waits for the original full-gate process if it is still
running, then links the current helper with the same fixed-time wrapper used
for the API migration. It also validates the local software-render JUnit data.
Phase 4 still awaits the actual `v0.1.0` release artifact and its inspection.

Windows work remains isolated on the fork's `windows-portability` branch.
Commit `3de463a` replaces POSIX directory scans/creation with a C interface to
C++17 filesystem operations, preserving POSIX creation permissions. The
standalone host tests pass on real Windows/MSVC and Linux at
https://github.com/dobbygl/gb-recompiled/actions/runs/35527532377 .
This proves the filesystem layer only; sockets, threading and GLES remain
pending, and the game's pinned `presentation-api-v1` runtime is unchanged.


### Phase 4 complete: inspected Linux release, 2026-09-20

PR #3 merged as `5554ccd7a69a0605214b4d12268f6c018ec2b4f1`.
Main CI passes: https://github.com/dobbygl/pokeyellow3d/actions/runs/35527950022 .
Tag `v0.1.0` points to that commit and its release workflow passes:
https://github.com/dobbygl/pokeyellow3d/actions/runs/35527967051 .

Release: https://github.com/dobbygl/pokeyellow3d/releases/tag/v0.1.0 .
Downloaded asset `pokeyellow3d-linux-x86_64-v0.1.0.zip`, 441,633 bytes,
SHA-256 `8fdfb478b4a557166803e47e90e7b0ea03c867184b759251f8f197c8848f2309`,
matching GitHub's asset digest. The archive contains exactly the executable,
`README.md`, `PALLET3D.md`, `RUN.md` and `DEPENDENCIES.txt`; no ROM or saves.
The executable permission is preserved, all linked libraries resolve on this
host, and `--list-games` identifies the cartridge without needing a ROM.

A private copy of the ROM was then added only to the extracted QA directory.
The **downloaded executable** verifies its SHA-1, extracts 145 asset sections,
boots without a savestate and exits normally at frame 180. Reviewed captures
show the original white startup at frame 60 and copyright at frame 180.
This is a package startup check, **not** the pending 3D title/main-menu `boot`
mode in the menus plan. The host's libcurl emits a version-information warning;
it does not prevent this launcher or boot test from succeeding.

Evidence under `build/qa/logs/`: `release-v0.1.0-{audit.json,run.json,boot.log,
boot-review.png}`, `ci-main-hardening.json`. Reproduce download/audit with:

```sh
gh release download -R dobbygl/pokeyellow3d v0.1.0 --pattern '*.zip' --dir build/qa/release-v0.1.0
python3 build/qa/logs/audit-release.py
cd build/qa/release-v0.1.0/extracted/pokeyellow3d-linux-x86_64-v0.1.0
# Private roms/pokeyellow.gbc supplied locally; never part of the archive.
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  ./pokeyellow3d --limit-frames 180 --dump-present-frames 60,180 --screenshot-prefix release
```

The full thirteen-suite gate, 24/24 CTest, separate ROM-free 8/8 and exact
38/179/215 capture comparisons recorded above validate this release's source.
The local `build/pokeyellow3d` has also been rebuilt from the merged commit.
Phase 5's fork-build criterion is independently met by this CI/release, which
fetches `presentation-api-v1` without runtime text patches. Its upstream PR and
complete Windows networking/GLES portability remain pending.

The isolated filesystem portability commit also passes the original runtime
presentation contract and a freshly regenerated procedural game. Frames
1/30/60 remain byte-identical (`gb-recompiled-windows/logs/windows-filesystem-*`).
This does not change the runtime version used by the release.

### Phase 5: upstream draft and Windows networking, 2026-09-21

Upstream contribution: https://github.com/GB-Recomp/gb-recompiled/pull/2 .
The draft contains the versioned presentation API, implementation, procedural
example/contract test and documentation, plus the host portability changes.
The added-line audit against upstream `6581880` contains no game-specific
references. Existing upstream game mocks retain their original purpose; their
filesystem calls use the same generic portability interface as other callers.

The fork's `windows-portability` branch includes:

- `9a1280c`: C++17 link/discovery workers and POSIX/Winsock sockets, preserving
  the C APIs and BGB/LAN packet formats. Real loopback tests cover exact wire
  bytes, master/slave transfers, fragmented reads, reconnects, rejected
  versions, failed starts, bounded cancellation, peer updates/expiry and
  persisted discovery identity. Native Linux and Windows/MSVC pass at
  https://github.com/dobbygl/gb-recompiled/actions/runs/35579385732 .
- `cb3fe7e`: Windows links matching ANGLE GLES2/EGL libraries and SDL uses the
  corresponding context. A separate Windows CI job builds the entire SDL
  runtime and runs the presentation contract with the Windows video driver.
  This integration run is still pending:
  https://github.com/dobbygl/gb-recompiled/actions/runs/35579683835 .

Both local runtime revisions pass the presentation contract after rebuilding
and regenerating the procedural cartridge; frames 1/30/60 remain byte-identical
to the original baseline. The game's separate directory without a ROM also
passes **11/11** tests against `cb3fe7e`, including the synthetic renderer and
tile animations. This is preflight evidence, not phase 5's full acceptance.

Reproduce with the private scripts/logs retained in their respective worktrees:

```sh
# In gb-recompiled-windows:
bash logs/run-windows-network-sync.sh
bash logs/run-windows-gles-sync.sh
# In pokeyellow:
bash build/qa/logs/run-api-phase5-preflight-no-rom.sh
```

The game's committed runtime pin remains `presentation-api-v1`. The Windows
job will be reactivated against a tested immutable fork revision once the
complete runtime compiles. The upstream CI criterion and phase 5 remain open;
the draft's current empty check list is not a successful CI result.


### Phase 5: Windows consumer integration in progress, 2026-09-21

The first full Windows runtime run reached the executable link and identified
two remaining POSIX string functions: `strcasecmp` and `strtok_r`. Fork commit
`6f88e30` maps them to the Windows CRT through private C-compatible helpers;
independent tokenizer streams and case comparisons are covered by the host
suite. The local **4/4** host suite, presentation contract and regenerated
procedural cartridge pass; frames 1/30/60 still match the original baseline.
Evidence and reproduction: `gb-recompiled-windows/logs/windows-strings-*` and
`bash logs/run-windows-strings-sync.sh`.

This game branch now pins that immutable candidate revision and enables the
Windows job with the same pinned vcpkg ports as the runtime. It requires a real
Windows/ANGLE synthetic-renderer result instead of accepting an SDL dummy-driver
skip. SDL main handling covers both launcher auto-start and test executables;
MSVC receives its own optimization options without editing generated C. Owned
sources retain `/W4 /WX`; upstream headers are classified as external, and
standard C stdio calls retain their portable spellings.

The complete Linux consumer build and its **11/11** ROM-free tests pass locally
(`api-phase5-consumer-{build,build-refresh,tests}.log`). Windows CI and the full
ROM-backed acceptance gate still need completion. The earlier runtime pin and
pending-CI notes above describe the preceding checkpoints, not a final acceptance.

### Phase 5: native runtime validation, 2026-09-21

Fork revision `00cc26dafb9a41ea9d935508e9fbe9e25b5f5a6e` passes all four
jobs at https://github.com/dobbygl/gb-recompiled/actions/runs/35583610428:
Linux and Windows/MSVC host portability, Linux/Mesa presentation, and the
complete Windows SDL/ANGLE runtime with the presentation contract. The upstream
[PR #2](https://github.com/GB-Recomp/gb-recompiled/pull/2) is now ready for review
and includes these results. Its own repository still reports no CI checks;
the passing fork run does not satisfy that separate acceptance criterion.

The consumer's initial GCC run found an optimized libstdc++ directory-iterator
link failure in exception cleanup. The wrapper now records terminal iteration
without assigning another iterator and releases resources on close. Host CI
uses MinSizeRel to cover that consumer configuration. Windows exposed a test
baseline taken before ANGLE completed an SDL window resize; the contract now
warms the resized surface and requires consecutive ordinary frames to match
before comparing exact same-frame fallback pixels. No pixel tolerance was added.

Local verification includes the 4/4 optimized host tests, the presentation
contract, regeneration/rebuild of the procedural cartridge and exact original
frames 1/30/60 (`bash logs/run-windows-resize-sync.sh` in the runtime worktree).
The game pins this immutable revision. Its full 28/28 CTest preflight passed;
the complete 18-suite ROM-backed regression is running through
`bash build/qa/logs/run-api-phase5-final-regressions.sh`. The game's first native
Windows consumer CI is still building its dependencies in
https://github.com/dobbygl/pokeyellow3d/actions/runs/35582586562 .

The first Windows consumer build completed its dependencies and then rejected
implicit narrowing and shadowed local names under `/W4 /WX`. Commit `6520b1c`
makes the existing byte/address/float conversions explicit and names the local
render buffers distinctly; warning severity remains unchanged. Its local
28/28 CTest passes. GCC, Clang, Linux 2D and format pass in
https://github.com/dobbygl/pokeyellow3d/actions/runs/35584341509 ; the native
Windows build is still running at this checkpoint. The earlier full regression
was stopped with exit 143 to apply these diagnostics; its `api-phase5-pre-msvc-*`
logs are retained but are not final acceptance. The full 18-suite driver will
run again only after this CI succeeds.


### Phase 5: Windows consumer CI passes, 2026-09-21

Game commit `0162956b1443206788d079adf6b7aa21e3425b5d` passes all enabled
jobs at https://github.com/dobbygl/pokeyellow3d/actions/runs/35585877841 :
GCC, Clang, Linux 2D, format and native Windows/MSVC. Windows builds the complete
application and runs **11/11** tests without a ROM and without skipped tests.
The downloaded JUnit artifact verifies SDL `driver=windows` with ANGLE using
Direct3D11's Microsoft Basic Render Driver. The renderer exercises 30 frames
across three synthetic maps plus a live overworld, reports no GL error, and
checks unchanged guest memory and a non-empty surface. Private evidence is in
`build/qa/logs/api-phase5-code-ci.json` and `api-phase5-windows-diagnostics/`.

The two rounds of strict MSVC diagnostics were fixed in the fixtures with
explicit byte/address conversions and non-shadowing local names; no assertion
or warning level was removed. The independent local build without a ROM also
passes 11/11; full local CTest passes 28/28. The earlier `api-phase5-pre-msvc-*`
and `api-phase5-pre-tests-*` partial runs are not accepted as the full regression.
The 18-suite driver now runs after successful native CI on this exact code;
`api-phase5-source-snapshot.json` records hashes of the 113 game source/test/build
files and 91 runtime files checked again by `check-api-phase5-regressions.py`.

The upstream CI gate is externally pending. The original repository reports
zero pull-request workflow runs and an empty check list on the ready PR; this
account has `pull` permission but no `push`, `maintain` or `admin` permission.
The PR body asks a maintainer to inspect whether its workflow needs enabling
or approval. The exact policy is not visible, so no specific approval cause is
assumed. `api-phase5-upstream-ci-status.json` records the API evidence. Its
checkbox stays unchecked until the original repository produces a passing run;
the green fork and consumer runs do not substitute for that requirement.


### Phase 5: full regression and external CI blocker — 2026-09-21

[Game PR #11](https://github.com/dobbygl/pokeyellow3d/pull/11) validates the
portable fork at `00cc26d`. Tested game code: `0162956`; all five enabled
CI jobs pass in [35585877841](https://github.com/dobbygl/pokeyellow3d/actions/runs/35585877841).
The final documentation commit must also pass CI before this PR is merged.

- **28/28** full CTest and **11/11** CTest in the independent directory without
  a ROM, with no skips; clang-format **18.1.8** passes.
- **All 18 `tests/*_qa.sh` suites pass**, exit 0. Every presentation frame retains
  its guards against WRAM, VRAM, cartridge RAM and framebuffer changes.
- **All 38 fixed exterior references are byte-identical**, with no exception.
  The broader comparison covers **1,895** PPMs against the completed B2 run:
  **1,863** are identical, including all 215 interiors, 192 tile-animation
  captures, 456 battle-UI captures and all PC/Pokédex settled captures.
- The other **32** captures were reviewed beside their references and amplified
  pixel differences. Nineteen boot/title images, eleven crossfade middle/paused
  images and the surf-entry image sample different opacities of the existing
  SDL wall-clock blend. One F2-to-original image differs only in native water
  and flower phase: its helper steps the guest while waiting for that blend.
  The 28,441 recorded boot/title trace rows have no differences in their guest
  fields. `qa_presentation_clock.h` still fixes ImGui DeltaTime, not SDL ticks;
  neither that adapter nor the title/blend logic changed. No fixed-reference
  comparison was relaxed and no new presentation difference was designed.
- Windows builds the executable and runs all eleven ROM-independent tests
  through native SDL/ANGLE. Cartridge-backed journeys above were run on Linux;
  this is not a claim of a full Windows story playthrough.

Reproduction from this checkout with the existing private fixtures:

```sh
bash build/qa/logs/run-api-phase5-final-regressions.sh
python3 build/qa/logs/check-api-phase5-regressions.py
```

The driver records each command/output, verifies the complete suite inventory,
rebuilds the main binary and independent no-ROM targets, checks formatting,
runs full CTest before/after the journeys and enforces all 38 fixed references.
`api-phase5-regression-evidence.json` records the exact source/runtime revisions,
CI, directories, comparisons, reviewed images and executable SHA-256.
`api-phase5-source-snapshot.json` proves that the 113 game and 91 runtime source
files did not change during the run. All reports and images are private under
`build/qa/logs/`; no ROM, state, save or new game capture is committed.

Private evidence directories:

- `battles`: `build/qa/battles-EPd3zb/`.
- `boot`: `build/qa/boot-XFZbLF/`.
- `dex_area`: `build/qa/dex-area-mY4F52/`.
- `dex_list`: `build/qa/dex-list-qbqdC3/`.
- `dex_portraits`: `build/qa/dex-portraits-vuqvzU/`.
- `firstperson`: `build/qa/firstperson-LDZP0b/`.
- `interiors`: `build/qa/interiors-CSJykj/`.
- `pc_details`: `build/qa/pc-details-oFqQzQ/`.
- `pc_focus`: `build/qa/pc-focus-msh4EE/`.
- `pc_storage`: `build/qa/pc-storage-NhgMrS/`.
- `tile_animation`: `build/qa/tile-animation-BOegiR/`.
- `title`: `build/qa/title-313y9J/`.
- `ui_battles`: `build/qa/ui-battles-5XYHR5/`.
- `ui_crossfade`: `build/qa/ui-crossfade-MqT8B1/`.
- `ui_menus`: `build/qa/ui-menus-VTCUDK/`.
- `ui_special_transitions`: `build/qa/ui-special-transitions-6qaxmb/`.
- `ui_transitions`: `build/qa/ui-transitions-blxH46/`.
- `world`: `build/qa/kanto-Eqy7Ry/`.

**External blocker:** the ready [upstream PR #2](https://github.com/GB-Recomp/gb-recompiled/pull/2)
has no reported checks or PR workflow runs, and this account cannot manage
Actions in the original repository. A maintainer has been asked in the PR body
to inspect whether enabling/approval is needed; the exact policy is unknown.
The acceptance checkbox therefore remains open. The user's integrated objective
explicitly says to record a blocked point and continue with the next one: after
merging this tested consumer PR, proceed to improvements 5B while retaining this
external CI requirement. The overall goal cannot be marked complete until the
original CI passes as well as all subsequent requirements.
