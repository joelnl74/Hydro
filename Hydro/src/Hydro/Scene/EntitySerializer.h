#pragma once
#include "Entity.h"

#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>

// Shared YAML encoding for entities and their components.
// Used by both SceneSerializer (.hyscene) and PrefabSerializer (.hyprefab),
// so a new component only has to be added in EntitySerializer.cpp.

namespace YAML
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (node.IsSequence() == false || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (node.IsSequence() == false || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (node.IsSequence() == false || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	Emitter& operator<<(Emitter& out, const glm::vec2& v);
	Emitter& operator<<(Emitter& out, const glm::vec3& v);
	Emitter& operator<<(Emitter& out, const glm::vec4& v);
}

namespace Hydro
{
	class EntitySerializer
	{
	public:
		// Writes one entity as a YAML map: { Entity: <uuid>, TagComponent: ..., TransformComponent: ..., ... }
		static void Serialize(YAML::Emitter& out, Entity entity);

		// Creates an entity in `scene` from a map written by Serialize().
		// keepUUID = true  -> reuse the saved UUID (loading a scene).
		// keepUUID = false -> give it a fresh UUID (instantiating a prefab, so copies don't clash).
		static Entity Deserialize(const YAML::Node& entityNode, Scene& scene, bool keepUUID);
	};
}
