#include "hypch.h"
#include "Gizmos.h"

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Scripting/ScriptableEntity.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <memory>

namespace Hydro
{
	void Gizmos::Line(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness)
	{
		m_Lines.push_back({ start, end, color, thickness });
	}

	void Gizmos::Arrow(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness)
	{
		Line(start, end, color, thickness);
		const glm::vec2 along(end.x - start.x, end.y - start.y);
		const float length = glm::length(along);
		if (length < 1e-4f)
		{
			return;
		}
		const glm::vec2 back = -along / length * std::min(0.35f, length * 0.4f);
		const glm::vec2 side(-back.y * 0.6f, back.x * 0.6f);
		Line(end, end + glm::vec3(back + side, 0.0f), color, thickness);
		Line(end, end + glm::vec3(back - side, 0.0f), color, thickness);
	}

	void Gizmos::WireCircle(const glm::vec3& centre, float radius, const glm::vec4& color, int segments)
	{
		segments = std::max(segments, 3);
		glm::vec3 previous = centre + glm::vec3(radius, 0.0f, 0.0f);
		for (int i = 1; i <= segments; i++)
		{
			const float angle = glm::two_pi<float>() * (float)i / (float)segments;
			const glm::vec3 point = centre + glm::vec3(std::cos(angle), std::sin(angle), 0.0f) * radius;
			Line(previous, point, color, 1.5f);
			previous = point;
		}
	}

	void Gizmos::Cross(const glm::vec3& position, float size, const glm::vec4& color)
	{
		const float half = size * 0.5f;
		Line(position - glm::vec3(half, 0.0f, 0.0f), position + glm::vec3(half, 0.0f, 0.0f), color);
		Line(position - glm::vec3(0.0f, half, 0.0f), position + glm::vec3(0.0f, half, 0.0f), color);
	}

	void Gizmos::Label(const glm::vec3& position, const std::string& text, const glm::vec4& color)
	{
		m_Labels.push_back({ position, text, color });
	}

	void Gizmos::Clear()
	{
		m_Lines.clear();
		m_Labels.clear();
		m_SelectedEntity = {};
		m_Selected = false;
	}

	void Gizmos::Begin(const Entity& owner, const Entity& selected)
	{
		m_SelectedEntity = selected;
		m_Selected = (bool)selected && owner == selected;
	}

	void CollectScriptGizmos(Scene& scene, const Entity& selected, Gizmos& gizmos)
	{
		auto view = scene.m_Registry.view<NativeScriptComponent>();
		for (auto handle : view)
		{
			NativeScriptComponent& scripts = view.get<NativeScriptComponent>(handle);
			const Entity entity{ handle, &scene };
			for (NativeScriptComponent::Script& binding : scripts.Scripts)
			{
				const ScriptTypeInfo* type = ScriptRegistry::FindType(binding.Name);
				if (type == nullptr || type->DrawsGizmos == false)
				{
					continue;
				}
				gizmos.Begin(entity, selected);
				if (binding.Instance != nullptr)
				{
					binding.Instance->OnDrawGizmos(gizmos);
					continue;
				}
				if (binding.Instantiate == nullptr)
				{
					continue;
				}
				// A stand-in with the saved values: what the script would start with.
				std::unique_ptr<ScriptableEntity> instance(binding.Instantiate());
				instance->m_Entity = entity;
				for (const ScriptProperty& property : type->Properties)
				{
					if (property.Apply == nullptr)
					{
						continue;
					}
					auto saved = binding.Properties.find(property.Name);
					const bool usable = saved != binding.Properties.end() && ScriptPropertyHasType(saved->second, property.Type);
					property.Apply(*instance, usable ? saved->second : property.DefaultValue);
				}
				instance->OnDrawGizmos(gizmos);
			}
		}
	}
}
