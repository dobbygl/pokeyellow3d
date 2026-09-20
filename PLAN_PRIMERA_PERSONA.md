# Plan: first-person mode on top of the 3D prototype

Date: 2026-09-19. Status: phases 1-3 complete and verified with the local ROM.

## Goal and scope

Add a first-person camera to the `build/pokeyellow3d` executable as an
alternative presentation mode to the current orthographic camera. The
recompiled engine remains the sole authority over movement, collisions,
turns, ledge jumps, encounters, dialogues, battles, and saving. The mode
does not write to the machine's memory: the only way to influence the game
is the runtime's virtual controller.

Out of scope for this plan: free movement between tiles, 3D interiors, 3D
battles, changes to game logic, and any edit to the downloaded runtime or
the generated C. Coexistence with `PLAN_KANTO_3D.md` is preserved: this plan
does not depend on its phases 3 and 4 being finished, but it does depend on
phase 2 being closed so as not to edit `src/pallet3d.cpp` in parallel.

## Verified starting point

- `src/pallet3d.cpp` draws static meshes per map with depth testing and
  camera-facing actor sprites. The vertex shader is orthographic: rotation
  via `camera`, focus via `focus`, depth `-toward/256`, no perspective
  division.
- `src/pallet_state.h` interpolates the player's position across the eight
  steps of `wWalkCounter` (0xCFC4) and exposes `view()` to decide between 3D
  and 2D.
- The player's facing direction is in
  `wSpritePlayerStateData1FacingDirection` (0xC109): 0 down, 4 up, 8 left,
  12 right. The same positions exist for each NPC in its 16-byte block.
- The SDL frontend combines `g_manual_joypad_*` with `g_script_joypad_*` in
  `update_effective_joypad_state()` and hides the whole controller while the
  Esc menu is open. `gb_platform_set_input_script` already injects presses
  per frame.
- `cmake/Pallet3D.cmake` generates an adapted copy of `platform_sdl.cpp`
  with textual insertion points that fail if the runtime changes.
- `pallet3d_event` receives every `SDL_Event` before the controller and can
  consume it by returning `true`. It only acts when 3D is being presented.
- World scale: one tile is one unit; a sprite is about 1.9 tall; a tree
  canopy up to 1.68; a house eave between 1.5 and 1.8.
- In Pokémon Yellow, a short directional press turns the player without
  moving; holding it starts the step. This is the mechanism this plan uses
  to turn the camera without simulating actual movement.
- The `pallet_render_smoke` tests check, every frame, for OpenGL errors, the
  actually presented mode, and that drawing does not modify WRAM.
  `tests/world_journey.h` drives the engine with normal inputs.

## Architecture decisions

1. A single draw pass and a single shader for both cameras. Replace the
   vertex shader's manual rotation with a `mat4` view-projection matrix. The
   orthographic camera is expressed with the same matrix; this keeps the
   change in `pallet3d.cpp` small and avoids branching the geometry.
2. The new code lives in `src/firstperson.h`: camera matrix computation,
   turn interpolation, and the control mapper. `pallet3d.cpp` only queries
   the active mode and requests the matrix.
3. The camera's orientation is the engine's orientation, never the other
   way around. The camera interpolates toward the value read at 0xC109; no
   separate yaw is stored that could diverge from who is being addressed
   with the A button.
4. Player-relative, tank-style controls. W holds the direction the player
   is facing. A and D inject a short tap of the perpendicular direction to
   turn without advancing. S injects the opposite tap to turn around. The
   arrow keys keep their original absolute function at all times.
5. The injection enters through its own mask combined with `&` in
   `update_effective_joypad_state()`, via a new `pallet_hook`.
   `g_script_joypad_*` is not reused, so as not to interfere with test
   scripts or input recording.
6. Outside the `View::Overworld` state, the mapper injects nothing and
   consumes no keys. Menus, dialogues, interiors, and battle behave exactly
   as they do today.
7. The mouse does not turn the player. At most it offers a cosmetic look of
   a few degrees that returns to center on release, without altering the
   interaction orientation.
8. The player's sprite and its golden outline are not drawn in first
   person. Pikachu and the other actors are, as they are today.

## Phase 1: perspective camera

Work:

- Add the `mat4 view_projection` uniform to the shader and use it to build
  the current orthographic camera. Verify with existing captures that the
  view does not change.
