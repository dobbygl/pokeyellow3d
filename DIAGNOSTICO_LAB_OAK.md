# Diagnosis: Oak's lab "looks 2D"

Date: 2026-09-20. Status: diagnosis closed, plan proposed, not implemented.
Scope: analysis only. `src/`, `tests/`, `cmake/`, `CMakeLists.txt`, and the
prior contents of `build/` were not modified. All runs were done on private
copies under `build/qa/oaklab/`, never on the user's original executable,
ROM, or save.

One-line summary: **the 3D does activate inside the lab; what fails is the
camera.** Interiors are drawn with `room_yaw = 0`, and with zero rotation the
orthographic projection in `firstperson::orthographic` collapses into a
frontal elevation where **every face with an X normal measures zero pixels
wide**. Only the top faces remain visible — which carry the original
tileset art — and the result is a uniform vertical flattening of the 2D
tilemap.

---

## 1. Verified facts

### 1.1 The binary matches the current code

The diagnosis was run against **two consecutive binaries** and gives the
same result in both. The second is the one currently in `build/`.

| Binary | Compiled | `sha256` | Most recent source | Verified |
| --- | --- | --- | --- | --- |
| First | 11:23:52 | `eb993e75…9375` | `src/pallet3d.cpp` 11:23:46, 790 lines | yes |
| Second (current) | 11:38:03 | `9c34a4ed…a408` | `src/pallet3d.cpp` 11:37:58, 803 lines | yes |

At both points, `find src cmake CMakeLists.txt -newer <binary>` returned
empty: each binary was newer than all of its sources. `git log -1` is still
`3654e37 fix: restore validated renderer before overlay refactor`; the 11:37
changes are uncommitted.

The only files newer than the binary were `tests/check_ui_traces.py` and
`tests/ui_transitions_qa.sh`, which are not compiled into `pokeyellow3d`.
**There was no need to build a separate copy**: the binary tested is the one
from the current code. Hashes of every input are in
`build/qa/oaklab/logs/inputs.sha256`.

The second build was made by the Codex agent midway through the analysis.
The repeat run against that binary is in `build/qa/oaklab/rebuild/` and gives
exactly the same result: `state=3` inside map 40, `vertices=5154`, and
identical captures with and without the camera (`md5 a3bb4ba3…f707`). Codex's
changes shifted line numbers but did not touch any of the functions
involved.

### 1.2 Inside the lab the state IS Overworld (3D active)

A copy of the user's real save (`build/pokeyellow.state10` →
`build/qa/oaklab/user-state10.state`, Pallet Town 8,15, party of 1, Pikachu
level 7, `font=0`) was loaded, and the walk to the lab was done with the
`play` mode of `pallet_render_smoke`, using only controller input. The route
was reconstructed step by step, all legs in
`build/qa/oaklab/logs/walk-step*.log`:

```
(8,15) → (12,15) → (12,14) → (14,14) → (11,14) → (8,16) → (8,6)
       → (8,11) → (8,13) → (9,13) → (9,12) → (12,12) → door
```

Trace of crossing the door, `build/qa/oaklab/logs/walk-enter-lab.log`
(`PALLET3D_TRACE=1`, input `10:U:40`, 260 frames):

```
[3D] state=3 map=0  xy=12,12 font=0 sprites=1 bgp=e4
[3D] state=0 map=40 xy=12,11 font=0 sprites=1 bgp=e4
[CROSSING] frame=29 0 -> 40 xy=12,11
[3D] state=1 map=40 xy=5,11 font=0 sprites=1 bgp=ff
[3D] state=3 map=40 xy=5,11 font=0 sprites=1 bgp=e4      <-- key line
[3D] mesh map=40 vertices=5154 bytes=185544 build=0.19ms resident=1
[PLAY] map=40 xy=5,11 battle=0 party=1 hp=21 font=0
```

Reading: one frame in `Unsupported` (0) while the header is already 40 but
the coordinates are still the outdoor ones, a stretch in `Transition` (1)
with `bgp=ff` (engine fade), and **stabilization in `Overworld` (3)**, after
which the renderer builds the interior mesh. The `play` mode validates on
every frame that `pallet3d_active()` matches `view()`; there was no
return-23 at any point.

