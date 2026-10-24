#include "hypch.h"
#include "EntitySerializer.h"
#include "ScriptableEntity.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <algorithm>
#include <filesystem>

namespace Hydro
{
	// Sprites and animators save the AssetHandle of their texture / animation set (TextureHandle,
	// AnimationsHandle). Scenes saved before handles hold the file's path instead (Texture, Animations,
	// relative to the project's asset folder when the file is inside it); those are still read, and
	// the next save writes the handle.

	// The asset a saved reference points to: the handle when the registry knows it, else the file a
	// path-only (older) scene names, registered now. A handle the registry doesn't know and no path to
	// fall back on is returned as it is, so saving the scene again keeps it (a deleted file can come back).
	static AssetHandle ResolveSavedAsset(const YAML::Node& handleNode, const YAML::Node& pathNode)
	{
		AssetHandle handle = 0;
		if (handleNode.IsDefined())
		{
			handle = AssetHandle(handleNode.as<uint64_t>(0));
			if (AssetManager::IsAssetHandleValid(handle))
			{
				return handle;
			}
		}

		if (pathNode.IsDefined())
		{
			const std::string savedPath = pathNode.as<std::string>("");
			if (savedPath.empty() == false)
			{
				const AssetHandle fromPath = AssetManager::ImportAsset(std::filesystem::u8path(savedPath));
				if ((uint64_t)fromPath != 0)
				{
					return fromPath;
				}
			}
		}

		return handle;
	}

	static const char* Rigidbody2DTypeToString(Rigidbody2DType type)
	{
		switch (type)
		{
		case Rigidbody2DType::Static: return "Static";
		case Rigidbody2DType::Kinematic: return "Kinematic";
		case Rigidbody2DType::Dynamic: return "Dynamic";
		}

		return "Dynamic";
	}

	static Rigidbody2DType Rigidbody2DTypeFromString(const std::string& name)
	{
		if (name == "Static")
		{
			return Rigidbody2DType::Static;
		}
		if (name == "Kinematic")
		{
			return Rigidbody2DType::Kinematic;
		}

		return Rigidbody2DType::Dynamic;
	}

	// Saved values the physics engine would choke on are pulled into range. Written as max(minimum, value)
	// so that NaN (which compares false with everything) also becomes the minimum.
	static float AtLeast(float minimum, float value)
	{
		return std::max(minimum, value);
	}

	static void SerializeMaterial(YAML::Emitter& out, const PhysicsMaterial2D& material)
	{
		out << YAML::Key << "Density" << YAML::Value << material.Density;
		out << YAML::Key << "Friction" << YAML::Value << material.Friction;
		out << YAML::Key << "Restitution" << YAML::Value << material.Restitution;
	}

