#include "hypch.h"
#include "EntitySerializer.h"
#include "ScriptableEntity.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Project/Project.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <filesystem>

namespace Hydro
{
	// Sprites and animators save the asset's handle (it follows renames and moves) and its path (readable,
	// the fallback for files the AssetManager doesn't know, and how scenes from before handles load).
	// TODO(assets): once components hold an AssetHandle (Roadmap AS3) the path keys and these helpers go away.

	// The file a saved reference points to: the handle's file when the registry has it, else the saved path.
	static std::filesystem::path ResolveSavedAsset(const YAML::Node& handleNode, const std::string& savedPath);

	// Relative to the active project's asset folder when the file is inside it (so projects can move),
	// otherwise absolute. Forward slashes either way.
	static std::string ToSavedAssetPath(const std::filesystem::path& path)
	{
		if (auto project = Project::GetActive(); project != nullptr && project->GetProjectDirectory().empty() == false)
		{
			std::error_code error;
			const std::filesystem::path relative = std::filesystem::relative(path, project->GetAssetDirectory(), error);
			if (error.value() == 0 && relative.empty() == false && *relative.begin() != "..")
			{
				return relative.generic_u8string();
			}
		}

		return path.generic_u8string();
	}

	static std::filesystem::path FromSavedAssetPath(const std::string& saved)
	{
		std::filesystem::path path = std::filesystem::u8path(saved);
		if (path.is_relative())
		{
			if (auto project = Project::GetActive(); project != nullptr && project->GetProjectDirectory().empty() == false)
			{
				return project->GetAssetDirectory() / path;
			}
		}

		return path;
	}

	// Where `handle` is now (the file may have been renamed since it was loaded), else `loadedPath`.
	static std::filesystem::path GetCurrentPath(AssetHandle handle, const std::filesystem::path& loadedPath)
	{
		return AssetManager::IsAssetHandleValid(handle) ? AssetManager::GetAbsolutePath(handle) : loadedPath;
	}

	static std::filesystem::path ResolveSavedAsset(const YAML::Node& handleNode, const std::string& savedPath)
	{
		if (handleNode.IsDefined())
		{
			const AssetHandle handle = AssetHandle(handleNode.as<uint64_t>(0));
			if (AssetManager::IsAssetHandleValid(handle))
			{
				return AssetManager::GetAbsolutePath(handle);
			}
		}

		return savedPath.empty() ? std::filesystem::path() : FromSavedAssetPath(savedPath);
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
			// Textures made in code have no file and aren't saved.
			if (sprite.Texture != nullptr && sprite.Texture->GetPath().empty() == false)
			{
				const AssetHandle textureHandle = sprite.Texture->GetHandle();
				if ((uint64_t)textureHandle != 0)
				{
					out << YAML::Key << "TextureHandle" << YAML::Value << (uint64_t)textureHandle;
				}
				out << YAML::Key << "Texture" << YAML::Value << ToSavedAssetPath(GetCurrentPath(textureHandle, sprite.Texture->GetPath()));
				// By name, so slicing the sheet again keeps the sprite; the index is the fallback.
				if (sprite.Texture->GetSpriteMode() == SpriteMode::Multiple)
				{
					out << YAML::Key << "Sprite" << YAML::Value << sprite.Texture->GetSprite(sprite.SpriteIndex).Name;
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
			if (animator.Animations != nullptr && animator.Animations->GetPath().empty() == false)
			{
				const AssetHandle animationsHandle = animator.Animations->GetHandle();
				if ((uint64_t)animationsHandle != 0)
				{
					out << YAML::Key << "AnimationsHandle" << YAML::Value << (uint64_t)animationsHandle;
				}
				out << YAML::Key << "Animations" << YAML::Value << ToSavedAssetPath(GetCurrentPath(animationsHandle, animator.Animations->GetPath()));
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

			// A texture that can't be loaded is logged by LoadFromFile; the sprite is drawn untextured.
			const std::string savedTexture = spriteNode["Texture"].as<std::string>("");
			const std::filesystem::path textureFile = ResolveSavedAsset(spriteNode["TextureHandle"], savedTexture);
			if (textureFile.empty() == false)
			{
				sprite.Texture = Texture2D::LoadFromFile(textureFile);
			}

			sprite.SpriteIndex = spriteNode["SpriteIndex"].as<uint32_t>(sprite.SpriteIndex);
			sprite.FlipX = spriteNode["FlipX"].as<bool>(sprite.FlipX);
			sprite.FlipY = spriteNode["FlipY"].as<bool>(sprite.FlipY);
			if (auto spriteNameNode = spriteNode["Sprite"]; spriteNameNode && sprite.Texture != nullptr)
			{
				const int32_t found = sprite.Texture->FindSprite(spriteNameNode.as<std::string>(""));
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

			// A set that can't be loaded is logged by LoadFromFile; the animator then does nothing.
			const std::string savedAnimations = animatorNode["Animations"].as<std::string>("");
			const std::filesystem::path animationsFile = ResolveSavedAsset(animatorNode["AnimationsHandle"], savedAnimations);
			if (animationsFile.empty() == false)
			{
				animator.Animations = SpriteAnimationSet::LoadFromFile(animationsFile);
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
