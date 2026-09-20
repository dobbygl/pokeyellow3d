# Plan: Pokédex and PC in 3D

Date: 2026-09-20. Status: A1, A2, B1 and B2 completed and verified; A3 completed and verified; B3 pending. Goal active after resuming the integrated objective.

## Goal and scope

Give a 3D presentation to the game's two most-used data screens: the
Pokédex, with its list, its data screen, its cry, and its area map, and the
PC, with Bill's Pokémon storage, the player's item storage, Oak's
evaluation, and the Hall of Fame. Today all of them fall back to the
original full-screen image.

The principles from the earlier plans still apply. The recompiled engine is
the sole authority over state, lists, cursors, text, sound, and saving.
Menus and text remain the image the game paints, composited over the 3D
view. The new module only reads ROM, WRAM, VRAM, cartridge RAM, and the
framebuffer, and never writes to them. Any unrecognized screen falls back
to the current behavior.

This plan depends on phase A3 of `PLAN_MENUS_TITULO_TRANSICIONES.md`,
which establishes the region compositor and the dimmed background for
full-screen views. It reuses the portrait decoder from `src/battle_state.h`
and the map catalog from `src/kanto_rom.h`. It does not touch battles or
first person.

Out of scope: 3D Pokémon models, changing the order or content of the
lists, redrawing the typeface, cable trading, and any edit to the
downloaded runtime or the generated C.

## Verified starting point

- WRAM available in `pokeyellow_internal.h`: `wPokedexOwned` (D2F6) and
  `wPokedexSeen` (D309), 19-byte bitmaps; `wCurrentMenuItem` (CC26),
  `wListScrollOffset` (CC36), `wMaxMenuItem` (CC28), `wListMenuID` (CF93),
  and `wListPointer` (CF8A) for every list; `wWhichPokemon` (CF91) and
  `wCurPartySpecies` (CF90) for the selected Pokémon; `wPartyMons` (D16A);
  `wBoxMons` (DA95) and `wCurrentBoxNum` (D59F) for the active box;
  `wBoxItems` (D53A) and `wNumBoxItems` (D539) for item storage;
  `wNumHoFTeams` (D5A1) and `wHallOfFameCurScript` (D64A).
- The species shown on the Pokédex data screen is `wPokedexNum` at D11D
  in canonical Yellow UE. Its meaning changes between list construction
  (dex number) and the data screen (internal species). D11E is not the
  Yellow address. Verified against the local symbols and the original
  `engine/menus/pokedex.asm`; detect the screen before interpreting it.
- Cartridge RAM accessible as `ctx->eram`. Boxes 1 through 12 and the Hall
  of Fame have the symbols `sBox1` through `sBox12` and `sHallOfFame`. Only
  the active box is in WRAM; the rest require reading `eram` with the
  correct bank, which is also read-only information.
- ROM: `BaseStats`, `MonPartyData`, `WildDataPointers`, `InternalMapEntries`,
  `ExternalMapEntries`, `DisplayTownMap`, and the logo graphics have a
  symbol with a bank-relative address. The absolute reads already used in
  the project, such as the Pokédex order table at 0x410B1, show how to
  resolve and verify the bank against the ROM.
- The Pokédex data screen draws the front portrait with the same mechanism
  as battle: seven columns of seven consecutive tiles in `wTileMap`, from
  VRAM. `battle::portrait` and `battle::palette` work unchanged; only the
  rectangle's origin changes, which must be verified in
  `engine/pokedex/pokedex.asm`.
- The Pokédex list and the PC lists do not draw portraits. To show images of
  species not present in VRAM, the Generation I image decompressor is
  needed, ported to read-only C++.
- Interiors already give the PC volume: `src/interior_scene.h` classifies
  the computer and desk tile as furniture. The interior camera can move
  close to it with the same matrices from `src/firstperson.h`.
- The outdoor world of 36 maps shares a common origin. The game's nest data
  comes from the per-map encounter tables, the same ones `FindNest` uses
  for the AREA option. With the catalog, each nest can be placed at
  coordinates in the 3D world.
- The `live_return` technique from `battle_state.h` detects active routines
  by their return address on the stack, verifying the call in ROM. It is
  the way to know whether the data screen, the area map, Bill's PC, or the
  storage is on screen, without flags the game does not have.

