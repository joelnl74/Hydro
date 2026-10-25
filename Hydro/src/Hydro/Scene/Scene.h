#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <entt.hpp>

#include "Hydro/Core/Base.h"
#include "Hydro/Core/UUID.h"
#include "Components/Components.h"

namespace Hydro
{
	class Entity;
	class PhysicsScene;
	class ParticleScene;
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

		// The game starts / stops running this scene: creates / destroys its 2D physics world (see
		// PhysicsScene for which entities become bodies) and its ParticleScene; starting also resets
		// every animator's playback (ResetSpriteAnimations). OnUpdateRuntime starts it by itself, before its first stage,
		// if it was not started, and the destructor stops it; the editor calls both around Play and Stop.
		void OnRuntimeStart();
		void OnRuntimeStop();
		// The running physics world, or null while editing / before OnRuntimeStart.
		PhysicsScene* GetPhysicsScene() { return m_Physics.get(); }
		// The live particles (ParticleScene), or null while editing / before OnRuntimeStart.
		ParticleScene* GetParticleScene() { return m_Particles.get(); }

		// Runtime (the game is running): builds a SystemContext (IsPlaying = true) and runs the
		// stages in order: scripts, physics, sprite animation, transforms, particles.
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
		void DestroyEntity(Entity entity);

		// Every entity made with CreateEntity, in list order: the order the hierarchy shows them,
		// scenes save them and Copy recreates them. New entities go last. The reference is
		// invalidated by creating, destroying or moving entities.
		const std::vector<entt::entity>& GetEntityOrder();

		// Moves `entity` directly before (or, with placeAfter, directly after) `target` in the list.
		// Don't call it while iterating GetEntityOrder().
		void MoveEntity(Entity entity, Entity target, bool placeAfter);

		// A new entity (new UUID, same name) in this scene with a copy of the entity's
		// CopyableComponents and native script bindings. Returns an empty Entity if `entity` is
		// not valid. Don't call it while iterating the registry.
		Entity DuplicateEntity(Entity entity);

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

	public:
		// THIS SHOULD BE PRIVATE AND EXPOSE A METHOD TO GET COMPONENTS/ENTITIES FROM REGISTRY.
		entt::registry m_Registry;

	private:
		// Delivers the physics step's collision and trigger events to the entities' scripts (inside a stage).
		void DispatchPhysicsEvents();
		// Calls one ScriptableEntity hook on every running script of `self`, with `other` as the argument.
		void NotifyScripts(entt::entity self, entt::entity other, void (ScriptableEntity::*hook)(Entity));

		// Stage 4: world-transform propagation, once there is a hierarchy (S3). Runs in both updates.
		void UpdateTransforms(const SystemContext& context);

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

		std::unordered_map<UUID, entt::entity> m_EntityMap;
		int m_StageDepth = 0;
		std::vector<DeferredCommand> m_Deferred;
		std::unordered_set<entt::entity> m_PendingDestroy;

		Scope<PhysicsScene> m_Physics; // exists while the game runs this scene
		Scope<ParticleScene> m_Particles; // exists while the game runs this scene

		std::string m_Name;
		std::vector<entt::entity> m_EntityOrder;
		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;

		friend class Entity;
		friend class SceneRenderer;
		friend void UpdateScripts(Scene&, const SystemContext&);
	};
}
