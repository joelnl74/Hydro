#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <entt.hpp>

#include "Hydro/Core/Base.h"
#include "Hydro/Core/UUID.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/Lighting.h"
#include "Hydro/Renderer/PostProcess.h"
#include "Hydro/Renderer/RenderProfile.h"
#include "Components/Components.h"
#include "SceneHierarchy.h"

namespace Hydro
{
	class Entity;
	class PhysicsScene;
	class PhysicsScene3D;
	class ParticleScene;
	class UIScene;
	struct UIEvent;
	class ScriptableEntity;
	struct SystemContext;

	class Scene
	{
	public:
		Scene(const std::string& debugName = "Scene", bool isEditorScene = false);
		~Scene();

		void Init();

		// A copy of `source` to run (play mode): every entity made with CreateEntity, with the same
		// UUIDs, its CopyableComponents, and its native scripts' bindings (the script instances are
		// created fresh on the copy's first OnUpdateRuntime). Raw registry entities without an
		// IDComponent are not copied. Also copies the name and the viewport size.
		static Ref<Scene> Copy(const Ref<Scene>& source);

		// The game starts / stops running this scene: creates / destroys its 2D and 3D physics worlds (see
		// PhysicsScene and PhysicsScene3D for which entities become bodies) and its ParticleScene; starting also resets
		// every animator's playback (ResetSpriteAnimations). OnUpdateRuntime starts it by itself, before its first stage,
		// if it was not started, and the destructor stops it; the editor calls both around Play and Stop.
		void OnRuntimeStart();
		void OnRuntimeStop();
		// The running physics world, or null while editing / before OnRuntimeStart.
		PhysicsScene* GetPhysicsScene() { return m_Physics.get(); }
		// The running 3D physics world (Jolt), or null while editing / before OnRuntimeStart.
		PhysicsScene3D* GetPhysicsScene3D() { return m_Physics3D.get(); }
		// The live particles (ParticleScene), or null while editing / before OnRuntimeStart.
		ParticleScene* GetParticleScene() { return m_Particles.get(); }
		// The running UI's state (focus, keys; UIScene), or null while editing / before OnRuntimeStart.
		UIScene* GetUIScene() { return m_UI.get(); }

		// Runtime (the game is running): builds a SystemContext (IsPlaying = true) and runs the
		// stages in order: UI input, scripts, physics, sprite animation, transforms, particles, UI layout.
		void OnUpdateRuntime(float deltaTime);
		// Editing: builds a SystemContext (IsPlaying = false) and runs only the stages that keep the
		// scene consistent while editing (transforms), without scripts or animation.
		void OnUpdateEditor(float deltaTime);
		void OnRender();

		// The size the scene is viewed at; scene cameras take their aspect ratio from it.
		void OnViewportResize(uint32_t width, uint32_t height);
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }
		float GetViewportAspectRatio() const { return (float)m_ViewportWidth / (float)m_ViewportHeight; }

		// The first entity with a CameraComponent marked Primary, or an empty Entity.
		Entity GetPrimaryCameraEntity();

