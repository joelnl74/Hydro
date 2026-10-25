#include "hypch.h"
#include "EntitySerializer.h"
#include "ScriptableEntity.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <algorithm>
#include <cfloat>
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

	static const char* SpriteFacingToString(SpriteFacing facing)
	{
		switch (facing)
		{
		case SpriteFacing::Flat: return "Flat";
		case SpriteFacing::Upright: return "Upright";
		case SpriteFacing::Billboard: return "Billboard";
		}

		return "Flat";
	}

	static const char* SpriteAlphaModeToString(SpriteAlphaMode mode)
	{
		switch (mode)
		{
		case SpriteAlphaMode::Default: return "Default";
		case SpriteAlphaMode::Cutout: return "Cutout";
		case SpriteAlphaMode::Translucent: return "Translucent";
		}

		return "Default";
	}

	static SpriteAlphaMode SpriteAlphaModeFromString(const std::string& name)
	{
		if (name == "Cutout")
		{
			return SpriteAlphaMode::Cutout;
		}
		if (name == "Translucent")
		{
			return SpriteAlphaMode::Translucent;
		}

		return SpriteAlphaMode::Default;
	}

	static const char* MeshShapeToString(MeshShape shape)
	{
		switch (shape)
		{
		case MeshShape::Box: return "Box";
		case MeshShape::Plane: return "Plane";
		case MeshShape::Ramp: return "Ramp";
		case MeshShape::Stairs: return "Stairs";
		case MeshShape::Cylinder: return "Cylinder";
		}

		return "Box";
	}

	static MeshShape MeshShapeFromString(const std::string& name)
	{
		if (name == "Plane")
		{
			return MeshShape::Plane;
		}
		if (name == "Ramp")
		{
			return MeshShape::Ramp;
		}
		if (name == "Stairs")
		{
			return MeshShape::Stairs;
		}
		if (name == "Cylinder")
		{
			return MeshShape::Cylinder;
		}

		return MeshShape::Box;
	}

	static void SerializeMeshMaterial(YAML::Emitter& out, const MaterialData& material)
	{
		out << YAML::BeginMap;
		if ((uint64_t)material.Albedo != 0)
		{
			out << YAML::Key << "AlbedoHandle" << YAML::Value << (uint64_t)material.Albedo;
		}
		if ((uint64_t)material.Emissive != 0)
		{
			out << YAML::Key << "EmissiveHandle" << YAML::Value << (uint64_t)material.Emissive;
		}
		out << YAML::Key << "Tint" << YAML::Value << material.Tint;
		out << YAML::Key << "Tiling" << YAML::Value << material.Tiling;
		out << YAML::Key << "EmissionColor" << YAML::Value << material.EmissionColor;
		out << YAML::Key << "EmissionIntensity" << YAML::Value << material.EmissionIntensity;
		out << YAML::Key << "AlphaCutoff" << YAML::Value << material.AlphaCutoff;
		out << YAML::Key << "Filter" << YAML::Value << (material.Filter == TextureFilter::Linear ? "Linear" : "Nearest");
		out << YAML::EndMap;
	}

	// The textures are loaded now, so the renderer (which never loads) finds them.
	static MaterialData DeserializeMeshMaterial(const YAML::Node& node)
	{
		MaterialData material;
		material.Albedo = ResolveSavedAsset(node["AlbedoHandle"], node["Albedo"]);
		material.Emissive = ResolveSavedAsset(node["EmissiveHandle"], node["Emissive"]);
		if ((uint64_t)material.Albedo != 0)
		{
			AssetManager::GetAsset<Texture2D>(material.Albedo);
		}
		if ((uint64_t)material.Emissive != 0)
		{
			AssetManager::GetAsset<Texture2D>(material.Emissive);
		}
		material.Tint = node["Tint"].as<glm::vec4>(material.Tint);
		material.Tiling = node["Tiling"].as<glm::vec2>(material.Tiling);
		material.EmissionColor = node["EmissionColor"].as<glm::vec3>(material.EmissionColor);
		material.EmissionIntensity = node["EmissionIntensity"].as<float>(material.EmissionIntensity);
		material.AlphaCutoff = node["AlphaCutoff"].as<float>(material.AlphaCutoff);
		material.Filter = node["Filter"].as<std::string>("Nearest") == "Linear" ? TextureFilter::Linear : TextureFilter::Nearest;
		return material;
	}

	static SpriteFacing SpriteFacingFromString(const std::string& name)
	{
		if (name == "Upright")
		{
			return SpriteFacing::Upright;
		}
		if (name == "Billboard")
		{
			return SpriteFacing::Billboard;
		}

		return SpriteFacing::Flat;
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

	static const char* EmitterShapeToString(EmitterShape shape)
	{
		switch (shape)
		{
		case EmitterShape::Point: return "Point";
		case EmitterShape::Circle: return "Circle";
		case EmitterShape::Box: return "Box";
		}

		return "Point";
	}

	static EmitterShape EmitterShapeFromString(const std::string& name)
	{
		if (name == "Circle")
		{
			return EmitterShape::Circle;
		}
		if (name == "Box")
		{
			return EmitterShape::Box;
		}

		return EmitterShape::Point;
	}

	// A FloatRange is saved as [Min, Max]. Read back clamped to at least `minimum`, with Min <= Max.
	static FloatRange ReadRange(const YAML::Node& node, FloatRange fallback, float minimum = -FLT_MAX)
	{
		const glm::vec2 values = node.as<glm::vec2>(glm::vec2(fallback.Min, fallback.Max));
		const float a = AtLeast(minimum, values.x);
		const float b = AtLeast(minimum, values.y);
		return { std::min(a, b), std::max(a, b) };
	}

	static uint32_t ReadCount(const YAML::Node& node, uint32_t fallback, uint32_t minimum, uint32_t maximum)
	{
		return std::clamp(node.as<uint32_t>(fallback), minimum, maximum);
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
			out << YAML::Key << "Facing" << YAML::Value << SpriteFacingToString(sprite.Facing);
			// Only when set, so scenes that don't use them are saved as before.
			if (sprite.AlphaMode != SpriteAlphaMode::Default)
			{
				out << YAML::Key << "AlphaMode" << YAML::Value << SpriteAlphaModeToString(sprite.AlphaMode);
			}
			if (sprite.Overlay)
			{
				out << YAML::Key << "Overlay" << YAML::Value << true;
			}
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
			out << YAML::Key << "Height" << YAML::Value << light.Height;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<MeshComponent>())
		{
			auto& mesh = entity.GetComponent<MeshComponent>();

			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Shape" << YAML::Value << MeshShapeToString(mesh.Shape);
			out << YAML::Key << "Size" << YAML::Value << mesh.Size;
			if (mesh.Shape == MeshShape::Stairs)
			{
				out << YAML::Key << "Steps" << YAML::Value << mesh.Steps;
			}
			if (mesh.Shape == MeshShape::Cylinder)
			{
				out << YAML::Key << "Segments" << YAML::Value << mesh.Segments;
			}
			out << YAML::Key << "CastShadows" << YAML::Value << mesh.CastShadows;
			out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
			for (const MaterialData& material : mesh.Materials)
			{
				SerializeMeshMaterial(out, material);
			}
			out << YAML::EndSeq;
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

		if (entity.HasComponent<ParticleEmitter2DComponent>())
		{
			auto& emitter = entity.GetComponent<ParticleEmitter2DComponent>();

			out << YAML::Key << "ParticleEmitter2DComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "PlayOnStart" << YAML::Value << emitter.PlayOnStart;
			out << YAML::Key << "Looping" << YAML::Value << emitter.Looping;
			out << YAML::Key << "Duration" << YAML::Value << emitter.Duration;
			out << YAML::Key << "Rate" << YAML::Value << emitter.Rate;
			out << YAML::Key << "BurstCount" << YAML::Value << emitter.BurstCount;
			out << YAML::Key << "MaxParticles" << YAML::Value << emitter.MaxParticles;
			out << YAML::Key << "Seed" << YAML::Value << emitter.Seed;
			out << YAML::Key << "Shape" << YAML::Value << EmitterShapeToString(emitter.Shape);
			out << YAML::Key << "ShapeSize" << YAML::Value << emitter.ShapeSize;
			out << YAML::Key << "Lifetime" << YAML::Value << glm::vec2(emitter.Lifetime.Min, emitter.Lifetime.Max);
			out << YAML::Key << "Speed" << YAML::Value << glm::vec2(emitter.Speed.Min, emitter.Speed.Max);
			out << YAML::Key << "Direction" << YAML::Value << emitter.Direction;
			out << YAML::Key << "Spread" << YAML::Value << emitter.Spread;
			out << YAML::Key << "StartSize" << YAML::Value << glm::vec2(emitter.StartSize.Min, emitter.StartSize.Max);
			out << YAML::Key << "StartRotation" << YAML::Value << glm::vec2(emitter.StartRotation.Min, emitter.StartRotation.Max);
			out << YAML::Key << "AngularVelocity" << YAML::Value << glm::vec2(emitter.AngularVelocity.Min, emitter.AngularVelocity.Max);
			out << YAML::Key << "EndSizeScale" << YAML::Value << emitter.EndSizeScale;
			out << YAML::Key << "StartColor" << YAML::Value << emitter.StartColor;
			out << YAML::Key << "EndColor" << YAML::Value << emitter.EndColor;
			out << YAML::Key << "Gravity" << YAML::Value << emitter.Gravity;
			out << YAML::Key << "Drag" << YAML::Value << emitter.Drag;
			out << YAML::Key << "Turbulence" << YAML::Value << emitter.Turbulence;
			if ((uint64_t)emitter.Texture != 0)
			{
				out << YAML::Key << "TextureHandle" << YAML::Value << (uint64_t)emitter.Texture;
			}
			out << YAML::Key << "SpriteIndex" << YAML::Value << emitter.SpriteIndex;
			out << YAML::Key << "Blend" << YAML::Value << (emitter.Blend == ParticleBlend::Additive ? "Additive" : "Alpha");
			out << YAML::Key << "Lighting" << YAML::Value << (emitter.Lighting == ParticleLighting::Unlit ? "Unlit" : "Lit");
			out << YAML::Key << "SortOrder" << YAML::Value << emitter.SortOrder;
			out << YAML::Key << "LightRadius" << YAML::Value << emitter.LightRadius;
			out << YAML::Key << "LightIntensity" << YAML::Value << emitter.LightIntensity;
			out << YAML::Key << "MaxLights" << YAML::Value << emitter.MaxLights;
			out << YAML::Key << "DestroyWhenFinished" << YAML::Value << emitter.DestroyWhenFinished;
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
			// Scenes from before facings have none: Flat, which draws as they always did.
			sprite.Facing = SpriteFacingFromString(spriteNode["Facing"].as<std::string>("Flat"));
			sprite.AlphaMode = SpriteAlphaModeFromString(spriteNode["AlphaMode"].as<std::string>("Default"));
			sprite.Overlay = spriteNode["Overlay"].as<bool>(false);
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
			light.Height = lightNode["Height"].as<float>(light.Height);
		}

		if (auto meshNode = entityNode["MeshComponent"])
		{
			auto& mesh = entity.AddComponent<MeshComponent>();
			mesh.Shape = MeshShapeFromString(meshNode["Shape"].as<std::string>("Box"));
			mesh.Size = meshNode["Size"].as<glm::vec3>(mesh.Size);
			mesh.Steps = meshNode["Steps"].as<uint32_t>(mesh.Steps);
			mesh.Segments = meshNode["Segments"].as<uint32_t>(mesh.Segments);
			mesh.CastShadows = meshNode["CastShadows"].as<bool>(mesh.CastShadows);
			if (auto materialsNode = meshNode["Materials"]; materialsNode && materialsNode.IsSequence())
			{
				mesh.Materials.clear();
				for (const YAML::Node& materialNode : materialsNode)
				{
					mesh.Materials.push_back(DeserializeMeshMaterial(materialNode));
				}
			}
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

		if (auto emitterNode = entityNode["ParticleEmitter2DComponent"])
		{
			// Missing keys keep the defaults; values that would break the simulation are pulled into range.
			auto& emitter = entity.AddComponent<ParticleEmitter2DComponent>();
			const uint32_t limit = ParticleEmitter2DComponent::MaxParticlesLimit;
			emitter.PlayOnStart = emitterNode["PlayOnStart"].as<bool>(emitter.PlayOnStart);
			emitter.Looping = emitterNode["Looping"].as<bool>(emitter.Looping);
			emitter.Duration = AtLeast(0.01f, emitterNode["Duration"].as<float>(emitter.Duration));
			emitter.Rate = AtLeast(0.0f, emitterNode["Rate"].as<float>(emitter.Rate));
			emitter.BurstCount = ReadCount(emitterNode["BurstCount"], emitter.BurstCount, 0, limit);
			emitter.MaxParticles = ReadCount(emitterNode["MaxParticles"], emitter.MaxParticles, 1, limit);
			emitter.Seed = emitterNode["Seed"].as<uint32_t>(emitter.Seed);
			emitter.Shape = EmitterShapeFromString(emitterNode["Shape"].as<std::string>("Point"));
			emitter.ShapeSize = emitterNode["ShapeSize"].as<glm::vec2>(emitter.ShapeSize);
			emitter.ShapeSize.x = AtLeast(0.0f, emitter.ShapeSize.x);
			emitter.ShapeSize.y = AtLeast(0.0f, emitter.ShapeSize.y);
			emitter.Lifetime = ReadRange(emitterNode["Lifetime"], emitter.Lifetime, 0.01f);
			emitter.Speed = ReadRange(emitterNode["Speed"], emitter.Speed);
			emitter.Direction = emitterNode["Direction"].as<float>(emitter.Direction);
			emitter.Spread = std::clamp(emitterNode["Spread"].as<float>(emitter.Spread), 0.0f, 180.0f);
			emitter.StartSize = ReadRange(emitterNode["StartSize"], emitter.StartSize, 0.0f);
			emitter.StartRotation = ReadRange(emitterNode["StartRotation"], emitter.StartRotation);
			emitter.AngularVelocity = ReadRange(emitterNode["AngularVelocity"], emitter.AngularVelocity);
			emitter.EndSizeScale = AtLeast(0.0f, emitterNode["EndSizeScale"].as<float>(emitter.EndSizeScale));
			emitter.StartColor = emitterNode["StartColor"].as<glm::vec4>(emitter.StartColor);
			emitter.EndColor = emitterNode["EndColor"].as<glm::vec4>(emitter.EndColor);
			emitter.Gravity = emitterNode["Gravity"].as<glm::vec2>(emitter.Gravity);
			emitter.Drag = AtLeast(0.0f, emitterNode["Drag"].as<float>(emitter.Drag));
			emitter.Turbulence = AtLeast(0.0f, emitterNode["Turbulence"].as<float>(emitter.Turbulence));
			emitter.SpriteIndex = emitterNode["SpriteIndex"].as<uint32_t>(emitter.SpriteIndex);
			emitter.Blend = emitterNode["Blend"].as<std::string>("Alpha") == "Additive" ? ParticleBlend::Additive : ParticleBlend::Alpha;
			emitter.Lighting = emitterNode["Lighting"].as<std::string>("Lit") == "Unlit" ? ParticleLighting::Unlit : ParticleLighting::Lit;
			emitter.SortOrder = emitterNode["SortOrder"].as<int32_t>(emitter.SortOrder);
			emitter.LightRadius = AtLeast(0.0f, emitterNode["LightRadius"].as<float>(emitter.LightRadius));
			emitter.LightIntensity = AtLeast(0.0f, emitterNode["LightIntensity"].as<float>(emitter.LightIntensity));
			emitter.MaxLights = ReadCount(emitterNode["MaxLights"], emitter.MaxLights, 0, limit);
			emitter.DestroyWhenFinished = emitterNode["DestroyWhenFinished"].as<bool>(emitter.DestroyWhenFinished);

			// Loaded now, like a sprite's texture, so the renderer (which never loads) finds it.
			if (auto textureNode = emitterNode["TextureHandle"])
			{
				emitter.Texture = AssetHandle(textureNode.as<uint64_t>(0));
				if ((uint64_t)emitter.Texture != 0)
				{
					AssetManager::GetAsset<Texture2D>(emitter.Texture);
				}
			}
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
