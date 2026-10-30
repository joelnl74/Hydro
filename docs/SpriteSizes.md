# Sprite sizes

Updated 2026-10-30. The size rules for the [Diorama RPG](DioramaRPG.md) art; the look is in the [style guide](StyleGuide.md). **These are our own authoring sizes, not a claim about any other game's sprite sizes.** Treat sizes as source-canvas pixels. Final apparent screen size depends on camera zoom and draw scale.

## World density: 32 texels per metre (decided 2026-10-30)

Every field asset is drawn at **one density: 32 texture pixels per metre**, within **+-20 %** (25.6 to 38.4). The world unit is the metre (the character body, fog, material tiling and the camera all use metres). One rule for sprites, shapes and models, so a crate, a wall and a character show pixels of the same size on screen.

- **Sprites**: the renderer draws a sprite as a 1 x 1 m quad times the transform's scale. `PixelsPerUnit` does **not** change the drawn size; it is only what the inspector's **Set Native Size** button uses. So keep `PixelsPerUnit: 32` on field art and size sprites with Set Native Size (scale = sprite pixels / 32), never by eye.
- **Built-in shapes** (Plane, Box, Stairs): texture width x the material's `Tiling` (repeats per metre) = 32. A 320 px grass texture takes `Tiling: 0.1`.
- **Models** (the medieval kit `.glb`s): UVs authored at 32 texels per metre in `MV_Kit.blend` (`build_kit.py`). Decals that are one picture (door, window glass, signs) may be up to 1.25x finer.
- **Check it**: `python scripts/check_texel_density.py Oxygen/project/Assets/Scenes/FarmVillage.hyscene` lists every sprite, shape material and model material outside the tolerance (`--all` lists everything, `--strict` fails the run).

**Proportions** (measured on Octopath Traveler II screenshots for the overall feel, not its sprite sizes): a door is about 1.35x the character's height, a field character fills about 13-14 % of the view height, and the environment's pixels are about the size of the characters' pixels. With our 32 texels per metre that means an adult of 1.55 to 1.75 m is **50 to 56 px tall**, a 2.2 m door is 70 px, and the field camera shows about 12 m vertically (`OrthographicSize` about 12).

### Audit, Farm Village (2026-10-30, `check_texel_density.py`)

| Asset | Texels per metre | Action |
|---|---:|---|
| Medieval kit models, roads, plots, ground, walls, flagstones | 32 (stairs 40) | In range; keep. `StepStone.hymat` Tiling 0.248 -> 0.199 |
| Miko (`Characters/Miko/Miko.png`, 96 x 96 cells, body 52 px) | 43.6 (scale 2.2: she is 1.19 m, the door 1.8x her height) | Scale **3.0** (1.63 m). Her cells have 16 px of empty space under the feet and pivot `[0.5, 0.5]`; an Upright sprite stands on the quad's bottom edge, so fix the pivot/rect at the same time (feet on the bottom row, pivot `[0.5, 0]`) and check in the editor |
| Villagers (`Characters/Villagers.png`, 256 x 384 cells) | 178 | Old high-resolution art, 5.5x finer than everything else: redraw at the field size (50-56 px adults, 96 x 96 cells like Miko) |
| Pines, oaks (`ShrineGlade/Foliage`) | 19-31 (median 23-28) | Redraw the trees at 32 (a 10 m pine is 320 px tall), or as a stopgap shrink the outliers until they are at least 25.6 |
| Bushes, moss, lavender | 22-41 | Normalise the scale per sprite (Set Native Size), vary with flips and different sprites, not with scale |
| Grass tufts, flowers, ferns, meadow strips | 25-46 | Same: one scale per sprite; most are scaled down too far |
| Wheat (`FarmVillage/Foliage/wheat_*`, 64 px) | 40-48 | Redraw at 32-40 px tall (1.0-1.25 m wheat); scaling the 64 px art up to 32 would make 2 m clumps |
| Slime, Wisp | 23-25 | Within reach: scale down to 32 |
| Sunset backdrop | 19.5 | Far background; exempt (distance makes it finer on screen) |

## Current state (audit, 2026-10-25)

What is in the repository today, against the targets below.

