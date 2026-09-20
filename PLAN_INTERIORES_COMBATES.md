# Plan: 3D interiors and battles

Date: 2026-09-19. Status: A1, A2, B1, and B2 complete and verified on 2026-09-20.

## Goal and scope

Extend `build/pokeyellow3d` to the two areas that today always fall back to
2D: interiors entered through a warp, and battles. The recompiled engine
remains the sole authority over state, rules, text, menus, and saving. No
part of this plan writes to the machine's memory; the view is derived from
ROM, WRAM, VRAM, and the framebuffer the game has already painted.

The plan has two independent blocks. Interiors extend the scene catalog and
the geometry rules. Battles add a new scene that shares no world with the
maps. Either one can be delivered without the other. Both depend on
`PLAN_KANTO_3D.md` having closed its phase 2, because they touch
`src/pallet3d.cpp` and `src/pallet_state.h`. First person from
`PLAN_PRIMERA_PERSONA.md` is orthogonal: if it exists, interiors inherit it;
battles do not use it.

Out of scope: link-cable battles, the old man tutorial fight, the Safari
Zone as a distinct scene, 3D Pokémon models, text or menus redrawn with a
different typeface, and any change to the downloaded runtime or the
generated C.

## Verified starting point

- `src/kanto_rom.h` already reads the 25 tilesets and, for each map, its
  warps with destination and entry point. The current catalog only
  instantiates the 36 connected maps plus Viridian Forest and the dock; warp
  destinations are not loaded.
- `pallet::view()` requires the map to be in the catalog, its tileset to
  match the expected one, and `wIsInBattle` (0xD056) to be zero. That is why
  interiors and battles currently present the 2D view.
- Each tileset exposes its list of walkable tiles (`collisions`), its
  graphics, and its blockset. That is enough to tell floor from wall or
  furniture without inventing a second simulation.
- `wCurMapTileset` (0xD366) identifies the live tileset. The interior
  tilesets in Yellow are: Red's house 1 and 2, mart, dojo, Pokémon Center,
  gym, house, forest gate, museum, underground, gate, ship, graveyard,
  interior, cave, lobby, mansion, laboratory, club, facility, and beach
  house. Cave and underground are caves, not buildings; they are handled
  separately.
- The `route` loop in `pallet_render_smoke` already enters the player's
  house and checks the return to 2D. It is the first interiors fixture.
- Battle symbols available in `pokeyellow_internal.h`: `wIsInBattle`,
  `wBattleType` (0xD059), `wCurOpponent` (0xD058), `wTrainerClass`,
  `wEnemyMonSpecies` (0xCFE4), `wEnemyMonHP` (0xCFE5), `wEnemyMonLevel`,
  `wEnemyMonStatus`, `wEnemyMonNick`, `wBattleMonSpecies` (0xD013),
  `wBattleMonHP` (0xD014), `wBattleMonLevel`, `wBattleMonStatus`,
  `wBattleMonNick`, `wPlayerMoveNum`, `wEnemyMoveNum`, `wMoveMenuType`,
  `wCurrentMenuItem`, `wEnemyHPBarColor`, `wSubAnimTransform`, and
  `wLinkState`.
- Battle portraits do not go through OAM: the game decompresses the
  opponent's front image and the player's back image into VRAM's background
  tile area and paints them into `wTileMap` (0xC3A0, 20 by 18). The
  `sprite_image` decoder already reads VRAM tiles by index; the same
  mechanism serves here.
- `gb_get_framebuffer(ctx)` returns the current frame's LCD image. The tests
  already use it. It lets us crop the text box and menu rows to composite
  them over the 3D view without redrawing them.
- The movement animation identifier (`wAnimationID` in pret) is not exported
  in the internal header. Its address must be verified against `wram.asm`
  before relying on it.

## Architecture decisions

1. Interiors are scenes of the same `kanto::World` catalog, with their own
   `component` per map and a zero origin. They do not share a world with the
   outdoors; a warp change explicitly resets the camera.
