# Scripting: writing the game's code

How the game's code is written: native C++ scripts on entities, the run's state, rules, views and data, and the tools that check them. It replaces the old *Scriptable entity* page (Nov 1, 2026, the [Scripting plan](https://github.com/joelnl74/Hydro/wiki/Scripting-Plan)'s SC9). The engine side (`NativeScriptComponent`, saving, the play copy) is in [Scene, ECS and systems](SceneAndECS.md#scripts); every file is in the [Code map](CodeMap.md#game-gamesrc).

Every example on this page is a file that compiles with the game; open it next to the section.

## Contents

1. [The pieces](#the-pieces)
2. [A script in one screen](#a-script-in-one-screen)
3. [Properties](#properties)
4. [Reaching other objects](#reaching-other-objects)
5. [Run state: the game session](#run-state-the-game-session)
6. [Rules, views and data](#rules-views-and-data)
7. [Where files go](#where-files-go)
8. [Lifecycle](#lifecycle)
9. [Where a script runs in the frame](#where-a-script-runs-in-the-frame)
10. [Every function you can override](#every-function-you-can-override)
11. [What a script can use](#what-a-script-can-use)
12. [Changing the scene safely](#changing-the-scene-safely)
13. [Renaming: former names](#renaming-former-names)
14. [Checks](#checks)
15. [Debugging](#debugging)
16. [Not there yet](#not-there-yet)
17. [Adding a hook](#adding-a-hook)

## The pieces

| Piece | What it is | Owns | Example |
| --- | --- | --- | --- |
| **Script** | A `ScriptableEntity` on one entity: behaviour that runs every frame and reacts to events | Its entity's behaviour; orchestration | `Game/src/Scripts/Field/EnemyChaseScript.h` |
| **State** | Plain structs on the run's game session (`RpgSession`): what the run remembers | The truth: party, inventory, quests, the battle hand-off | `Game/src/State/QuestLog.h`, `State/PartyState.h`, `State/BattleState.h` |
| **Rule** | Plain static functions over state and data that return results | Calculations: damage, levels, loot, equipment | `Game/src/Rules/BattleRules.h`, `Rules/PartyRules.h` |
| **View** | A plain class a script owns: finds its UI entities once, writes them from state | Presentation of one screen or panel | `Game/src/Scripts/Field/FieldMenuView.h`, `Views/MessageBannerView.h` |
| **View script** | A view on its own canvas when it has its own update or input, or several scripts use it | A reusable box with a life of its own | `Game/src/Views/DialogueScript.h`, `Views/QuestTrackerScript.h` |
| **Data** | `HY_DATA` structs in definitions (`.hydef`) and tables (`.hytable`) | Anything to tune, translate or balance | `Game/src/Data/BattleDefinitions.h`, `Assets/Data/` |

The rules of thumb (the plan's [decisions](https://github.com/joelnl74/Hydro/wiki/Scripting-Plan#decisions)):

1. **Content is data.** Numbers, names, words, loot, encounters: rows and definitions, read through a property that defaults to the file. Flow (quest steps, battle phases) stays in code until there is a story system. The UI's own words wait for a text table.
2. **Run state belongs to the game session**, never in a script's members (they die with the scene) or a static (it survives Stop and Play).
3. **Rules don't depend on presentation.** They return results (`HitResult`) and never touch entities, UI, effects or cameras. That is what makes them testable: `Tools/DataTests/GameRulesTest.cpp`.
4. **Scripts orchestrate.** Split a class when it does several distinct things (a description that needs "and" twice); ~500 lines is a reason to look, not a rule.
5. **Views present state** and never change it. A view script changes state only through what it was asked (the dialogue box reports that it closed).
6. **Serialized names are an API.** Classes, properties, data types and fields are what files save: rename with a [former name](#renaming-former-names).
7. **Engine conventions apply.** `namespace Hydro` only, no `!` (`== false`, `== nullptr`), braces on every body, `m_PascalCase` members, no `System` / `Service` / `Handler` / `Helper` / `Controller` / `Module` suffixes; state is named for what it holds, rules `<Area>Rules`, views `<Name>View`, scripts `<Name>Script`. `scripts/lint_conventions.py` checks most of it.

## A script in one screen

From `Game/src/Scripts/Examples/TriggerZoneScript.h` (shortened):

```cpp
namespace Hydro
{
	HY_SCRIPT()                                            // registered as "TriggerZoneScript"
	class TriggerZoneScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Tooltip("The tag of the entities the zone reacts to"))
		std::string Watched = "Player";                    // saved per entity, set before OnCreate
		HY_PROPERTY(Tooltip("Shown while a watched entity is inside"))
		EntityRef Target;                                  // another entity, picked in the inspector

	protected:
		void OnCreate() override                           // once, before the first OnUpdate
		{
			SetTargetShown(false);
		}

		void OnTriggerEnter(Entity other) override         // after the physics step
		{
			if (IsWatched(other) == false)
			{
				return;
			}
			m_Inside++;
			SetTargetShown(true);
		}

	private:
		int m_Inside = 0;                                  // the zone's own state: a plain member
	};
}
```

- Derive publicly from `ScriptableEntity`; keep the overrides `protected`: the engine calls them, not other code.
- No setup in a constructor: the entity is set just before `OnCreate`.
- Scripts are header-only for now (`Game/src/**` is globbed: run `scripts\Win-GenProjects.bat` or `scripts\Build.bat` after adding a file).
- A script is created through its registered factory and owned by its `NativeScriptComponent`; never delete one yourself.

**Registering.** `HY_SCRIPT()` marks the class for `Tools/ReflectionGenerator/generate_reflection.py`, which the build runs: it writes `Game/src/Generated/GeneratedScriptReflection.cpp` (commit it; CI fails when it is stale: `generate_reflection.py --check`). `RegisterGame()` (`Game/src/GameRegistration.cpp`) registers it and the session type before any scene loads.

**Attaching.**

| Where | How |
| --- | --- |
| Editor | Select the entity, **Add Component > Script > Name**, or **Add Script** on its Native Script section |
| Scene file | `NativeScriptComponent: { Scripts: [ { Name: TriggerZoneScript, Properties: { Watched: Player } } ] }` |
| Code | `entity.AddComponent<NativeScriptComponent>().Bind<T>()` (or `.Bind("Name")`); it starts on the next update |

One script of a type per entity: `Bind<T>()` and **Add Script** refuse a second, and loading keeps the first with a warning. Several *different* scripts on one entity run in the order bound. A name no script is registered under is kept with its values (so saving doesn't lose them), never runs, and shows in red.

## Properties

`HY_PROPERTY(...)` above a public field shows it in the inspector, saves it with the scene and applies it before `OnCreate`. Its initialiser is the default; the saved key is the field's name. During Play the inspector shows live values and edits go straight into the script.

| Type | Use it for | Arguments |
| --- | --- | --- |
| `bool`, `int`, `float` | Tuning | `Range(min, max)`, `Min`, `Max` |
| `glm::vec2/3/4` | Positions, sizes, colours | `Color` for a colour picker |
| `std::string` | Tags, names (prefer references and rows) | |
| `AssetHandle` | A texture, prefab, definition or table | `Asset(Texture2D)`, `Asset(Prefab)`, `Asset(Definition, T)`, `Asset(DataTable, T)` (required) |
| `DataRowRef` | A row of a data table (an item, an enemy, a quest) | `DataRow(ItemDefinition)` (required) |
| `EntityRef` | Another entity, picked in the inspector or dragged onto the field | |
| `ComponentRef<T>` | A component or script on another entity (`T` from the declaration) | |
| An `HY_ENUM` enum class | A choice from a fixed set (a route's `Mode`), saved by name | |
| An `HY_DATA` struct | A group of values (a nested foldout) | |
| `std::vector<T>` of any of these, or of `EntityRef` | A route's waypoints, a shop's stock (`std::vector<DataRowRef>`), a list of tags | The element's: `Range`, `Color`, `Asset(...)`, `DataRow(...)` |

All take `Tooltip("...")`, `DisplayName("...")`, `ReadOnly`, `Hidden`, `FormerName("...")`, and the single reference kinds `Required` (the inspector and `scripts/validate_project.py` flag it while empty; the code still handles `nullptr`). The full list is in `Hydro/Scene/Scripting/ReflectionMacros.h`. Enums, structs and lists (Nov 3, 2026) are saved and drawn exactly like data fields ([Rules, views and data](#rules-views-and-data)): a list has up / down / remove and **+ Add** in the inspector, and an entity entry is picked like a reference.

Real uses: `EnemyChaseScript` (`DataRowRef Enemy`, `DataRowRef Encounter`), `SpawnerScript` (`Asset(Prefab)`), `FieldDirectorScript` (`ComponentRef<DialogueScript> Dialogue`, `Asset(Definition, InventoryDefinition) StartingInventory`), `NpcRouteScript` (`RouteMode Mode`, `std::vector<EntityRef> Waypoints`).

**Data on an entity without behaviour** is a game component, not a script (Nov 3, 2026): `HY_COMPONENT()` above a struct of `HY_PROPERTY` fields (the data kinds plus `EntityRef`). It is saved with the entity, copied on Play and Duplicate, and in **Add Component > Game**; scripts read it with `TryGetComponent<T>()`. `WaypointComponent` (what a townsperson does at a waypoint) is one: it was a script that did nothing at runtime. See [Scene and ECS, Game components](SceneAndECS.md#game-components-hy_component-nov-3-2026).

## Reaching other objects

| Situation | Use | Example |
| --- | --- | --- |
| A dependency known while building the scene or prefab | An `EntityRef` / `ComponentRef<T>` property, set in the editor (or by a builder script) | `TriggerZoneScript::Target`, `FieldDirectorScript::Dialogue` |
| An entity known only at runtime (a collision's `other`, a ray's hit, a picked target) or a sibling on your own entity | `other.TryGetComponent<T>()` (a pointer or `nullptr`; works for components and scripts) | `EnemyChaseScript::OnCollisionEnter` |
| The inside of what one view owns (a panel's rows) | `ViewHelpers::Find(scene, tag, owner)` once in `Bind`, or `Entity::FindChild("A/B/C")` | `FieldMenuView::Bind` |
| Things that shouldn't know each other | State on the session | the battle hand-off: `BattleState` |

`EntityRef` and `ComponentRef<T>` save the target's UUID, resolve with `Get()` in the owner's scene on every call (no cached pointer), and are remapped when an entity tree is duplicated or instantiated from a prefab. `GetComponent<T>()`, `HasComponent<T>()`, `TryGetComponent<T>()` and `GetComponents<T>()` accept script types too (a base class finds its subclasses); outside Play, script lookups return `nullptr`. `AddComponent` / `RemoveComponent` refuse script types: bind them.

A reference graph that spans the game (battle → HUD → party → quests) means state is in the wrong place. `Scene::FindEntityByTag` exists for tools and the few UI lookups still by tag; new code takes references.

## Run state: the game session

The engine makes one game session when a run starts (the editor's Play, the player's start) and destroys it when it ends; it survives every scene change in between. The game's type is `RpgSession` (`Game/src/State/RpgSession.h`), registered in `RegisterGame()`:

```cpp
class RpgSession : public GameSession
{
public:
	static RpgSession& Get() { return GameSession::Get<RpgSession>(); }
	PartyState Party;       // who has joined, by hero ID; the first ones fight
	BattleState Battle;     // the battle hand-off
	QuestLog Quests;
	Inventory Items;        // the party's items
	...
};
```

Each state struct has a `Get()` that reads the running session (`QuestLog::Get()`, `Inventory::Get()`, `PartyState::Get()`, `BattleState::Get()`), valid while a game runs; `GameSession::IsRunning()` says whether one does (the editor's inspector, a scene not playing). Stop and Play start a fresh run with no reset code anywhere: that is the point. What must cross scenes (who started a battle, who won, a quest's stage) is state; a scene left on the scene stack is suspended, not destroyed, and reads the state when it resumes (`EnemyChaseScript` reads `BattleState::Outcome`).

**The party** (`PartyState`) holds the members in the order they joined, looked up by hero ID (a row of `Assets/Data/Characters/Heroes.hytable`); `Party.hydef` sets who starts and how many fight (the line-up, chosen on the field menu's Party page; the rest wait in reserve, and the menu's hero pages reach them all). A new hero is a row (stats, abilities, `BattleAnimations`, portrait) and a `PartyState::Get().Join("<Id>")` where the story says so; nothing else changes. The battle (`Scripts/Battle/`) puts the fighters and the encounter's enemies into `Combatants` (one list with stable IDs, so actors can join mid-battle), runs `BattleFlow` (the rules' course, no entities) and shows it through `BattleStage`, `BattleActors` and its views; `BattleScript` only conducts.

**Saving** is game code too (`State/SaveGame.*`, yaml-cpp): `SaveGame::Write` puts the session's state (party, inventory, quest log, weaknesses found, time of day) and where the player stands in `<project folder>/Saves/Slot1.hysave`; `Load` reads it into the running session and the director asks for `SceneTransition::Replace(<saved scene>)`, whose field director places the player (`SaveGame::TakeArrival`). New run state that should survive a save gets a line in `Write` and one in `Load`. The field menu's System page (and the save points, `Save <name>`) save, load, restart the run (`SceneTransition::Restart`) and delete the save, in every build.

State shown by a retained view may keep a `Revision` its changing methods bump (`QuestLog::Revision`: the tracker flashes when it moves).

## Rules, views and data

**Rules** are `class <Area>Rules` with static functions over plain values (`Rules/BattleRules.h`: `ResolveHit`, `LevelFor`, `StatsAt`; `Rules/LootRules.h`: `Roll`; `Rules/PartyRules.h`: `Equip`, `SetAbilitySlot`; `Rules/ItemRules.h`: `UseInField`). They take what they need as arguments (a random function, the inventory's add and remove as lambdas), so `Tools/DataTests/GameRulesTest.cpp` tests them without a session, a scene or a GPU (`scripts/test_data.sh`). A rule returns what happened (`HitResult`, `ItemRules::UseResult`); the script decides what to show.

**Views** are plain classes a script owns:

```cpp
class QuestsView                                   // Game/src/Scripts/Field/QuestsView.h
{
public:
	void Bind(Scene& scene);                       // find its entities once (OnCreate)
	bool HandleEvent(const UIEvent& event, Request& request); // its rows' events; says what was asked
	void Present();                                // write the UI from state
};
```

The owning script calls `Bind` in `OnCreate`, hands its `OnUIEvent` to the view, and acts on the `Request` (use an item, close). The field menu (`FieldMenuView`) owns five page views the same way. A view becomes a **view script** on its canvas when it has its own update or input (the dialogue box types its lines in) or several scripts share it; others reach it with a `ComponentRef<T>`.

**Data**: `HY_DATA(Definition)` / `HY_DATA(Table)` structs of `HY_PROPERTY` fields (all script property types plus `HY_ENUM` enums, nested `HY_DATA` structs and `std::vector`s), in `.hydef` files (one record) and `.hytable` files (rows by ID); see [Resource creation](ResourceCreation.md). Read with `Data::Get<T>(handle)`, `Data::GetRow<T>(rowRef)`, `Data::GetTable<T>(handle)`; a missing file or row gives the defaults and one warning. `BattleData` (`State/BattleState.h`) shows a per-frame cache over several files. Example data: `Assets/Data/Characters/Miko.hydef` (`HeroDefinition`), `Enemies.hytable`, `Loot.hytable`, `Quests.hytable`.

## Where files go

The test is who uses the file: used by more than one area or scene, it goes in the shared folder of its kind; used by exactly one, in that area's folder. An area folder may include the shared folders, never another area's (the linter checks it). Moving a file changes no saved name: scenes save the class name.

```
Game/src/
  Data/          data types (HY_DATA)
  State/         run state on the session: PartyState, BattleState, Inventory, QuestLog, RpgSession; BattleData (numbers from data)
  Rules/         rules and their results: BattleRules, ItemRules, LootRules, PartyRules
  Views/         reusable views and view scripts: ViewHelpers, MessageBannerView, ScreenFadeView,
                 InteractPromptView, LetterboxView, DialogueScript, QuestTrackerScript
  Scripts/
    Common/      reusable behaviour: DioramaWalkScript, CameraFollowScript, CameraZoneScript, DioramaCamera,
                 DioramaDirectorScript and FieldSceneScript
    Field/       the field scenes' gameplay: FieldDirectorScript, EnemyChaseScript, the field menu's views
    Battle/      the battle: BattleScript (conducts), Combatants and BattleFlow (the model and its course), ActionSequence,
                 BattleStage, BattleActors, StrikeEffects, the views (CommandMenuView, PartyStatusView, TurnOrderView,
                 BattleOverlay), BattleResults, BattleEffects, BattleText
    ShrineGlade/ FarmVillage/ Interiors/ Countryside/   one area each
    Examples/    small, complete examples (TriggerZoneScript, SpawnerScript)
    Tests/       test scripts for the runtime UI
```

## Lifecycle

```
Bind (editor, scene load, code)            only the factory and the saved values are stored
        │
        ▼
first Scene::OnUpdateRuntime that sees the entity:
   1. every new instance is created and its properties applied
   2. then every new instance's OnCreate()        (another script exists, with its properties, but its own
   3. then OnUpdate(dt) for every script           OnCreate may not have run yet)
        │
        ▼
every later runtime frame:  OnUpdate(dt), and after the physics step, if something touched:
                            OnCollisionEnter / OnCollisionExit / OnTriggerEnter / OnTriggerExit
        │
        ▼
OnDestroy()   when: the entity is destroyed | the script is removed | the scene is destroyed
```

| Moment | What happens |
| --- | --- |
| **Play** (editor) | The game session starts; the scene is copied. The copy has the bindings and values but no instances, so every script gets a fresh `OnCreate` |
| **Every frame** | `OnUpdate(deltaTime)` with the frame time (clamped to 0.1 s), only while the game runs this scene |
| **Pause / Step** | Nothing runs; Step runs one normal frame |
| **`Scene::DestroyEntity`** | During a stage it only marks the entity (it stops at once); the removal, `OnDestroy` included, happens when the stage ends. Outside a stage, at once |
| **Scene destroyed** (Stop, or popped off the stack) | `OnDestroy` on every live script; requests to the scene from `OnDestroy` are dropped |
| **Scene pushed under another** (`SceneTransition::Push`) | Suspended: no update, no events, no `OnDestroy`; it carries on when the scene above is popped |
| **Stop** | Every scene goes, then the session (all run state) |

An entity spawned during play exists at once; its scripts are created on the next update, its physics body at the start of that frame's physics step.

## Where a script runs in the frame

`Scene::OnUpdateRuntime(dt)`, in order:

| # | Stage | Hooks |
| --- | --- | --- |
| 0 | **UI input** | `OnUIEvent` (Submit, Cancel, FocusEnter, FocusExit) on the target's scripts, then its ancestors' |
| 1 | **Scripts** | Creation as above, then `OnUpdate` of every script |
| 2 | **Physics** | Fixed steps of 1/60 s; then the collected events: `OnCollisionEnter/Exit`, `OnTriggerEnter/Exit` |
| 3 | **Sprite animation** | none (a clip started this frame shows this frame) |
| 4 | **Transforms** | none |

What a script writes in `OnUpdate` (a velocity, a kinematic body's transform, a colour) is used by the same frame's physics, animation and drawing. Physics events arrive after every script's `OnUpdate`.

## Every function you can override

All `protected virtual` on `ScriptableEntity`, empty by default.

| Function | Called | Typical use |
| --- | --- | --- |
| `OnCreate()` | Once, before the first `OnUpdate` | Find what you need (`Bind` views), read state, set up |
| `OnUpdate(float deltaTime)` | Every runtime frame | Input, movement, AI, timers, flow |
| `OnDestroy()` | The entity or script is removed, or the scene ends | Release what you took |
| `OnCollisionEnter/Exit(Entity other)` | After the physics step two solid colliders started / stopped touching (2D; 3D sends triggers only) | Damage, battle start |
| `OnTriggerEnter/Exit(Entity other)` | After the step something entered / left this entity's sensor | Zones, exits, pickups |
| `bool OnUIEvent(const UIEvent& event)` | Stage 0, for a button or capturing canvas at or below this entity | Menus: hand it to the view; return `true` when handled |
| `OnDrawGizmos(Gizmos& gizmos)` | Every editor frame, also outside Play (then on a stand-in with the saved values, no `OnCreate`) | Lines and labels in the Editor view: a route, a zone's reach (`Gizmos.h`; `NpcRouteScript`) |

`other` may be pending destroy or have no scripts: test `other.IsValid()` and use `TryGetComponent`.

## What a script can use

| What | How |
| --- | --- |
| Its entity | `GetEntity()`, `GetComponent<T>()`, `TryGetComponent<T>()` |
| Positions | `GetWorldPosition()`, `SetWorldPosition(p)`; another entity: `scene->GetWorldPosition(entity)`. `Translation` is relative to the parent: use the world functions for anything compared or placed |
| The scene | `Scene& scene = *GetEntity().GetScene();` then `CreateEntity`, `DuplicateEntity`, `GetEntityByUUID`, `DestroyEntity`, `RemoveScript`, `FindEntityByTag` |
| Run state | `QuestLog::Get()`, `Inventory::Get()`, `PartyState::Get()`, `BattleState::Get()` ([above](#run-state-the-game-session)) |
| Data | `Data::Get<T>`, `Data::GetRow<T>`, `Data::GetTable<T>` |
| Input | `Input::IsKeyPressed(KeyCode::E)` (polling; "pressed this frame" is your own edge: `SpawnerScript`); ignore keys while `UI::CapturesInput(scene)` |
| Physics | `Physics2D::SetLinearVelocity` / `GetLinearVelocity`; `Physics3D::Raycast`, `Physics3D::OverlapSphere` (`FieldDirectorScript`: line of sight and exits) |
| Animation | `SpriteAnimator::Play(entity, "Clip")` (safe every frame), `Stop`, `IsPlaying`; load a set first with `AssetManager::GetAsset<SpriteAnimationSet>` before swapping it |
| Effects | `Vfx::Play(scene, effectPrefab, position)`; particle presets: `Scripts/Battle/BattleEffects.h` |
| Scenes | `SceneTransition::Push("Scene")`, `Pop()`, `Quit()`; the host acts after the frame |
| Runtime UI | `UI::Focus`, `GetFocused`, `SetText`, `SetVisible`, `IsVisible`, `SetInteractable`, `CapturesInput` (`Hydro/Scene/UI/UI.h`); `ViewHelpers::SetBarFill`, `SetAlpha` |
| Logging | `HY_INFO`, `HY_WARN` (the Console window) |

## Changing the scene safely

| You want to | Do | Why it is safe |
| --- | --- | --- |
| Destroy an entity | `scene.DestroyEntity(entity)` | Deferred to the end of the stage |
| Spawn | `DuplicateEntity`, `CreateEntity`, a prefab's `Instantiate` | Exists at once; scripts start next update |
| Remove a script | `scene.RemoveScript(entity, index)` | Deferred |
| Add a component | `entity.AddComponent<T>()` | Invalidates any `T&` you hold |
| Bind a script during play | `AddComponent<NativeScriptComponent>().Bind<T>()` | Starts next update |

The script stage walks a snapshot, so spawning and destroying in callbacks can't corrupt it.

## Renaming: former names

Scenes and data files save class, property, data type and field names. To rename one, keep the old name for one round of saving:

```cpp
HY_SCRIPT(FormerName("MvpDirectorScript"))      // scenes saved before the rename still load
class FieldDirectorScript : public ScriptableEntity
{
	HY_PROPERTY(FormerName("Speed"))            // a renamed property keeps its saved value
	float WalkSpeed = 3.0f;
};
```

Loading accepts the old names, saving writes the new ones; `validate_project.py` warns about files still using a former name. Once every file is saved, delete the former name (`FieldDirectorScript`'s was deleted Nov 1, 2026). Data types take `HY_DATA(FormerName(...))` and fields `HY_PROPERTY(FormerName(...))` the same way.

## Checks

| Check | What it catches | Where |
| --- | --- | --- |
| `scripts/check.sh [files]` | Compiles the sources with g++ (`-Wall -Wextra`), the shaders with glslang, then the two below. Not a build: MSVC (CI) is the real one | Locally, a cloud session |
| `scripts/lint_conventions.py` | `!`, missing braces, other namespaces, `m_` naming, an area folder including another area's files | `check.sh`, CI |
| `scripts/validate_project.py` | Unknown scripts and properties, wrong value types, broken or empty `Required` references, a `ComponentRef<T>` to an entity without `T`, a script bound twice, broken asset handles and data rows | `check.sh`, CI |
| `generate_reflection.py --check` | Generated registration out of date | CI |
| `scripts/test_data.sh` | Data files read back, the script serializer, the game rules (`GameRulesTest`) | CI |
| CI (`.github/workflows/build.yml`) | MSVC Debug and Release builds of every push, plus all of the above | GitHub |

A header can be checked on its own by including it from a temporary `.cpp` under `Game/src/` and running `check.sh` on that file.

## Debugging

- `HY_INFO` the moments that matter; the Console window shows them.
- **Pause**, then **Step**: one frame at a time, hooks included.
- **Overlays > Collider Outlines** draws every collider: when a trigger doesn't fire, check that both shapes exist and overlap.
- Select an entity while playing: the inspector shows the live copy and its scripts' current values (edits are lost on Stop).
- A script that never runs: is it registered (regenerate the reflection, rebuild), on an entity of the *running* scene (not a suspended one), not pending destroy, and did `OnCreate` find what it needs (`ViewHelpers::Find` warns for every missing tag)?

## Not there yet

- **No per-step hook** (`OnFixedUpdate`), no enable / disable or late-update hooks, no hook when a scene is suspended or resumed. Drawing is editor-only (`OnDrawGizmos`); no runtime debug draw.
- **No timers, tweens or coroutines** (G6): member timers and `deltaTime`.
- **No animation events** (AN6).
- **No event queue on the session** yet (the plan's decision is ready: double-buffered, read the frame after); state does the job so far.
- **Property types:** no lists of `ComponentRef<T>` (use `std::vector<EntityRef>`), no `Required` list, no maps.
- **3D physics sends trigger events only**; no contact points or hit speed.
- **Compiled in:** a change needs a rebuild and an editor restart; no game DLL or hot reload (E7).
- **The field camera** (`DioramaView`) still keeps static state.
- **Saves:** one slot, and no title screen with Continue yet: a run starts fresh and loads from the System page.
- **The line-up's order** can't be changed yet: the field menu's Party page moves members in and out (a returning member goes last). Guests and actors joining mid-battle fit the model (`Combatants::AddHero` / `AddEnemy`) but no data drives them yet; `Battle.hyscene` has four party slots and three enemy slots.

## Adding a hook

1. Add a `protected virtual` empty function to `ScriptableEntity` (`On...`), documented with when it is called.
2. Call it from the one place that owns the moment: the script stage for frame hooks, `Scene::NotifyScripts` for events. Never outside a stage: structural changes are only deferred inside one.
3. `Scene` is a friend of `ScriptableEntity`: no public API is needed.
4. Add a row to [Every function you can override](#every-function-you-can-override) and to [Scene, ECS and systems](SceneAndECS.md#scripts), and a use in the [Test bed](TestBed.md).