	static void DeserializeMaterial(const YAML::Node& node, PhysicsMaterial2D& material)
	{
		material.Density = AtLeast(0.0f, node["Density"].as<float>(material.Density));
		material.Friction = AtLeast(0.0f, node["Friction"].as<float>(material.Friction));
		material.Restitution = AtLeast(0.0f, node["Restitution"].as<float>(material.Restitution));
	}

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
			out << YAML::Key << "SortOrder" << YAML::Value << sprite.SortOrder;
			// The handle is saved even when the file is missing, so a texture that comes back is found again.
			if ((uint64_t)sprite.Texture != 0)
			{
				out << YAML::Key << "TextureHandle" << YAML::Value << (uint64_t)sprite.Texture;
				// By name, so slicing the sheet again keeps the sprite; the index is the fallback.
				const Ref<Texture2D> texture = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
				if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
				{
					out << YAML::Key << "Sprite" << YAML::Value << texture->GetSprite(sprite.SpriteIndex).Name;
				}
			}
			out << YAML::Key << "SpriteIndex" << YAML::Value << sprite.SpriteIndex;
			out << YAML::Key << "FlipX" << YAML::Value << sprite.FlipX;
			out << YAML::Key << "FlipY" << YAML::Value << sprite.FlipY;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<SpriteAnimatorComponent>())
		{
			auto& animator = entity.GetComponent<SpriteAnimatorComponent>();

			// Only the settings; the play state starts fresh.
			out << YAML::Key << "SpriteAnimatorComponent";
			out << YAML::BeginMap;
			if ((uint64_t)animator.Animations != 0)
			{
				out << YAML::Key << "AnimationsHandle" << YAML::Value << (uint64_t)animator.Animations;
			}
			out << YAML::Key << "StartClip" << YAML::Value << animator.StartClip;
			out << YAML::Key << "PlayOnStart" << YAML::Value << animator.PlayOnStart;
			out << YAML::Key << "Speed" << YAML::Value << animator.Speed;
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

		if (entity.HasComponent<Light2DComponent>())
		{
			auto& light = entity.GetComponent<Light2DComponent>();

			out << YAML::Key << "Light2DComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Type" << YAML::Value << (light.Type == Light2DType::Global ? "Global" : "Point");
			out << YAML::Key << "Color" << YAML::Value << light.Color;
			out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
			out << YAML::Key << "Radius" << YAML::Value << light.Radius;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<Shadow2DComponent>())
		{
			auto& shadow = entity.GetComponent<Shadow2DComponent>();

			out << YAML::Key << "Shadow2DComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << shadow.Offset;
			out << YAML::Key << "Size" << YAML::Value << shadow.Size;
			out << YAML::Key << "Strength" << YAML::Value << shadow.Strength;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& body = entity.GetComponent<Rigidbody2DComponent>();

			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Type" << YAML::Value << Rigidbody2DTypeToString(body.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << body.FixedRotation;
			out << YAML::Key << "IsBullet" << YAML::Value << body.IsBullet;
			out << YAML::Key << "GravityScale" << YAML::Value << body.GravityScale;
			out << YAML::Key << "LinearDamping" << YAML::Value << body.LinearDamping;
			out << YAML::Key << "AngularDamping" << YAML::Value << body.AngularDamping;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& collider = entity.GetComponent<BoxCollider2DComponent>();

			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
			out << YAML::Key << "Size" << YAML::Value << collider.Size;
			SerializeMaterial(out, collider.Material);
			out << YAML::Key << "IsSensor" << YAML::Value << collider.IsSensor;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& collider = entity.GetComponent<CircleCollider2DComponent>();

			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
			out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
			SerializeMaterial(out, collider.Material);
			out << YAML::Key << "IsSensor" << YAML::Value << collider.IsSensor;
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
			sprite.SortOrder = spriteNode["SortOrder"].as<int32_t>(sprite.SortOrder);

			// The asset is loaded now (a texture that can't be loaded is logged by the manager) so the
			// renderer, which never loads, finds it. The handle is kept either way.
			sprite.Texture = ResolveSavedAsset(spriteNode["TextureHandle"], spriteNode["Texture"]);
			const Ref<Texture2D> texture = (uint64_t)sprite.Texture != 0 ? AssetManager::GetAsset<Texture2D>(sprite.Texture) : nullptr;

			sprite.SpriteIndex = spriteNode["SpriteIndex"].as<uint32_t>(sprite.SpriteIndex);
			sprite.FlipX = spriteNode["FlipX"].as<bool>(sprite.FlipX);
			sprite.FlipY = spriteNode["FlipY"].as<bool>(sprite.FlipY);
			if (auto spriteNameNode = spriteNode["Sprite"]; spriteNameNode && texture != nullptr)
			{
				const int32_t found = texture->FindSprite(spriteNameNode.as<std::string>(""));
				if (found >= 0)
				{
					sprite.SpriteIndex = (uint32_t)found;
				}
			}
		}

		if (auto animatorNode = entityNode["SpriteAnimatorComponent"])
		{
			auto& animator = entity.AddComponent<SpriteAnimatorComponent>();
			animator.StartClip = animatorNode["StartClip"].as<std::string>(animator.StartClip);
			animator.PlayOnStart = animatorNode["PlayOnStart"].as<bool>(animator.PlayOnStart);
			animator.Speed = animatorNode["Speed"].as<float>(animator.Speed);

			// A set that can't be loaded is logged by the manager; the animator then does nothing.
			animator.Animations = ResolveSavedAsset(animatorNode["AnimationsHandle"], animatorNode["Animations"]);
			if ((uint64_t)animator.Animations != 0)
			{
				AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
			}
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

		if (auto lightNode = entityNode["Light2DComponent"])
		{
			auto& light = entity.AddComponent<Light2DComponent>();
			light.Type = lightNode["Type"].as<std::string>("Point") == "Global" ? Light2DType::Global : Light2DType::Point;
			light.Color = lightNode["Color"].as<glm::vec3>(light.Color);
			light.Intensity = lightNode["Intensity"].as<float>(light.Intensity);
			light.Radius = lightNode["Radius"].as<float>(light.Radius);
		}

		if (auto shadowNode = entityNode["Shadow2DComponent"])
		{
			auto& shadow = entity.AddComponent<Shadow2DComponent>();
			shadow.Offset = shadowNode["Offset"].as<glm::vec2>(shadow.Offset);
			shadow.Size = shadowNode["Size"].as<glm::vec2>(shadow.Size);
			shadow.Strength = shadowNode["Strength"].as<float>(shadow.Strength);
		}

		if (auto bodyNode = entityNode["Rigidbody2DComponent"])
		{
			auto& body = entity.AddComponent<Rigidbody2DComponent>();
			body.Type = Rigidbody2DTypeFromString(bodyNode["Type"].as<std::string>("Dynamic"));
			body.FixedRotation = bodyNode["FixedRotation"].as<bool>(body.FixedRotation);
			body.IsBullet = bodyNode["IsBullet"].as<bool>(body.IsBullet);
			body.GravityScale = bodyNode["GravityScale"].as<float>(body.GravityScale);
			body.LinearDamping = AtLeast(0.0f, bodyNode["LinearDamping"].as<float>(body.LinearDamping));
			body.AngularDamping = AtLeast(0.0f, bodyNode["AngularDamping"].as<float>(body.AngularDamping));
		}

		if (auto boxNode = entityNode["BoxCollider2DComponent"])
		{
			auto& collider = entity.AddComponent<BoxCollider2DComponent>();
			collider.Offset = boxNode["Offset"].as<glm::vec2>(collider.Offset);
			collider.Size = boxNode["Size"].as<glm::vec2>(collider.Size);
			collider.Size.x = AtLeast(0.001f, collider.Size.x);
			collider.Size.y = AtLeast(0.001f, collider.Size.y);
			DeserializeMaterial(boxNode, collider.Material);
			collider.IsSensor = boxNode["IsSensor"].as<bool>(collider.IsSensor);
		}

		if (auto circleNode = entityNode["CircleCollider2DComponent"])
		{
			auto& collider = entity.AddComponent<CircleCollider2DComponent>();
			collider.Offset = circleNode["Offset"].as<glm::vec2>(collider.Offset);
			collider.Radius = AtLeast(0.001f, circleNode["Radius"].as<float>(collider.Radius));
			DeserializeMaterial(circleNode, collider.Material);
			collider.IsSensor = circleNode["IsSensor"].as<bool>(collider.IsSensor);
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
