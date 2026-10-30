# Diorama RPG: the first full test project

> Written October 25, 2026, from the JRPG starter pack (GDD v4, October 23, 2026: `GDD.md`, `COMBAT_DESIGN.md`, `ART_PIPELINE.md`, `UNITY_ARCHITECTURE.md`, `CHARACTER_BIBLE.md`) and the engine as of commit `fc8d440`. The starter pack was written for Unity 6 URP; this page translates it to Hydro.

**Diorama RPG** is Hydro's first full test project: a small but complete game whose job is to exercise every engine system together, from rendering, physics and animation to UI, input, audio, scene flow and saving. It grows out of the [test bed](TestBed.md) village, and each engine feature is proven by the part of the game that needs it.

The game is a small JRPG with pixel-art characters walking through lit 3D dioramas (the style of games such as *Octopath Traveler* and *Final Fantasy Resonance*), turn-based battles built around elemental weaknesses, and ornate menus. "Diorama RPG" is a working name for the project, chosen to avoid third-party trademarks. This page lists what the game needs from the engine, what Hydro has, and the order of work. It also lists what can be built **now** with today's features, so engine work and game work move forward together.

## Short version

1. **Now, no engine changes:** turn the village and battle into a small JRPG: combat rules as plain C++ with data files, Orin and Lila against a Forest Wisp and a Stone Scarab, Octopath-style shields and Break, random encounters in the Slime Field, interactable signs, chests and NPCs, and a persistent party state ([Game track](#game-track-what-can-be-built-now)).
2. **First engine step (small; in since October 25, 2026, see [Progress](#progress)):** stand the village up. Keep the ground on the XY plane, make Z "up", tilt the camera, and draw trees, houses and characters as upright cards that face the camera (H1, H2). Box2D, the lights and the blob shadows keep working unchanged, because they already live on the ground plane.
3. **Then the diorama renderer, in order:** depth with alpha cut-out (H3), blockout meshes (H4), 3D forward lighting (H5), shadows (H6), HDR and post (bloom, tilt-shift depth of field, grading, vignette, fog; H7), a low-resolution world under a sharp UI (H8).
4. **Alongside, independent of rendering:** text and UI (H11), action-mapped input and gamepads (H12), audio (H13), timers, tweens and save games (H14). Particles (H10) are designed as camera-facing billboards from the start.

## What the target games do

| Area | *Octopath Traveler* | *Final Fantasy Resonance* (Oct 22, 2026) | GDD v4 | What the engine needs |
| --- | --- | --- | --- | --- |
| World | Pixel-art sprites in 3D environments; staged camera | Pixel sprites in 3D scenes, cutscenes, voiced characters | Orthographic, 40–45° tilt, 3D modular dioramas | A tilted 3D camera, depth, upright sprite cards, meshes |
| Look | Tilt-shift / depth of field, bloom, point lights with shadows, volumetric light, fog, particles | Same family | Bloom 0.45–0.9, contrast +10, saturation +5, vignette 0.10–0.18, fog, DoF off in play | HDR target, post stack, 3D lights, shadow maps |
| Combat | Turn-based; enemies have shield points and hidden weaknesses revealed on hit; Break skips the enemy's next turn; Boost Points (one a turn) power up actions; turn order shown | Turn-based, strategic; summoned "visions" (Cloud, Terra, Clive) with special moves; based on *Brave Exvius* season 1 | Weak 1.5×, resist 0.5×, immune, absorb; Break Pressure; Momentum / Overdrive; 2 heroes vs 2 enemies first | Gameplay code and data, timers and tweens for sequencing, text, VFX, audio |
| Travel | Towns, routes, dungeons | Overworld by chocobo or airship | World map later; one village, one route, one shrine first | Scene switching (have), save games |
| UI | Ornate menus | | UI Toolkit, black/parchment/gold nine-slice panels, Cinzel and EB Garamond, gamepad navigation, screen stack | Runtime text and UI, nine-slice, focus navigation, input maps |

The GDD's Break Pressure and Momentum fit Octopath's shields and Boost Points closely. Using that model keeps battles readable: weakness icons, a shield number and a turn-order bar.

## Where Hydro is

| Need | Hydro today | Gap | Roadmap |
| --- | --- | --- | --- |
| Pixel sprites, sheets, animation | Have: sheets, Sprite Editor, `.hyanim`, animator | Animation events (AN6), state machine (AN5) | D3–D7 |
| Tilted perspective camera | `SceneCamera` has perspective; the editor camera orbits in 3D | Sprites are flat XY quads; sorting is `SortOrder` then Z | H1, H2 |
| Depth | `VulkanImage` can make depth images; the pipeline has depth-test fields | No depth attachment; Vulkan 1.2 with render passes | R3, RA1 |
| 3D environment | `Renderer3D` is a placeholder | Meshes, materials, import | R9, R11, AS6 |
| Lighting | Global and point 2D lights and blob shadows, all as world-space quads on XY (light buffer multiplied over the view) | Lights have no height; no directional sun; nothing lit per pixel in 3D; no emissive | D1, D17, R13 |
| Shadows | Blob shadows | Shadow maps | R14 |
| Post-processing | A full-screen composite pass per view exists | HDR target, bloom, DoF, grading, fog | R16, RA6 |
| Movement and collision | Box2D at 60 Hz, triggers, callbacks, `Physics2D` velocity API; the diorama scenes walk on Jolt 3D collision (Oct 28, 2026, [Jolt physics MVP](https://github.com/joelnl74/Hydro/wiki/Jolt-Physics-MVP)): a capsule character with interpolation, stairs as ramps, mesh and footprint colliders, ray and overlap queries, exit triggers | Curved bridges, slopes and caves from `_COL` / `_WALK` proxy nodes, NPCs on the character body, the layer matrix as a setting, occlusion fade (the [World collision plan](https://github.com/joelnl74/Hydro/wiki/World-Collision-Plan) WC3–WC7) | P9–P14, WC0–WC7 |
| Battles and scene flow | `Battle.hyscene`, `SceneTransition::Push/Pop`, `BattleState`, slimes that chase | Party, data, rules, presentation | Game track |
| Text, menus, dialogue | Coloured quads only | Fonts, canvas, widgets, navigation | U1–U7 |
| Input | Key polling | Action maps, gamepads, pressed-this-frame | C1 |
| Audio | None | Everything | AU1–AU4 |
| Particles and juice | Additive blend mode exists | Emitters, shake, hit-stop, tweens | V1–V9, AN2 |
| Save games | Scene saving (YAML) | Game-state slots | G2 |

## The key decision: a Z-up diorama on today's 2D world

The diorama look does not need a separate 3D game. Pick the convention **ground = XY plane, Z = up**:

- **Box2D stays as it is** for 2D games and the top-down village. Bodies move on the ground plane, which is already XY. The tilted diorama fields went further than this first plan expected (terraces, stairs, then curved bridges, slopes and caves in the V9 re-stage): they move to **3D collision with Jolt and a capsule character controller** ([World collision plan](https://github.com/joelnl74/Hydro/wiki/World-Collision-Plan)), on the same Z-up convention. Its MVP landed on Oct 28, 2026: the player is a `CharacterBodyComponent` that walks the scenes' mesh colliders (`DioramaGround` is gone).
- **2D lights and blob shadows keep working.** `Renderer2D` builds each light and shadow as a quad at `(x ± r, y ± r, z)` drawn with the view's camera. With a tilted camera these quads lie on the ground and appear as correctly foreshortened pools of light. Today's lighting carries over to the first diorama view as it is.
- **The village becomes a diorama by data, not a rewrite.** Ground, roads, decals, flowers and water stay flat. Trees, houses, lamps, signs, props and characters become upright cards standing on their feet. Camera tilt 0° gives today's top-down look; 30–50° gives the diorama look. A key in the test bed can switch between them.
- **Y-sorting becomes depth sorting.** Sorting upright cards by their distance along the camera's view direction gives the same order as `VillageYSort`, without a script. Once depth exists (H3), alpha cut-out makes the order exact per pixel.

The alternative (Y-up with the ground on XZ) matches glTF and most 3D tools. It would mean remapping every Box2D body, light, shadow, waypoint and scene file. glTF import (AS6) can convert axes once at import instead (an import setting the roadmap already plans). **Recommended: Z-up.**

Where this approach runs out: upright lights are still lit from the ground behind them on screen, because the light buffer is screen-space. That is acceptable for the first step and fixed by per-pixel forward lighting (H5). Buildings as single cards look like cardboard cut-outs until meshes arrive (H4).

## Engine track

In order, with dependencies. Each item becomes part of the test bed when it lands (see [Test project phases](#test-project-phases)).

| ID | Item | Depends on | Roadmap |
| --- | --- | --- | --- |
| H1 | **Upright and billboard sprites.** `SpriteRendererComponent` gets a pivot (native size and pivot, D18 #93) and a `Facing`: `Flat` (lies on the ground, as today), `Upright` (stands on its pivot and rotates about the ground axis to face the camera; for trees, houses, characters), `Billboard` (faces the camera fully; for particles, light glows, icons). `SceneRenderer` builds the matrix from the camera rotation. Sorting: `SortOrder` first (ground layers), then view-space depth of the pivot instead of Z. Serialized, copied on Play, in the inspector | D18 | new |
| H2 | **Diorama camera.** Perspective with a narrow FOV (about 20–30°) from further away gives the "miniature" look with little distortion; pitch 30–50°. A `CameraFollow` component (target, dead zone, damping, bounds; D11) replaces `VillageCamera`. Zoom moves the distance, not the ortho size | H1 | D11 |
| H3 | **Depth and alpha cut-out.** Decide on Vulkan 1.3; dynamic rendering, `RenderTarget` with depth (RA1). Sprites with an alpha cut-off write depth, and pixel art has hard alpha. That solves the GDD's tree, grass and player ordering problem (GDD §19) per pixel, with no split trees or sorting tricks. Translucent things (water, particles, glows) are drawn after, sorted back to front | Vulkan 1.3 decision | R3, RA1, RA2 |
| H4 | **Blockout meshes.** `MeshComponent` with built-in shapes first (plane, box, ramp/stairs, cylinder) and one material (albedo with tiling, tint, emission, nearest filtering for pixel-art textures). Enough to build ground with height, cliffs, walls, a bridge and a house body. Then glTF import (AS6) for Blender kits, with Z-up conversion at import | H3, AS1 | R9, R11, AS6 |
| H5 | **Forward lighting in 3D.** Lights in a storage buffer (RA4), shaded per pixel in the sprite and mesh shaders: a directional sun (time of day), point lights with height, a hemisphere ambient. Upright sprites use a normal facing the camera (normal maps later, D1). Emissive sprites (D17) for windows, lanterns and fire. `Light2DComponent` gains a height, or becomes `LightComponent` with a type. Top-down views can keep the light buffer | H3, RA3, RA4 | R13, D17 |
| H6 | **Shadows.** A shadow map for the sun. Upright sprites cast shadows by drawing their cut-out cards into the shadow map, as the characters in those games do. Blob shadows stay as the cheap option. Point-light shadows much later. **Done (Oct 26, 2026, #130):** the sun's map, meshes and cut-out sprites cast, sprites as drawn or turned to face the sun (`SpriteShadows`) | H5 | R14 |
| H7 | **HDR and the post stack.** `RGBA16F` scene target; bloom; tonemapping; grading (contrast, saturation, lift/gamma/gain, then a LUT); vignette; **depth of field focused on the player** (the tilt-shift look; blur grows with depth distance from the focal plane); distance and height fog. The look is set **per scene**, shareable through **render profile** assets (`.hyrender`), instead of a `PostProcessComponent` on the camera (decided Oct 26, 2026). **Planned:** HDR, PBR Neutral tonemap, bloom, emissive sprites and render profiles in the [Post-processing plan](https://github.com/joelnl74/Hydro/wiki/Post-Processing-Plan) (#131, #102, #132, #133, #134); depth of field #135, grading #137. Render graph v1 with hand-written transitions carries HDR and bloom (as it did the shadow map); the frame graph (RA6) comes when the chains grow. Vignette and a colour tint can start now as one more multiply pass after the composite | H3 | R16, RA6 |
| H8 | **Low-resolution world, sharp UI.** Render the world view at a fixed lower resolution (for example 960×540 or 1280×720) with nearest upscaling; UI at native resolution | H7, RT2 | D10, RT2 |
| H9 | **Atmosphere.** Foliage sway in the vertex shader; a water material (scrolling UV, tint by depth, a shoreline); light shafts as additive billboard cards first, screen-space god rays later; fireflies and dust motes from particles | H5, H10 | V10 |
| H10 | **Particles as billboards.** V1–V5 as planned, but with `vec3` world positions and camera-facing quads from the start (H1's `Billboard`), so they work in top-down and diorama views alike. Emissive option, per-emitter light | H1 helps | V1–V5 |
| H11 | **Runtime text and UI for a JRPG.** Text first (U1, MSDF; Cinzel, EB Garamond and Cormorant are SIL OFL licensed), then canvas, images and nine-slice (U2–U4, D13), widgets and focus navigation for keyboard and gamepad (U5, U6), and a screen stack (hub → inventory, equipment, status; dialogue; battle). Damage numbers, the dialogue box and portraits use it | AS1 | U1–U7, D13 |
| H12 | **Input actions and gamepads.** `Gameplay` and `UI` maps, pressed / released this frame, gamepads through GLFW, one owner of the active map. The GDD's Unity bug (switching maps inside a callback) cannot happen with polled input, as long as map switches are applied at the start of the next frame | | C1 |
| H13 | **Audio.** miniaudio, clips, sources, mixer groups (music, SFX, UI, voice); battle and field music, hit and menu sounds | AS1 | AU1–AU4 |
| H14 | **Gameplay framework.** Timers and coroutines on the scene clock (G6) and tweens with easing (AN2) for battle sequencing; scene time scale (hit-stop, pausing for menus); a game event bus (G5); save slots (G2); data assets (planned Oct 30, 2026 as definitions and data tables, the [Definitions and data tables plan](https://github.com/joelnl74/Hydro/wiki/Definitions-Plan)) for combatants, abilities, items and encounters (a YAML type in the asset manager) | | G2, G5, G6, AN2 |

Items that are **not** needed for this kind of game and can wait: skeletal animation (AN3–AN7), 3D physics (P9–P14), PBR and IBL (R12, R15), terrain, networking. A stylised Lambert or half-Lambert with pixel-art textures is the diorama look; PBR adds little.

## Game track: what can be built now

All of this uses existing features (native scripts, Box2D sensors and callbacks, the scene stack, the animator, lights, coloured quads) and goes into `StatueVillage` and `Battle.hyscene` as gameplay, following the test bed rule.

| ID | Item | Notes |
| --- | --- | --- |
| J1 | **Combat rules as plain C++.** `CombatantDefinition` (stats, affinities per damage type, shield points, skills), `CombatantRuntime` (HP, MP, statuses, shield, broken), `AbilityDefinition` (type, power, cost, targets, status), `AffinityResolver` (weak 1.5×, neutral, resist 0.5×, immune, absorb; GDD defaults), `TurnQueue` (speed order for this round and the next), `BattleController` (Entering → AwaitAction → AwaitTarget → Resolving → Victory / Defeat). No rendering in any of it, so it can be unit-tested (a first doctest target for A2). Put it in `Oxygen/src/Game/`, the seed of the `GameScripts` library (RT4) |
| J2 | **Data files.** Heroes, enemies, abilities, items and encounter groups as data instead of C++. Planned Oct 30, 2026 as engine assets rather than loose YAML read by path: `.hydef` definitions and `.hytable` tables of reflected `HY_DATA` types, edited in the inspector (the wiki's [Definitions and data tables plan](https://github.com/joelnl74/Hydro/wiki/Definitions-Plan), issues [#168](https://github.com/joelnl74/Hydro/issues/168)–[#172](https://github.com/joelnl74/Hydro/issues/172); D4 moves the test bed's dialogue and items, D5 the battle's heroes, enemies and abilities) |
| J3 | **Battle v2.** Orin and Lila against up to three enemies, side view (party right, enemies left). Each enemy shows its shield count and weakness icons (sprite icons: no text needed) revealed when hit. Break skips the enemy's next turn and raises damage taken; Momentum works like Boost Points (+1 per turn, spend 1–3 to strengthen an action), with the GDD's Overdrive at full meter. A turn-order bar of portrait chips. Commands: Attack, Skill, Guard, Item, Momentum Art. The current `BattleScript` becomes a presenter driven by J1's events |
| J4 | **Encounters.** Random encounters by distance walked in the Slime Field, visible slimes kept as they are, encounter groups from J2, a battle backdrop per area, the existing fade overlay as the transition |
| J5 | **Persistent game state.** `GameState` (party, levels, EXP, inventory, gold, story flags) that survives the scene stack; it replaces most of `BattleState`. A first save and load to a YAML slot file (the first slice of G2) |
| J6 | **Interaction.** An `Interactable` script on sensor zones and an Interact key: signs, chests (an item, an open sprite), NPC talk (a speech-bubble icon until text exists), an inn that heals the party, a save point |
| J7 | **Characters.** Orin and Lila field sprites (four directions, idle and walk; the GDD says 64×96) and battle sprites (side idle, attack, cast, hurt, KO), drawn in code like the village art or in Aseprite. The reference portraits have a painted checkerboard instead of transparency and need alpha cleaned before use |
| J8 | **Enemies.** The GDD's Forest Wisp (weak to Fire, resists Wind) and Stone Scarab (weak to Blunt and Water, resists Slash), the existing slime, and the planned wolf |
| J9 | **Prepare the village for the diorama.** Mark every sprite as flat (ground, roads, decals, flowers, water) or upright (trees, houses, lamps, props, characters), and set pivots at the feet. This is generator work in `gen_village.py` that pays off the day H1 lands |
| J10 | **Battle feel with what exists.** Hit flash through the sprite colour, a lunge and knock-back through the transform, camera shake in a script, a point light for spell flashes, the full-screen overlay for the Break flash |

### Progress

October 26, 2026:

- **The battle in the Shrine Glade's style** (`scripts/shrine_glade/battle.py`). The party is **Miko**, the shrine maiden, alone (`HeroCount` in `BattleScripts.h`; the code is written for any count): a 128 x 128 battle sheet cut from her source atlas (the west-facing action swing as Attack and Cast, the stance with a breath as Idle, recoil shifts as Hurt, the stance on its back as Down), Orin's timing, bare-handed Blunt attacks and two skills, Flame Kick (Fire) and Holy Palm (Light), whose labels are new rows of `BattleText.png` made from the sheet's own glyphs. The backdrop is a sunset shrine plaza composed from the glade's painted sky, pines, the stone wall and flagstone textures and the painted props (the shrine, torii, sacred tree, twisted tree, lanterns, banners, jizo, fence, bushes and flowers). `Battle.hyscene` is edited in place: the Lila entities gone, Orin's renamed to Miko and pointed at her sheet, one HUD row in the party panel. Orin and Lila stay the field characters of the village and the MVP scene for now. The slime's sheet (`Slime.png`, `.hymeta`, `.hyanim`, same handles) now lives in `Characters/Enemy/Slime` next to the wisp's folder.

October 25, 2026 (quick prototype code, for play testing; see [Test bed](TestBed.md#slime-field-and-battle)):

- **J3 Battle v2, playable.** Orin and Lila against up to three enemies; turn order by speed shown at the top; weaknesses as icons ("?" until found); shields and Break; Momentum pips and boosting; guard, potions, run; target cursor; damage numbers. Rules, data and presentation still live together in `Oxygen/src/Scripts/BattleScripts.h` (J1 and J2 split them out).
- **J7 Characters.** Orin and Lila field sheets (32 x 56, four directions, walk; `scripts/diorama_rpg/heroes.py`). Lila follows Orin in the village (`PartyFollow`). Battle sheets are V2: 96 x 128 cells from a small pose rig in `scripts/diorama_rpg/battle_sprites.py` (Idle 4, Anticipation 2, Attack 6, Cast 5, Hurt 3, Guard 2, Down 3, and Heal 4 for Lila), with a time per frame in the `.hyanim`; sizes in [Sprite sizes](SpriteSizes.md).
- **J8 Enemies, partly.** The Spirit Wisp (weak to Fire and Light, resists Slash and Pierce) roams the meadow and the Slime Field with its own light; the slime stays. The Stone Scarab and the wolf are next.
- **J10 Battle feel, partly.** Lunges, hit flash and shake, a break banner, fades, particle sparks, defeat puffs and heal motes. Milestone B of the [scene review](SceneReview-2026-10-25.md): Orin's heavy slash (wind-up, two strike frames, a held contact frame of 0.12 s as the hit-stop, recovery) with a slash sprite and its light on the target; Lila's heal (staff raised, a ring and rising motes on the healed hero, a cool light); extra hits from boosting replay only the strike. The Wisp's contact shadow stays on the ground while it bobs. No camera shake or engine time scale yet.
- **Battle layout.** The party's HP, MP, names and Momentum pips moved to a panel at the bottom right so the taller heroes are not covered.
- **Phase 2 started: the Shrine Glade** (October 26, 2026). `Assets/Scenes/ShrineGlade.hyscene` (`scripts/shrine_glade/build.py`), after the hillside-shrine concept: a lower path, two flights of stone steps between mossy retaining walls and an upper terrace with the shrine, all meshes (built-in shapes and two imported glTF models, the shrine and a stone lantern), with the sacred tree, pines, banners, grave stones, undergrowth and the characters as upright sprites. Orin walks over the terraces (`DioramaWalk`); the MVP's interactions, menu and battle are copied in. Unlit until M6; the light shafts, bloom and depth of field of the concept are H5 to H8. See [Test bed](TestBed.md#shrine-glade).
- **MVP scene.** `Assets/Scenes/MVP.hyscene` (`scripts/diorama_rpg/mvp.py`) is the GDD's "First Scene Minimum Viable Test" on its own: one timber-frame inn with warm window lights, a cobblestone path that blends into packed earth and grass, two hanging lanterns at the door, a forest border of pines and oaks with grass clumps, ferns, flowers, rocks and bushes, a stream with a plank bridge, and Orin walking with the camera following him. It uses existing scripts only (`VillagerWalk`, `VillageYSort`, `CameraFollow`, `VillageDirector`), so it needs no engine change; see [Test bed](TestBed.md#mvp-scene).
- **MVP mechanics** (`Oxygen/src/Scripts/MvpScripts.h`, `MvpDirector`): interact prompts and actions on a sign, a chest (opens once, gives a Potion), the inn's door (restores the party, a dip to black), a save point (says saving is not here yet), the exits (fade, then push the village) and an NPC; a sample inventory menu on Tab (twelve items; runtime UI since Oct 30, 2026: a capturing canvas, so the walking keys are gated by `UI::CapturesInput`; Enter or a click uses potions and ethers, Esc closes, the selection is kept), NPC dialogue in a typed-in box (`DialogueScript`); a wisp for the explore -> battle -> return loop. This is J6 (interaction) in prototype form and the GDD's prototype HUD; menus proper wait for the UI system. The UI is label and icon sprites that follow the camera.
- **H1, H2 and J9: the diorama camera (engine).** `SpriteRendererComponent::Facing` is `Flat` (lies on the ground, the default), `Upright` (stands on the middle of its quad's bottom edge and turns to face the camera: yaw and lean, never roll) or `Billboard` (faces the camera about its centre). A top-down camera draws all three as before, so nothing changes until the camera tilts. Sorting: `SortOrder` first, then Flat sprites by Z and the standing ones (Upright, Billboard, particle emitters) from far to near along the camera's ground direction. `Light2DComponent::Height` lifts a point light up an Upright sprite (a lantern's glass, a window) the same way. Particles are drawn as billboards under a tilted camera (they still move on the ground plane: H10). Saved, copied on Play, in the inspector. The camera is game code for now: `Oxygen/src/Scripts/DioramaCamera.h` (`DioramaView`), used by `CameraFollow` and `VillageCamera`: **V** eases between top-down and a 50 degree tilt (the GDD's 40 degree pitch above the ground), **O** switches the tilted view between perspective (25 degree FOV, from the distance that shows `OrthographicSize` units at the focus point) and orthographic. `scripts/diorama_rpg/facing.py` marked the MVP and village scenes (ground, pond, bridge and the north-south `*_v` pieces Flat, the `UI *` sprites Billboard, everything else Upright) and gave 41 lights inside a building or prop a height; `mvp.py` and `build.py` run it after writing. The MVP UI follows the camera on its view plane (`DioramaView::OnScreen`, `Above`). Not done: the D18 pivot for single textures (an Upright stands on its quad's bottom edge instead, so no scene data had to move), pixel snapping of the camera, side-on art for north-south pieces, camera bounds for the tilted view (it shows past the ground at the map's edge). `scripts/diorama_rpg/preview_diorama.py` renders a scene through the tilted camera offline with the same rules.
- **Imported art repaired.** The village's imported sprites (houses, the inn, props, trees, villagers) had pinholes and semi-transparent outlines from their background removal (grass showed through roofs); `regrade.clean_alpha` now fills the holes and makes the alpha binary, and the validator passes on `StatueVillage` too.
- **Validator.** `scripts/diorama_rpg/check_assets.py` checks binary alpha, painted checkerboards, sheet grids, pivots, clip frames and the battle feet anchor (within 1 px).
- **Look.** The village art is regraded towards the palette (`scripts/diorama_rpg/regrade.py`), a new forest-clearing battle backdrop, dusk as the default time of day, the camera a little closer.
- **The village as a diorama map.** The ground is repainted (cobbled streets and plaza with mossy joints, rich grass with blades and wild flowers, packed-earth roads outside the village, a darker forest edge), the houses have stone foundations, mossy roofs, eave shadows and flower boxes, the street lamps are hanging lanterns, ferns, tall grass and mushrooms grow at the foot of trees, hay bales and hand carts sit by the field and the market, and dark foreground leaves frame the map (`scripts/diorama_rpg/env.py`). The rules behind it are the [Style guide](StyleGuide.md).

## Test project phases

Each phase adds rows to the Features table in [Test bed](TestBed.md) with how to check them.

| Phase | Engine | Test bed | Check |
| --- | --- | --- | --- |
| 0, now | none | J1–J8, J10 in the village and battle | Win and lose a 2-versus-2 battle against Wisp and Scarab; break a shield; the party's HP and items carry over between battles and through a save and load |
| 1 | H1, H2 (+ J9) | A key (for example **V**) switches the village camera between top-down and diorama | Walk behind and in front of houses and trees in both views; lamp pools lie on the ground; blob shadows stay under their owners; nothing flickers in sort order |
| 2 | H3, H4 | A new **Shrine Glade** area, the GDD's first scene: an inn, a cobbled path, two lanterns, a stream with a bridge, pines, built with blockout meshes and upright sprites, with one raised ledge and stairs | Trees and the player cut each other per pixel; the bridge is walkable; meshes and sprites sort correctly |
| 3 | H5–H8 | Day, dusk and night with a real sun, lanterns lighting upright sprites and walls, the player's shadow, bloom on lanterns, depth of field on the background | A screenshot comparison with the GDD reference image (`media/image3.jpg`); MF14 runtime screenshots make this repeatable |
| 4 | H10–H14 | Dialogue with portraits, the hub menu and inventory with a gamepad, music and sound, the battle staged in the area's diorama with damage numbers and VFX | The GDD's milestone 6: explore → encounter → victory → return, with no input leaking into the field and menus keeping their selection |

## From the Unity GDD to Hydro

| GDD (Unity) | Hydro |
| --- | --- |
| URP Global Volume | The scene's look and a render profile asset (H7, #133) |
| UI Toolkit, UXML/USS, PanelSettings | Runtime UI (H11); the screen stack and selection-scope rules in the GDD carry over as they are |
| Input System action maps | `InputAction` maps (C1, H12) |
| ScriptableObjects | Definitions and data tables (`.hydef`, `.hytable`; J2, H14, the [Definitions and data tables plan](https://github.com/joelnl74/Hydro/wiki/Definitions-Plan)) |
| CharacterController | A dynamic Box2D body with fixed rotation and damping (the village's `Player`) |
| Cinemachine | `CameraFollow` (H2) |
| SpriteRenderer sorting layers | `SortOrder` plus depth sorting (H1), then the depth buffer (H3) |
| `Time.timeScale` | Scene time scale (H14) |
| The starter pack's Unity test-scene generator | `scripts/statue_village/gen_village.py` and new areas added to the village |

## Decisions to make

1. **Z-up with the ground on XY** (recommended above) or Y-up.
2. **Require Vulkan 1.3** (dynamic rendering, synchronization2). It unblocks H3 and everything after it; R3 already asks for this decision.
3. **Pixel scale.** Choose pixels per unit and the low-resolution target together (H8), so 64×96 characters stay pixel-crisp at the chosen camera distance.
4. **Where game code lives.** Start `Oxygen/src/Game/` now (J1) and move it into the `GameScripts` library when the runtime (RT4) exists.
5. **Cards or meshes for buildings.** Upright cards first (H1), meshes for hero buildings after H4. The GDD's 70/20/10 modular-kit split applies once glTF import exists.

## Sources

- JRPG starter pack v4 (GDD, combat, art pipeline, Unity architecture, character bible), October 23, 2026
- [Wikipedia: the art style of these games](https://en.wikipedia.org/wiki/HD-2D): the techniques (3D backgrounds, sprites and billboards, tilt-shift and depth of field, bloom, dynamic point lights with shadows, volumetric light, fog, particles)
- [Octopath Traveler's art style (Unreal Engine spotlight)](https://www.unrealengine.com/en-US/spotlights/octopath-traveler-s-hd-2d-art-style-and-story-make-for-a-jrpg-dream-come-true)
- [Final Fantasy Resonance announcement (VGC)](https://videogameschronicle.com/news/final-fantasy-resonance-is-the-first-final-fantasy-game-to-get-the-hd-2d-treatment/)

> Trademark note: *HD-2D* is a Square Enix trademark. This project and its docs describe the style in generic terms (pixel-art sprites in 3D dioramas) and don't use the name for Hydro features.