Therefore **the activation-failure hypothesis is ruled out**. `ensure_scene`
does not fail (scene 40 exists and is used), `valid_live_map` does not fail
(if it failed the state would be 1, `Transition`), `load_catalog` does not
fail (the log says «39 scenes» after discovering the interior), and
`font=0` throughout the whole walk: in this save there is no active Oak
script loading the font.

The same state is reproduced with the `build/qa/interiors/logs/oaks-lab.state`
fixture (`build/qa/oaklab/logs/probe-oaks-lab.log`:
`[SMOKE] view=3 map=40 xy=5,11`). The save and the fixture land in the same
place.

Saved state of the user's save already inside the lab:
`build/qa/oaklab/logs/user-inside-lab.state`.

### 1.3 The resulting image is geometrically indistinguishable from 2D

New captures (raw PPM in `logs/`, PNG converted with dependency-free Python
at the root of `build/qa/oaklab/`):

| Capture | What it shows |
| --- | --- |
| `build/qa/oaklab/user-oaklab.png` | The user's real save inside the lab, `state=3` |
| `build/qa/oaklab/user-pallet.png` | The same save in Pallet Town, for contrast |
| `build/qa/oaklab/ref-oaks-lab.png` | A1 fixture of the lab (converted from `build/qa/interiors/logs/oaks-lab.ppm`) |
| `build/qa/oaklab/ref-oaks-lab-fp.png` | The same lab in first person |
| `build/qa/oaklab/ref-mart-2f.png` | Celadon Department Store 2F with `room_yaw=0` |
| `build/qa/oaklab/ref-mart-2f-camera.png` | The same map after ten presses of `E` |
| `build/qa/oaklab/ref-exterior-final-camera.png` | Reference outdoor capture (`build/qa/logs/final-camera.ppm`) |

### 1.4 The lab's camera is completely locked

`pallet3d.cpp:693-702` only allows rotating and zooming in interiors when
`large = room->width>14 || room->height>14`. ROM audit
(`build/qa/logs/interiors.csv`): **OAKS LAB is 10 × 12 tiles**, so `large`
is false and `Q`, `E`, and the mouse wheel return `true` without doing
anything.

Empirical check with the smoke's `camera` mode, which presses `E` ten times
and applies a wheel `+4` before capturing:

```
md5  a3bb4ba358e05497b49d6fc6d276f707  logs/user-oaklab-camera.ppm
md5  a3bb4ba358e05497b49d6fc6d276f707  logs/user-oaklab-plain.ppm
```

**The two captures are byte-for-byte identical.** The HUD still announces
«Q / E Girar     Rueda Zoom» (`pallet3d.cpp:596`), controls that don't
exist on this map.

Scope of the lockout across the full catalog: of the **179 interiors, 107
(59.8%) have the camera frozen** at `room_yaw = 0`; only 72 can be rotated.

### 1.5 Exact geometry breakdown for map 40

`build/qa/logs/interiors.csv`, row for map 40:

```
map=40  OAKS LAB  interior=1  tileset=5  width=10  height=12  warps=2
floor=80  warp_cells=2  wall=8  furniture=30  counter=0  water=0
unclassified_flat_cells=0  unknown_graphics=0
```

Correction note on the brief: the lab uses **tileset 5**
(`case 5: case 7:` in `interior_scene.h:42`, «Lab / dojo / gym»), not 20.
Tileset 20 («Research lab») is used by 7 other maps. The palette branch is
the same (`pallet3d.cpp:186` groups `id==5||id==7||id==20||id==22`).

The lab's art classification is **complete**: 0 cells without a rule and 0
unknown graphics. Lack of classifier coverage plays no part in this issue.

Vertex budget reconstructed from `interior_map` and checked against the
`vertices=5154` trace:

