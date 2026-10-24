# Style guide

The art direction of the Diorama RPG ([plan](DioramaRPG.md)), the engine's first full test project. It says what the game should look like, the rules every new asset follows, and how assets are made today, so that the art keeps moving towards one look while the engine grows (2D sprites now, depth, meshes and post-processing later). The palette and constants in this page are code: `scripts/diorama_rpg/style.py`. When they change, change both.

Written October 25, 2026. VFX get their own section once the particle work lands ([Planned](#planned-vfx)).

## The look in one paragraph

A small, hand-made world seen like a **miniature diorama**: pixel-art characters in front of richly textured, softly lit places. Late-afternoon and dusk light: **warm lantern and window light against cool green-blue shade**. Surfaces are worn and lived-in (cobbles with moss in the joints, mossy terracotta roofs, weathered plaster, ferns at the foot of trees). The edges of a scene fall into dark foliage, so the playable space reads as a lit stage. Every material is a few-step colour ramp with crisp pixels, never a smooth gradient or a flat fill.

Reference points (for their qualities, not to copy): the concept art in `docs/images/` (the project's own), *Octopath Traveler* and *Final Fantasy Resonance* (dark foreground framing, tilt-shift depth, lantern pools, side-view battles with large enemies). Screenshots of other games are not kept in the repository.

![Village concept](images/diorama_concept_village.jpg)

## Camera and scale

| Thing | Rule |
| --- | --- |
| Pixel density | **32 px per world unit** (`PixelsPerUnit: 32`) for everything in the field. Nearest filtering, no mipmaps |
| View | Top-down three-quarter: the ground is seen from above, buildings, props and characters show their front and their top. Later the same sprites stand up as cards under a tilted camera (Diorama RPG plan, H1 and H2), so draw them so they also read from the front |
| Field characters | 32 x 56 px (1 x 1.75 units), feet on the bottom row, pivot `[0.5, 0]`. Four directions (Down, Up, Left, Right = mirrored Left), idle + 4-frame walk (stand, step, pass, step) |
| Battle characters | 64 x 64 px cells, side view facing left (the party stands on the right), pivot at the feet. Poses: Idle (2), Attack (4), Cast (3), Hurt, Guard, Down |
| Enemies | Field: 32 x 32. In battle they are drawn larger than the heroes (bosses much larger), facing right |
| Props | Sized to their real size at 32 px per unit: a barrel is about 0.75 x 1, a house 4 to 5 units wide, a tree 2 to 3 wide |
| Camera | Orthographic, about 11 units visible vertically (the village camera), so a character is about 1/6 of the screen height |

## Light

- The key light comes from the **upper left** (`style.LIGHT`). Every sprite is shaded for it: lit top-left edges, darker bottom-right, a darker band where a roof or canopy overhangs.
- **Dusk** is the default time of day. Day, dusk and night are presets of the global light (`VillageDirector`, keys 1 to 3); night is blue-teal, never black.
- Warm light sources (lanterns, windows, braziers, torches) are point lights in **lantern amber** `#F2A43A` and are placed where the art shows the source. Magic and spirits are cool: **magic blue** `#45C7E8`.
- Sprites are drawn to be lit: mid-value colours, so a warm light can brighten them and the night can darken them without losing the silhouette.
- Every standing thing has a blob shadow (`Shadow2DComponent`) at its feet.

## Palette

The ramps live in `scripts/diorama_rpg/style.py`; a material uses one ramp, never a single flat colour. The GDD's palette sheet is `docs/images/diorama_palette.jpg`.

| Ramp | Colours (dark to light) | Used for |
| --- | --- | --- |
| Grass | `#1b2a19 #26391f #334a26 #41592b #53692f #6b7d36 #8a923f` | ground, bushes |
| Moss | `#2b3d1e #3f5527 #5a7030 #7d8c3a` | cobble joints, roofs, stones |
| Dirt | `#3e2c1c #57402a #705336 #8A6740 #a07c4e #b6925a` | roads outside the village, arena, path rims |
| Cobble | `#38342d #4f493e #665e4f #7d7461 #9B9074 #b5a986` (joints `#3a342b`) | streets, plaza |
| Terracotta | `#3a160e #5A2417 #83391f #A44D27 #c2632f #D07A3C` | roofs, pots |
| Plaster / timber | `#8a7a62 ... #e2d2b0` / `#2a1a10 #3B2317 #5a3820 #7A421F` | timber-frame houses, carts, fences |
| Stone | `#3e3b36 #5a564e #7a756a #9a9384 #b9b19f` | foundations, walls, rocks |
| Foliage | `#132013 #1f3320 #2f4a2a #4a6a30 #76913a #b0b04a` | trees, ferns, leaves |
| Forest shade | `#101a12` | the map's edges, foreground leaves |
| Light | lantern `#F2A43A`, window glow `#c97a2a #f2a43a #ffd27a #fff0c0`, magic `#45C7E8` | light sources |
| UI | black glass `#15120c`, gold `#c99a3f`, parchment `#D9BE83`, text `#f3e3b8` | menus, battle UI |

## Pixel rules

1. **Binary alpha.** A pixel is opaque or empty; no soft edges, no semi-transparent fringe. Never trust a "transparent" image from an AI tool: check the alpha channel (a painted checkerboard is not transparency).
2. **Inner outline.** Sprites get a 1 px outline inside their silhouette, made by darkening the sprite's own edge colour (about 0.45x), not pure black. Ground textures have no outline.
3. **Ramps, not gradients.** 4 to 7 steps per material, chosen with `style.step_ramp`. Light dithering (a 4 x 4 Bayer pattern) only to break up large flat areas.
4. **Texture at the right scale.** Detail is 1 to 3 px: grass blades, stone joints, wood grain, roof shingles. Nothing finer than a pixel and no blurry upscales.
5. **Shading follows the light.** Upper-left lit, lower-right shaded; each stone, shingle and leaf clump has its own little highlight and shadow.
6. **No pure black, no pure white,** except the hottest centre of a light or a spark.
7. **Readability first.** Characters must stand out from the ground: warmer or brighter than what is behind them, with a darker outline.

## Materials

| Material | How it is drawn (see `scripts/diorama_rpg/env.py`) |
| --- | --- |
| Grass | Mid-scale noise picks steps from the grass ramp; dark clover clumps; short vertical blades (light tip over a darker base); a few wild-flower groups; big soft patches of sun and canopy shade over everything |
| Cobblestone | Irregular stones (Voronoi cells, about 8 px in streets, 11 px in the plaza), each with its own tone from the cobble ramp, lit on its upper left and darker on its lower rim; thin dark joints with moss near the grass |
| Dirt road | Dirt ramp with fine noise, pebbles (a light pixel over a dark one), darker toward the grass |
| Path edges | Grass overhangs the first one or two pixels of every path, so edges are ragged, never a clean line |
| Roofs | Terracotta shingles in rows, moss in patches, darker eave line, a 2 to 3 px shadow on the wall below the eaves |
| Walls | Timber frame (dark beams) and weathered plaster; a rough stone foundation; lit windows with flower boxes on the ground floor |
| Foliage | Clumps lit from the upper left with a rim of the lightest ramp step; ferns, tall grass and mushrooms at the foot of trees |
| Water | Deep teal ramp `#123A3C #1E6565 #7BC9B8`, animated (the pond sheet), a stone rim |

## Composing a map

What makes the village read as a diorama and not as a tile map:

- **A frame.** The ground darkens over the last 5 units toward the map's edges, and dark foreground leaves (`Canopy *`) overlap the bottom, top and sides. The player sees a lit stage inside a dark forest.
- **Paths lead the eye.** Cobbled streets in the village, packed earth outside it; paths are irregular, never straight lines.
- **Clusters, not scatter.** Set dressing gathers where people live and work: crates, barrels and carts by the market and the houses, hay bales by the field, ferns and mushrooms at the foot of trees, flowers along paths. Open grass between clusters stays quiet.
- **Light pools.** Every lantern, window and brazier is a warm pool on the ground at dusk; the space between them stays cool and dim.
- **Landmarks.** Each area has one thing to recognise it by: the statue plaza, the pond and bridge, the arena.
- **Walkable space stays readable.** Decoration without a collider is small and low (ferns, grass, flowers); anything that blocks movement looks solid and has a collider and a shadow.

## Battles

- Side view: the party on the right facing left, enemies on the left facing right, all standing on the backdrop's floor (`Battle_Background.png`: a forest clearing with an earth arena, framed by dark leaves, warm light from one side).
- Enemies are larger than heroes. Each shows its HP bar, shield count and weakness icons above it; heroes show HP, MP and Momentum pips above them.
- UI: black glass panels with a gold frame, the 5 x 7 pixel font (`art.py`), parchment text. Damage numbers: white normal, orange **weak**, grey-blue **resisted**, green **heal**. Banners: WEAK! orange, BREAK! red.

## UI

Black glass `#15120c` panels with a gold `#c99a3f` frame and bright gold corners; parchment text with a dark drop shadow. Until the engine has text (U1), labels, icons and numbers are sprite sheets (`DioramaRPG/UI`), drawn with the pixel font in `art.py`; later the fonts from the GDD (Cinzel for titles, EB Garamond or Cormorant for menus, all SIL OFL) replace it.

## Planned: VFX

Particles and effects are in progress (Roadmap V1 to V5). Until this section is written, follow the light rules: additive glows in lantern amber or magic blue, effects short and readable, never hiding the characters, dust and fireflies sparse.

## How assets are made today

All the Diorama RPG art is drawn in code, so it can be re-generated and tuned:

| Script | Makes |
| --- | --- |
| `scripts/diorama_rpg/style.py` | The palette, light direction and pixel density (this page as code) |
| `scripts/diorama_rpg/env.py` | Ground repaint, the house detail pass, set dressing (ferns, tall grass, mushrooms, hay bales, hand cart, hanging lantern post, foreground canopy) |
| `scripts/diorama_rpg/regrade.py` | Recolours the older village art towards the palette (from its version at commit `5307eb8`) |
| `scripts/diorama_rpg/heroes.py`, `art.py` | Orin and Lila, the Spirit Wisp, the battle backdrop, the battle UI sheets |
| `scripts/diorama_rpg/build.py` | Runs all of the above and edits `StatueVillage.hyscene` (`--battle` also rewrites `Battle.hyscene`) |
| `scripts/diorama_rpg/preview_scene.py` | An offline mockup of a scene (approximate lighting) for checking the look without the editor |

To add an asset: draw it with `lib.Canvas` and the ramps from `style.py`, save it with `build.save_texture` (the handle comes from the path, so re-running keeps it), and place it from `build.py` with an entity tag the script owns (`Decor *`, `Canopy *`), so a re-run replaces it instead of duplicating it. Close the village scene in the editor before running `build.py`, or the editor saves over it.

Hand-made art (Aseprite, Krita, AI-assisted) follows the same rules; check before importing: binary alpha, 32 px per unit, the outline, the palette, the light from the upper left.

## When the renderer grows

What stays the same when the engine gets the diorama camera, depth, meshes and post-processing (Diorama RPG plan H1 to H8): the pixel density, the palette, the light direction, binary alpha (it becomes alpha cut-out with depth), and the composition rules. What changes: buildings, props and characters stand up as cards facing the camera; ground and walls can become meshes with these same pixel textures (nearest filtering); the dark frame and tilt-shift come from depth of field instead of painted shade; lantern pools come from real lights. Draw new assets so they also work standing up: no baked ground shadow inside the sprite, feet on the bottom row.