## Architecture decisions

1. One module, `src/dex3d.h`, for the Pokédex, and another, `src/pc3d.h`,
   for the PC, with positive detection via `live_return` and a fallback by
   tile layout. `pallet3d.cpp` only hands off the frame when `view()`
   returns the new `Pokedex` and `Computer` states.
2. An image decompressor in `src/mon_pic.h`, ported from the original
   routine, with a cache per species and orientation. It is validated
   against VRAM: once the game has loaded a portrait, the decompressor's
   output must match the VRAM tiles byte for byte. That test is the port's
   guarantee.
3. The Pokédex is presented as a device: a red body in perspective, the
   left screen with the list composited from the framebuffer, and the
   right screen with the portrait of the species under the cursor. The
   data screen's text stays the original, composited image.
4. The AREA option shows the real 3D world from a great height with the
   orthographic camera, centered on Kanto, with nest markers derived from
   the encounter tables and blinking at the same rate as the original map.
   Maps not cataloged as connected outdoors, such as caves, are marked on
   their corresponding entrance.
5. The PC is presented from the interior it is in: the camera moves in on
   the monitor, the PC's main menu is composited on its screen, and the
   lists are composited full-screen over the dimmed interior. The boxes
   are also shown as a 3D shelf with the decompressed portraits of their
   Pokémon, with the active box in the foreground.
6. The Hall of Fame is presented as a gallery: one pedestal per Pokémon
   with its portrait, reading the teams from `sHallOfFame`. The original
   text is composited below.
7. No list data is interpreted twice: the cursor, the scroll offset, and
   the selection are read from the game; the 3D view only reflects them.
8. Any failed detection, any unverified RAM bank, or any portrait whose
   decompression does not match VRAM falls back to the current behavior:
   the original image over a dimmed background.

## Block A: Pokédex

### Phase A1: verified decompressor and 3D data screen

Work:

- Port the Generation I image decompression to `mon_pic.h`: reading
  per-species pointers from `BaseStats` and the Mew exceptions, image size,
  two bit planes, pair encoding, and blend modes. Output in 8-by-8 tiles in
  the same order as VRAM.
- Equivalence test: with already available battle and data-screen
  savestates, compare the output against `vFrontPic` in VRAM for at least
  twenty species of different sizes, including 5-by-5 and 6-by-6 tile ones.
- Detect the Pokédex data screen by `live_return` of the data routine and
  verify its portrait rectangle in `wTileMap`.
- Data screen scene: a device in perspective, the portrait on the right
  screen as a billboard with the ROM's palette, number, name, category,
  height, and weight composited from the framebuffer. The cry button does
  not change: the sound is the original one.

Acceptance criteria:

- [x] The twenty comparisons against VRAM are byte-for-byte identical and the test is part of CTest.
- [x] Opening Pikachu's data screen from the Start menu shows the device with the correct portrait and the original text.
- [x] Visiting successive data screens through the original list updates the portrait without showing a previous species. (Yellow exits data with A/B; left/right do not page data, as verified in the original routine.)
- [x] Zero OpenGL errors and WRAM, VRAM, cartridge RAM, and framebuffer intact per frame.

### Phase A2: list with portrait under the cursor

Work:

- Detect the Pokédex list and read the cursor and scroll offset from WRAM
  to know which number is selected; convert the number to an internal
  species using the order table already used by `battle::dex`.
- Left screen with the composited list; right screen with the decompressed
  portrait if the species is registered as caught, a dark silhouette if
  only seen, and empty if absent. This follows exactly the game's bitmaps.
- Seen and caught counters on the device's body, read from the same
  bitmaps.
- Preloading and caching of portraits per species with a memory limit and
  eviction of the oldest ones.

Acceptance criteria:

- [x] Scrolling the list updates the portrait at the same pace as the original cursor.
- [x] Seen, caught, and absent species are distinguished according to the bitmaps and match the game's counters.
- [x] The cache does not grow when paging through the 151 numbers from end to end several times.

### Phase A3: area over the 3D world

Work:

- Read the grass and water encounter tables for each map and build the
  nests for a species: map and encounter type. Compare against the
  original map's output for three species with nests on several maps.
- Detect the AREA screen by `live_return` of `DisplayTownMap` with the nest
  mode active.
