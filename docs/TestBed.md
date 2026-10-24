# Test bed

The **StatueVillage** scene (`Oxygen/project/Assets/Scenes/StatueVillage.hyscene`, the Sandbox project's start scene) is the engine's gameplay test bed. It is a small playable game, not a separate test area: every new engine feature is added to it as part of the game (a building gets a collider, the player gets a body, a plaza gets a trigger zone, a meadow gets pushable crates), so a feature is checked in the place where it will be used, together with everything else, and nothing falls out of date.

This page lists what the scene contains, how to interact with each feature, and what to check. The rules for adding to it are in [Scene, ECS and systems](SceneAndECS.md#test-bed-scene). This page is `docs/TestBed.md` and is mirrored to the wiki; it describes the scene as of October 24, 2026.

The map has two parts: the **village** (x -30 to 30, the original scene) and the **East Meadow** (x 30 to 66), reached through the East Gate. Together the map is 96 x 36 world units.

## Quick start

1. Build and run **Oxygen**; the Sandbox project opens `StatueVillage` (558 entities).
2. Press **Play** in the toolbar (click the **Game** window if the keys do not respond).
3. Walk with **WASD** or the arrows. To reach the East Meadow, follow the road east out of the plaza and through the gap in the fence at x = 31. **Stop** returns to the editor scene untouched.

## Controls (play mode)

| Keys | What it does | Script |
| --- | --- | --- |
| **W A S D** or arrows | Walk the player; plays `Walk<Direction>` / `Idle<Direction>` | `VillagerWalk` |
| **-** / **=** | Zoom the game camera out / in (8 to 36 units visible vertically) | `VillageCamera` |
| **1** / **2** / **3** | Day, dusk, night (global light and how strongly lamps and windows burn) | `VillageDirector` |
| **L** | All point lights on / off | `VillageDirector` |
| **H** | Blob shadows on / off | `VillageDirector` |
| **F** | Flame flicker on / off | `VillageDirector` |
| **B** | Statue <-> brazier A/B check | `VillageDirector` |
| **[** / **]** | Point light radius smaller / larger | `VillageDirector` |

There is no in-game text: the Director's status line goes to the **Console** window, and the **Stats** window shows lit views, light and shadow counts and GPU time.

In the editor (not playing): click an entity in the **Hierarchy** or the **Editor** window to inspect it; every component, including the physics ones, is editable. Type in the Hierarchy's search box to filter rows by name; double-click a row (or press **F** over the Editor window) to move the scene camera to the entity. The selected entity is outlined in orange, and **Overlays > Collider Outlines** in the Editor window draws every collider (yellow sensor, green static, blue dynamic, cyan kinematic).

## Features

| Feature | Where in the scene | How to interact and what to check |
| --- | --- | --- |
| Textured sprites and sprite sheets | Everything visible: the village ground (60 x 36), the East Meadow ground (36 x 36), props, buildings, characters | Open the scene: nothing is missing or drawn pink, and the meadow ground continues the road at the village edge without a seam |
| Sprite animation (`SpriteAnimatorComponent`, `.hyanim` clips) | `Player`, `Walker *` (8 villagers), `Pond`, `Bridge Torch 1/2` | Press Play: water and torches animate, walkers play the clip for their direction, the player switches walk and idle as you move |
| Native scripts and `ScriptRegistry` | `Player` (`VillagerWalk`, `VillageYSort`), `Walker *` (`WaypointWalk`, `VillageYSort`), `Camera` (`VillageCamera`), `Village Director`, `Moving Cart` (`Bob`), pushable props (`VillageYSort`) | Select one: the inspector lists its scripts. Add `Spin` or `Bob` to a prop (Add Component > Script) and press Play |
| Y-sorting | Player, walkers and pushable props vs. trees, houses, fences | Walk behind a house or tree (hidden by it) and in front of it (drawn over it) |
| Camera follow, zoom, map clamp | `Camera` entity | The camera glides after the player, stops at the edge of the 96 x 36 map (west edge of the village, east edge of the meadow), and zooms with - and = |
| Waypoint walkers | `Walker *` and `Waypoint <Name> <n> [Facing] [Seconds]` | Eight villagers loop between their waypoints and pause facing the given way. Move a waypoint in the editor and Play: the route changes |
| 2D lighting (`Light2DComponent`) | `Ambient Light`, `Light Lamp/Window/Market/Torch/Pond/Brazier *` (35 lights, two of them at the meadow's gate and finish) | Keys 1 / 2 / 3 change the time of day, L toggles the lamps, [ and ] change their reach. At night the meadow is dark except for the two lamps |
| Blob shadows (`Shadow2DComponent`) | About 265 props, buildings and characters | Press H: shadows vanish and return |
| Systems and stages (`SystemContext`, script stage, deferred destroy) | Everything that moves runs through them | Play and Stop several times, press Pause and Step: no crash, same behaviour each time |
| Play copy and selection | The whole scene | Select an entity, press Play, then Stop: the same entity stays selected (found again by its UUID, `GetEntityByUUID`) |
| Scene saving and loading | The whole scene | Move a prop, save, reload the scene (or restart): the change and all physics values stay |
| 2D physics (Box2D, 60 Hz) | Buildings, trees, props, `Player`, `Walker *`, meadow areas; see [Collision layout](#collision-layout) | Select a building, tree, crate or the player: the inspector shows Rigidbody 2D and Box / Circle Collider 2D sections. Press Play and walk: the player stops at walls, buildings, trunks, fences and the map edge, pushes crates and barrels, and walks through bushes and zones. Turn on **Overlays > Collider Outlines** to see what blocks |

## East Meadow

Everything in the meadow is built from the village's own art. Each area is a gameplay element that exercises part of the physics; the last column is what to check in Play. Collision callbacks (zone enter and exit events) are not there yet.

| Area | Where | What is in it | What to check |
| --- | --- | --- | --- |
| **East Gate** | Fence line at x = 31, gap at y -2 to 2 where the road crosses; `Zone East Gate`, `Street Lamp 11`, `Sign East Meadow` | 31 fence segments (static) with a 4-unit opening | The player passes through the gap and is stopped by the fence everywhere else. The zone's enter and exit events fire once per crossing |
| **Crate Yard** | Pen at x 34.6 to 44.6, y 8 to 13, open on the south side; `Zone Crate Yard`, `Sign Crate Yard` | 6 `crate` (wood), 3 `barrel`, dynamic, density 1 to 1.5 | Walking into a crate pushes it and it stops quickly; crates and barrels push each other; nothing leaves through the fence |
| **Weigh Lane** | x 34 to 42, y -12 to -7; `Zone Weigh Lane`, `Sign Weigh Lane` | Back row: 3 `Heavy Barrel` (density 4). Front row: 3 `Light Crate` (density 0.3) | The same push moves a light crate much further than a heavy barrel |
| **Bounce Pen** | Pen at x 49.6 to 58.4, y 5 to 11, with a 2-unit opening on the west side | 4 `Bounce Stone` (circle colliders, restitution 0.9, low damping) | A shoved stone rebounds off the fences several times before it settles; each rebounds more than a crate |
| **Moving Cart** | x 51, y -6 | `market_stall_red`, kinematic, bobs 0.5 units up and down with the `Bob` script | The cart carries on moving and pushes the player instead of being pushed |
| **Finish** | x 60 to 65, y -3 to 3; `Zone Finish`, `Finish Board`, 2 `Banner Finish`, `Street Lamp 12` | Static props around a sensor zone | Entering the zone logs a trigger-begin event (P3); leaving logs trigger-end |
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
| Trigger zones | `Zone Bridge`, `Zone Statue Plaza`, `Zone Market`, `Zone East Gate`, `Zone Crate Yard`, `Zone Weigh Lane`, `Zone Finish` (invisible) | Sensor boxes | Enter and exit events for the collision callbacks; see "Planned" |

The scene has 40 rigidbodies (31 dynamic, 9 kinematic), 7 sensor zones and 366 colliders in total. To see a collider's shape now, select the entity and read its Box Collider 2D values (offset and size are in the entity's local units, so a collider on a 3 x 4 tree is 1/3 and 1/4 of its world size). The collider outlines will be drawn by the debug-draw toggle once it exists.

## Planned for the test bed

- **Physics tuning**: collider sizes were set by eye; adjust them (and player speed against damping) after the first Play.
- **Collision and trigger callbacks** (P3): entering `Zone Statue Plaza` tints the brazier light, `Zone Market` plays a sound once audio lands, `Zone Bridge` slows the player, `Zone Finish` logs a message.
- **Debug drawing in play mode** (P7): collider outlines in the Game window; the editor view already has them.
- **Script safety** (#40): a script that spawns and destroys a crate on a key press (in the Crate Yard), to watch the deferred stage.
- Audio, tilemaps and runtime UI as each lands: each becomes part of the game (footstep sounds, a tiled path, a HUD), with a row in the Features table.