| Source | Calculation | Vertices |
| --- | --- | --- |
| Floor, `create_map:227-231` | 20 × 24 quads × 6 | 2,880 |
| Room plinth, `create_map:235` | 5 quads × 6 | 30 |
| 30 pieces of furniture | (5 box quads + 4 top quads) × 6 | 1,620 |
| 8 walls | (5 + 4 + 4 quads) × 6 | 624 |
| **Total** | | **5,154** ✓ |

56% of the vertices are flat floor, and the 38 cells with volume contribute
2,244 vertices, a breakdown that matches the trace to the vertex. For
scale, Pallet Town generates 15,162 vertices over 360 tiles versus the
lab's 120: per tile, 42 vertices outdoors and 43 indoors. **The problem is
not a lack of geometry.** (The outdoor total's internal split between
houses, trees, ledges, and grass has not been broken down; only the total
is compared.)

### 1.6 Measured color diversity

Pixels distinct from the background, counted over the 800×720 captures
(`build/qa/oaklab/stats.py`, no external dependencies; the PNG conversion
uses `build/qa/oaklab/ppm2png.py`):

| Capture | Unique colors | Pixels outside the green/teal band |
| --- | ---: | ---: |
| `user-oaklab.ppm` (lab) | 323 | 0.5% |
| `mart-2f.ppm` (interior, `yaw=0`) | 206 | 20.1% |
| `mart-2f-camera.ppm` (interior, `yaw≈0.4`) | 349 | 31.3% |
| `user-pallet.ppm` (outdoor) | 2,751 | 41.1% |
| `kanto-pallet.ppm` (outdoor) | 3,220 | 48.4% |

The outdoor scene has **8.5× more colors** than the lab and almost half its
image outside the green band. The lab is, for practical purposes,
monochrome.

---

## 2. Diagnosis

> Line references correspond to `src/pallet3d.cpp` at 803 lines
> (2026-09-20 11:37, binary `sha256 9c34a4ed…a408`). Codex is still editing
> the file in parallel, so the reliable anchor is the quoted expression, not
> the line number. The diagnosis was re-verified against that freshly built
> binary: same `state=3`, same 5,154 vertices, and the same byte-for-byte
> identical capture (`build/qa/oaklab/rebuild/`).

### 2.1 Root cause: with `room_yaw = 0` the projection degenerates

`pallet3d.cpp:460` picks the camera rotation:

```cpp
float view_yaw = current.interior ? room_yaw : yaw;
```

The initial values are in `pallet3d.cpp:45-46`:

```cpp
float yaw = -.32f, zoom = 1.f;   // exterior
float room_yaw = 0, room_zoom = 1;   // interior
```

and `update_meshes` (`pallet3d.cpp:307`) resets `room_yaw = 0` on every
component change, i.e. **every time a room is entered**.

`firstperson::orthographic` (`firstperson.h:55-61`) is, in column-major
form:

```
clip.x = sx·( c·X − s·Z ) + …
clip.y = sy·( up·Y − tilt·( s·X + c·Z ) ) + …      tilt = 0.78   up = 0.6257795
```

With `yaw = 0` (`c = 1`, `s = 0`) this becomes:

```
clip.x = sx · (X − focus_x)
clip.y = sy · ( 0.6258·Y − 0.78·(Z − focus_z) )
```

`clip.x` **does not depend on Z**. Direct consequence: the two faces with
an X normal of every `box()` (`pallet3d.cpp:133-135`, constant-X quads)
have all four vertices with the same `clip.x` and **project with zero
width**. They are never drawn. Of the five faces `box()` emits, only the
top and the front survive; the back is occluded.

Worse, the top face of a piece of furniture at height *h* appears at
`clip.y = sy·(0.6258·h − 0.78·Z)`: the **same 0.78 depth factor as the
floor**, just shifted upward. The whole interior image is therefore a
uniform vertical flattening to 78% of the 2D tilemap, with furniture
nudged up a few pixels. Since the original Game Boy art is already drawn
in elevation (a shelf "is seen face-on" in the 2D map), laying it flat onto
the top face literally reproduces the 2D image.