2. Interior classification uses the tileset's collisions as its base: a
   walkable tile is floor; a non-walkable tile on the top rows or the
   perimeter is a wall; a non-walkable tile surrounded by floor is
   furniture. Warps from the header mark doors, stairs, and rugs, which are
   always flat. Any unclassified tile is drawn flat with its original
   texture and logged, as in the Kanto plan.
3. The battle scene is a separate module, `src/battle3d.h`, with its own
   camera and geometry. `pallet3d.cpp` hands it the frame when `view()`
   returns a new `Battle` state. It is not mixed with map meshes.
4. Battle portraits are extracted from VRAM following `wTileMap`, not from
   compressed ROM. This way the image always matches what the engine
   decided to show, including substitutions, Pokémon changes, and
   transformations.
5. Text, fight menus, move boxes, and party lists are composited from the
   original framebuffer as a texture over the 3D view. No menu is
   reimplemented.
6. Name, level, health bar, and status markers are drawn with ImGui reading
   WRAM, with the same information the game shows. The health bar
   interpolates between reads so the drop is continuous.
7. Any uncovered state falls back to the full 2D view. That is correct
   behavior, not a bug: link battles, tutorial, Safari Zone, unrecognized
   transformations, or a tilemap without a portrait.
8. No new module writes to WRAM, VRAM, or the framebuffer. The tests keep
   checking this on every frame.

## Block A: interiors

### Phase A1: interior catalog and first building

Work:

- Extend `kanto::World` to instantiate, on demand, the warp destinations of
  already-cataloged maps, with `component` equal to the map's own id and a
  zero origin. Resolve the special "last map" destination without loading
  anything.
- Read each interior's title from its map constant's name; until then, keep
  "MAP n".
- Add scene selection for interiors to `pallet_state.h`: map in catalog,
  matching tileset, matching dimensions, and LCD and sprite state equal to
  that used outdoors.
- Interior geometry rule in `create_map`: walls with the original tile's
  texture on the top two rows and on the perimeter, fixed height of two
  units; furniture as boxes with the tile's texture on the top face and a
  derived color on the sides; flat floor with its tile. Doors, stairs, and
  rugs flat, following the warps.
- Orthographic camera framing the whole room, with no free rotation in
  small rooms, and an explicit reset on entering or leaving.
- An interior palette distinct from the outdoor one in the atlas, chosen
  per tileset.
- First milestone: the player's house, ground floor and upper floor, and
  Professor Oak's laboratory.

Acceptance criteria:

- [x] Entering the player's house from Pallet Town shows the room in 3D; going up the stairs changes scene; leaving returns the outdoors with its camera.
- [x] The room's NPCs stand on the floor and conversations work with the current fallback to 2D.
- [x] The PC, the TV, the table, and the bed have volume; no piece of furniture permanently hides the player.
- [x] `route` still passes and adds a check that the interior is presented in 3D.
- [x] Zero OpenGL errors and WRAM intact on every frame.

### Phase A2: coverage of the buildings in the 36 maps

Work:

- Walk every warp of the 36 outdoor maps and of the interiors reached from
  them, with a depth limit, and audit: dimensions, tileset, unclassified
  tiles, inferred furniture, and walls. CSV report under `build/qa/logs/`,
  as in the Kanto plan.
- Per-tileset rules for mart, Pokémon Center, gym, house, laboratory, gate,
  mansion, lobby, club, facility, museum, ship, and graveyard. Counters and
  shelves are tall furniture; healing tables and gym doors are low.
- Multi-floor interiors and large buildings: Pokémon Tower, Silph Co.,
  Celadon Department Store, and the ship. Camera with limited rotation and
  zoom in wide rooms.
- Caves and the underground: dark ceiling with fog, rock walls by collision,
  interior water. These are accepted as scenes of this block but do not
  block delivery of the rest.
- Reuse the live WRAM block substitutions for doors that open, elevators,
  and walls that move by script.

Acceptance criteria:

- [x] The audit lists every reachable interior and none fails to decode.
- [x] Mart, Pokémon Center, and gym in Viridian City reviewed with captures.
- [x] A journey healing at the Pokémon Center, shopping at the mart, and returning to Route 1 passes in `pallet_render_smoke`.
- [x] Changing floors, using an elevator, and entering a cave leave no frames with the wrong scene.
- [x] The mesh cache remains bounded to the current map and its neighbors.