		Entity CreateEntity(const std::string& name = std::string());
		// A UUID already used by an entity of this scene is an error (logged); the new entity then gets
		// a fresh UUID instead, so one bad file can't make two entities share an identity.
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());

		// The entity with this UUID, or an empty Entity (converts to false) when there is none, e.g.
		// it was destroyed. Use it instead of keeping an Entity across frames, and to carry a
		// reference over to another scene: Play makes a copy of the scene with the same UUIDs.
		// An entity pending destruction is still found; check Entity::IsValid for that.
		Entity GetEntityByUUID(UUID uuid);
		// Destroys the entity and its whole subtree (children first). During a stage the whole subtree stops
		// running at once and is removed when the stage ends (see below).
		void DestroyEntity(Entity entity);

		// Every entity made with CreateEntity, in list order: the order the hierarchy shows them,
		// scenes save them and Copy recreates them. The list is the hierarchy in depth-first order: a
		// parent comes before its children and its whole subtree follows it directly (see "Hierarchy"
		// below). New entities go last, as roots. The reference is invalidated by creating, destroying,
		// moving or reparenting entities.
		const std::vector<entt::entity>& GetEntityOrder();

		// Moves `entity` with its subtree directly before `target` (or, with placeAfter, directly after
		// target's subtree), as target's sibling: it takes target's parent, keeping its place in the world
		// (as SetParent with keepWorldTransform). Ignored (logged) when target is inside entity's subtree.
		// Don't call it while iterating GetEntityOrder().
		void MoveEntity(Entity entity, Entity target, bool placeAfter);

		// A copy of the entity and its whole subtree: new entities (new UUIDs, same names) with copies of
		// their CopyableComponents and native script bindings, placed right after the original's subtree
		// with the same parent. References inside the copied set (children's parents) point to the copies;
		// references to entities outside it are kept. Returns the copy of `entity`, or an empty Entity if
		// `entity` isn't an entity of this scene's list. Don't call it while iterating the registry.
		Entity DuplicateEntity(Entity entity);

		// Entities copied together (DuplicateEntity, prefab instantiation) get new UUIDs. This makes every
		// reference from one of `entities` to another entity of the copied set follow the copy (`remap`: old
		// UUID -> new UUID); references to entities outside the set stay as they are. It covers every component
		// field that holds an entity's UUID (today RelationshipComponent::Parent).
		void RemapEntityReferences(const std::vector<Entity>& entities, const std::unordered_map<UUID, UUID>& remap);

		// ---- Hierarchy (Roadmap S3) -------------------------------------------------------------
		// An entity's parent is its RelationshipComponent (none = a root). The entity list is the tree in
		// depth-first pre-order, and these functions keep it that way: change parents only through
		// SetParent and MoveEntity. The tree's rules and layout are SceneHierarchy; the scene maps entities
		// to list indices. docs/SceneAndECS.md#hierarchy.

		// Makes `child` the last child of `parent` (an empty Entity makes it a root, last in the list); its
		// whole subtree moves with it. With keepWorldTransform the child's local transform is rewritten so it
		// stays where it is in the world (see SetWorldTransform); without, the local values are kept and it
		// moves with its new parent. Refused (logged, false) when `parent` is `child` or inside its subtree,
		// or either isn't an entity of this scene. Don't call it while iterating GetEntityOrder().
		bool SetParent(Entity child, Entity parent, bool keepWorldTransform = true);
		// Its parent, or an empty Entity for a root (or an entity that isn't in the list).
		Entity GetParent(Entity entity);
		// Its direct children, in list order (cleared first).
		void GetChildren(Entity entity, std::vector<Entity>& children);
		bool HasChildren(Entity entity);
		// The entity and every descendant, in list order, the entity first (cleared first).
		void GetSubtree(Entity entity, std::vector<Entity>& subtree);
		// True when `entity` is somewhere below `ancestor` (not when they are the same entity).
		bool IsAncestorOf(Entity ancestor, Entity entity);
		// 0 for a root.
		uint32_t GetDepth(Entity entity);

		// Restores the rule after entities were added with their parents already set (loading a scene):
		// a missing parent, an entity that is its own parent and one entity of every cycle become roots
		// (each logged), and the list is rebuilt depth-first, keeping roots and each entity's children in
		// their current order. A list that already keeps the rule is left as it is.
		void RepairHierarchy();
		// Checks the rule and logs every entity that breaks it; true when it holds. Debug builds also run
		// it after every edit of the hierarchy (create, destroy, move, reparent, duplicate, repair).
		bool ValidateHierarchy();

		// ---- World transforms ---------------------------------------------------------------------
		// TransformComponent is local (relative to the parent); WorldTransformComponent is the result of the
		// whole chain. An entity without a TransformComponent passes its parent's world on to its children.

		// Writes every WorldTransformComponent from the TransformComponents, parents first: one pass over the
		// entity list, no dirty flags. Runs as stage 4 in both updates, at the start of the game (before the
		// physics worlds are built from it) and at the start of SceneRenderer::RenderEditor / RenderRuntime
		// (application systems run after the scene update). Entities outside the list are roots.
		void ResolveTransforms();
		// The entity's world transform computed now from its chain of parents: always current, unlike
		// WorldTransformComponent. Identity for an empty entity.
		glm::mat4 GetWorldTransform(Entity entity);
		// Writes the entity's TransformComponent so its world transform becomes `world`. Under a rotated parent
		// with non-uniform scale `world` may need shear, which a TransformComponent can't hold: then the
		// closest translation / rotation / scale is written, a warning is logged (once per entity) and it
		// returns false. Also false, writing nothing, without a TransformComponent or under a parent with a
		// zero scale. Its WorldTransformComponent catches up at the next ResolveTransforms.
		bool SetWorldTransform(Entity entity, const glm::mat4& world);
		// Its position in the world now (GetWorldTransform's translation).
		glm::vec3 GetWorldPosition(Entity entity);
		// Moves it so its position in the world becomes `position`, changing only its local Translation: its
		// rotation and scale are left exactly as they are (no shear can appear). For a root this is a plain write of
		// Translation. False, writing nothing, without a TransformComponent or under a parent with a zero scale.
		// Physics writes positions back through it.
		bool SetWorldPosition(Entity entity, const glm::vec3& position);

		// ---- Runtime UI (#164) ----------------------------------------------------------------------
		// Writes every UIResolvedRectComponent from the UIRectComponents, in canvas pixels: one pass over the entity
		// list, parents first, like ResolveTransforms. A canvas (UICanvasComponent) covers the viewport at its scale
		// (UILayout::GetCanvasMetrics); each element resolves against its nearest UI ancestor's rect (entities without
		// a rect in between pass it on). An element with no canvas above it, or under a canvas that is itself under
		// another canvas (not supported: logged once), gets Canvas = 0 and isn't drawn. Runs as stage 6 in both
		// updates, at the start of the game and at the start of SceneRenderer::RenderEditor / RenderRuntime, after
		// ResolveTransforms. docs/SceneAndECS.md#runtime-ui.
		void ResolveUILayout();

		// Removes one of the entity's native scripts (or its whole NativeScriptComponent), calling
		// OnDestroy on scripts that are running.
		void RemoveScript(Entity entity, size_t index);
		void RemoveScripts(Entity entity);

		// Safe to call from script callbacks. While a stage runs (scripts, later physics),
		// DestroyEntity, RemoveScript and RemoveScripts don't act at once: the entity or script stops
		// running immediately and is really removed (OnDestroy called) when the stage ends. Outside a
		// stage (the editor's hierarchy and inspector) they act at once. Entities created during a
		// stage exist at once, but their scripts start on the next update.
		// True from DestroyEntity being called during a stage until the stage ends.
		bool IsPendingDestroy(Entity entity) const;

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		// How the scene is rendered: the project's render features, or its own (saved, copied on Play).
		const SceneRenderSettings& GetRenderSettings() const { return m_RenderSettings; }
		void SetRenderSettings(const SceneRenderSettings& settings) { m_RenderSettings = settings; }
		// The scene's own ambient and fog under forward lighting (M6; saved, copied on Play). What is drawn
		// is GetActiveEnvironment: these, or its render profile's.
		const SceneEnvironment& GetEnvironment() const { return m_Environment; }
		void SetEnvironment(const SceneEnvironment& environment) { m_Environment = environment; }
		// The scene's own exposure and bloom of HDR views (post-processing plan P1, P2; saved, copied on Play).
		// What is drawn is GetActivePostProcess.
		const ScenePostProcess& GetPostProcess() const { return m_PostProcess; }
		void SetPostProcess(const ScenePostProcess& postProcess) { m_PostProcess = postProcess; }
		// Its render profile and which sections come from it (plan P4; saved as "Look", copied on Play).
		const SceneLook& GetLook() const { return m_Look; }
		void SetLook(const SceneLook& look) { m_Look = look; }
		// Switches the profile (0 = none), keeping which sections use it: a script changes the look this way
		// (day, dusk, night). Load the profile first (AssetManager::GetAsset<RenderProfile>), like any handle
		// a component is given: the scene never loads.
		void SetRenderProfile(AssetHandle profile) { m_Look.Profile = profile; }
		// The profile if it is loaded, else nullptr (none, missing or not loaded yet).
		Ref<RenderProfile> GetRenderProfile() const;
		// What the frame uses: the profile's section when the look takes it from a loaded profile, else the
		// scene's own values.
		// Copies: the profile can be reloaded (its file changed) while the caller still holds the values.
		SceneEnvironment GetActiveEnvironment() const;
		ScenePostProcess GetActivePostProcess() const;
		// The scene's sun (DirectionalLightComponent): the one marked IsMainLight, else the first in entity
		// order; invalid when there is none. More than one marked logs a warning once.
		Entity GetMainLight();

	public:
		// THIS SHOULD BE PRIVATE AND EXPOSE A METHOD TO GET COMPONENTS/ENTITIES FROM REGISTRY.
		entt::registry m_Registry;

	private:
		// Delivers the physics steps' collision and trigger events (2D and 3D) to the entities' scripts (inside a stage).
		void DispatchPhysicsEvents();
		// Delivers the UI's events (stage 0) to the scripts of each target and then its ancestors, until one handles it;
		// clears `events`.
		void DispatchUIEvents(std::vector<UIEvent>& events);
		// Calls one ScriptableEntity hook on every running script of `self`, with `other` as the argument.
		void NotifyScripts(entt::entity self, entt::entity other, void (ScriptableEntity::*hook)(Entity));

		// Stages that run user code (scripts, later physics callbacks) bracket their work with these.
		// While a stage is open, structural changes that would pull an entity or script out from under
		// the running code are queued; EndStage applies them once the outermost stage ends.
		void BeginStage();
		void EndStage();
		void ApplyDeferred();
		// The immediate versions behind DestroyEntity and RemoveScript(s).
		void DestroyEntityNow(Entity entity);
		void RemoveFlaggedScriptsNow(entt::entity handle);

		struct DeferredCommand
		{
			enum class Type : uint8_t { DestroyEntity, RemoveFlaggedScripts };
			Type Kind = Type::DestroyEntity;
			entt::entity Handle{ entt::null };
		};

		// Rebuilds m_EntityMap from the registry: drops entries for entities destroyed behind the
		// scene's back and adds ones that got an IDComponent without CreateEntity.
		void RebuildEntityMap();

		// Hierarchy: m_Hierarchy is the layout of m_EntityOrder, rebuilt (with m_OrderIndex) on first use
		// after anything changed the list or a parent (m_HierarchyDirty).
		void EnsureHierarchy();
		std::vector<SceneHierarchy::Node> MakeHierarchyNodes();
		// The entity's index in m_EntityOrder, or -1. Rebuilds the layout first if needed.
		int32_t GetOrderIndex(entt::entity handle);
		// Sets or (parent 0) removes the RelationshipComponent; doesn't move anything in the list.
		void SetParentUUID(entt::entity handle, UUID parent);
		// Moves the subtree at `index` to `insertAt` (an index in the list before the move: from
		// SceneHierarchy::GetLastChildInsertIndex / GetSiblingInsertIndex).
		void MoveSubtree(size_t index, size_t insertAt);
		// Debug builds: ValidateHierarchy after an edit. Release: nothing.
		void CheckHierarchyAfterEdit();
		// Destroys one entity (not its children) at once: DestroyEntityNow's body before hierarchies.
		void DestroySingleEntityNow(Entity entity);

		std::unordered_map<UUID, entt::entity> m_EntityMap;
		SceneHierarchy m_Hierarchy;
		std::unordered_map<entt::entity, uint32_t> m_OrderIndex;
		bool m_HierarchyDirty = true;
		// ResolveTransforms' world matrix per list entry (kept so the pass allocates nothing once warm).
		std::vector<glm::mat4> m_WorldMatrices;
		// ResolveUILayout's rect per list entry: what that entry's UI children resolve against.
		struct UILayoutEntry
		{
			glm::vec2 Min{ 0.0f, 0.0f };
			glm::vec2 Max{ 0.0f, 0.0f };
			bool Visible = false;
			bool InCanvas = false; // below a canvas (even an ignored, nested one)
			UUID Canvas = 0;       // the canvas its UI children are drawn on (0 = none)
		};
		std::vector<UILayoutEntry> m_UILayout;
		// Nested canvases already warned about, so the log isn't flooded.
		std::unordered_set<entt::entity> m_NestedCanvasWarned;
		// Entities SetWorldTransform already warned about (shear), so the log isn't flooded.
		std::unordered_set<entt::entity> m_ShearWarned;
		int m_StageDepth = 0;
		std::vector<DeferredCommand> m_Deferred;
		std::unordered_set<entt::entity> m_PendingDestroy;

		Scope<PhysicsScene> m_Physics; // exists while the game runs this scene
		Scope<PhysicsScene3D> m_Physics3D; // exists while the game runs this scene
		Scope<ParticleScene> m_Particles; // exists while the game runs this scene
		Scope<UIScene> m_UI; // exists while the game runs this scene
		std::vector<UIEvent> m_UIEvents; // this frame's, reused

		std::string m_Name;
		SceneRenderSettings m_RenderSettings;
		SceneEnvironment m_Environment;
		ScenePostProcess m_PostProcess;
		SceneLook m_Look;
		bool m_MainLightWarned = false;
		std::vector<entt::entity> m_EntityOrder;
		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;

		friend class Entity;
		friend class SceneRenderer;
		friend void UpdateScripts(Scene&, const SystemContext&);
	};
}