The outdoors doesn't suffer from this because it starts at `yaw = -0.32`:
there, `clip.x = sx·(c·X − s·Z)` with `s ≈ -0.3146`, the constant-X faces
gain `0.3146·sx` of width per unit of depth, and the sides of houses,
trees, and rocks are visible.

### 2.2 A/B test on the same map, same mesh, and same palette

`ref-mart-2f.png` and `ref-mart-2f-camera.png` are Celadon Department
Store 2F (map 123, 20 × 8, `large = true`) rendered by the same binary with
the same classification and the same color ramp. The only difference is
`room_yaw`:

- `room_yaw = 0`: flat reading, shelf sides invisible, it reads as a
  tilemap.
- `room_yaw ≈ 0.4` after ten `E`s: unambiguous volume, sides, face shading,
  and depth.

That pair of captures isolates the variable. And map 40 can never reach the
second case because `large` is false (§1.4).

### 2.3 Aggravating factors (real, but secondary)

All four contribute to the illegibility; none explains the 2D look on its
own, and all of them would be partially offset by the camera rotation.

1. **A single monochrome ramp per room.** `create_atlas`
   (`pallet3d.cpp:185-194`) defines four interior ramps for the **21**
   interior tilesets: green for `{5,7,20,22}` (41 maps, including the lab),
   blue for `{2,6}` (21 maps), gray for `{11,17}` (21 maps), and cream by
   default (96 maps). In addition, for interiors the atlas's palette slots
   0 and 1 are both filled with the **same** `room`, and `floor_palette`
   (`pallet3d.cpp:86`) always returns 0 except for cave water: inside a
   room there is only **one** four-tone ramp. Outdoor maps use three
   distinct ramps (`ground`, `facade`, `water`).

   Important nuance, to avoid misattributing the effect: **part of the
   color deficit is a consequence of the camera, not of the palette.** In
   the A/B pair for the same map (§1.6), going from `yaw=0` to `yaw≈0.4`
   raises unique colors from 206 to 349 (+69%) without touching a single
   line of the atlas, because the `shade(c,.75f)`, `.85f`, and `.67f`
   tints `box()` applies to three of its five faces (`pallet3d.cpp:133-135`)
   only get painted once those faces have width. The residual deficit,
   once the camera is rotated, is the part that truly belongs to the
   palette.

2. **Furniture sides in flat color.** `interior_map`
   (`pallet3d.cpp:207-211`) computes `side` as the **average of the 64
   pixels** of the cell's top-left tile and calls `box(...)` **without a
   detail UV**, so `detailed_quad` takes the early `if(detail.w==0)` exit
   and emits an untextured quad. The sides and the front of each piece of
   furniture are a solid color. It's clearly visible in
   `ref-oaks-lab-fp.png`, where tables have a textured top and plain
   sides.

3. **Art is duplicated on the top and front, and only on walls.** Only
   `Kind::Wall` and `Kind::Counter` receive a textured front face
   (`pallet3d.cpp:217-220`), and that face reuses **the same tile `t`**
   already painted on the top, stacked by row: the graphic appears twice.
   The lab's 30 pieces of furniture (`Kind::Furniture`, including the
   shelves at `top==0x0d`) **have no textured front face at all**.

4. **No furniture contact shadows.** `shadow()` is called at
   `pallet3d.cpp:152` (houses), `:245` and `:265` (trees), and `:470`
   (actors). `interior_map` never calls it. Nothing built inside a room
   casts a shadow, and without a shadow there is no height cue once the
   side silhouette is invisible.

**Correction to one of the task's hypotheses:** sprites *do* cast shadows
indoors. `pallet3d.cpp:470` runs `shadow(vertices,a.x,a.z,.32f,.20f)` for
every actor without distinguishing interior from outdoor, and the ellipses
are visible under the NPCs and under the player in `user-oaklab.png`.
What's missing is shadow from the **scenery**.

### 2.4 What is NOT broken

To avoid unnecessary work, the following are explicitly ruled out:

- `pallet::view()`, `ensure_scene`, `valid_live_map`, and `load_catalog`
  for map 40 (§1.2). `pallet_state.h` does not need to be touched.
- The font flag from Oak scripts: `font=0` throughout the whole real-save
  walk.
- The WRAM live blocks: the state would be `Transition`, not `Overworld`.
- Classifier coverage: 0 cells without a rule in map 40 (§1.5).
- Geometry volume: 38 cells with volume out of 120, a proportion
  equivalent to the outdoors (§1.5).
- First person: `ref-oaks-lab-fp.png` shows that the same mesh, viewed
  with perspective, reads unambiguously as 3D.

### 2.5 Comparison with the outdoors (question 4)

`user-pallet.png` and `user-oaklab.png` come from the **same save** minutes
apart. Differences that explain the legibility gap:

| 3D cue | Outdoors (Pallet) | Interior (lab) |
| --- | --- | --- |
| Camera rotation | `yaw = -0.32`, adjustable `±0.75` | `room_yaw = 0`, locked |
| Visible side faces | yes, all | none (zero width) |
| Simultaneous color ramps | 3 (`ground`, `facade`, `water`) | 1 |
| Unique colors on screen | 2,751 | 323 |
| Volumes with silhouette (gabled roofs, tiered tree canopies) | yes | no, straight prisms |
| Cast shadows | houses, trees, actors | actors only |
| Zoom | `0.7 … 2.8` | locked |

