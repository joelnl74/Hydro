#pragma once
#include <entt.hpp>
#include <glm/glm.hpp>

#include <unordered_set>
#include <vector>

namespace Hydro
{
	class Entity;
	class Scene;
	class SceneGraph;

	// A scene's world transforms. TransformComponent is local (relative to the parent); WorldTransformComponent is the
	// whole chain, comes and goes with it and is written only by Resolve. Scene owns one and forwards its API here.
	class SceneTransforms
	{
	public:
		SceneTransforms(Scene& scene, SceneGraph& graph);

		// Writes every WorldTransformComponent, parents first, in one pass.
		void Resolve();
		// The world matrices of the last Resolve, in list order.
		const std::vector<glm::mat4>& GetResolvedMatrices() const { return m_WorldMatrices; }

		glm::mat4 GetWorld(Entity entity);
		bool SetWorld(Entity entity, const glm::mat4& world);
		glm::vec3 GetWorldPosition(Entity entity);
		bool SetWorldPosition(Entity entity, const glm::vec3& position);

	private:
		Scene& m_Scene;
		SceneGraph& m_Graph;
		// Kept between passes so the pass allocates nothing once warm.
		std::vector<glm::mat4> m_WorldMatrices;
		// Already warned about shear, so the log isn't flooded.
		std::unordered_set<entt::entity> m_ShearWarned;
	};
}