| Asset | Now | Target | Status |
|---|---|---|---|
| Field heroes, villagers (`StatueVillage/Characters`, `DioramaRPG/Characters`) | 32 x 56 cells, 32 PPU, pivot `[0.5, 0]` | Profile A | Done |
| Old `StatueVillage/Player.png`, `Villagers.png` | 256 x 384 cells, 128 PPU | Profile A | Old art; not used by the Diorama RPG scenes |
| Battle hero Miko (`DioramaRPG/Battle/Miko_Battle`) | **128 x 128** cells, 64 PPU, drawn at 8 x 8 units (she stands about 110 px, 7 units); pivot at the feet `[0.6, 0.031]`, right of centre so the strike has room in front | Battle V2 | Done (October 26, 2026). Cut from her 181 px source atlas by `scripts/shrine_glade/battle.py` (the west-facing action swing; the stance with a breath for Idle, recoil shifts for Hurt, the stance turned on its back for Down), one 32 colour palette. Rows: Idle 4, Anticipation 2, Attack 6, Cast 5, Hurt 3, Guard 2, Down 3, with Orin's per-frame timing so the code's contact and peak times hold |
| Battle heroes Orin, Lila (`DioramaRPG/Battle`, no longer in the party) | **96 x 128** cells, 64 PPU, drawn at 6 x 8 units; pivot at the feet (Orin `[0.656, 0.031]`, Lila `[0.677, 0.031]`, the body stands right of centre so the weapon has room in front) | Battle V2 | Done (Milestone B). Rows: Idle 4, Anticipation 2, Attack 6, Cast 5, Hurt 3, Guard 2, Down 3, and Heal 4 for Lila. Per-frame timing in the `.hyanim` |
| Slime | 32 x 32, pivot `[0.5, 0.125]` | 96 x 96 | Not started |
| Spirit Wisp | 32 x 32, 6 frames, pivot `[0.5, 0.12]` | 96 x 96 | Not started |
| Battle effects (`FX_Slash`, `FX_Heal`) | 64 x 64 cells, 5 and 6 frames, binary alpha | - | Done (Milestone B) |
| Battle UI text (`BattleText`) | 128 x 16 cells | 16-24 px glyph height | Close |
| Battle icons (`BattleIcons`) | 16 x 16 | 16-24 (weakness), 32-48 (action) | Weakness size fine; action icons not separate yet |
| UI drawing | Sprites in the game view, same resolution, through the 2D light buffer | UI pass at native resolution | Engine work, not started |
| Filtering | Nearest on every texture | Nearest | Done |
| Alpha | DioramaRPG: binary everywhere except the opaque battle backdrop. StatueVillage: some older images (Torch and several unused villager variants) have soft alpha | Binary | Fix Torch when it is next redrawn |

Check with `python scripts/diorama_rpg/check_assets.py [folder]`.

## Decide on a density profile - do not mix them implicitly

| Profile | Status | Field PPU | Player frame | World-unit player | Use |
|---|---|---:|---:|---:|---|
| **A: Existing** | **ACTIVE / KEEP** | 32 | 32 x 56 | 1.0 x 1.75 | Current scenes and immediate 2D playtest |
| **B: HD-field upgrade** | **OPTIONAL - explicit migration** | 48 | 48 x 84 | 1.0 x 1.75 | More expressive pixel character after A/B test |

Do not set 48 x 84 art to 32 PPU without intentionally changing the character's world scale. Keep the **same world dimensions** as you transition. First regenerate one Orin sprite and one NPC at Profile B and evaluate them next to the statue/plaza screenshot at final game resolution. Do not bulk-convert all field art yet. Re-export neighboring props/textures at commensurate texel density only if needed for cohesion.

### Field - ACTIVE 32 PPU / profile A

