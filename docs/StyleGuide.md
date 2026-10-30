# Style guide

The art direction of **EONLORE: The Verdant Heir** in the Hydro engine. It covers the look we are aiming for, the rules every new asset follows, and how assets are made and imported today. The target is **HD-2D**, the style of *Octopath Traveler II* and *Final Fantasy Resonance*: pixel-art characters and props inside 3D places, lit and framed like a miniature diorama.

Updated October 30, 2026 (V3: HD-2D direction; replaces the V2 2D-renderer guide of Oct 25). Read [Sprite sizes](SpriteSizes.md) before changing asset dimensions. Engine features this page depends on are marked **(engine: done)** or **(engine: to do, #issue)**.

## The look in one paragraph

A small, hand-made world seen like a **tilt-shift photograph of a diorama**. Crisp pixel-art people stand in 3D streets, fields and shrines whose surfaces are also pixel art (32 texels per metre, nearest filtered). The light does the storytelling. A low warm sun casts long real shadows. Windows and lanterns throw amber pools against cool blue-green shade. Highlights bloom softly, and the foreground and background melt into depth of field, so the eye stays on the lit stage where the player stands. Places are dense and lived-in, with verticality: terraces, stairs, bridges, roofs you look down on. The camera is authored per area and moves for story moments. **Sprite pixels stay crisp; light, fog, blur and bloom may be smooth.**

## What we take from each reference

References are for principles, never for copying. Use original designs, and never store game screenshots in the repository.

| From *Octopath Traveler II* | From *Final Fantasy Resonance* |
| --- | --- |
| The diorama frame: tilt-shift depth of field, vignette, a lit centre with darker edges | Lighting, depth of field and bloom are **tuned per scene** until sprites and 3D look like one space, not two layers |
| Pixel characters about 3 heads tall, with lots of personality in small idle and walk loops | Sprite orientation and placement are checked **per camera angle**, so nobody seems to float above or sink into the ground |
| Environments built from 3D geometry with **pixel-art textures** at the same density as the sprites | The camera is planned early (in the blockout) and is allowed to move and rotate in towns, dungeons and events |
| Strong times of day and weather per region; the same town reads differently at dusk and at night | Event scenes are carried by character acting and camera work, not fades to black |
| Dense set dressing in clusters that tell small stories (laundry, crates, carts, flower boxes) | Big set pieces (summons, airships, ultimate attacks) can be **pixelized 3D models** ("cinematic pixel"), so they still read as pixel art |
| Battles in a 3D arena that comes from the field area, with weakness and shield UI | Battles use dynamic camera on big skills; characters can be taller and more expressive in cinematics |

## World scale and pixel density

| Thing | Rule |
| --- | --- |
| Density | **32 texels per metre** for every field asset (sprites, shape materials, model textures), within ±20 % (25.6–38.4): [world density](SpriteSizes.md#world-density-32-texels-per-metre-decided-2026-10-30). Check with `scripts/check_texel_density.py`. A sprite's world size is its transform scale; keep `PixelsPerUnit: 32` and use **Set Native Size**. |
| Filtering | Nearest filtering and clamp on all pixel art; no mipmaps on sprites. |
| Characters | Adults 50–58 px tall (1.6–1.8 m), Miko 52 px. Children about 40 px. Proportions about 3 heads. |
| Props | Real-world size: barrel 0.6 x 0.9 m (about 20 x 29 px), crate 0.8 m, hay bale 0.9 x 0.65 m, door 1.1 x 2.2 m, a one-storey cottage 6–7 m wide. A prop may be shown down to 26 px/m to keep a footprint; never above 38. Approved barrels (Oct 30, 2026, at 30 px/m, never mirrored): `FarmVillage/Props/BarrelA`-`D`, `BarrelSideA`-`D`, `BarrelKeg`, `BarrelStackA`-`C`, `RainBarrelA`-`B`; plain brown wood with dark iron hoops only. |
| Exceptions | Written in the [Sprite sizes](SpriteSizes.md) audit table, with a reason (today: the wheat at about 42 px/m). |

## Camera

- **Field:** a tilted perspective-style view showing about **12 m vertically** at the focus point, so a 1.6 m character fills about 13–14 % of the screen. Roofs, terraces and the front faces of buildings must all be visible. Check legibility at the target resolution and in motion.
- **Authored per area:** each map gets its camera during blockout, before set dressing. The route and the next landmark should be readable from the framing alone.
- **Cinematic cameras** (events, big skills) may dolly, tilt and orbit. Every angle used is checked for sprite grounding: feet on the ground, contact shadow visible, no paper-thin sprites side-on. Redraw key poses for a special angle rather than letting a sprite turn edge-on.
- **Battle:** perspective, about a 30° field of view, party on the right facing left; the camera pushes in on big skills.
- **Standing sprites (decided Oct 30, 2026):** in the field, trees, props and people stand **straight up** like cardboard standees in a model and only turn left and right to the camera (Render Settings > Upright sprites = Stand vertical), as in Octopath Traveler. They no longer lean back with the camera's tilt, which made them look propped against what stands behind them. Seen from the field camera (55 to 70 degrees from straight down) a standing sprite looks 6 to 18 % shorter than drawn; draw for that rather than stretching it. Hero trees go further: a 3D trunk with pixel-art foliage cards (V9 representation policy). The battle stage still uses leaning sprites (its actor placement expects them).

## Depth, light and post-processing (the HD-2D layer)

This layer is what makes the art read as HD-2D; without it the same assets look like a flat top-down RPG.

| Element | Rule | Engine |
| --- | --- | --- |
| Sun | Low, warm key light from the **upper left** of the screen. Long real shadows from buildings, props and sprites (`SpriteShadows: AsDrawn` or `FaceSun`). | done |
| Contact shadow | Every standing character and free-standing prop has a `Shadow2DComponent` at its feet, sized to its footprint. | done |
| Local light | Lanterns, windows and braziers are point lights in lantern amber `#F2A43A`; magic and spirits are cool `#45C7E8`. Place a light wherever the art shows a source. | done |
| HDR and bloom | Bloom only on emissive sources and the brightest highlights; never a general haze. PBR Neutral tonemap keeps the palette intact below the highlights. | done |
| Fog | Distance haze measured from the focus point, coloured by the time of day (warm at dusk, blue at night). | done |
| Depth of field | **Tilt-shift:** sharp band on the player's plane; foreground and background blur with distance. Never blur the active gameplay plane, UI or the selected target. | to do, #135 |
| Vignette | Subtle darkening at the screen corners on every field camera. | to do (H9) |
| Colour grading | One grade per region and time of day (render profiles, then a LUT). | profiles done; LUT to do, #137 |
| Atmosphere | Light shafts through trees and windows, dust motes, falling petals or leaves, chimney smoke. | to do (H9) |
| Pixel stability | Sprite and texture pixels must not shimmer while the camera moves. | to do, #136 |

**Times of day.** **Dusk** is the default look. Day is clear and bright with soft haze; night is blue-teal (never black), carried by the lanterns and windows. Each scene has `Day`, `Dusk` and `Night` render profiles.

## Palette and value

- **Warm against cool:** warm light (sun, lanterns, windows) against cool shade. Shadows lean blue-green or violet, never grey.
- **Saturation by importance:** the party is the most saturated (Miko's red and white), villagers are muted earth tones, and the environment is the most muted. Saturated accents (flowers, banners, produce) go in small clusters.
- **Value structure:** the playable centre is the lightest; edges and foreground occluders are darker. A character must separate from the ground in value at native size, without post-processing.
- **No pure black or white** in base art, except the hottest centre of a light or spark.
- **Ramps:** each material uses one ramp of 3–5 shades (characters may use about 24 colours in total); no per-pixel confetti. The old ramp table lives in `scripts/diorama_rpg/style.py` as reference.

## Pixel rules (all sprites and textures)

1. **Binary alpha.** A pixel is opaque or empty. Check the alpha channel of anything a tool exports; a painted checkerboard is not transparency.
2. **Selective outline.** Characters and props get a 1 px outline in a darker shade of their own colour (dark brown or deep green), not pure black. Inner detail lines only where they explain form.
3. **Clusters, not noise.** Large deliberate pixel groups, 3–5 shades per material, dithering only on large surfaces and never on faces.
4. **Shading follows the light:** lit upper-left, shaded lower-right, a darker band under every overhang.
5. **Made at the final size.** Generate or draw at the size it will be used. A 2x downscale with outlines kept is allowed as a rescue (characters, props), but not for thin shapes like wheat or grass. Never use a blurry or fractional upscale.
6. **Feet or base on the bottom row** of the cell, pivot `[0.5, 0]` for characters. Single-image props end on their bottom row.

## Characters

| Thing | Rule |
| --- | --- |
| Shape | Semi-chibi, about 3 heads tall, big readable head, simplified hands and feet; one clear silhouette feature per character (hat, staff, basket, beard). |
| Authoring | **64 x 64 canvas**, low top-down view, 8 or 4 directions (PixelLab: same settings as Miko). |
| Field sheet | `Characters/Villagers/<Name>.png` (heroes in their own folder): **96 x 72 cells**, rows Down, Right, Up, Left, columns 0–3 idle and 4–7 walk; feet on the bottom row; pivot `[0.5, 0]`; scale **3 x 2.25** (32 px/m). Idle-only NPCs: one row facing the camera. |
| Animation | Idle 4 frames at 4 fps (breathing, a blink); walk 4–8 frames at 8–10 fps. Clips are `Idle<Dir>` / `Walk<Dir>` in `Characters/<Name>.hyanim`. |
| Battle sheet | A separate, larger set: Miko's battle sheet uses 96 x 96 cells with a fighting-stance idle (`BattleIdle`), walk, kick, punch and death. Never reuse the field idle in battle. |
| Costume | Medieval fantasy: wool, linen and leather in earthy dyes (cream, oat, russet, moss, woad blue, walnut). No modern clothing. Shrine culture shows in paper charms, wooden tokens and spiral symbols. |
| Acting | Small, believable motion, as in OT2. Comic exaggeration only in jokes; serious events use posture and pauses. |
| Portraits | Dialogue portraits are separate high-resolution art; keep face, hair and accessories consistent with the sprites. |

## Environments

**Structure is 3D, surfaces are pixel art.** Buildings, terrain, walls, bridges and wells are meshes (the medieval kit, `MedievalVillage/Models/Source/MV_Kit`, built by `_scripts/build_kit.py`). Their textures follow the 32 texels per metre rule and the pixel rules above.

| Element | Rule |
| --- | --- |
| Buildings | One material language per storey: a stone ground floor, a timber-and-plaster upper floor, then the roof; no random mixing within a wall. Read depth from the architecture: deep window frames with the glass set back, jettied upper floors on joists, thick sills, chimneys with caps. |
| Roofs | Rows of shingles or thatch with variation (moss, repairs, darker patches), a ridge cap and deep shadow under the eaves. Roofs fill much of the screen; give them detail. |
| Contact shading | Darker at the foot of walls, under eaves and in corners (vertex colour AO in the kit). |
| Terrain | Blended materials, never straight borders (terrain system, to do). Grass overhangs path edges; pebbles, tufts and darker soil where ground meets walls. |
| Water | Stylised and animated: a dark-to-light ramp, moving highlights, a light line along the edges, darker under bridges and overhangs. |
| Vegetation | Trees, bushes, flowers and crops are upright sprites at 32 px/m (26 px/m allowed to keep a big tree's footprint). Their sun is painted on the upper left, so **never mirror them** (FlipX off); variety comes from more variants, at least 3 per kind. Approved style (Oct 30, 2026): `ShrineGlade/Foliage/OakSunA`, `OakSunB`, `OakSunC`, `OakGnarledSmall`, `OakYoungB`, `LindenAutumnA`-`F`, `FirTallA`-`D`, `FirSmallA`-`G`, `SakuraWeeping`, `BushDense01`-`32`, `BushSmall01`-`12`, `ShrubLargeA`-`D`, `FlowerBush01`-`28`, `TopiaryA`-`D`, `Lavender01`-`28`, `HedgeA`-`P`, `GrassTuft01`-`24`, `Fern01`-`16`, `Wildflowers01`-`24`, `FlowerClump01`-`24`, `LeafyPatch01`-`16`, `Seedling01`-`08`, `LowGrass01`-`12`, `LowGrassShade01`-`06`, `LowGrassFlower01`-`12` (low grass strips for wall feet and edges, 26-38 px/m); generate plants with `art_incoming/style_refs/palette_foliage_32.png`, not the 48-colour palette (it pulls mint and lemon); the prompts for the rest are in `art_incoming/style_refs/FOLIAGE_PROMPTS.md`. Later: 3D trees (trunk mesh, cut-out leaf cards with outward normals) for foreground and village trees. |
| Ground details | Small decal sprites (pebbles, leaves, flowers, cracks) break up every large surface. |

**3D or sprite for a prop?** Pick one per kind of prop for the whole game:

- **Sprites:** free-standing small props seen from the front, at 32 px/m, upright, with a contact shadow. Examples: barrels, crates, sacks, baskets, buckets, tools, hay bales, crops, signs.
- **3D kit models:** architectural and large things, and anything a cinematic camera orbits. Examples: wells, carts, market stalls, fences, bridges, lantern posts.

The village's barrels are sprites now; `MV_Barrel` is left only behind the inn's bar (a 3D interior). Replace `MV_Crate` with sprites so the two styles stop mixing outdoors.

## Composition (towns, fields, dungeons)

- **Diorama frame:** dark foliage or architecture at the screen edges and in the foreground, a lit playable centre.
- **Verticality:** terraces, stairs, raised squares, bridges, roofs below the camera. Every area has at least two height levels.
- **Paths lead the eye** to the next landmark; they curve and narrow, never ruled lines.
- **Clusters that tell a story:** a crate stack with a sack and a broom by a door, laundry between two houses, a wheelbarrow and watering can by the garden. Quiet open space between clusters.
- **Landmarks:** one per area (the well and square, the bridge, the shrine's torii).
- **Light pools:** every lantern and window is a warm pool at dusk; the space between stays cool.
- **People in context:** NPCs stand where their job is (the farmer at the field's edge, not inside the rows), never inside props or on wall tops, and with natural spacing between them.
- **Walkable space stays readable:** anything that blocks movement looks solid and has a collider and a shadow; small decoration is low and has no collider.

## Battles

- The arena is built from the field area the fight starts in (biome, light, props), not a generic oval.
- Party on the right facing left, enemies on the left; enemies are larger than the heroes.
- Three depth bands: a blurred far background, the midground stage, and foreground framing that never covers the UI.
- Skills: wind-up, impact (a brief hit pause), recovery. Big skills push the camera in and may flash the scene's light. Effects use sprite flipbooks plus real lights and bloom.
- UI: black glass panels `#15120c` with gold `#c99a3f` frames and parchment text `#f3e3b8`; shields and weakness icons under each enemy; damage numbers white (normal), orange (weak), grey-blue (resisted), green (heal).

## Asset production

### Generating with PixelLab (characters, props, crops)

- Characters: **Create Character**, size **64 x 64**, view **low top-down**, 4 or 8 directions; then animations on that character (idle, walk). Paste the shared style block below, then one character line.
- Props, crops and foliage: **create image** at the final size (for example 32 x 32 for a barrel, 64 x 64 for a tree clump); one object per image, or one row of objects on a transparent background.
- Export the PNG (and the JSON for characters) into `art_incoming/` and let Claude import it.

Shared style block:

```text
Authentic hand-pixeled 32-bit JRPG sprite in the HD-2D style of Octopath Traveler, semi-chibi proportions
about 3 heads tall, large deliberate pixel clusters, limited palette with 3 to 5 shades per material,
selective dark outline in a darker shade of each colour (not pure black), soft cel shading lit from the
upper left, warm muted medieval-fantasy colours, crisp aliased edges, transparent background.
Do not add anti-aliasing, smooth gradients, painterly shading, tiny noisy detail, high-definition pixel art,
cast shadows, ground, text or modern items.
```

### Importing (conventions)

| Asset | Import |
| --- | --- |
| Character field sheet | `Characters/Villagers/<Name>.png`, `.hymeta` Grid 96 x 72, RowNames `<Name>_Down/Right/Up/Left`, DefaultPivot `[0.5, 0]`, Nearest, Clamp, PPU 32; clips in `Characters/<Name>.hyanim` |
| Prop or crop sprite | `<Area>/Foliage/<Name>.png` or `<Area>/Crops/<Name>.png`, SpriteMode Single, Nearest, Clamp, PPU 32; scale = pixels / 32; Upright facing; contact shadow |
| Texture for a mesh or shape | Snap fake pixel art with `scripts/snap_pixel_texture.py`, then set Tiling for 32 texels per metre (the script prints it) |
| Kit model | Edit `build_kit.py`, run it headless; the importer re-extracts textures when a `.glb` changes |

Code-generated art (`scripts/diorama_rpg/`, `scripts/shrine_glade/`, `scripts/farm_village/`) follows the same rules; it predates this guide and is replaced as new art arrives.

### Review checklist (before an asset is merged)

1. Binary alpha, no painted checkerboard, nearest filtering.
2. Density 32 px/m (±20 %) or an exception written in [Sprite sizes](SpriteSizes.md); `check_texel_density.py` passes.
3. Feet or base on the bottom row; correct pivot; it stands on the ground in a capture (`Oxygen --scene <Scene> --play --capture out.png`).
4. Outline, shading direction and palette match Miko and the villagers side by side.
5. Seen in the game at dusk and at night: still readable, lights placed where the art shows sources.
6. `Oxygen --validate` reports no missing assets.

## When the renderer grows

Keep the density, palette, light direction, pivots and composition rules. As depth of field, vignette, grading and atmosphere arrive (#135, #137, H9), tune each scene's render profile until sprites and 3D share one space, the way *Final Fantasy Resonance* tunes per scene. Depth of field supports composition but does not replace it. Cinematic pixel (pixelized 3D models for summons, airships and ultimate attacks) is a later step, after the field and battle look are solid.