- Top-down orthographic camera over the 36-map world with light fog and
  city labels; markers at the center of each map with a nest, blinking in
  sync with the game's counter. Unconnected maps are marked at their
  entrance in the outdoor world.
- The original text box with the species name is composited at the bottom.

Acceptance criteria:

- [x] The nests for Pidgey, Zubat, and Magikarp match those shown by the original map.
- [x] The view opens and closes with no full 2D frames and respects the resident meshes on returning to the map.
- [x] A species with no nest shows the original message and a world with no markers.

## Block B: PC

### Phase B1: accessing the PC from the interior

Work:

- Detect the PC's main menu and each submenu by `live_return`: Bill's PC,
  the player's PC, Oak's PC, and the Hall of Fame, both in the Pokémon
  Center and in the player's room.
- Locate the computer in the scene: the furniture piece classified as a
  computer adjacent to the player in the direction they are facing.
- Camera movement toward the monitor using the first-person matrix, from
  the current camera, over about 400 ms; return on closing the PC.
- Main menu composited on the monitor's screen in perspective; the power-on
  and power-off text unchanged.

Acceptance criteria:

- [x] Turning on the PC at Viridian City's Pokémon Center moves the camera in and shows the menu on the monitor.
- [x] The PC in the player's room works the same from both cameras.
- [x] Turning off the PC returns the camera and HUD to the previous state with no jumps.

### Phase B2: Bill's boxes with a 3D shelf

Work:

- Read the active box from WRAM and the rest from `eram`, with each box's
  bank verified by its checksums, without relying on the cartridge's
  current bank selection.
- 3D shelf with twelve boxes; the active one in the foreground with up to
  twenty decompressed portraits in a grid, with name and level; the rest
  as closed boxes with their counter.
- Deposit, withdraw, release, and change box: the original lists are
  composited full-screen over the dimmed shelf; the list cursor highlights
  the corresponding portrait in the grid.
- The grid is rebuilt only when box or party data changes, by comparing a
  fingerprint of the bytes read.

Acceptance criteria:

- [x] Depositing a Pokémon from the party and withdrawing it updates the grid with no frames showing stale data.
- [x] Switching boxes shows the correct content of the chosen box, including boxes read from cartridge RAM.
- [x] Releasing a Pokémon removes it from the grid after the original confirmation.
- [x] The capture fixture test deposits and withdraws that Pidgey successfully.

### Phase B3: item storage, Oak's evaluation, and the Hall of Fame

Work:

- Player's PC: the withdraw, deposit, and toss lists are composited
  full-screen; the monitor shows the stored-item count read from WRAM.
- Oak's PC: the evaluation text is composited; the monitor shows seen and
  caught counts.
- Hall of Fame: read the teams from `sHallOfFame`, one pedestal per Pokémon
  with a decompressed portrait, name, and level; the camera moves along
  the row at the pace of the original text.

Acceptance criteria:

- [ ] Storing and withdrawing a Poké Ball in storage keeps the scene and the correct counter.
- [ ] Oak's evaluation reads on the monitor with matching counters.
- [ ] With a private champion fixture, the Hall of Fame shows the real team read from cartridge RAM.

## Accepted limitations

- Pokémon are original portraits, with the ROM's palette, not models.
- Text and lists remain the enlarged original image.
- Species whose portrait cannot be verified against VRAM in the tests are
  shown as a silhouette until a fixture that loads them is added.
- The area map does not draw water routes or caves as scenes; it marks the
  entrance in the outdoor world.
- The Hall of Fame requires a save with a champion to be tested; without
  one, the phase is validated by reading data and with manual captures.

## Validation and delivery

- CTest and the `world_qa.sh`, `firstperson_qa.sh`, and `interiors_qa.sh`
  batches before and after each phase; outdoor captures remain identical.
- New unit tests: decompressor against VRAM, nests against the original
  map, box reading with checksums, and detection of each screen with local
  savestates.
- New `pallet_render_smoke` modes: `dex` runs through the list, data
  screen, cry, and area; `pc` turns on the PC, deposits, withdraws,
  switches boxes, and stores an item. Both check the presented mode, GL,
  and intact memory per frame.
