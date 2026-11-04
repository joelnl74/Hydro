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
#include "SceneGraph.h"
#include "SceneTransforms.h"
#include "SceneUILayout.h"
#include "SystemContext.h"
#include "UI/UILayout.h"

namespace Hydro
{
	class Entity;
	class PhysicsScene;
	class PhysicsScene3D;
	class ParticleScene;
	class UIScene;
	struct UIEvent;
	class ScriptableEntity;
	class Gizmos;

	// A world of entities and their components, with the update stages that run it (editing or playing).
	class Scene
	{
	public:
		Scene(const std::string& debugName = "Scene", bool isEditorScene = false);
		~Scene();

		void Init();

		// A play-mode copy of `source`: every CreateEntity entity with the same UUIDs, its registered components and
		// script bindings (instances are created fresh), plus the name and viewport size.
		static Ref<Scene> Copy(const Ref<Scene>& source);

		// Creates / destroys the physics worlds and the ParticleScene and resets animator playback. OnUpdateRuntime
		// starts it if needed and the destructor stops it; the editor calls both around Play and Stop.
		void OnRuntimeStart();
		void OnRuntimeStop();
		// Null while editing or before OnRuntimeStart, like the three below.
		PhysicsScene* GetPhysicsScene() { return m_Physics.get(); }
		PhysicsScene3D* GetPhysicsScene3D() { return m_Physics3D.get(); }
		ParticleScene* GetParticleScene() { return m_Particles.get(); }
		UIScene* GetUIScene() { return m_UI.get(); }

		// Runs every stage in order: UI input, scripts, physics, sprite animation, transforms, particles, UI layout.
		void OnUpdateRuntime(float deltaTime);
		// Runs only the stages that keep the scene consistent while editing (transforms, UI layout).
		void OnUpdateEditor(float deltaTime);
		// The update's stages, in the order they run (docs/SceneAndECS.md#frame-order). A new engine system is a stage here.
		static const std::vector<SceneStage>& GetStages();
		void OnRender();

		// The pointer over the Game view for the runtime UI: view pixels from the bottom-left (Y up), over or not,
		// primary button. The layer sets it every frame before OnUpdateRuntime; UIScene reads it at stage 0.
		struct UIPointer
		{
			glm::vec2 Position{ 0.0f, 0.0f };
			bool Inside = false;
			bool Down = false;
		};
		void SetUIPointer(const glm::vec2& position, bool inside, bool down) { m_UIPointer = { position, inside, down }; }
		const UIPointer& GetUIPointer() const { return m_UIPointer; }

		// Scene cameras take their aspect ratio from this size.
		void OnViewportResize(uint32_t width, uint32_t height);
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }
		float GetViewportAspectRatio() const { return (float)m_ViewportWidth / (float)m_ViewportHeight; }

		Entity GetPrimaryCameraEntity();

