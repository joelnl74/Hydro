#include <hypch.h>
#include "SceneTransforms.h"
#include "Entity.h"
#include "Scene.h"
#include "SceneGraph.h"
#include "TransformMath.h"

#include <cmath>

namespace Hydro
{
	// WorldTransformComponent comes and goes with TransformComponent (registry signals), so Resolve never adds or
	// removes components.
	static void AddWorldTransform(entt::registry& registry, entt::entity entity)
	{
		registry.emplace_or_replace<WorldTransformComponent>(entity);
	}

	static void RemoveWorldTransform(entt::registry& registry, entt::entity entity)
	{
		registry.remove_if_exists<WorldTransformComponent>(entity);
	}

	SceneTransforms::SceneTransforms(Scene& scene, SceneGraph& graph)
		: m_Scene(scene), m_Graph(graph)
	{
		m_Scene.m_Registry.on_construct<TransformComponent>().connect<&AddWorldTransform>();
		m_Scene.m_Registry.on_destroy<TransformComponent>().connect<&RemoveWorldTransform>();
	}


	static void WriteWorldTransform(WorldTransformComponent& world, const glm::mat4& matrix)
	{
		world.Matrix = matrix;
		if (TransformMath::Decompose(matrix, world.Translation, world.Rotation, world.Scale) == false)
		{
			// A zero scale somewhere up the chain: there is no rotation to find; keep the position.
			world.Translation = glm::vec3(matrix[3]);
			world.Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			world.Scale = glm::vec3(0.0f);
		}
	}

	void SceneTransforms::Resolve()
	{
		m_Graph.Ensure();

		static const glm::mat4 s_Identity(1.0f);
		const size_t count = m_Graph.GetList().size();
		m_WorldMatrices.resize(count);
		size_t written = 0;
		for (size_t i = 0; i < count; i++)
		{
			const entt::entity handle = m_Graph.GetList()[i];
			const int32_t parent = m_Graph.GetLayout().GetParentIndex(i);
			const glm::mat4& parentWorld = parent >= 0 ? m_WorldMatrices[(size_t)parent] : s_Identity;
			if (m_Scene.m_Registry.has<TransformComponent>(handle) == false)
			{
				// No transform of its own: its children are placed by its parent's.
				m_WorldMatrices[i] = parentWorld;
				continue;
			}

			const glm::mat4 local = m_Scene.m_Registry.get<TransformComponent>(handle).GetTransform();
			m_WorldMatrices[i] = parent >= 0 ? parentWorld * local : local;
			HY_CORE_ASSERT(m_Scene.m_Registry.has<WorldTransformComponent>(handle),
				"Scene: an entity has a TransformComponent but no WorldTransformComponent");
			WriteWorldTransform(m_Scene.m_Registry.get<WorldTransformComponent>(handle), m_WorldMatrices[i]);
			written++;
		}

		// Entities outside the list (raw registry entities without an IDComponent) have no parent.
		if (written < m_Scene.m_Registry.size<TransformComponent>())
		{
			auto view = m_Scene.m_Registry.view<TransformComponent, WorldTransformComponent>();
			for (entt::entity handle : view)
			{
				if (m_Graph.IsListed(handle) == false)
				{
					WriteWorldTransform(view.get<WorldTransformComponent>(handle), view.get<TransformComponent>(handle).GetTransform());
				}
			}
		}
	}

	glm::mat4 SceneTransforms::GetWorld(Entity entity)
	{
		if ((bool)entity == false || m_Scene.m_Registry.valid(entity) == false)
		{
			return glm::mat4(1.0f);
		}

		const int32_t index = m_Graph.GetIndex((entt::entity)entity);
		if (index < 0)
		{
			return m_Scene.m_Registry.has<TransformComponent>(entity) ? m_Scene.m_Registry.get<TransformComponent>(entity).GetTransform()
				: glm::mat4(1.0f);
		}

		// Up the chain: each ancestor's local transform goes on the left.
		glm::mat4 world(1.0f);
		for (int32_t i = index; i >= 0; i = m_Graph.GetLayout().GetParentIndex((size_t)i))
		{
			const entt::entity handle = m_Graph.GetList()[(size_t)i];
			if (m_Scene.m_Registry.has<TransformComponent>(handle))
			{
				world = m_Scene.m_Registry.get<TransformComponent>(handle).GetTransform() * world;
			}
		}
		return world;
	}

	bool SceneTransforms::SetWorld(Entity entity, const glm::mat4& world)
	{
		if ((bool)entity == false || m_Scene.m_Registry.valid(entity) == false
			|| m_Scene.m_Registry.has<TransformComponent>(entity) == false)
		{
			return false;
		}

		glm::mat4 parentWorld(1.0f);
		const int32_t index = m_Graph.GetIndex((entt::entity)entity);
		if (index >= 0)
		{
			const int32_t parent = m_Graph.GetLayout().GetParentIndex((size_t)index);
			if (parent >= 0)
			{
				parentWorld = GetWorld(Entity{ m_Graph.GetList()[(size_t)parent], &m_Scene });
			}
		}
		if (std::abs(glm::determinant(glm::mat3(parentWorld))) < 1e-12f)
		{
			HY_CORE_WARN("Scene '{}': can't place '{}' in the world: a parent has a zero scale", m_Scene.GetName(), entity.GetName());
			return false;
		}

		const glm::mat4 local = glm::inverse(parentWorld) * world;
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
		if (TransformMath::Decompose(local, translation, rotation, scale) == false)
		{
			HY_CORE_WARN("Scene '{}': can't place '{}' in the world: the transform has a zero scale", m_Scene.GetName(), entity.GetName());
			return false;
		}

		TransformComponent& transform = m_Scene.m_Registry.get<TransformComponent>(entity);
		transform.Translation = translation;
		transform.Rotation = TransformMath::ToEulerAngles(rotation, transform.Rotation);
		transform.Scale = scale;

		if (TransformMath::IsTRS(local))
		{
			return true;
		}
		if (m_ShearWarned.insert((entt::entity)entity).second)
		{
			HY_CORE_WARN("Scene '{}': '{}' would need shear to stay in place (a rotated parent with non-uniform scale); it got the closest transform without",
				m_Scene.GetName(), entity.GetName());
		}
		return false;
	}

	glm::vec3 SceneTransforms::GetWorldPosition(Entity entity)
	{
		return glm::vec3(GetWorld(entity)[3]);
	}

	bool SceneTransforms::SetWorldPosition(Entity entity, const glm::vec3& position)
	{
		if ((bool)entity == false || m_Scene.m_Registry.valid(entity) == false
			|| m_Scene.m_Registry.has<TransformComponent>(entity) == false)
		{
			return false;
		}

		const int32_t index = m_Graph.GetIndex((entt::entity)entity);
		const int32_t parent = index >= 0 ? m_Graph.GetLayout().GetParentIndex((size_t)index) : -1;
		if (parent < 0)
		{
			m_Scene.m_Registry.get<TransformComponent>(entity).Translation = position;
			return true;
		}

		// world = parentWorld * translate(local) * rotation * scale: the world position is parentWorld applied to the
		// local Translation, so only that changes.
		const glm::mat4 parentWorld = GetWorld(Entity{ m_Graph.GetList()[(size_t)parent], &m_Scene });
		if (std::abs(glm::determinant(glm::mat3(parentWorld))) < 1e-12f)
		{
			return false;
		}
		m_Scene.m_Registry.get<TransformComponent>(entity).Translation = glm::vec3(glm::inverse(parentWorld) * glm::vec4(position, 1.0f));
		return true;
	}
}