- Private fixtures under `build/qa/`: Pallet Town with the Pokédex,
  Viridian City in front of the PC with two Pokémon in the party, a save
  with several occupied boxes, and, if available, a champion save. Not
  added to the repository.
- Deliver `build/pokeyellow3d`, `PALLET3D.md` updated with the Pokédex and
  PC scope, and captures in `build/qa/logs/`.
- Only check off boxes with recorded evidence.

## Recommended order

1. A1, because the verified decompressor unlocks everything else.
2. B1 and A2, which reuse the compositor and do not depend on each other.
3. B2, the most visible part of the PC.
4. A3 and B3, which complete the area map, storage, Oak, and the Hall of Fame.

## Technical references

- [Pokédex: list, data screen, and area](https://github.com/pret/pokeyellow/blob/master/engine/menus/pokedex.asm).
- [Image decompression](https://github.com/pret/pokeyellow/blob/master/home/uncompress.asm).
- [Loading front portraits](https://github.com/pret/pokeyellow/blob/master/home/pics.asm).
- [Town map and nests](https://github.com/pret/pokeyellow/blob/master/engine/items/town_map.asm).
- [Encounter tables](https://github.com/pret/pokeyellow/blob/master/data/wild/grass_water.asm).
- [Bill's PC](https://github.com/pret/pokeyellow/blob/master/engine/pokemon/bills_pc.asm).
- [Player's PC](https://github.com/pret/pokeyellow/blob/master/engine/menus/players_pc.asm).
- [Hall of Fame](https://github.com/pret/pokeyellow/blob/master/engine/movie/hall_of_fame.asm).
- [Game memory and cartridge RAM](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).
- `src/battle_state.h`: `portrait`, `palette`, `dex`, and `live_return`.

## Execution record

### A1 started, 2026-09-20

- Baseline before production changes: CTest 16/16; world
  `build/qa/kanto-miIHwE/`, first person `build/qa/firstperson-E9MrJ4/`,
  interiors `build/qa/interiors-uQmhLs/`, all PASS. Logs under
  `build/qa/logs/dex-a1-before-*.log`.
- Corrected two PC addresses against `pokeyellow_internal.h`: D59F and
  D5A1, not DA9F and DAA1 (which are inside box-mon data).
- Current upstream source paths are `engine/menus/pokedex.asm` and
  `home/pics.asm`; the old paths in the proposal return 404. Read-only
  reference copies are under `build/qa/dex/references/`.
- Yellow's `GetMonHeader` uses the ordinary 151-entry BaseStats table for
  Mew, unlike Red/Blue. The data portrait starts at tile (1,1), mirrored;
  the original data screen exits with A/B and does not page with left/right.
  Paging acceptance will therefore exercise the original list controls
  between data screens without adding new engine controls.
- Added the read-only decoder and a private QA path that grants dex flags
  solely to visit all 151 entries with the original menu. Equivalence is
  still being established; no phase is marked complete yet.
- Extended the per-frame memory guard to include cartridge RAM as required
  by this plan, in addition to WRAM, VRAM and the framebuffer.

### A1 completed, 2026-09-20

- `src/mon_pic.h` decodes all 151 front portraits using private buffers,
  bounds-checked bit reads, both planes, all three blend modes, differential
  decoding, alignment and mirrored orientation. It neither calls the engine
  decompressor nor changes a bank or an emulated byte.
- Original UI replay: `build/qa/dex-portraits-mV2lhb/`. The engine's own dex
  menus load every portrait; the test compares all 784 bytes of each
  `vFrontPic`. **118,384 bytes identical**, covering 47 portraits of 5x5,
  46 of 6x6 and 58 of 7x7; modes 0/1/2 occur 5/61/85 times.
- `mon_pic` checks canonical/sparse runtime ROM equivalence and malformed
  input. `mon_pic_vram` repeats the original-engine comparisons from private
  evidence at `build/qa/dex/front-vram.bin` (explicit skip if not generated).
  `dex_state` checks the live call, popped stack, invalid species, palette,
  battle exclusion, mirrored layout and visible BG/window tilemap.
- `src/dex_state.h` identifies the data screen through the verified live
  call at 0x40104 and its mirrored rectangle (1,1). `View::Pokedex` hands it
  to `src/dex3d.h`. Text regions retain the original number, name, category,
  measurements, description and page arrow. The right screen uses the
  original palette and transparent portrait. The cry stays in the engine.
- Full device replay: `build/qa/dex-portraits-nR5VsH/`, all 151 original
  entries from Start with a per-frame stale-image assertion, GL checks,
  neutral relative controls and full memory guard. Its saved VRAM evidence
  is byte-identical to the original presentation replay above. Captures
  reviewed at `build/qa/logs/dex-a1-data-review.png`, including all three
  sizes, Pikachu and Mew. The original Mew category is `NEW SPECIE`.
- Independent saved-data loads in both camera preferences pass in
  `build/qa/dex/device/logs/{run,run-fp}.log`. Repeated frozen frames upload
  no new textures. A controlled selection/VRAM mismatch displays the
  original-image fallback, never a cached previous portrait.
- After-phase validation: **CTest 19/19**, world `build/qa/kanto-Y0MSxV/`,
  first person `build/qa/firstperson-QO5KGi/`, interiors
  `build/qa/interiors-EdQKAF/`, all PASS. All **38 exterior captures are
  byte-identical** to the before-phase baseline; report at
  `build/qa/logs/dex-a1-exterior-comparison.txt`. Runtime remains unmodified.
- `build/pokeyellow3d` and `PALLET3D.md` updated. This completes A1 only.
  The next implementation is A2's list selection, caught/seen/absent states
  and bounded portrait cache; then B1/B2, A3 and B3 remain in full scope.

### A2 in progress, 2026-09-20

- A1's final three regression runs above are the unchanged before-A2 baseline.
- Added a fixed 32-entry LRU cache (<27 KiB of portrait data), keyed by
  species/orientation and reset when the ROM buffer changes. `mon_pic` tests
  repeated complete traversals, eviction, hits and ROM invalidation.
- The original list and side-menu calls determine selection; the side-menu
  cursor is separate from the selected Pokémon. Original list pixels remain
  on the left; the right shows caught/seen/absent as portrait/silhouette/empty.
  The body displays the original seen/owned counter glyphs.
- Initial three-traversal runs pass in `build/qa/dex/list-{ortho,fp}/`, but
  additional timing instrumentation found WRAM scroll changes precede the
  actual LCD by up to three frames. The renderer now checks the displayed
  number glyphs and cursor before publishing a new selection. A partial LCD
  transfer leaves the portrait empty unless its previous cursor is still
  visibly selected. Final timing tests and after-phase regressions are pending.

### A2 completed, 2026-09-20

- Final list QA **PASS**: `build/qa/dex-list-eD1XZk/`, with ortho and FP
  preferences, three complete traversals of all 151 entries per camera,
  original side-menu/CRY actions and return to the resident world. Input ROM
  and state hashes remain unchanged. Repeat with `tests/dex_list_qa.sh ROM
  WORLD_WITH_POKEDEX`.
- The private bitmap fixture uses Bulbasaur caught, Ivysaur seen, Venusaur
  absent and Mew seen to make number 151 reachable. Selected portraits,
  silhouettes and empty images match these bits; original displayed counters
  are 3 seen / 1 owned. No list, cursor or portrait bytes are injected.
- Each camera encounters **1,336 LCD-transfer frames** during traversal.
  The per-frame observer checks that any displayed portrait matches its
  actually visible original number and cursor, and that a ready new cursor
  updates immediately. No stale or premature portrait passes this check.
- Cache unit tests cover four full traversals, both orientations, hits,
  actual least-recently-used eviction, fixed 32-entry residency and ROM
  invalidation. Rendering preserves WRAM, VRAM, cartridge RAM and framebuffer;
  the original world matrix and resident meshes survive the complete menu.
- Final captures reviewed at `build/qa/logs/dex-a2-list-final.png`.
  A1's complete 151-entry device replay also passes again in
  `build/qa/dex-portraits-R5ZynC/`. All its VRAM records and the five reviewed
  device captures are byte-identical to A1; report:
  `build/qa/logs/dex-a2-data-comparison.txt`.
- After-phase validation **PASS**: CTest 19/19; world
  `build/qa/kanto-nIUnWo/`, first person `build/qa/firstperson-cr3myd/`,
  interiors `build/qa/interiors-ABONC4/`, and the eight general menu scenarios
  in `build/qa/ui-menus-eqZtuv/`. The generic menu test now verifies the
  dedicated dex handoff while preserving original tilemap classification.
  All 38 exterior captures remain byte-identical to the before-A2 baseline
  (`build/qa/logs/dex-a2-exterior-comparison.txt`).
- Build and documentation updated; runtime and generated game C untouched.
  The goal remains active with B1/B2, A3 and B3 still required.

### Next: B1 preparation

- Original PC references downloaded read-only to `build/qa/dex/references/`:
  `engine_menus_pc.asm`, `engine_menus_players_pc.asm`,
  `engine_pokemon_bills_pc.asm`, `engine_menus_oaks_pc.asm` and related files.
- Home text-script entries are 33EF (item PC), 33F9 (Bill), 340E (Center).
  Their dispatch shares CALL 3408 → 3E84, including the vending-machine
  branch: a live return alone cannot identify a PC. Confirm the adjacent
  furniture/tileset and the relevant menu context. The bank-5 submenu calls
  use `LD B,bank; LD HL,target; CALL Bankswitch`, in the opposite order to
  the existing battle detector. These anchors are present in the sparse ROM.
- Center fixture/actions already exist in `tests/menu_integration.h`:
  map 41, player (13,4), facing north. Bedroom must use its own original
  item-PC menu, rather than inventing access to Bill/Oak there.
- Current interior geometry extrudes furniture but paints its graphic on top.

### Paused checkpoint: B1 in progress, 2026-09-20

- Initial PC detection, monitor presentation, camera approach/return and
  diagnostic integration are implemented in `src/pc_state.h`, `src/pc3d.h`
  and the renderer. They compile but are not yet validated end to end.
- CTest currently fails `pc_state` with `each original submenu is detected`.
  The terminal locator matches furniture at map 41, (0,6), while the known
  Center PC is used from player (13,4), facing north. Verify its actual ROM
  tiles and correct detection before continuing the presentation tests.
- A1/A2's successful regressions above predate these B1 changes. PC monitor
  pixel checks, camera timing, submenu integration and the after-B1 world,
  first-person and interior regressions remain pending.
- Preserve this as a work-in-progress checkpoint. Resume with the terminal
  locator and failing test, then finish B1, B2, A3 and B3; the full goal is
  not complete.

### B1 completed after resumption, 2026-09-20

- Corrected terminal detection from actual ROM tiles: Center 42/52 at
  (13,3), bedroom 42/32 at (0,1). The former 20/30 match was seating;
  the bedroom's 40/20 graphic is the upper wall, not the interaction cell.
  Center PCs now have furniture geometry rather than a perimeter wall.
- Bank-validated live calls distinguish Center, items, Bill, Oak and Hall;
  synthetic unit cases assert the exact mode, not merely a nonempty result.
  The bedroom retains its original item-storage-only menu.
- The camera animates a copied projection over 400 ms of guest time;
  resident map matrices, meshes and actors remain untouched. Main menus
  use the monitor, partial dialogue retains its original compositor, and
  full submenus retain the original LCD over the dimmed scene. The stand
  ends below the last LCD row, so it cannot obscure text.
- `tests/pc_focus_qa.sh ROM CENTER_ENTRANCE_STATE WORLD_STATE` passes in
  `build/qa/pc-focus-xiR0Sr/`: Center and bedroom in both cameras,
  24-frame approach / 23-frame visible return, settings/focus pauses,
  preserved camera preference, GL and read-only memory on every frame.
  All 23,040 monitor pixels and 7,680 power-on dialogue pixels match the
  original framebuffer. Bill deposit/withdraw and healing also pass in
  both cameras with full original-menu pixel comparisons.
- Captures reviewed at `build/qa/logs/pc-b1-focus-review.png`.
  After-phase CTest **20/20**, world `build/qa/kanto-nl3Tqg/`, first person
  `build/qa/firstperson-ZdNNhO/`, interiors `build/qa/interiors-n7wA7k/`:
  all PASS. The 38 exterior captures remain byte-identical to A2
  (`build/qa/logs/pc-b1-exterior-comparison.txt`). These are the unchanged
  production baseline for B2.

### B2 in progress

- Added an initially standalone read-only storage reader and tests. It
  resolves the twelve boxes in SRAM banks 2/3, verifies the aggregate and
  six individual checksums per bank, gives the active WRAM box precedence,
  and fingerprints box/party bytes. Never-used boxes follow the original
  first-change initialization rule; uninitialized SRAM is not read.
- Bounds, differing banks, malformed records, partial updates and checksum
  corruption pass standalone tests. The reader is not yet connected to a
  shelf, and original-engine save/change-box evidence remains required.
- Original save/checksum reference:
  https://github.com/pret/pokeyellow/blob/master/engine/menus/save.asm,
  local read-only copy in `build/qa/dex/references/engine_menus_save.asm`.

### B2 completed, 2026-09-20

- `pc_storage.h` reads active box/party WRAM and both saved-box banks without
  changing the selected bank. Both aggregate and individual checksums must
  pass; incomplete records, bad bounds, invalid levels/species or unterminated
  names fall back to the original menu over the dimmed interior.
- `pc_boxes.h` keeps twelve numbered cubbies and an open twenty-portrait box
  around the original centered full LCD; a separate party row lets deposit
  selections remain visible too. Names, levels and counters use the original
  ROM font. Projection-aligned glyph quads preserve all strokes at native
  glyph size. Portrait/geometry updates depend on data fingerprints or resize;
  frozen frames perform no new uploads or rebuilds. The portrait cache is
  bounded to 32 entries.
- `pc_box_state.h` distinguishes list and action cursors using verified live
  calls. It verifies the actually displayed original nickname and arrow before
  highlighting a portrait; Cancel and incomplete LCD transfers highlight none.
  Unit cases cover scroll offsets, WRAM ahead of LCD, action-menu cursors and
  popped calls, in addition to corruption and the twelve bank-relative reads.
- Final original-engine QA **PASS** in `build/qa/pc-storage-JZqDwI/`, with
  orthographic and FP preferences. The helper encountered Rattata, escaped,
  then naturally encountered and caught Pidgey with one bought Poké Ball.
  That Pidgey was deposited, saved through a switch to box 7, loaded from
  bank 2, withdrawn, deposited again and released after the original prompt.
  Per-frame observers check current box/party portraits, intact game memory,
  GL, and coverage; every captured full LCD has 23,040 identical pixels.
- A separate, explicitly synthetic stress fixture clones the captured record
  into twelve occupied boxes and twenty active entries. It follows the real
  game's party-level-to-box-level conversion. Original scrolling selects all
  twenty names correctly, Cancel clears selection, and original switches to
  boxes 7/12/1 load 7/12/20 records. Corrupting a bank checksum triggers the
  original-image fallback; restoring it recovers the shelf.
- Private original WRAM/SRAM exports are `build/qa/pc/boxes.{wram,sram}`.
  `pc_storage_original` independently checks the decoder against that actual
  deposit/change-box result (explicit skip when local evidence is absent).
  Final CTest **22/22**. Repeat with
  `tests/pc_storage_qa.sh ROM ROUTE1_10_4_WITH_BOUGHT_BALLS_STATE`.
- World `build/qa/kanto-00tyiB/`, FP `build/qa/firstperson-hVjAo6/` and interiors
  `build/qa/interiors-tLeSE7/` all pass. All 38 exterior captures remain
  byte-identical to B1 (`build/qa/logs/pc-b2-exterior-comparison.txt`). Final
  shelf/font captures: `build/qa/logs/pc-b2-full-grid-final.png` and
  `build/qa/logs/pc-b2-font-final.png`. These general regressions are the
  unchanged world baseline for A3; the last shelf-only glyph alignment is
  additionally covered by the final four PC runs above.

### A3 preparation

- Added a standalone, bounds-checked `dex_nests.h` reader; it is not yet
  connected to the renderer. ROM results: Pidgey 13 maps/13 town-map locations;
  Zubat 12 maps/4 locations; Magikarp none. Comparison with the original AREA
  sprites passes for all three species in `build/qa/dex-area-oracle/logs/run.log`.
  The 3D renderer, transitions and complete `dex` smoke are still required.
- The original `FindWildLocationsOfMon` is in `engine/items/item_effects.asm`.
  It checks grass and water tables only, not fishing. Thus Magikarp is the
  required no-nest case, rather than a reason to invent fishing markers.
  `DisplayWildLocations` additionally suppresses Cerulean Cave (coordinate 19).
- Verified ROM anchors: WildDataPointers CB95, external/internal town-map
  entries 7139C/7140B; AREA predef call 40118 -> 3EB4 (LD A,4A). The original
  title is on tile row 0 and must be composited at the bottom of the new view.
  Blink counter D08A hides at 25 and shows/resets at 50; enable flag D09A.
- Source copies are private under `build/qa/dex/references/`; the independent
  map/warp metadata graph supplies entrance positions without modifying the
  resident-world cache or its camera.

### Paused checkpoint: B1/B2 verified, A3 preparation, 2026-09-20

- Goal paused at the user's request; this checkpoint does not complete the plan.
- CTest passes 22/22. The AREA reader and state detector are still standalone;
  `tests/dex_nests_test.cpp` has not yet been registered with CMake.
- Resume with A3: register its tests, build the independent Kanto overview,
  markers and original-text compositor, and verify AREA transitions and the
  complete Pokédex flow. Then finish B3 and the combined `dex`/`pc` smoke modes.
- ROMs, saves, screenshots and private QA evidence remain under ignored paths.


### Paused checkpoint: AREA implementation and regression evidence, 2026-09-20

This checkpoint supersedes the A3 preparation status above. The implementation
is saved for review; acceptance checkboxes remain unchanged until the final
phase audit. B3 and the combined PC smoke mode remain pending.

- `src/dex_area3d.h` renders a private 36-map Kanto overview with ROM city
  names, original encounter markers and original LCD title/unknown-area text.
  Its geometry and texture caches preserve the resident world's camera and
  meshes. The complete `dex` and `dex-fp` flows cover list, DATA, CRY and AREA.
- CMake now registers `dex_nests` and the private original-engine oracle.
  Pidgey, Zubat and Magikarp match the original AREA results. Both camera
  preferences and cold AREA loads pass, including blink, exact original text,
  preserved caches and per-frame guest-memory invariants.
- Final CTest: **24/24** (`build/qa/logs/dex-a3-ctest-final.log`).
  ROM-free CTest: **8/8** (`build/qa/logs/dex-a3-no-rom.log`); the independent
  `build/qa/no-rom` directory contains no ROM. Software rendering also passes
  with llvmpipe (`dex-a3-software-render.log`).
- All thirteen `tests/*_qa.sh` suites pass. Logs are
  `build/qa/logs/dex-a3-{world,firstperson,interiors,battles,ui_battles,ui_crossfade,ui_menus,ui_transitions,dex_list,dex_portraits,dex_area,pc_focus,pc_storage}.log`.
  The sequential resumed batch exited 0 (`dex-a3-remaining.exit`).
- All **38 exterior** and **179 interior catalog** captures are byte-identical
  to B2. Reports: `dex-a3-exterior-comparison.txt` and
  `dex-a3-interior-catalog-comparison.txt` under `build/qa/logs/`.
  Five additional live-journey captures differ; these are recorded in
  `dex-a3-interior-comparison.txt`, not counted as byte-identical. Camera
  settling tied to real elapsed time is a suspected cause, not a proven
  explanation for every difference.
- Reviewed AREA contact sheet: `build/qa/logs/dex-a3-area-review.png`.
  Final dedicated AREA evidence: `build/qa/dex-area-b8VaUF/`.

Reproduce the focused checks with local, private fixtures:

```sh
cmake --build build --target pokeyellow_launcher pallet_render_smoke dex_nests_test
ctest --test-dir build --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
tests/dex_area_qa.sh build/roms/pokeyellow.gbc build/qa/ui-menus-R28lDE/pallet.state
bash build/qa/logs/run-dex-a3-regressions.sh
```

The last command is the locally retained full fixture-based regression script;
private ROMs, savestates, screenshots and logs are not committed.


### A3 acceptance audit, 2026-09-20

The three A3 criteria are now closed against the preceding checkpoint's
completed evidence: the original-engine nest oracle, full `dex`/`dex-fp`
journeys and cold loads, original unknown-area pixels, and preserved world
caches. All thirteen QA suites and both CTest configurations completed before
this audit. B3 remains open; no PC/Hall criteria are implied by A3 closure.