		Entity CreateEntity(const std::string& name = std::string());
		// A UUID already in use is logged and replaced by a fresh one, so a bad file can't make two entities share one.
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());

		// An empty Entity when none (destroyed). Keep UUIDs rather than Entities across frames and scenes (Play copies
		// keep UUIDs). Entities pending destruction are still found: check Entity::IsValid.
		Entity GetEntityByUUID(UUID uuid);
		// The first entity in list order with this tag. Tags aren't unique: prefer an EntityRef property or
		// Entity::FindChild. Walks the whole list, so look it up once (OnCreate), not every frame.
		Entity FindEntityByTag(const std::string& tag);
		// Destroys the entity and its subtree (children first). During a stage it is removed when the stage ends.
		void DestroyEntity(Entity entity);

		// The hierarchy in depth-first order (each parent directly followed by its subtree; new entities go last as
		// roots). Invalidated by creating, destroying, moving or reparenting entities.
		const std::vector<entt::entity>& GetEntityOrder();

		// Moves `entity` and its subtree directly before `target` (or after target's subtree) as its sibling, keeping its
		// world placement. Ignored (logged) when target is inside entity's subtree. Not while iterating GetEntityOrder().
		void MoveEntity(Entity entity, Entity target, bool placeAfter);

		// Copies the entity and its subtree (new UUIDs) right after the original, with the same parent; references
		// inside the copied set point to the copies. Empty when `entity` isn't in this scene. Not while iterating.
		Entity DuplicateEntity(Entity entity);

		// After copying a set of entities (`remap`: old UUID -> new UUID), makes references inside the set follow the
		// copies, in every component field and script property holding an entity UUID.
		void RemapEntityReferences(const std::vector<Entity>& entities, const std::unordered_map<UUID, UUID>& remap);

		// ---- Hierarchy (docs/SceneAndECS.md#hierarchy) --------------------------------------------
		// Parents are RelationshipComponents; the entity list stays depth-first, so reparent only through these.

		// Makes `child` the last child of `parent` (empty = a root), with its subtree. keepWorldTransform rewrites the
		// local transform so it stays put. False (logged) for a cycle or a foreign entity. Not while iterating.
		bool SetParent(Entity child, Entity parent, bool keepWorldTransform = true);
		// Empty for a root or an entity that isn't in the list.
		Entity GetParent(Entity entity);
		// Direct children in list order; clears `children` first.
		void GetChildren(Entity entity, std::vector<Entity>& children);
		bool HasChildren(Entity entity);
		// The entity first, then every descendant in list order; clears `subtree` first.
		void GetSubtree(Entity entity, std::vector<Entity>& subtree);
		// False when they are the same entity.
		bool IsAncestorOf(Entity ancestor, Entity entity);
		uint32_t GetDepth(Entity entity);

		// Makes the list depth-first again after loading entities with parents already set: missing parents, self
		// parents and one entity per cycle become roots (logged); the existing order is kept where possible.
		void RepairHierarchy();
		// Logs every entity that breaks the hierarchy rule. Debug builds run it after every hierarchy edit.
		bool ValidateHierarchy();

		// ---- World transforms ---------------------------------------------------------------------
		// TransformComponent is local; an entity without one passes its parent's world on to its children.

		// Writes every WorldTransformComponent, parents first, in one pass. Runs as stage 4, at game start and at the
		// start of rendering.
		void ResolveTransforms();
		// Computed now from the parent chain, unlike WorldTransformComponent. Identity for an empty entity.
		glm::mat4 GetWorldTransform(Entity entity);
		// Writes the TransformComponent so the world transform becomes `world`. False when that needs shear (the closest
		// fit is written, warned once) or there is no TransformComponent or a zero-scale parent (nothing written).
		bool SetWorldTransform(Entity entity, const glm::mat4& world);
		glm::vec3 GetWorldPosition(Entity entity);
		// Changes only the local Translation (rotation and scale untouched, so no shear). False, writing nothing,
		// without a TransformComponent or under a zero-scale parent. Physics writes positions back through it.
		bool SetWorldPosition(Entity entity, const glm::vec3& position);

		// ---- Runtime UI (#164, docs/SceneAndECS.md#runtime-ui) ------------------------------------
		// Lays out every UIResolvedRectComponent in canvas pixels, parents first. Stage 6, after ResolveTransforms.
		void ResolveUILayout();
		// Where the last layout put a top-level canvas in the view. False for one it didn't place (hidden, nested, a
		// World canvas off screen or without a camera).
		bool GetUICanvasMetrics(UUID canvas, UICanvasMetrics& metrics) const;

		// Removes one native script (or the whole component), calling OnDestroy on running scripts.
		void RemoveScript(Entity entity, size_t index);
		void RemoveScripts(Entity entity);

		// Safe from script callbacks: during a stage, destroying entities and removing scripts is deferred to the stage's
		// end (they stop running at once). True from such a DestroyEntity until the stage ends.
		bool IsPendingDestroy(Entity entity) const;

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		// The project's render features or the scene's own (saved, copied on Play).
		const SceneRenderSettings& GetRenderSettings() const { return m_RenderSettings; }
		void SetRenderSettings(const SceneRenderSettings& settings) { m_RenderSettings = settings; }
		// The scene's own ambient and fog for forward lighting. What is drawn is GetActiveEnvironment.
		const SceneEnvironment& GetEnvironment() const { return m_Environment; }
		void SetEnvironment(const SceneEnvironment& environment) { m_Environment = environment; }
		// The scene's own exposure and bloom for HDR views. What is drawn is GetActivePostProcess.
		const ScenePostProcess& GetPostProcess() const { return m_PostProcess; }
		void SetPostProcess(const ScenePostProcess& postProcess) { m_PostProcess = postProcess; }
		// Its render profile and which sections come from it (saved as "Look").
		const SceneLook& GetLook() const { return m_Look; }
		void SetLook(const SceneLook& look) { m_Look = look; }
		// Switches the profile (0 = none), as a script does for day and night. Load it first
		// (AssetManager::GetAsset<RenderProfile>): the scene never loads.
		void SetRenderProfile(AssetHandle profile) { m_Look.Profile = profile; }
		// Null when none, missing or not loaded yet.
		Ref<RenderProfile> GetRenderProfile() const;
		// The profile's section when the look takes it from a loaded profile, else the scene's own values. Copies,
		// because the profile can reload while the caller holds them.
		SceneEnvironment GetActiveEnvironment() const;
		ScenePostProcess GetActivePostProcess() const;
		// The DirectionalLightComponent marked IsMainLight, else the first; invalid when none. Warns once about several.
		Entity GetMainLight();

	public:
		// TODO: make this private and expose ways to get components and entities from the registry.
		entt::registry m_Registry;

	private:
		// Delivers the physics steps' collision and trigger events to the entities' scripts (inside a stage).
		void DispatchPhysicsEvents();
		// Delivers UI events to each target's scripts, then its ancestors', until one handles it; clears `events`.
		void DispatchUIEvents(std::vector<UIEvent>& events);
		// Calls one ScriptableEntity hook on every running script of `self`, with `other` as the argument.
		void NotifyScripts(entt::entity self, entt::entity other, void (ScriptableEntity::*hook)(Entity));

		// Stages that run user code bracket it with these: structural changes are queued while a stage is open and
		// applied when the outermost one ends.
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

		// Drops entries for entities destroyed behind the scene's back and adds IDComponents made without CreateEntity.
		void RebuildEntityMap();

		// Destroys one entity at once, not its children.
		void DestroySingleEntityNow(Entity entity);

		std::unordered_map<UUID, entt::entity> m_EntityMap;
		// The entity list as a hierarchy, and the world transforms and UI rects resolved from it.
		SceneGraph m_Graph{ *this };
		SceneTransforms m_Transforms{ *this, m_Graph };
		SceneUILayout m_UILayout{ *this, m_Graph, m_Transforms };
		int m_StageDepth = 0;
		std::vector<DeferredCommand> m_Deferred;
		std::unordered_set<entt::entity> m_PendingDestroy;

		Scope<PhysicsScene> m_Physics; // exists while the game runs this scene
		Scope<PhysicsScene3D> m_Physics3D; // exists while the game runs this scene
		Scope<ParticleScene> m_Particles; // exists while the game runs this scene
		Scope<UIScene> m_UI; // exists while the game runs this scene
		std::vector<UIEvent> m_UIEvents; // this frame's, reused
		UIPointer m_UIPointer;

		std::string m_Name;
		SceneRenderSettings m_RenderSettings;
		SceneEnvironment m_Environment;
		ScenePostProcess m_PostProcess;
		SceneLook m_Look;
		bool m_MainLightWarned = false;
		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;

		friend class Entity;
		friend class SceneRenderer;
		friend void UpdateScripts(Scene&, const SystemContext&);
		friend void CollectScriptGizmos(Scene&, const Entity&, Gizmos&);
	};
}
