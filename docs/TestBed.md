# Test bed

The **StatueVillage** scene (`Oxygen/project/Assets/Scenes/StatueVillage.hyscene`, the Sandbox project's start scene) is the engine's gameplay test bed. It is a small playable game, not a separate test area: every new engine feature is added to it as part of the game (a building gets a collider, the player gets a body, a plaza gets a trigger zone, a meadow gets pushable crates), so a feature is checked in the place where it will be used, together with everything else, and nothing falls out of date.

This page lists what the scene contains, how to interact with each feature, and what to check. The rules for adding to it are in [Scene, ECS and systems](SceneAndECS.md#test-bed-scene). This page is `docs/TestBed.md` and is mirrored to the wiki; it describes the scene as of October 25, 2026.

The map has three parts: the **village** (x -30 to 30, the original scene), the **East Meadow** (x 30 to 66), reached through the East Gate, and the **Slime Field** (x 66 to 96), where the road ends in a dirt arena and slimes roam. Together the map is 126 x 36 world units. Touching a slime or a spirit wisp starts a party battle in a second scene, `Battle.hyscene`; see [Slime Field and battle](#slime-field-and-battle).

The test bed is growing into the engine's first full test project, the **Diorama RPG** ([plan](DioramaRPG.md)): the player is **Orin** (warrior), **Lila** (white mage) follows him, and the art has been regraded towards the diorama palette (mossy grass, warm roads and cobbles, terracotta roofs, warm highlights and cool shadows, a darker forest edge around the map). The new art and the scene edits come from `scripts/diorama_rpg/build.py`; `preview_scene.py` next to it draws an offline mockup of a scene.

## Quick start

1. Build and run **Oxygen**; the Sandbox project opens `StatueVillage`.
2. Press **Play** in the toolbar (click the **Game** window if the keys do not respond).
3. Walk with **WASD** or the arrows. To reach the East Meadow, follow the road east out of the plaza and through the gap in the fence at x = 31. **Stop** returns to the editor scene untouched.

## Controls (play mode)

| Keys | What it does | Script |
| --- | --- | --- |
| **W A S D** or arrows | Walk Orin (the `Player`); plays `Walk<Direction>` / `Idle<Direction>`. Lila follows along his path | `VillagerWalk`; `PartyFollow` on `Party Lila` |
| **-** / **=** | Zoom the game camera out / in (8 to 36 units visible vertically) | `VillageCamera` |
| **V** | Top-down <-> diorama view: the camera eases to a 50 degree tilt and the trees, buildings, props and characters stand up and face it (also in the MVP scene; the view is kept through a battle) | `VillageCamera`, `CameraFollow` (`DioramaView`, `DioramaCamera.h`) |
| **O** | In the diorama view: perspective <-> orthographic | `VillageCamera`, `CameraFollow` |
| **1** / **2** / **3** | Day, dusk, night (global light and how strongly lamps and windows burn); the game starts at dusk | `VillageDirector` |
| **L** | All point lights on / off | `VillageDirector` |
| **H** | Blob shadows on / off | `VillageDirector` |
| **F** | Flame flicker on / off | `VillageDirector` |
| **B** | Statue <-> brazier A/B check | `VillageDirector` |
| **E** | Drops a crate in front of the player (at most 10; the oldest disappears) | `VillageCrateSpawner` on `Crate Spawner` |
| **1** to **6** in a battle | Attack, skill, skill, guard, potion, run for the hero whose turn it is; **Left / Right** (A / D) pick the target, **Up / Down** (W / S) set the boost. Only in the battle scene | `Battle` on `Battle Director` |
| **[** / **]** | Point light radius smaller / larger | `VillageDirector` |
| **F11** (or **Full screen** in the Game window's menu bar) | The Game window fills the editor and, by default, the whole monitor; **F11** or **Escape** leaves. Stop leaves too. Both are options in the Game window's **Options** menu and under Editor Settings > Play mode | Oxygen (`EditorLayer`) |
| **P** | Pause / resume the scenery animations (pond, torches: the animators no script drives) through `SpriteAnimator::Pause` / `Resume` | `VillageDirector` |

There is no in-game text: the Director's status line goes to the **Console** window, and the **Stats** window shows lit views, light and shadow counts and GPU time.

In the editor (not playing): click an entity in the **Hierarchy** or the **Editor** window to inspect it; every component, including the physics ones, is editable. Type in the Hierarchy's search box to filter rows by name; double-click a row (or press **F** over the Editor window) to move the scene camera to the entity. The selected entity is outlined in orange, and **Overlays > Collider Outlines** in the Editor window draws every collider (yellow sensor, green static, blue dynamic, cyan kinematic).

## Features

| Feature | Where in the scene | How to interact and what to check |
| --- | --- | --- |
| Textured sprites and sprite sheets | Everything visible: the village ground (60 x 36), the East Meadow ground (36 x 36), props, buildings, characters | Open the scene: nothing is missing or drawn pink, and the meadow ground continues the road at the village edge without a seam |
| Sprite animation (`SpriteAnimatorComponent`, `.hyanim` clips) | `Player`, `Walker *` (8 villagers), `Pond`, `Bridge Torch 1/2` | Press Play: water and torches animate, walkers play the clip for their direction, the player switches walk and idle as you move. **Playback state (`SpriteAnimatorState`):** select `Pond` while playing: the inspector shows "Playing <clip>, frame N" with N counting. Press **P**: the pond and torches freeze on their frame and the line ends in "(stopped)"; press **P** again: they carry on from that frame (not from frame 0), while scripted characters (walkers, Lila, enemies) never stop. Stop and Play again: everything starts from its start clip, frame 0. With the Animation window open, change a frame duration of the pond's set and save while playing: the pond keeps its clip and restarts it from its first frame. Slimes and wisps speed their walk animation up when they chase (`SpriteAnimator::SetSpeed`) |
| Native scripts and `ScriptRegistry` | `Player` (`VillagerWalk`, `VillageYSort`), `Walker *` (`WaypointWalk`, `VillageYSort`), `Camera` (`VillageCamera`), `Village Director`, `Moving Cart` (`Bob`), pushable props (`VillageYSort`) | Select one: the inspector lists its scripts. Add `Spin` or `Bob` to a prop (Add Component > Script) and press Play |
| Y-sorting | Player, walkers and pushable props vs. trees, houses, fences | Walk behind a house or tree (hidden by it) and in front of it (drawn over it) |
| Camera follow, zoom, map clamp | `Camera` entity | The camera glides after the player, stops at the edge of the 96 x 36 map (west edge of the village, east edge of the meadow), and zooms with - and = |
| Waypoint walkers | `Walker *` and `Waypoint <Name> <n> [Facing] [Seconds]` | Eight villagers loop between their waypoints and pause facing the given way. Move a waypoint in the editor and Play: the route changes |
| 2D lighting (`Light2DComponent`) | `Ambient Light`, `Light Lamp/Window/Market/Torch/Pond/Brazier *` (35 lights, two of them at the meadow's gate and finish) | Keys 1 / 2 / 3 change the time of day, L toggles the lamps, [ and ] change their reach. At night the meadow is dark except for the two lamps |
| Particles (`ParticleEmitter2DComponent`, `ParticleScene`) | `Smoke Chimney house_tall_blue` (the tall blue house in the village's north-west), `Embers Bridge Torch 1/2`; in `Battle.hyscene`: effects made by the `Battle` script | Press Play: smoke drifts up and to the right from the tall blue house's chimney and fades; embers rise and flicker from the two bridge torches (press 3 for night: they are lit by the torch light). In a battle: sparks fly where a hit lands (orange for fire, gold for light, bigger on weak or critical hits), a smoke puff where an enemy is defeated, green motes on a healed hero. The **Stats** window's Scene tab shows live particles, emitters and update time while playing; after a battle the effect entities are gone again |
| Effect prefabs (`Vfx`, `Prefab` assets) | `Crate Spawner` (`VillageCrateSpawner`), `Assets/Effects/DustPuff.hyprefab` | Press **E**: a puff of dust at the new crate's base; drop more than 10 and a puff marks the spot where the oldest one vanishes. The dust entities destroy themselves (watch the Hierarchy or the Stats window's entity count). Tune the puff by editing the prefab: drop it into a scene, change the emitter, **Save as Prefab** over the file |
| Blob shadows (`Shadow2DComponent`) | About 265 props, buildings and characters | Press H: shadows vanish and return |
| Systems and stages (`SystemContext`, script stage, deferred destroy) | Everything that moves runs through them | Play and Stop several times, press Pause and Step: no crash, same behaviour each time |
| Play copy and selection | The whole scene | Select an entity, press Play, then Stop: the same entity stays selected (found again by its UUID, `GetEntityByUUID`) |
| Scene saving and loading | The whole scene | Move a prop, save, reload the scene (or restart): the change and all physics values stay |
| 2D physics (Box2D, 60 Hz) | Buildings, trees, props, `Player`, `Walker *`, meadow areas; see [Collision layout](#collision-layout) | Select a building, tree, crate or the player: the inspector shows Rigidbody 2D and Box / Circle Collider 2D sections. Press Play and walk: the player stops at walls, buildings, trunks, fences and the map edge, pushes crates and barrels, and walks through bushes and zones. Turn on **Overlays > Collider Outlines** to see what blocks |
| Trigger zones and collision callbacks | `Zone Bridge`, `Zone Statue Plaza`, `Zone Market`, `Zone East Gate`, `Zone Crate Yard`, `Zone Weigh Lane`, `Zone Finish` | Walk across the bridge: the Console logs the zone and the player slows down. Stand in the plaza: the brazier lights (and the statue fades) until you leave. Cross the Finish: the Console logs the finish line. Walk through the East Gate: enter and exit are logged |
| Scene stack (`SceneTransition`) | `Slime 1` to `Slime 5`, `Wisp 1` to `Wisp 4`, `Battle.hyscene` | Walk into a slime or wisp: the Console logs `battle starts` and the game switches to the battle scene. Win, lose or run: the village is back exactly as you left it (same crates, same positions, same lights) |
| Party and follower (Diorama RPG) | `Player` (Orin, `DioramaRPG/Characters/Orin`), `Party Lila` (`PartyFollow`, no body) | Walk around: Lila follows Orin's path about a step behind, turns and animates with him, and never blocks him or starts a battle |
| Diorama map and set dressing | The ground textures, the 5 houses, `Street Lamp *` (hanging lanterns), `Decor *` (ferns, tall grass, mushrooms, hay bales, hand carts), `Canopy *` (foreground leaves along the map's edges, drawn above everything) | Walk to the map's edges: dark leaves overlap the view and the ground darkens, framing the village. At dusk (2) and night (3) the lanterns' pools sit on cobbled streets. Hay bales and carts block you (box colliders); ferns and grass don't. All of it comes from `scripts/diorama_rpg/build.py`; the look is described in the [Style guide](StyleGuide.md) |
| Sprite as a light source | `Wisp 1` to `Wisp 4` (a `Light2DComponent` on the moving enemy itself) | Press 3 (night) in the meadow or the field: each wisp lights the grass around it and the light moves and breathes with it |
| Sprite sheets as UI (labels, icons, digits) | `Battle.hyscene`: `Command *`, `Order Chip *`, `Message`, `Popup *`, `* Digit *`, `* Weak *` | In a battle: the command names, turn order, damage numbers, shields and weakness icons are sprites whose `SpriteIndex` the script sets, standing in for runtime text until U1 |
| Spawning and destroying bodies in play | `Crate Spawner` | Press E: a crate appears in front of you (a copy of a village crate) and is solid at once: push it, walk into it. Press E more than ten times: the oldest crate disappears. The Console logs each drop. Stop and Play again: the dropped crates are gone |
| Meshes (M4) and depth-aware sprites (M3) | `MVP.hyscene`, west of the start: `Mesh Test Block` (a cobble box), `Mesh Test Stairs` (three steps up to it), `Mesh Test Ramp` (dirt, inline material), `Mesh Test Pillar` (a cylinder); the block, stairs and pillar share `Assets/Materials/Cobble.hymat` (M4b), `Mesh Test Road 0` to `4` (a dirt road partly under the block and stairs), `Mesh Test Tree tree_round_02`. All outside the start view, so the `mvp.png` baseline is unchanged | Open **Render Settings > Rendering (scene)**, tick **Override the project**, pick the **3D** preset and set Lighting mode = None (forward lighting is M6; no validation messages are left; don't save the scene unless you want to keep the override). Press Play and walk west (A). Press **V** (tilt) and **O** (perspective / orthographic): the shapes show their cobble and dirt textures, crisp (nearest) and repeating per metre; no faces are missing (back faces are culled, so a missing face means wrong winding); Orin is hidden by the block when he walks behind it and covers it in front; the tree's leaves cover the block with holes where the tree is transparent; the road never draws over the block or stairs; nothing flickers. Walk around the inn and trees: depth alone orders Orin (no Y-sort needed); prompts stay on top. Select the block: change Shape, Size and Tint in the inspector (Mesh section) and see it change; Add Component > Mesh on a new entity. The Stats window's Meshes section shows 4 draw calls per view (8 with the Scene and Game views both drawing). Material assets (M4b): click `Materials/Cobble.hymat` in the Project window and change its Tint: the block, stairs and pillar change together, the ramp doesn't; reopen the scene and rename the file (F2): it stays. On the ramp, **Save as Material Asset** makes `Materials/Mesh Test Ramp.hymat`; right-click > **New Material** makes a white one; drop a material tile on a mesh's material Asset field; clear the field (X): the mesh looks the same, now with its own settings. Turn on Wireframe in Render Settings: the meshes draw as wireframe. Untick the override: the meshes disappear (the Console warns once that the scene has meshes it doesn't draw) and the scene draws exactly as before |
| View captures (Capture menu, F9) | Game and Editor windows | Press Play, Pause, F9: the Console logs `Saved capture ... (1280 x 720)` a moment later and the PNG in `.hydro/captures` shows the Game view. Shift+F9 does the Editor view (with its outlines). Capture the same paused frame twice and run `scripts/compare_captures.py` on the two files: 0 pixels differ. See [Baseline captures](#baseline-captures) |

## Baseline captures

Renderer changes (the [3D renderer MVP plan](https://github.com/joelnl74/Hydro/wiki/Renderer-3D-MVP-Plan)) are checked by comparing captures from before and after. Make the baselines once before the render pipeline work starts, and again whenever a difference is accepted.

1. In the Game or Editor window's **Capture** menu, leave **Fixed Size (1280 x 720)** on.
2. For each row below: open the scene, press Play, set it up, press **Pause**, then **F9** (Game view) or **Shift+F9** (Editor view). The Console logs the file (`<project>/.hydro/captures/`).
3. Move the files to `.hydro/captures/baseline/` with the names below (git-ignored: captures stay on your machine).
4. After a change: capture the same way and run `python scripts/compare_captures.py .hydro/captures/baseline/<name>.png <new>.png --out diff.png`. 0 differing pixels means unchanged.

| Name | Scene and setup |
| --- | --- |
| `village_topdown.png` | `StatueVillage`, Play, the camera at its start position, Pause, F9 |
| `village_dusk.png` | `StatueVillage`, Play, press 2 (dusk), Pause, F9 |
| `village_diorama.png` | `StatueVillage`, Play, press V and wait for the tilt, Pause, F9 |
| `battle.png` | `StatueVillage`, Play, walk into a slime, wait for the command menu, Pause, F9 |
| `mvp.png` | `MVP`, Play, Pause, F9 |

Animated things (water, torches, particles, walkers) only match if they are paused on the same frame; compare with `--tolerance` or look at the diff image when a capture can't be repeated exactly.

## Slime Field and battle

The road continues east through the Slime Field (x 66 to 96): `Slime Field Ground` (its west edge is the meadow's east edge mirrored, so the road has no seam), a dirt arena at (86, -0.5) with four `Arena Torch` and lights, `Banner Slime Gate 1/2`, `Sign Slime Field`, trees, rocks, bushes and flowers around it (about 130 props), and the zones `Zone Slime Field` (the entrance) and `Zone Slime Arena`. `Wall East` moved to x = 96.5 and the north and south walls grew to match.

| Item | Where | What it does |
| --- | --- | --- |
| `Slime 1` to `Slime 5` | (73.5, 4.5), (79, -8.5), (90.5, 7), (92, -7), (86, 0.5) | Dynamic body, `EnemyChase` + `VillageYSort`. Wanders within 3 units of where it was placed; runs at the player (speed 2.2, the player walks at 3) when closer than 5.5; gives up beyond 10; touching the player starts the battle |
| `Wisp 1` to `Wisp 4` | (59.5, -9.5), (63.5, 6.5) in the meadow; (72.5, -5.5), (83, 6) in the field | Spirit wisps: the same `EnemyChase` (a tag starting with `Wisp` makes it a wisp: 15 % faster, it sways as it drifts), a cyan point light of their own |
| `Battle.hyscene` | `Assets/Scenes` | Written by `scripts/diorama_rpg/build.py` (`--battle` rewrites it; a normal run only updates the hero quads, the effect sprites and the party panel in place): a forest-clearing backdrop, Orin and Lila on the right (96 x 128 sprites drawn at 6 x 8 units), `FX Slash` and `FX Heal` (an animated sprite and a point light each, hidden until used), a party panel at the bottom right, three enemy slots on the left (each a slime and a wisp, the battle shows the one in use, plus a light for wisps), HP and MP bars, HP numbers, Momentum pips, shield and weakness icons, the turn order at the top, a command box, a message banner, damage numbers and a full-screen overlay for the fades. `Battle Director` has the `Battle` script |

**The battle** (prototype values, in `BattleScripts.h`). The enemy you touched brings 0 to 2 friends (slimes or wisps). Each round everyone acts once, fastest first; the order is shown at the top. On a hero's turn press **1** attack, **2** / **3** skills, **4** guard (half damage until the hero's next turn), **5** potion (+25 HP to the most hurt hero, three per battle) or **6** run (50 %); **Left / Right** move the target cursor, **Up / Down** set the boost.

| | HP / MP | Attack | Skill 2 | Skill 3 |
| --- | --- | --- | --- | --- |
| **Orin** | 62 / 20 | Slash | Flame Edge (Fire, 5 MP) | Crush (Blunt, 4 MP) |
| **Lila** | 44 / 30 | Pierce | Holy Light (Light, 5 MP) | Heal (6 MP, the most hurt hero) |

| Enemy | HP | Shield | Weak to | Resists |
| --- | --- | --- | --- | --- |
| Slime | 30 | 2 | Pierce, Fire | Blunt |
| Spirit Wisp | 24 | 3 | Fire, Light | Slash, Pierce |

A weak hit does 1.5x (orange number, "WEAK") and takes a point off the enemy's shield; a resisted hit does 0.5x. Weaknesses show as **?** until the party hits them, and stay known for the rest of the session. At shield 0 the enemy is **broken** ("BREAK!", red shield): it loses its turns this round and the next, takes 2x damage, then its shield comes back. Each hero gains one **Momentum** pip per round (not in a round after boosting), up to five; boosting spends 1 to 3: an attack hits once more per point, a skill gets 60 % stronger per point. HP and MP carry over between battles (a fallen hero gets up with 1 HP); losing restores the party. Winning removes the enemy you touched from the village; losing or running leaves it, and it ignores the player for 4 seconds and walks home. Every step is logged in the Console.

What to check for Milestone B: Orin's attack winds up, holds on the contact frame as the slash appears on the target, then recovers; boosting replays only the strike for each extra hit; Lila's Heal and the potion play her raised-staff pose and the ring on the healed hero; both heroes' feet stay on the same ground line in every pose; the party panel covers no actor; the Wisp's shadow stays on the ground as it bobs.

What to check: a slime's Pierce/Fire weakness breaks it in two hits (Lila's attack, Orin's Flame Edge); a wisp shrugs off Orin's sword ("RESIST") but not Holy Light; boosting an attack with 3 pips hits four times; a broken enemy skips its turn in the order bar; Heal and potions pick the most hurt hero; KO'd heroes lie down and are skipped.

**How the scenes switch.** `EnemyChase.OnCollisionEnter` records which slime it was in `BattleState` and calls `SceneTransition::Push("Battle")`. After the frame the editor loads `Assets/Scenes/Battle.hyscene`, puts the village on a stack (not updated or drawn, kept as it was) and runs the battle. The battle ends with `SceneTransition::Pop()`; the village resumes and the slime reads `BattleState::Outcome`.

## East Meadow

Everything in the meadow is built from the village's own art. Each area is a gameplay element that exercises part of the physics; the last column is what to check in Play. Zones fire trigger events: the Console logs `Zone: entered/left <Name>` when the player crosses one.

| Area | Where | What is in it | What to check |
| --- | --- | --- | --- |
| **East Gate** | Fence line at x = 31, gap at y -2 to 2 where the road crosses; `Zone East Gate`, `Street Lamp 11`, `Sign East Meadow` | 31 fence segments (static) with a 4-unit opening | The player passes through the gap and is stopped by the fence everywhere else. The zone's enter and exit events fire once per crossing |
| **Crate Yard** | Pen at x 34.6 to 44.6, y 8 to 13, open on the south side; `Zone Crate Yard`, `Sign Crate Yard` | 6 `crate` (wood), 3 `barrel`, dynamic, density 1 to 1.5 | Walking into a crate pushes it and it stops quickly; crates and barrels push each other; nothing leaves through the fence |
| **Weigh Lane** | x 34 to 42, y -12 to -7; `Zone Weigh Lane`, `Sign Weigh Lane` | Back row: 3 `Heavy Barrel` (density 4). Front row: 3 `Light Crate` (density 0.3) | The same push moves a light crate much further than a heavy barrel |
| **Bounce Pen** | Pen at x 49.6 to 58.4, y 5 to 11, with a 2-unit opening on the west side | 4 `Bounce Stone` (circle colliders, restitution 0.9, low damping) | A shoved stone rebounds off the fences several times before it settles; each rebounds more than a crate |
| **Moving Cart** | x 51, y -6 | `market_stall_red`, kinematic, bobs 0.5 units up and down with the `Bob` script | The cart carries on moving and pushes the player instead of being pushed |
| **Finish** | x 60 to 65, y -3 to 3; `Zone Finish`, `Finish Board`, 2 `Banner Finish`, `Street Lamp 12` | Static props around a sensor zone | Entering logs `Zone: entered Finish` and `Zone: finish line crossed` (once), leaving logs `Zone: left Finish` |
| **Meadow edge** | North and south edges, x 31 to 66 | 22 `pine_01` and 4 `rock_medium_01` with trunk and rock colliders; invisible `Wall *` at the map edge | The player stops at the map edge and walks between trunks, not through them |

## Collision layout

Colliders are part of the village. The shapes are footprints at the foot of each sprite, sized by eye, so tune them in the inspector (outlines on, Play, walk into them).

| Group | Entities | Body | Why it is there |
| --- | --- | --- | --- |
| Player | `Player` | Dynamic, fixed rotation, gravity scale 0, linear damping 10, 0.5 x 0.3 box at the feet | The controllable body. A top-down game has no gravity, and damping stops it sliding. `VillagerWalk` sets its velocity every frame (density 10, so it can push the heavy barrels slowly and the light crates easily) |
| Villagers | `Walker *` (8) | Kinematic, same box | Moved by `WaypointWalk`; they push the player and crates but are not pushed |
| Buildings | 5 `House *`, `Market Stall`, `Statue`, `well`, `notice_board` | Static (a collider without a Rigidbody) | The player cannot walk through them; only the base (about 1 unit deep) blocks, so you walk behind the roof line |
| Trees | 89 village `tree_*` and `pine_*`, 22 meadow `pine_01` | Static, trunk box 0.6 to 0.8 wide | Walk between trunks, not through them |
| Street furniture | `Street Lamp` (12), `Bollard` (4), `Bridge Torch` (2), `planter_*` (4), `bench_h/v` (3), `signpost`, `woodpile`, meadow signs and banners | Static | Small obstacles |
| Fences | `fence_h` (58), `fence_v` (59), village and meadow | Static, thin box | Fence lines block |
| Rocks and stumps | `rock_*`, `stump_*` (43) | Static | Natural obstacles. Bushes, flowers and grass tufts have no collider: you walk through them |
| Pushables | Village: 4 `barrel`, 4 `crate*`, 3 `sack`. Meadow: Crate Yard, Weigh Lane, Bounce Stones | Dynamic, fixed rotation (the stones can spin), gravity scale 0, linear damping 8 (stones 0.6) | The player shoves them across the ground and they come to rest quickly. They carry `VillageYSort` so they sort correctly as they move |
| Moving cart | `Moving Cart` | Kinematic, driven by `Bob` | A moving obstacle that must push the player (teleport handling in the physics plan) |
| Map walls | `Wall North/South/East/West` (invisible) | Static | Keep every body inside the 96 x 36 map |
| Pond | `Water West`, `Water East` (invisible) | Static | The pond is blocked except along the bridge |
| Trigger zones | `Zone Bridge`, `Zone Statue Plaza`, `Zone Market`, `Zone East Gate`, `Zone Crate Yard`, `Zone Weigh Lane`, `Zone Finish` (invisible) | Sensor boxes | Each carries the `VillageZone` script: `OnTriggerEnter/Exit` for the player logs, and the Bridge slows the player to half speed, the Statue Plaza lights the brazier instead of the statue while the player stands in it |

The scene had 40 rigidbodies (31 dynamic, 9 kinematic), 7 sensor zones and 366 colliders before the Slime Field; the field adds 5 dynamic slimes and 2 sensor zones, and a few dozen static colliders (trees, rocks, torches). To see a collider's shape now, select the entity and read its Box Collider 2D values (offset and size are in the entity's local units, so a collider on a 3 x 4 tree is 1/3 and 1/4 of its world size). The collider outlines will be drawn by the debug-draw toggle once it exists.

## MVP scene

`Assets/Scenes/MVP.hyscene` is a second, separate scene: the GDD's "First Scene Minimum Viable Test", built from the existing art by `scripts/diorama_rpg/mvp.py` (run it again to regenerate; handles and entity IDs are stable). Open it from the editor instead of `StatueVillage`.

| Item | Where | What it does |
| --- | --- | --- |
| `Inn` | (-3, 2.5), the middle of the map | `inn_two_story`, with `Light Window 1..3` on its lit windows and a cobbled yard at the door |
| `Lantern 1` / `2`, `Light Lamp 1` / `2` | Either side of the door | Hanging lantern posts; the lights follow the day / dusk / night presets like the village's lamps |
| `MVP Ground` | 60 x 40 units | One texture (`DioramaRPG/Environment/MVP_Ground.png`): grass, the cobblestone path (cobbles in the middle, packed earth at the sides, grass creeping over the rim) from the door south over the bridge and a branch west, a stream across the south with muddy banks, and the forest's shade beyond the walkable 40 x 24 map |
| `Bridge`, `Stream West` / `East` | x -3, y -6 | A plank bridge; the two colliders along the stream mean the bridge is the only way across |
| `Tree *`, `Decor *`, `Canopy *` | Everywhere | About 300 pines and oaks (dense outside the walls, a few in the clearing), 130 grass clumps, ferns, flowers, rocks, bushes and stumps on the grass, dark leaves along the top and bottom |
| `Player` | (-3, -1) | Orin: `VillagerWalk`, `VillageYSort`, a dynamic body |
| `MVP Director` | | `MvpDirector` (`Oxygen/src/Scripts/MvpScripts.h`): the interact prompts, the messages and the inventory menu below |
| `Sign Fork`, `Chest 1`, `Door Inn`, `Save Stone` + `Light Save`, `Exit South` / `Exit West`, `Npc Elder` | The fork, beside the inn, the door, west of the yard, the path's ends, by the bench | Interactables: the tag's first word is the kind. Within 1.7 units a prompt shows above Orin (`E: Read` / `Open` / `Enter` / `Save` / `Leave` / `Talk`) |
| `Wisp 1` | (9, 2.5) | A spirit wisp (`EnemyChase`): touching it pushes `Battle.hyscene`; the MVP scene waits below and reads the outcome when the battle pops |
| `UI *` | Authored around the origin | The UI is sprites (`DioramaRPG/UI/MvpText.png` labels, the battle icons' digits and cursor, `Panel.png`) that `MvpDirector` keeps on the camera; all start with alpha 0 |
| `Camera` | | `CameraFollow`: glides after the player, clamped at x 17, y 23 (the ground is painted that far); **V** tilts it into the diorama view, **O** switches perspective and orthographic there |
| `Wall North` / `South` / `West` / `East` | The map's edges | Keep the player in the 40 x 24 clearing |

**The mechanics** (prototype values, in `MvpScripts.h`). **E** interacts with the nearest thing in range: the sign shows two lines; the chest opens once (its lid sprite changes) and adds a Potion to the inventory, then says it is empty; the inn's door restores the party's HP and MP with a dip to black ("You feel rested"); the save stone says there is no save slot yet (J5 is not written); the two exits fade out and push `StatueVillage` (there is no way back to the MVP scene yet); the elder says one of three lines. **Tab** opens the inventory: twelve rows (six visible, the list scrolls so the selection stays in view), **Up / Down** (or W / S) select with wrap-around, **Enter** / **Space** use the item (a Potion heals both heroes 25, a Hi-Potion 60, an Ether gives 10 MP; everything else says it cannot be used here), **Esc** closes. While the menu is open `UIState::MenuOpen` is set and `VillagerWalk` ignores the movement keys, so nothing leaks into the field; the selection is kept between opens. Messages sit in a banner at the bottom for 3 s.

What to check: walk the path from the door to the bridge and over it; each interactable shows its prompt and does its thing; open the menu while walking: Orin stops and does not drift; press Down past the sixth row: the list scrolls; close and reopen: the same row is selected; touch the wisp: the battle starts, win it, and the wisp is gone when the scene returns (lose: it goes home and ignores you for 4 s); use a Potion after a battle and check the HP in the next battle's panel; Orin passes behind the inn's wall and in front of the lanterns correctly; the camera follows smoothly and never shows past the forest; 1 / 2 / 3 switch day, dusk and night and the lanterns and windows follow; the stream cannot be crossed except on the bridge.

## Planned for the test bed

- **Physics tuning**: collider sizes were set by eye; adjust them (and player speed against damping) after the first Play.
- **Zone Market** plays a sound once audio lands (it only logs for now).
- **Collision callbacks on props** (E2 follow-up): a crate that logs or reacts when something hits it hard (hit events carry speed).
- **Wolf**: a third enemy kind (a sprite sheet, more HP, a faster chase).
- **Battle UI**: labels, icons and numbers are sprite sheets drawn by `scripts/diorama_rpg/build.py`; real UI and text come with the UI system (U1). The battle scene is the place to prototype them.
- **Diorama RPG next steps**: the game track in [DioramaRPG.md](DioramaRPG.md) (J1 rules as plain C++ with tests, J2 data files, J4 random encounters, J5 save game, J6 interaction).
- **Debug drawing in play mode** (P7): collider outlines in the Game window; the editor view already has them.
- **Script safety** (#40): a script that spawns and destroys a crate on a key press (in the Crate Yard), to watch the deferred stage.
- Audio, tilemaps and runtime UI as each lands: each becomes part of the game (footstep sounds, a tiled path, a HUD), with a row in the Features table.