- Add the perspective camera in `src/firstperson.h`: eye at the player's
  interpolated position plus a height of 1.1, a field of view of about 70
  degrees, near plane 0.1, and a far plane matching the resident maps.
- Interpolate yaw toward the engine's orientation with a constant of about
  150 ms; 180-degree turns take the short way around.
- Hide actor 0 and the x-ray pass when the mode is active.
- Reorient actor billboards to the camera's real position, anchored to the
  ground, instead of the orthographic camera's fixed tilt.
- Sky with a gradient and distance fog in the fragment shader; both
  disabled in the orthographic camera.
- The F3 key toggles first person. F2 still toggles 3D and 2D. The HUD
  shows the mode and the keys.

Acceptance criteria:

- [x] The orthographic camera produces captures equivalent to those before the shader change.
- [x] In first person the camera advances with the player's step, with no jumps or rollbacks.
- [x] A player turn in 2D is reflected as a smooth camera turn.
- [x] The player does not appear in the image; Pikachu and the NPCs do, standing on the ground.
- [x] Zero OpenGL errors and WRAM intact on every frame, measured by `pallet_render_smoke`.

## Phase 2: player-relative controls

Work:

- New `pallet_hook` that inserts a custom d-pad mask into
  `update_effective_joypad_state()`, with a setter accessible from
  `firstperson.h`. The insertion must fail explicitly if the integration
  point changes.
- Mapper: W holds the direction at 0xC109. A and D compute the
  perpendicular and emit a tap of the minimum duration that turns without
  starting a step; measure that duration with the engine and fix it as a
  documented constant. S emits the opposite tap. While a tap is in
  progress, new turn commands are ignored.
- Consume W, A, S, and D in `pallet3d_event` only when the mode is active
  and `view()` returns `Overworld`. In any other state the keys follow
  their usual path and the mask returns to 0xFF.
- Respect `g_show_menu`: nothing is injected while the Esc menu is open.
- The R key recenters the cosmetic look, if implemented.

Acceptance criteria:

- [x] With W the player walks in the direction they are facing; releasing it stops them at the next tile.
- [x] A and D turn 90 degrees without moving the player, even under rapid repetition.
- [x] S turns the player around without moving them.
- [x] Z talks to the NPC or sign occupying the center of the view.
- [x] When a dialogue, menu, or battle opens, the mask is 0xFF and WASD return to the original controller.
- [x] A `gb_platform_set_input_script` script produces the same route with and without first person active.

## Phase 3: journey, transitions, and polish

Work:

- Test journey in first person over `tests/world_journey.h`: leave Pallet
  Town, enter Route 1, cross a map boundary, jump a ledge, trigger an
  encounter, battle in 2D, and return to 3D in first person.
- When returning from a battle or an interior, restore the camera with the
  engine's orientation without animating from the old value.
- Overlay the game's text box on the 3D view in `View::Dialogue` when the
  map stays the same: copy the original framebuffer's bottom rows as a
  texture. If the box cannot be detected reliably, keep the full fallback
  to 2D and log the case.
- Add tiled side faces to houses and cover the gaps visible from inside the
  map. Review trees and rocks up close; keep the visible pixel style but
  avoid untextured faces.
- Small vertical camera sway on the ledge jump, derived from the step
  counter, without touching the ground height.
- Measure frame rate with the five resident maps visible from the ground
  and add distance-based mesh culling if needed.

Acceptance criteria:

- [x] The full journey passes in `pallet_render_smoke` with a `firstperson` argument.
- [x] Map changes, entering interiors, and returning from battle produce no frames with an incorrect camera.
- [x] Dialogues are readable without leaving the 3D view, or the case is documented as a fallback to 2D.
- [x] Reviewed captures of Pallet Town and Route 1 from the ground, in all four orientations.
- [x] No performance measurement is worse than the orthographic camera.

## Accepted limitations

- Movement is tile-based and the camera can only look in four interaction
  directions. This is not a free-movement game.
- NPCs outside the original Game Boy screen are drawn on their tile with
  orientation but no animation. From the ground, visibility extends farther
  than in the original view, so stationary actors will be noticeable at a
  distance.
- Ledges still have no real height; the jump is represented with camera
  sway, not a level change.
