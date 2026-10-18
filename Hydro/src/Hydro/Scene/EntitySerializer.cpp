#include "hypch.h"
#include "EntitySerializer.h"
#include "ScriptableEntity.h"

namespace Hydro
{
	void EntitySerializer::Serialize(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << (uint64_t)entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>();

			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Tag" << YAML::Value << tag.Tag;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<TransformComponent>())
		{
			auto& transform = entity.GetComponent<TransformComponent>();

			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Translation" << YAML::Value << transform.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << transform.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << transform.Scale;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			auto& sprite = entity.GetComponent<SpriteRendererComponent>();

			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Color" << YAML::Value << sprite.Color;
			out << YAML::Key << "TilingFactor" << YAML::Value << sprite.TilingFactor;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			const SceneCamera& camera = cameraComponent.Camera;

			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "Projection" << YAML::Value << (camera.Projection == ProjectionType::Perspective ? "Perspective" : "Orthographic");
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.PerspectiveFOV;
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.PerspectiveNear;
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.PerspectiveFar;
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.OrthographicSize;
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.OrthographicNear;
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.OrthographicFar;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<NativeScriptComponent>())
		{
			auto& scripts = entity.GetComponent<NativeScriptComponent>();

			// Scripts are saved by their ScriptRegistry name. Ones bound in code without a name can't
			// be found again on load, so they are left out.
			out << YAML::Key << "NativeScriptComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Scripts" << YAML::Value << YAML::BeginSeq;
			for (const auto& script : scripts.Scripts)
			{
				if (script.Name.empty())
					HY_CORE_WARN("Entity '{}': a script bound in code without a ScriptRegistry name isn't saved", entity.GetName());
				else
					out << script.Name;
			}
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}

		out << YAML::EndMap; // Entity
	}

	Entity EntitySerializer::Deserialize(const YAML::Node& entityNode, Scene& scene, bool keepUUID)
	{
		std::string name;
		if (auto tagNode = entityNode["TagComponent"])
			name = tagNode["Tag"].as<std::string>("");

		UUID uuid; // fresh random id by default
		if (keepUUID && entityNode["Entity"])
			uuid = UUID(entityNode["Entity"].as<uint64_t>());

		// CreateEntityWithUUID adds the Tag + ID components.
		Entity entity = scene.CreateEntityWithUUID(uuid, name);

		if (auto transformNode = entityNode["TransformComponent"])
		{
			auto& transform = entity.AddComponent<TransformComponent>();
			transform.Translation = transformNode["Translation"].as<glm::vec3>(transform.Translation);
			transform.Rotation = transformNode["Rotation"].as<glm::vec3>(transform.Rotation);
			transform.Scale = transformNode["Scale"].as<glm::vec3>(transform.Scale);
		}

		if (auto spriteNode = entityNode["SpriteRendererComponent"])
		{
			auto& sprite = entity.AddComponent<SpriteRendererComponent>();
			sprite.Color = spriteNode["Color"].as<glm::vec4>(sprite.Color);
			sprite.TilingFactor = spriteNode["TilingFactor"].as<float>(sprite.TilingFactor);
		}

		if (auto cameraNode = entityNode["CameraComponent"])
		{
			auto& cameraComponent = entity.AddComponent<CameraComponent>();
			SceneCamera& camera = cameraComponent.Camera;

			cameraComponent.Primary = cameraNode["Primary"].as<bool>(cameraComponent.Primary);
			camera.Projection = cameraNode["Projection"].as<std::string>("Orthographic") == "Perspective" ? ProjectionType::Perspective : ProjectionType::Orthographic;
			camera.PerspectiveFOV = cameraNode["PerspectiveFOV"].as<float>(camera.PerspectiveFOV);
			camera.PerspectiveNear = cameraNode["PerspectiveNear"].as<float>(camera.PerspectiveNear);
			camera.PerspectiveFar = cameraNode["PerspectiveFar"].as<float>(camera.PerspectiveFar);
			camera.OrthographicSize = cameraNode["OrthographicSize"].as<float>(camera.OrthographicSize);
			camera.OrthographicNear = cameraNode["OrthographicNear"].as<float>(camera.OrthographicNear);
			camera.OrthographicFar = cameraNode["OrthographicFar"].as<float>(camera.OrthographicFar);
		}

		if (auto scriptNode = entityNode["NativeScriptComponent"])
		{
			auto& scripts = entity.AddComponent<NativeScriptComponent>();
			for (auto nameNode : scriptNode["Scripts"])
			{
				const std::string name = nameNode.as<std::string>("");
				if (name.empty())
					continue;

				scripts.Bind(name);
				if (ScriptRegistry::Find(name) == nullptr)
					HY_CORE_WARN("Entity '{}': script '{}' isn't registered; it is kept but won't run", entity.GetName(), name);
			}
		}

		return entity;
	}
}