## Block B: battles

### Phase B1: static scene with portraits and markers

Work:

- Add `View::Battle` to `pallet_state.h`: `wIsInBattle` nonzero, `wLinkState`
  zero, `wBattleType` other than the tutorial, LCD active, and the
  opponent's portrait present in `wTileMap`. Until the portrait appears, the
  original 2D transition is kept.
- `src/battle3d.h`: arena with a floor derived from the terrain where the
  battle started, read from the tile of the player's square on the previous
  map: grass, dirt, water, cave, or gym interior. Two platforms, one distant
  for the opponent and one near for the player.
- Decode the opponent's front portrait and the player's back portrait from
  VRAM following the rectangles the engine paints in `wTileMap`. Verify
  those rectangles in `engine/battle/core.asm` and document them as
  constants. Color with the ROM's per-species palette when available;
  grayscale from the game otherwise.
- Billboards for both portraits facing the camera; camera behind and above
  the player looking at the opponent, with a slow idle sway.
- ImGui markers: nickname, level, health bar with the color the game
  chooses, status, experience bar, and party balls in trainer battles, all
  read from WRAM.
- Composite the LCD's bottom six rows from the framebuffer as a texture at
  the foot of the screen. When the game opens the party list, the bag, or a
  full-screen menu, composite the whole screen.
- Pokémon switch and fainting: when the species changes or HP reaches zero,
  the billboard fades out and the new one appears; the silhouette is not
  invented.

Acceptance criteria:

- [x] A wild encounter on Route 1 is presented in 3D with both portraits correct and their markers matching WRAM values.
- [x] The fight menu, move selection, bag, party list, and run are operable and legible.
- [x] Switching Pokémon, an opponent fainting, and the end of the battle leave no frames with the wrong portrait.
- [x] The `journey` run wins its battle with the 3D scene active and returns to Route 1 in 3D.
- [x] Zero OpenGL errors and WRAM, VRAM, and framebuffer intact on every frame.

### Phase B2: animations and feedback

Work:

- Verify the address of `wAnimationID` and of the sub-animation counters in
  `wram.asm`, and detect the start and end of each movement animation.
- First delivery: during an animation, composite the full original screen
  over the 3D view, with a fade-in and fade-out. This is faithful, covers
  all 165 moves and the Poké Ball captures, and requires no classification.
- Second delivery: 3D effects by category, derived from the move's type
  read from the ROM's `Moves` table and from whether the target is the
  opponent or the user: physical hit with a billboard lunge, projectile
  with a trail, status with a flash over the target, self-move with a
  glow. Unclassified moves keep the first delivery.
- Damage feedback: billboard shake and flicker when HP drops; the bar
  animates at the game's pace.
- Wild Pokémon capture: ball trajectory and shakes following the text and
  the engine's state, with the first delivery as a fallback.
- Trainer battles: the trainer's portrait from VRAM during the intro, with
  the class read from `wTrainerClass`.

Acceptance criteria:

- [x] Every movement animation displays fully, either composited 2D or 3D, with no black frames or duplicated portraits.
- [x] At least the four basic categories have a 3D effect and reviewed captures show each one.
- [x] A wild Pokémon capture and a trainer battle pass in `pallet_render_smoke` with the scene active.
- [x] No effect depends on writing machine state.

## Accepted limitations

- Pokémon are flat portraits from the game facing the camera, not models.
  This is consistent with the prototype's actors and with the original
  resolution.
- Interiors have interpreted heights and furniture; tiles without a rule
  are shown flat until classified.
- Text and menus are the enlarged original image. They keep their typeface
  and their typing pace.
- Link battles, the old man tutorial, and the Safari Zone remain in 2D.
- Caves are accepted as interior scenes with minimal rules; their artistic
  fidelity does not block delivery.

## Validation and delivery

- Run CTest and the current `pallet_render_smoke` modes before and after
  each phase. Outdoor captures must remain equivalent.