| Type | Recommended frame or canvas | Rendered role and rules |
|---|---|---|
| Hero Orin | **32 x 56** | Human body 30-46 px tall inside frame; sword can be separate sprite if it exceeds the bounds; feet at `(0.5, 0)` |
| Hero Lila | **32 x 56** | Same body height family; white/blue staff/spear silhouette recognizable even at distance |
| Adult NPC | **32 x 56** | Keep roughly same proportion as heroes. Different silhouettes via hats, hair, cloaks and props |
| Child NPC | **24 x 40** | Shorter than adult, world size 0.75 x 1.25 |
| Large NPC / guard | **40 x 64** | Deliberate proportion exception; same base/pivot |
| Tiny animal | **32 x 24** | Cat, small bird, familiar; centered on feet |
| Merchant cart / sign | **64 x 48** | Prop, footprint defined by separate collider |
| Small foliage clump | **32 x 32** | No outline on each leaf; nonblocking |
| Grass tuft / fern | **16 x 24** or **32 x 32** | Render under player unless foreground variant is separately authored |
| Barrel | **24 x 32** | 0.75 x 1 world units, base pivot |
| Crate | **32 x 32** | Top-left light |
| Tall lantern pole | **32 x 96** | Light effect separate sprite/component |
| Tree (full field cutout) | **96 x 160** | ~3 x 5 units; sorting pivot at trunk **ground contact**, not bitmap center |
| Large tree | **128 x 224** | Canopy may use two layers for occlusion |
| Bush | **48 x 40** | Lower world element, no collision unless dense |
| House façade sprite | **128-160 x 128-160** | Only a blockout today; prefer modular textures/meshes after 3D |
| Ground tile | **32 x 32** | Tileable surface texture. Different logical tile shape from big authored set dressing |
| Ground tile alternate | **64 x 64** | Same world-scale pattern density where it covers 2 x 2 world units |
| Cliff/steps tile | **32 x 32**, modules **32 x 64** | Edge and normal variation |
| Water animated tile | **32 x 32** | 6-8 frames, clear water edges generated separately |
| NPC interaction marker | **16 x 16** | Animated sparkle/question/exclamation at native UI pixel grid |

These sizes are *starting canvases*, not an excuse to fill every pixel with noisy detail. For bigger sprites, adjust base pivot and collision independently of artwork alpha bounds.

### Field - optional 48 PPU / profile B

When **and only when** a fidelity upgrade is approved, scale source-canvas dimensions by **1.5** *and change field PPU from 32 to 48* to preserve world sizes:

| A frame at 32 PPU | B frame at 48 PPU | Same world size |
|---|---|---|
| 32 x 56 hero/NPC | **48 x 84** | 1 x 1.75 |
| 24 x 40 child | **36 x 60** | 0.75 x 1.25 |
| 96 x 160 tree | **144 x 240** | 3 x 5 |
| 32 x 32 ground tile | **48 x 48** | 1 x 1 |
| 64 x 64 ground alternate | **96 x 96** | 2 x 2 |

Do **not** interpolate Profile A sprites into B and treat them as new master art. Redraw pixel clusters from the art reference, or keep A intact.

## Battle - separate sprite set / recommended V2 target

Battle canvases are independent of the field PPU; size them for the side-on battle camera and animation needs. They are not to be directly assigned to 32-PPU field renderers. Aim for the *visible* party character to occupy **~22-32% of the combat viewport height**, then fit background/enemy independently. Combat art can be sharper/more expressive than field art because its screen time and scale are greater.

| Combat asset | Cell / canvas in source px | Direction / framing | Notes |
|---|---:|---|---|
| Orin - hero combat | **96 x 128** | Facing **left** | Sword can cross outside body area but must remain inside cell or on weapon overlay. The pivot may sit right of centre to leave weapon room |
| Lila - hero combat | **96 x 128** | Facing **left** | Spear/staff head may occupy upper-left edge; preserve readability |
| Guest ally / humanoid NPC | **96 x 128** | Facing left | Same height family |
| Small enemy: Spirit Wisp | **96 x 96** | Facing **right** | Core and silhouette animation 4-6 frames |
| Medium enemy: Stone Scarab | **128 x 144** | Facing right | Thick shell, distinct blunt weakness cue |
| Large enemy / miniboss | **192 x 192** | Facing right | Can use multiple parts for attacks |
| Boss | **256 x 256** (up to **384 x 256** wide) | Facing right | Only if stage/UI permits and size is earned by encounter |
| Target arrow | **16 x 16** or **24 x 24** | Attached as UI/overlay | Above ground plane |
| Damage number glyphs | logical **16-24 px glyph height** | UI scale | Drawn in UI pass; not part of enemy atlas |
| Action icon | **32 x 32** or **48 x 48** | UI | Item, attack, shield, magic |
| Weakness icon | **16 x 16** or **24 x 24** | UI | Recognizable without relying only on color |
| Party portrait in combat HUD | **128 x 128** | Face/shoulders | Separate from dialogue portrait |
| Dialogue bust or face | **512 x 512** | Face/upper torso | One PNG per character, no painted checkerboard |
| Full character concept | **1024 x 1536** (flexible) | Character sheet | Reference art only, not combat sprite |

### Combat animation minimum, per hero (V2)

