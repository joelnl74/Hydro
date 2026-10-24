#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <entt.hpp>

#include "Hydro/Core/Base.h"
#include "Components/Components.h"

namespace Hydro
{
	class Entity;
	class UUID;
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

		// Runtime (the game is running): builds a SystemContext (IsPlaying = true) and runs the
		// stages in order: scripts, physics (none yet), sprite animation, transforms.
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
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
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
		// OnDestroy on scripts that are running. Don't call these from inside a script callback.
		void RemoveScript(Entity entity, size_t index);
		void RemoveScripts(Entity entity);

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

	public:
		// THIS SHOULD BE PRIVATE AND EXPOSE A METHOD TO GET COMPONENTS/ENTITIES FROM REGISTRY.
		entt::registry m_Registry;

	private:
		// Stage 4: world-transform propagation, once there is a hierarchy (S3). Runs in both updates.
		void UpdateTransforms(const SystemContext& context);

		std::string m_Name;
		std::vector<entt::entity> m_EntityOrder;
		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;

		friend class Entity;
		friend class SceneRenderer;
	};
}
