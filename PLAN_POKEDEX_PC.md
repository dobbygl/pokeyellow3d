# Plan: Pokédex and PC in 3D

Date: 2026-09-20. Status: proposal; no phase started.

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
  `wBoxMons` (DA95) and `wCurrentBoxNum` (DA9F) for the active box;
  `wBoxItems` (D53A) and `wNumBoxItems` (D539) for item storage;
  `wNumHoFTeams` (DAA1) and `wHallOfFameCurScript` (D64A).
- The species shown on the Pokédex data screen is in `wd11e` according to
  pret. It is not exported in the internal header; it must be verified
  against `wram.asm` and fixed as a documented constant, as was done for
  the facing direction.
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

- [ ] The twenty comparisons against VRAM are byte-for-byte identical and the test is part of CTest.
- [ ] Opening Pikachu's data screen from the Start menu shows the device with the correct portrait and the original text.
- [ ] Paging through several data screens with left and right changes the portrait with no frames showing the previous one.
- [ ] Zero OpenGL errors and WRAM, VRAM, cartridge RAM, and framebuffer intact per frame.

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

- [ ] Scrolling the list updates the portrait at the same pace as the original cursor.
- [ ] Seen, caught, and absent species are distinguished according to the bitmaps and match the game's counters.
- [ ] The cache does not grow when paging through the 151 numbers from end to end several times.

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

- [ ] The nests for Pidgey, Zubat, and Magikarp match those shown by the original map.
- [ ] The view opens and closes with no full 2D frames and respects the resident meshes on returning to the map.
- [ ] A species with no nest shows the original message and a world with no markers.

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

- [ ] Turning on the PC at Viridian City's Pokémon Center moves the camera in and shows the menu on the monitor.
- [ ] The PC in the player's room works the same from both cameras.
- [ ] Turning off the PC returns the camera and HUD to the previous state with no jumps.

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

- [ ] Depositing a Pokémon from the party and withdrawing it updates the grid with no frames showing stale data.
- [ ] Switching boxes shows the correct content of the chosen box, including boxes read from cartridge RAM.
- [ ] Releasing a Pokémon removes it from the grid after the original confirmation.
- [ ] The capture fixture test deposits and withdraws that Pidgey successfully.

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

- [Pokédex: list, data screen, and area](https://github.com/pret/pokeyellow/blob/master/engine/pokedex/pokedex.asm).
- [Image decompression](https://github.com/pret/pokeyellow/blob/master/home/uncompress.asm).
- [Loading front portraits](https://github.com/pret/pokeyellow/blob/master/engine/gfx/sprites.asm).
- [Town map and nests](https://github.com/pret/pokeyellow/blob/master/engine/menus/town_map.asm).
- [Encounter tables](https://github.com/pret/pokeyellow/blob/master/data/wild/grass_water.asm).
- [Bill's PC](https://github.com/pret/pokeyellow/blob/master/engine/pokemon/bills_pc.asm).
- [Player's PC](https://github.com/pret/pokeyellow/blob/master/engine/menus/players_pc.asm).
- [Hall of Fame](https://github.com/pret/pokeyellow/blob/master/engine/events/hall_of_fame.asm).
- [Game memory and cartridge RAM](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).
- `src/battle_state.h`: `portrait`, `palette`, `dex`, and `live_return`.
