# Eonlore: notes for Claude

Eonlore is a JRPG built with Hydro, a workspace of its own (`hydro-workspace.json`, game id `eonlore`): its game code (`Game/src`, the `EonloreGame` library), its project (`Project/Eonlore.hyproj` with `Assets/`), its art and scene tools (`scripts/`), docs (`docs/`, game code map in `docs/CodeMap.md`) and rule tests (`Tools/Tests/`). The engine (Hydro, Oxygen, Runtime) and its rules are in the Hydro repository: read its `CLAUDE.md` and `docs/` first, and follow the `hydro-engine-architecture` skill for game code too.

## Building and checking

- `scripts\Build.bat [Debug|Release|Dist]` builds this workspace (Hydro is `HYDRO_DIR`, else `..\Hydro` next to this repository); solution `build\Hydro.sln`, binaries in `bin\`. `scripts\RunOxygen.bat` opens the project in this workspace's Oxygen.
- `scripts\Package.bat [Release|Distribution]` makes `Builds\Eonlore-<Kind>\` (also Oxygen's File > Build).
- `bash scripts/check.sh [files...]`: Hydro's g++ syntax check, conventions and project validation on this workspace; `bash scripts/check.sh --tests`: the data tests with this game's rule tests.

## Test bed

The gameplay test bed is the field scenes `Project/Assets/Scenes/ShrineGlade.hyscene` and `FarmVillage.hyscene` (plus `Battle.hyscene`; the old `StatueVillage` scene was deleted, see `docs/TestBed.md` (here)). When a new or drastic system lands (physics, audio, tilemaps, UI...), add it to those scenes as a gameplay element (colliders on buildings and the player, trigger zones, sound sources), not as a separate demo area, add a row to the Features table in `docs/TestBed.md` (here), and say in the summary what to check by pressing Play.

Runtime UI work (canvases, text, buttons, focus, pointer, menus) is checked in `Project/Assets/Scenes/Battle.hyscene`, which already has a lot of UI worth making functional; keep the Shrine Glade free of UI test entities (it is the visual look's showcase). The game's own UI (battle HUD and menus, the field's inventory, dialogue and prompt) is built into the three scenes by `scripts/game/ui/build_game_ui.py` (art by `scripts/game/ui/make_ui_art.py`; check it offline with `scripts/game/ui/preview_ui.py`): change the builder and rerun it rather than hand-editing those entities.

3D renderer work (meshes, lighting, shadows, post-processing) is checked in `Project/Assets/Scenes/ShrineGlade.hyscene` instead: a hillside shrine mixing meshes and upright sprites, built by `scripts/shrine_glade/build.py` (see `docs/TestBed.md` (here), "Shrine Glade"). **That builder (and `scripts/farm_village/`) is not in the repository or the user's folder right now**; until it is restored, edit the scenes in the editor and say so.