- Interiors and battles remain in 2D. Entering a door leaves the
  first-person view until returning outdoors.
- Building side faces and heights are visual interpretations, as in the
  rest of the prototype.

## Validation and delivery

- Run the full CTest suite and `pallet_render_smoke` in its current modes
  before and after each phase; orthographic captures must remain
  equivalent.
- Add unit tests for the mapper: orientation to d-pad, perpendiculars, tap
  duration, and the 0xFF mask outside the overworld.
- Use isolated copies of the ROM and save under `build/qa/`. Do not add
  ROM, graphics, or savestates to the repository.
- Deliver `build/pokeyellow3d` with F3 working, `PALLET3D.md` updated with
  the key table and limitations, and captures in `build/qa/logs/`.
- Only check off this plan's boxes with recorded evidence.

## Coordination with the Kanto plan

The four phases of `PLAN_KANTO_3D.md` were closed before implementing this
plan. First person preserves the 38 scenes, the 140 buildings, and the
five-map cache. No concurrent edits were made to the renderer.

## Technical references

- [Overworld loop and turn without a step](https://github.com/pret/pokeyellow/blob/master/home/overworld.asm).
- [Player sprite advance](https://github.com/pret/pokeyellow/blob/master/engine/overworld/advance_player_sprite.asm).
- [Sprite data in WRAM](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).
- [Ledge logic](https://github.com/pret/pokeyellow/blob/master/engine/overworld/ledges.asm).
- `build/_deps/gb_recompiled-src/runtime/src/platform_sdl.cpp`, function
  `update_effective_joypad_state`, the mask's insertion point.

## Implementation and verification log

### Camera, controls, and journey

- Implemented `src/firstperson.h`, the shared matrix in `pallet3d.cpp`, F3,
  vertical sprites without the player, sky, and fog. House side faces,
  leaves, and rocks reuse atlas tiles without increasing geometry or draw
  calls.
- The turn tap was measured with the engine
  (`build/qa/firstperson/logs/turn-probe-free.log`). Two effective frames,
  140,448 cycles, turn without starting a step; the script requires three
  frames because its first frame is the activation frame. The relative
  channel uses cycles, confirms the orientation, and waits for rest before
  emitting another tap.
- Directions cross-checked against `pokeyellow_internal.h`: orientation
  C109, step counter CFC4, movement D527, turn check CC4B, jump index D713,
  and movement flags D735. The renderer only reads this data.
- `tests/firstperson_test.cpp`: projections, equivalence with the previous
  matrix, smoothing and the short path, perpendiculars, duration, engine
  confirmation, W, and a neutral mask outside the overworld and after
  stepping cycles back.
- The SDL adaptation generates `build/pallet-runtime/platform_sdl.cpp`; it
  does not change the downloaded sources or the game's C. The hooks check
  their anchor. The relative mask is independent of the scripts and is
  folded into manual recording the same way real controller directions are.
- Final first-person suite: **PASS**, `build/qa/firstperson-CWqk6R/`, run
  via `tests/firstperson_qa.sh`. Its four CTest tests pass and the ROM and
  private fixture hashes match at the end.
- `logs/journey-fp.log` and `logs/journey-ortho.log`: 3,197 frames in each
  mode, 1,927 outdoors, 1,231 in battle, a real victory, and an identical
  final WRAM state `fe0b1e2988cb990d`. First person checks the interpolated
  position on every frame, 30 frames of jump sway, absence of the player,
  and correct orientation on the first frame after returning from battle.
- `logs/controls.log`: real SDL keyboard, A/D/S, 16 turns with different
  sampling phases, rapid repetition, W press and release, arrow keys, Esc,
  Start menu, F2, and Z facing the sign. `logs/relative-input.txt` contains
  the four directions converted from the relative control, with their
  durations in cycles.
- `logs/house.log`: entering the house in 2D, neutral mask, and restoring
  first person with orientation checked from the first outdoor frame.
- `logs/pallet-views.log` and `logs/route-views.log`: the four orientations
  without changing position. PNGs reviewed in
  `build/qa/logs/firstperson/`, including `eight-views.png`. Pikachu and
  the NPCs remain visible and grounded.
- `sign-dialogue.png`: Pallet Town's original text overlaid on 3D. The
  detector requires a complete bottom edge, the same map, and no text on
  the top part. Start, full screens, and other unrecognized boxes keep 2D;
  this limitation is documented in `PALLET3D.md`.
- Five resident maps in Saffron City: 204,360 vertices, 7,356,960 bytes of
  meshes. Seven rounds of 100 presentations synced with `glFinish` on Intel
  UHD 620, 800x720: median 3.639 ms FP and 3.377 ms orthographic in the
  renderer's final version (`logs/benchmark-*.log`). This is presentation
  time, not engine FPS. No distance-based culling is needed for that draw
  budget.

### Graphics and performance regression

- Previous baseline kept in `build/qa/firstperson-baseline/`. The first
  comparison of the 38 images, excluding the updated HUD, gave a maximum of
  0.021% of pixels with an RGB difference above 8; the minimal differences
  are consistent with matrix rounding. Initial report:
  `build/qa/firstperson/logs/ortho-images.json`.
- Full Kanto regression before the last shader adjustment: **PASS**,
  `build/qa/kanto-GdyDKx/`, via `tests/world_qa.sh`: journeys, menus,
  interiors, Cut and reload, surf, bike, forest, dock, and 38 meshes.
- The first performance comparison found orthographic overhead. Flat
  material resolution was moved to the vertex shader and the orthographic
  uniform path was separated from the FP effects, keeping a single
  program. The following closeout records the comparison and regression of
  the final version.

### Closeout: final version, regression, and performance

- **PASS** of the full CTest suite and of `tests/firstperson_qa.sh` in
  `build/qa/firstperson-CWqk6R/`. **PASS** of `tests/world_qa.sh` in
  `build/qa/kanto-e34e4y/`, with all previous regression modes. Summaries:
  `build/qa/firstperson/logs/firstperson-release.log` and
  `kanto-release.log`.
- Final optimizations: only transfer sprites when their pixels change, and
  skip the 2D upload/composite when the initialized 3D renderer is going to
  fully cover the outdoors. The engine's framebuffer and its counters keep
  being produced; F2, initialization, menus, transitions, and interiors
  keep the original path. `f2-original.png` and `house-2d.png` were
  reviewed, along with the eight cardinal views and the dialogue, in
  `build/qa/logs/firstperson/`.
- The 38 final orthographic captures are compared against the previous
  executable, excluding only the updated HUD strips. Maximum of
  **0.02099%** of pixels with an RGB difference greater than 8; the matrix
  also passes the algebraic equivalence test. Report:
  `build/qa/firstperson/logs/ortho-images-final.json`.
- Orthographic performance: four alternating previous/current pairs, 200
  synced frames for each of the 38 scenes, **60,800 presentations** in
  total. All samples from this batch are kept; vertex and geometry byte
  counts are identical across all maps and rounds. Global mean
  **2.104 -> 1.423 ms** (**32.35% less**). The medians of the 38 maps
  improve between **17.74% and 44.89%**. The 95% intervals of the paired
  per-map differences are negative; the least favorable upper bound is
  -0.213 ms.
- Logs and comparison: `build/qa/firstperson/logs/performance-release/`
  and its `comparison.json`. Earlier batches, affected by external load or
  by intermediate versions, are kept as history and are not mixed with
  this final measurement. Hardware: Intel UHD 620, 800x720 surface, SDL
  `offscreen`.
- To reproduce the measurements, from `build/qa/firstperson`, run
  `CATALOG_BENCH_FRAMES=200 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
  ./pallet_render_smoke ../roms/pokeyellow.gbc ../progress.state10 catalog`;
  swap the helper for `../firstperson-baseline/pallet_render_smoke` for the
  previous reference and alternate the order of each pair. That reference
  is a local artifact kept from before the change; it is not distributed
  with the code.
- Delivered `build/pokeyellow3d`, with F3 working; `PALLET3D.md` and
  `README.md` updated. Executable fingerprints in
  `build/qa/firstperson/logs/artifacts.sha256`. The tests use private
  copies, check their hashes, and do not invoke the battery save. The
  downloaded runtime sources and the generated game C were not modified.

Deliverable limits: tile-based controls and four orientations; 2D
interiors and battles; unrecognized text boxes also 2D; heights, side
faces, and the jump are visual interpretations. The camera does not
introduce a second physics system or allow passing through the game's
collisions.