- New `pallet_render_smoke` modes: `interior` for the house and the
  laboratory, `town` for the Viridian City route, `battle3d` for
  encounter, capture, and trainer. Each mode checks the presented mode, GL,
  and memory intact per frame, and leaves captures in `build/qa/logs/`.
- Unit tests for the interior classifier, the `Battle` selection, and
  reading portrait rectangles with local savestates.
- Private fixtures under `build/qa/`: a savestate in the player's house, in
  Viridian City's Pokémon Center, at the start of an encounter, and facing
  a Route 22 trainer. Not added to the repository.
- Deliver `build/pokeyellow3d`, `PALLET3D.md` updated with the scope of
  interiors and battles, the interiors CSV report, and captures.
- Only check off boxes with recorded evidence.

## Recommended order

1. Phase A1, because it reuses the existing reader and meshes and has a fixture.
2. Phase B1, because it unlocks the most visible part with bounded risk.
3. Phase A2 and phase B2 in parallel if two work streams are available; otherwise A2 first, because its audit also serves the Kanto plan.

## Technical references

- [Tileset and collision headers](https://github.com/pret/pokeyellow/blob/master/data/tilesets/tileset_headers.asm).
- [Tileset constants](https://github.com/pret/pokeyellow/blob/master/constants/tileset_constants.asm).
- [Map warps and objects](https://github.com/pret/pokeyellow/blob/master/macros/scripts/maps.asm).
- [Battle core and portrait drawing](https://github.com/pret/pokeyellow/blob/master/engine/battle/core.asm).
- [Move animations](https://github.com/pret/pokeyellow/blob/master/engine/battle/animations.asm).
- [Per-species palettes](https://github.com/pret/pokeyellow/blob/master/data/pokemon/palettes.asm).
- [Game memory](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).

## Execution log

### A1: player's house and laboratory, 2026-09-20

- On-demand catalog, constant-derived titles, isolated scenes, and a
  per-tileset atlas. `LAST_MAP` and the Silph elevator's dynamic
  destination are not instantiated.
- `interior_test`: classifier, flat doors/stairs, furniture on both floors,
  view selection, and resources available in the runtime ROM. As
  preparation for A2, the recursive walk finds 221 maps and 25 tilesets;
  its artistic audit and A2's full integration are still missing.
- `interior` and `interior-fp`: a run through both floors, the conversation
  with the mother, the return to Pallet Town, Professor Oak's laboratory,
  and exit. Real engine movement, without modifying the save; GL and
  WRAM/VRAM/framebuffer checked on every frame. Evidence in
  `build/qa/interiors/logs/interior-a1*.log` and captures
  `reds-house-*.ppm`, `mother-dialogue*.ppm`, `oaks-lab*.ppm`.
- CTest 5/5: `build/qa/interiors/logs/a1-ctest.log`.
- Full Kanto regression: `build/qa/kanto-02mrNg/`; first person:
  `build/qa/firstperson-VRYzJq/`. Both PASS. The 38 outdoor captures are
  byte-for-byte identical to the reference before A1, `kanto-Jth3Ld`;
  verification in `build/qa/interiors/logs/a1-exterior-comparison.txt`.
- Executable `build/pokeyellow3d` compiled. These results close A1; they do
  not certify the criteria for A2, B1, or B2.

### B1: first encounter and menus, 2026-09-20 (incomplete phase)

- `src/battle_state.h` verifies the normal battle, the HUD, and the
  portrait rectangles. `init_battle.asm` confirms seven-tile columns,
  opponent at (12,0), player at (1,5). The LCD window normally uses 9C00
  with WX=7/WY=0; checking only the 9800 background map would fail to
  validate the portraits.
- `src/battle3d.h` presents the arena, platforms, its own camera, VRAM
  portraits with the ROM's CGB palettes, names, level, status, HP, and
  experience. It keeps the original image during long menus and
  animations. Fixed the transparency of the white torso in back portraits
  cropped at the bottom.
- `battle_state_test` verifies selection, rejection of link/tutorial/Safari
  battles, marker reading, tile order, transparency, palettes, experience,
  availability in the manifest, and that `wAnimationID` alone does not
  indicate an animation. The return of `PlayMoveAnimation` on the active
  stack is verified; the counters and the ID are aliased and persist
  outside the animation.
- `build/qa/interiors/logs/battle-probe-3d.log`: a full real encounter, 913
  frames with the battle scene, 529 with the arena visible; GL and all
  WRAM/VRAM/framebuffer regions intact on every frame. Reviewed capture:
  `build/qa/interiors/logs/battle-arena.ppm`.
- `build/qa/interiors/logs/b1-menus.log`: fight, moves, bag, party, run,
  and return to Route 1, using the original buttons. Captures and private
  states `battle-{fight,moves,bag,party,escaped}.*` in the same folder.
- `build/qa/interiors/logs/b1-journey.log`: a real victory, 446 frames with
  the battle scene active, and a return to the outdoors, with no renderer
  changes in memory.
- **Still needed to close B1:** validate Pokémon switches, the player
  fainting, and opponent replacement without stale portraits. B2 remains
  pending: the original fallback is not equivalent to having the four 3D
  effects, capture, or the trainer intro.
- Regressions for this version: CTest 6/6 and the full Kanto suite in
  `build/qa/kanto-N2q3I6/`; full first person in
  `build/qa/firstperson-OIbyVs/`, including exact final-state parity
  between both cameras and SDL controls. The 38 outdoor captures remain
  identical to the reference before A1; result in
  `build/qa/interiors/logs/b1-exterior-comparison.txt`.
- Fixed a helper expectation: an overlaid dialogue keeps the first-person
  camera alive, while the battle scene uses a different camera. The
  orientation check on return distinguishes both cases.

### A2: catalog, Viridian City, and transitions, 2026-09-20

- `tests/interiors_qa.sh` reproduces A1 and A2 with private copies of the
  ROM and states. Full run PASS in `build/qa/interiors-fJDbZk/`, log
  `build/qa/interiors/logs/a2-suite.log`; CTest 7/7. Input hashes are
  verified at the end and the battery save is never invoked.
- `interior_audit`: 221 maps reached, 179 interiors, and 25 tilesets, with
  a depth limit of 32. CSV in `build/qa/logs/interiors.csv` and in the
  batch's folder. Zero out-of-range graphics or raised warps. **2,960
  tiles with no artistic classification across 104 interiors** are logged:
  they keep their flat texture. Full decoding is not conflated with
  complete art.
- Per-tileset-family rules for shelves, counters, tables, plants, healing
  equipment, gym doors, walls, and rock. Perimeter furniture is classified
  before applying the generic wall. Unrecognized cases no longer
  automatically receive a furniture box.
- `town`: healing from 18 to 21 HP, first visit to the mart, delivering the
  package and getting the Pokédex, buying ten Poké Balls, a second visit to
  the center, and returning to Route 1. All via original movement and
  buttons; ends at `logs/town-route1.state` with ten balls. The Pokédex
  dialogue is awaited until the script returns control, with a safety
  limit.
- `interior-transitions`: 1F-2F-1F stairs at the department store,
  rotation/zoom in a large room, entering the elevator, real selection of
  2F, and exiting to the chosen floor. Diglett's Cave: entry, switching to
  the main cave, and return. Setting up these fixtures requests a warp from
  the engine; the evaluated crossings and the elevator menu use normal
  controls.
- Reviewed captures of the center, mart, and gym in Viridian City, the
  floors, and the elevator menu. Contact sheets in
  `build/qa/interiors/logs/a2-{viridian,elevator}-review.png`. Additional
  capture looking down the cave corridor:
  `build/qa/interiors/logs/diglett-cave-fp.png`, with dark ceiling and fog.
- `interior-catalog` builds the 179 meshes with the same renderer, checks
  cache eviction down to one resident mesh, and GL and all WRAM, VRAM, and
  framebuffer regions intact. The journeys check the same per-frame
  invariants, including dialogues and transitions.
- Interiors reuse the current scene's live-block reading and invalidation.
  The Cut/reload regression keeps checking that shared mechanism; it is not
  claimed that every door in Silph Co. or every script has been triggered.
- Full Kanto regression PASS in `build/qa/kanto-0dH5RK/`; first person PASS
  in `build/qa/firstperson-9cB3FJ/`, including engine state parity. The 38
  outdoor captures are byte-for-byte identical to `kanto-Jth3Ld`, per
  `build/qa/interiors/logs/a2-exterior-comparison.txt`.
- `build/pokeyellow3d` compiled. A2 is closed with the artistic limitations
  above. The full goal still awaits B1 and B2.

### B1/B2 preparation: real capture, 2026-09-20

- New test mode `battle-capture`, starting from `town-route1.state`. A wild
  encounter by normal movement, opening the bag, and throwing the
  purchased Poké Balls. A level 4 Pidgey is captured after four attempts
  and the party goes from one to two members, without granting a Pokémon
  or modifying inventory or RNG from the helper.
- PASS evidence: `build/qa/interiors/logs/b2-capture-probe.log`, with 1,573
  frames of the battle scene and memory/GL checked per frame. Visual
  review in `build/qa/interiors/logs/b2-capture-review.png`. Resulting
  private party state: `build/qa/interiors/logs/capture-complete.state`.
- The capture animation is still the original 2D one. This test prepares
  the validation of party changes and does not yet certify the 3D effects
  or the trainer battle required by B2.

### B1 and B2: switches, trainer, capture, and effects, 2026-09-20

Reproducible batch:

```sh
tests/battles_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/logs/town-route1.state \
  build/qa/firstperson-9cB3FJ/route.state
```

Result **PASS** in `build/qa/battles-xtPavN/`; summary in
`build/qa/logs/b2-battles-final.log`. This run resolves the outstanding
items from B1 and B2 in the earlier historical entries.

- `battle3d` chains a real Rattata capture after two throws, with 891
  frames of the scene and 537 of the Poké Ball; Pikachu/reserve swaps with
  555 exact checks of the portraits uploaded to the GPU against VRAM;
  healing; approaching and battling the Route 22 rival. It does not depend
  on capturing a specific species and does not grant Pokémon in these
  tests.
- The rival shows their trainer portrait, sends out Spearow and Eevee, and
  the replacement is verified. Pikachu faints, the reserve is chosen via
  the original menu, and the player wins. There are 3,448 frames of arena
  checked, and the following dialogue is also finished before certifying
  90 frames of stable 3D world. `trainer-result.ppm` shows that return.
- The earlier test `build/qa/interiors/logs/b1-trainer.log` also covered
  both party members fainting: return to Viridian City and original
  healing of the party (26/17 HP). The helper distinguishes that teleport
  from an edge connection and checks the healing destination.
- Four effects derived from the ROM's `Moves` table: physical hit,
  projectile with trail, status on the opponent, and glow on self. In the
  integrated test, 32, 136, 90, and 24 frames are observed respectively;
  the two damaging attacks also show 24 frames of shake/flicker with the
  bar interpolated within the engine's HP bounds.
- Agility has a two-frame original flash. Its event is kept and a cosmetic
  trail of up to 0.55 s is added, without slowing down the game. Poké
  Balls use the original sub-animation counter for the trajectory and
  shakes; the engine decides escape, failure, and success.
- Live calls to `PlayMoveAnimation`, its cleanup, `MoveAnimation`, and
  `TossBallAnimation` bound the presentation. The animation ID and
  counters are aliased; their isolated persistence does not trigger
  effects.
- All 165 entries are present in the manifest and have a presentation
  path: 45 physical, 26 projectile, 24 status, 20 self, and 50 that keep
  the original LCD. The unit test checks the animation's ownership and
  exit for all 165 IDs even with the tilemap cleared and the palette
  flashing. Special animations outside that table also use the original
  fallback. It is not claimed that 165 different battles were played.
- Fly verifies the fallback during a real two-turn animation: 79 frames of
  opaque composite and **zero differences across the 23,040 pixels** of
  the LCD and its presented image. The fade hides the billboards while
  compositing the LCD, avoiding duplicated portraits. Captures of both
  paths have been reviewed in `logs/b2-review.png` in this run's folder.
- `battle-effects` prepares moves only in private QA states; it triggers
  them through the original menu. Production does not write WRAM, VRAM, or
  the framebuffer. `QaWalk` compares the full 32 KiB, 16 KiB, and 23,040
  pixels before and after each presented frame and requires zero GL
  errors.
- Fixed the terrain invalidation when loading another battle: returning to
  the Route 1 fixture from Route 22 recovers grass. This is explicitly
  verified.
- Fixed the QA input clock: the SDL counter survives several helper
  instances and the CPU clock is 32 bits. Presses stay active until
  explicitly released, even across that boundary. The changes affect the
  helper, not the downloaded runtime or the generated C.

Final validations already passed: CTest 7/7; full Kanto in
`build/qa/kanto-PW04nG/`; first person and engine parity in
`build/qa/firstperson-uorAnd/`; battle in `build/qa/battles-xtPavN/`.
The 38 outdoor captures remain byte-for-byte identical to the reference
before A1, per `build/qa/logs/b2-exterior-comparison.txt`.
The full interiors repeat run also passes in
`build/qa/interiors-VfIUU4/`: the house in both cameras, center and mart,
delivering the package, buying, returning to Route 1, stairs, elevator,
cave, and the 179-mesh catalog. A real encounter occurs in the cave: the
helper flees via the original menu before capturing the corridor in first
person and returning via the stairs. The encounter is not suppressed and
the RNG is not modified.

### Delivery audit, 2026-09-20

| Requirement | Final inspected evidence |
| --- | --- |
| A1: both floors, furniture, NPCs, laboratory, camera, and return | `interiors-VfIUU4/logs/house.log`, `house-fp.log`, `final-review.png`; `interior_test` |
| A2: full catalog, classification, flat warps, and limits | `interiors-VfIUU4/logs/interiors.csv`, `catalog.log`; `interior_audit` and `interior_test` |
| A2: healing, shopping, stairs, elevator, and cave | `interiors-VfIUU4/logs/{town,elevator,cave}.log`, their captures, and `result.txt` |
| B1: portraits, markers, menus, switch, fainting, and new rival | `battles-xtPavN/logs/{menus,battle3d}.log`, `b2-review.png`; `battle_state_test` |
| B1: victory and return to the map | `kanto-PW04nG/logs/journey.log`, `battles-xtPavN/logs/trainer-result.ppm` |
| B2: categories, LCD fallback, capture, and trainer | `battles-xtPavN/logs/battle3d.log`, `effect-*.csv` traces, and `b2-review.png` |
| Memory, GL, cache, and scene-selection invariants | Per-frame checks by `QaWalk`/`ReadOnlyMemory` across all four batches; full catalogs |
| Outdoor and first-person regression | `kanto-PW04nG/`, `firstperson-uorAnd/`; 38/38 images identical in `logs/b2-exterior-comparison.txt` |
| Unit tests and build | CTest 7/7 in `logs/b2-interiors-final.log`; `build/pokeyellow3d` compiled |
| Required local fixtures | `interiors-VfIUU4/logs/{reds-house-1f,viridian-center}.state`; `battles-xtPavN/logs/{capture-start,route22-trainer}.state` |
| CSV and documentation | `build/qa/logs/interiors.csv`, `PALLET3D.md`, `README.md`, and this plan |

The abbreviated paths in the table start from `build/qa/`. The ROM and
input-fixture hashes for the batches remain intact. The downloaded runtime
remains untouched and pinned to `6581880fce60e6f139901a5942fc984e9c1db8ab`;
the generated game C was not modified.

The accepted limitations still stand: 2,960 interior tiles with no artistic
rule remain flat (zero invalid graphics indices); Pokémon are billboards;
caves have basic geometry; link, tutorial, and Safari battles keep 2D.
Complex animations use the original LCD. The playable checks are
representative and are complemented by catalog audits and audits of the
165 move entries; it is not claimed that the story has been finished. No
criteria remain pending within the scope of this plan.