The outdoors accumulates six depth cues; the interior keeps only one (the
sprites' shadow ellipses). That is the whole difference.

---

## 3. Solution plan

Four phases, independent except where noted. Phase 1 fixes the root cause;
the rest raise the legibility bar. All the work falls in `src/pallet3d.cpp`
and `src/interior_scene.h`, with tests in `tests/interior_integration.h`
and `tests/interiors_qa.sh`.

### Phase 1 — Camera rotation in interiors (root cause)

Concrete work:

1. `src/pallet3d.cpp:46` — give `room_yaw` a non-zero initial value.
   Proposed starting point: `-0.32f`, the same as the outdoors, so entering
   and leaving a building doesn't change the player's mental orientation.
   Note to the implementer: that value is an **extrapolation** from the
   outdoors, not a measurement. The only interior rotation empirically
   confirmed to read as 3D is `≈0.4` (`ref-mart-2f-camera.png`, the current
   `clamp`'s upper bound). Decide the final value by comparing captures of
   map 40 at `-0.32`, `0.32`, and `0.4`.
2. `src/pallet3d.cpp:307` — in `update_meshes`, on a component change
   reset `room_yaw` to the default value, not to `0`.
3. `src/pallet3d.cpp:695` — remove the `large` condition. The original
   reason (that a small room wouldn't fit when rotated) is already
   covered: the `unit` calculation at `:526-527` includes `|cs|` and
   `|sn|` and reframes the whole room for any rotation. Keep the
   `clamp(room_yaw, -.4f, .4f)` clamp.
4. `src/pallet3d.cpp:696` — also enable the wheel (`room_zoom`) in small
   rooms, or deliberately leave it disabled and remove «Rueda Zoom» from
   the HUD (`:596`) when it doesn't apply.
5. `src/pallet3d.cpp:700` — `R` should reset to the default rotation, not
   to `0`.

Acceptance criteria:

- With the `build/qa/oaklab/logs/user-inside-lab.state` state, the smoke's
  `camera` mode produces a capture **different** from the default mode's
  (today they are identical: md5 `a3bb4ba3…f707` for both).
- In a capture of map 40, furniture side faces are distinguishable: for a
  1×1 furniture cell, the on-screen width of the constant-X face goes from
  0 px to ≥ 8 px.
- The `px_fuera_banda_verde` counter for `user-oaklab.ppm` does not go
  down; unique colors go up (the `shade(c,.75f)/.85f/.67f` shading in
  `box()` starts contributing tones).
- `tests/interiors_qa.sh` keeps passing, with all 179 catalog captures
  regenerated and reviewed.
- The 107 interiors currently locked respond to `Q`/`E`.

Controlled risk: review the narrowest rooms in the catalog (`width`/`height`
column of `build/qa/logs/interiors.csv`) to confirm none falls outside the
framing at `room_yaw = ±0.4`.

### Phase 2 — Palette by tileset family

Concrete work, in `create_atlas` (`src/pallet3d.cpp:182-198`):

1. Replace the four-`if` block with a ramp table indexed by tileset, with
   one entry for each of the 21 interior families. The current groupings
   mix lab, dojo, gym, and facility into a single green ramp (41 maps).
2. Use the atlas's three palette slots, which are wasted today: in
   interiors, slots 0 and 1 both receive the **same** `room`. Assign slot
   0 to the floor, slot 1 to walls and furniture, and slot 2 to special
   materials (water, glass, metal).
3. `floor_palette` (`src/pallet3d.cpp:85-95`) must return the correct slot
   for interiors instead of a fixed `0`, following the same logic it
   already applies to `scene.tileset==0/14/23` outdoors.
4. `interior_map` (`:206`) must request `tile_uv(scene, tile, slot)`
   consistent with the cell class returned by `interior::classify`.

Acceptance criteria:

- The color criterion is measured **against a capture taken after Phase
  1**, not against the current one: rotating the camera already raises
  the count on its own (§2.3.1), and attributing that gain to the palette
  would pass Phase 2 without any work. Steps: (a) after Phase 1,
  regenerate `user-oaklab.ppm` and record its new baseline with
  `build/qa/oaklab/stats.py`; (b) require Phase 2 to **double** that
  unique color count and bring the pixels outside the green/teal band
  above 15% (today 0.5%). Upper reference: the outdoors, 2,751 colors and
  41%.
- The 179 catalog captures show at least four color families
  distinguishable at a glance; the A1 report
  (`build/qa/interiors/logs/interior-families-*.png`) is regenerated.
- No regression in caves: `interior::cave` keeps using its gray ramp and
  water its own slot.

### Phase 3 — Textured sides and furniture shadows

Concrete work, in `interior_map` (`src/pallet3d.cpp:201-223`):

1. Pass a real detail `UV` to `box(...)` at `:212`, instead of the
   implicit `Solid`, so `detailed_quad` takes the textured branch. The
   outdoors already does this with `foliage` and `stone` (`:267`, `:271`).
   Keep `side` as a multiplicative tint, not as a substitute for the
   texture.
2. Extend the textured front face from `:217-220` to `Kind::Furniture`,
   and stop reusing the same tile on the top and the front: the top row
   of the 2×2 block belongs to the elevation (front face) and the bottom
   row to the plan (top face). That removes the duplication described in
   §2.3.3.
3. Call `shadow(scenery, …)` for every cell with `cell.height > 0`, sized
   with the cell's height, the same way `make_house:152` does.
4. Review the `interior_scene.h` rules for `case 5: case 7:` with the lab
   in front: today `top==0x0d` gives a shelf of 1.45 and `top==0x3b` a
   table of 0.65, without distinguishing a workbench from a countertop.

Acceptance criteria:

- In the regenerated `ref-oaks-lab-fp.png`, table sides stop being flat
  color.
- Every furniture or wall cell in map 40 casts a visible contact shadow in
  the orthographic capture.
- Map 40's vertex count stays within an agreed budget (today 5,154;
  shadows add 60 vertices per cell, 38 cells → +2,280; evaluate whether to
  use a single quad instead of `shadow()`'s 20-triangle fan).
- The interior catalog's presentation measurement does not worsen by more
  than 15% relative to the times recorded in
  `build/qa/interiors/logs/`.

### Phase 4 — Evidence and documentation

1. Replace `docs/screenshots/oaks-lab.png` (referenced in `README.md:44`)
   with a capture taken after phases 1-3. The current one documents
   exactly the defect the user is complaining about.
2. Add a camera assertion to `tests/interior_integration.h`: after
   entering an interior, `room_yaw` must be non-zero, and a `Q`/`E`
   sequence must change the rendered surface. Today `interior_journey`
   validates the map, resident meshes, component, and camera preference,
   but not that the camera is at a useful orientation.
3. Update `PALLET3D.md` («Las salas pequeñas se encuadran completas; las
   grandes permiten giro limitado y zoom»), which describes the lockout
   as a design decision without saying it affects 59.8% of interiors.
4. Record the result in `PLAN_INTERIORES_COMBATES.md`, «Registro de
   ejecución» section.

Acceptance criterion: a README reader unfamiliar with the project
identifies the interior as 3D without needing to read the text.

### Recommended order

Phase 1 first and alone: it's a change of a few lines, it fixes the
complaint, and it lets phases 2 and 3 be re-judged once the camera is
already correct. It's possible that with the rotation in place, Phase 3
drops in priority. Phase 2 is independent and can run in parallel. Phase 4
goes last.

---

## 4. Risks

| Risk | Mitigation |
| --- | --- |
| Rotating small rooms may leave the north wall covering the player, or push corners outside the framing | The `unit` calculation at `:526-527` already accounts for rotation; validate the CSV's smallest-area rooms before fixing the default value. The gold X-ray silhouette (`draw_world_frame:575-579`) already covers occlusion |
| A non-zero initial `room_yaw` changes all 179 catalog captures and the journey captures | Regenerate the full suite with `tests/interiors_qa.sh` and review contact sheets by family, as already done in A2 |
| Texturing the sides multiplies the vertex count and the presentation cost | `detailed_quad` adds no geometry (it reuses the alpha channel as the atlas cell index); the real cost is in the shadows. Measure with `CATALOG_BENCH_FRAMES` before and after |
| Touching `create_atlas` also affects the outdoors, which shares the function | The outdoor and interior branches are already separated by `current.interior`; keep the separation and cover it with `pallet_state_test` |
| Another agent (Codex) is editing `src/` and `tests/` in parallel | This diagnosis was made against binary `sha256 eb993e75…9375`, frozen in `build/qa/oaklab/`. Re-verify the analysis is still valid before implementing |
| The walk's coordinates depend on the user's specific save | The intermediate state `build/qa/oaklab/logs/user-inside-lab.state` reproduces the case without repeating the walk |
| Changing the color ramp may break the reading of caves and of the dock | `interior::cave` and `floor_palette` already handle those cases separately; include the cave and the elevator in the visual review (`interior-transitions`) |

---

## 5. Evidence index

All under `build/qa/oaklab/`:

```
logs/inputs.sha256               hashes of the executable, ROM, and states used
logs/probe-user-state10.log      real save: map=0 xy=8,15 view=3 font=0
logs/probe-user-state1.log       earlier real save: map=0 xy=9,8 view=3
logs/probe-oaks-lab.log          A1 fixture: map=40 xy=5,11 view=3
logs/walk-step1..11.log          route reconstruction up to the door
logs/pallet-at-lab-door.state    user's save in Pallet Town (12,12)
logs/walk-enter-lab.log          crossing trace: state 3 -> 0 -> 1 -> 3
logs/user-inside-lab.state       user's save inside map 40
logs/user-oaklab.ppm             capture inside the lab
logs/user-oaklab-plain.ppm       capture without touching the camera
logs/user-oaklab-camera.ppm      capture after E x10 + wheel (identical)
logs/user-pallet.ppm             outdoor capture, same save
user-oaklab.png                  PNG of the previous one
user-pallet.png                  PNG of the outdoors
ref-oaks-lab.png                 converted A1 fixture
ref-oaks-lab-fp.png              first person in the lab
ref-lab-return.png               exit from the lab
ref-mart-2f.png                  large interior with room_yaw = 0
ref-mart-2f-camera.png           the same with room_yaw ~ 0.4
ref-exterior-final-camera.png    reference outdoor capture
rebuild/                         re-verification against the 11:38 binary
ppm2png.py  stats.py             conversion and measurement utilities, no dependencies
```

ROM data reused: `build/qa/logs/interiors.csv` (audit of the 179 interiors,
generated previously by `build/interior_audit`).