| Animation | Frames | Timing recommendation | Required at first quality pass? |
|---|---:|---|---|
| Idle | **4** | 90-160 ms/frame, gentle offset | Yes |
| Anticipation | **2** | 65-110 ms/frame | Yes |
| Physical attack | **6** | 55-95 ms/frame + short contact pause | Yes |
| Skill attack | **6** | 55-95 ms/frame | Later if no skill action in demo |
| Cast | **5** | 75-120 ms/frame | Yes (Lila) |
| Heal/assist | **4** | 80-120 ms/frame | Yes (Lila) |
| Hurt | **3** | 60-100 ms/frame | Yes |
| Guard | **2** | hold while guarding | Yes |
| KO/down | **3** | one-off | Yes |
| Victory | **4** | 100-160 ms/frame | Nice-to-have |

**Minimum V2 character atlas:** 4+2+6+5+3+2+3 = **25 frames** for actor with combat casting; physical-only actor can replace cast with special/skill. Old 64 x 64 atlas is allowed as a placeholder but not a final specification. Animation timing should be authored as metadata rather than hard-coded FPS.

### Field animation minimum, per direction

| Animation | Current MVP | Recommended polished 2D target |
|---|---|---|
| Idle | 1-2 frames | **4** frames, breathing/blink; 0.8-1.3 s loop |
| Walk | **4** frames | **6** frames, 80-130 ms/frame (pace dependent) |
| Run | none | **6-8** frames, separate from walk only if running is in design |
| Talk | 1 frame | **2-3** frames (gesture or face) |
| Interact / celebrate | 1 frame | **3-4** frames |
| Hurt/knockback field | none | **2** frames if field hazards needed |

All directions get identical cell dimensions and stable **foot anchor**. Left/Right mirroring is a production shortcut, **not** mandatory (asymmetric weapon/costume and shield art must be checked). Define front/back/side hair consistently.

## Sprite-sheet layout contract

- Default: **one animation per horizontal strip**, source frames arranged left to right; animation names/directions in metadata.
- Exactly **one fixed cell size per sprite sheet**; no automatic tight packing unless the engine also stores per-frame offsets/pivots.
- Row order for field: `idle_down`, `walk_down`, `idle_left`, `walk_left`, `idle_right`, `walk_right`, `idle_up`, `walk_up` (extend explicitly).
- `sheetWidth = frameWidth * maxFrameCount`; `sheetHeight = frameHeight * animationRowCount` for a fixed-grid sheet. A smaller frame count on a row leaves *empty transparent cells*, never shifted row starts.
- No shadows baked beneath a character; generate separate contact shadow with engine renderer.
- Pure sprite body uses binary alpha (0 or 255); **particles/glow** may use smooth alpha in a different texture and draw pass (add it to `SMOOTH_ALPHA_OK` in the validator).
- For 2D: point sampling and pixel snapping. For future rotated 3D cards, test point sampling and pixel-alignment under camera movement separately; expect edge artifact tradeoffs.
- Put `origin/pivot` at feet of standing actors and base of props. Define horizontal trim in frame while preserving pivot.
- Pack UI sprites separately at native UI resolution; do not composite UI in the intentionally low-resolution game framebuffer.

## Metadata in Hydro

The sheet's `.hymeta` holds the grid (`SpriteMode: Multiple`, `CellSize`, `Columns`, `Rows`, `RowNames`, `DefaultPivot`, `Filter: Nearest`); sprites are named `<Row>_<column>`. The `.hyanim` holds the clips, with a time per frame:

```yaml
SpriteAnimationSet:
  Texture: Orin_Battle.png
  Clips:
    - Name: Attack
      Loop: false
      Frames:
        - {Sprite: Anticipation_0, Duration: 0.1}
        - {Sprite: Anticipation_1, Duration: 0.12}
        - {Sprite: Attack_0, Duration: 0.07}
        - {Sprite: Attack_2, Duration: 0.12}   # contact frame, held
```

Both are written by `scripts/diorama_rpg/build.py`; timings live in `battle_sprites.TIMING`.

## Acceptance tests

- Sprite standing feet position is stable within **1 source pixel** between all frames.
- Character readable at 100% in **actual battle camera**, and its weapon can be identified at a glance.
- Skin/hair/clothing/sword use **consistent palette ramps** across idle, attack, hurt and portrait.
- Every generated PNG has actual alpha; the background is not a painted chessboard.
- Atlas dimensions match the layout declared in metadata. No frame cropped by sheet borders.
- Show screenshots of Orin and Lila in both field and battle together, not isolated on grey checkerboard.
- A/B compare **current 32 x 56** vs a manually drawn **48 x 84** field actor before switching world art density.
