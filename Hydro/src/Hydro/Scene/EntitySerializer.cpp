#include "hypch.h"
#include "EntitySerializer.h"
#include "Scripting/ScriptableEntity.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Font.h"
#include "Hydro/Renderer/MaterialSerializer.h"
#include "Hydro/Renderer/Mesh.h"
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

	// A script property's value (HY_PROPERTY): scalars and strings as they are, vectors as [x, y, z], an asset as
	// its handle number.
	static void WriteScriptPropertyValue(YAML::Emitter& out, const ScriptPropertyValue& value)
	{
		std::visit(
			[&out](const auto& typed)
			{
				using Value = std::decay_t<decltype(typed)>;
				if constexpr (std::is_same_v<Value, AssetHandle>)
				{
					out << (uint64_t)typed;
				}
				else
				{
					out << typed;
				}
			},
			value);
	}

	// A saved value of `property`; its default when the node holds something else (a property whose type changed).
	static ScriptPropertyValue ReadScriptPropertyValue(const YAML::Node& node, const ScriptProperty& property)
	{
		const ScriptPropertyValue& fallback = property.DefaultValue;
		try
		{
			switch (property.Type)
			{
				case ScriptPropertyType::Bool: return node.as<bool>(std::get<bool>(fallback));
				case ScriptPropertyType::Int: return node.as<int>(std::get<int>(fallback));
				case ScriptPropertyType::Float: return node.as<float>(std::get<float>(fallback));
				case ScriptPropertyType::Vec2: return node.as<glm::vec2>(std::get<glm::vec2>(fallback));
				case ScriptPropertyType::Vec3: return node.as<glm::vec3>(std::get<glm::vec3>(fallback));
				case ScriptPropertyType::Vec4: return node.as<glm::vec4>(std::get<glm::vec4>(fallback));
				case ScriptPropertyType::String: return node.as<std::string>(std::get<std::string>(fallback));
				case ScriptPropertyType::Asset: return AssetHandle(node.as<uint64_t>((uint64_t)std::get<AssetHandle>(fallback)));
			}
		}
		catch (const YAML::Exception&)
		{
			// A vector saved with the wrong number of components, or a scalar where a list was expected.
		}
		return fallback;
	}

	// Saved values the physics engine would choke on are pulled into range. Written as max(minimum, value)
	// so that NaN (which compares false with everything) also becomes the minimum.
	static const char* UIScaleModeToString(UIScaleMode mode)
	{
		switch (mode)
		{
		case UIScaleMode::ScaleWithScreen: return "ScaleWithScreen";
		case UIScaleMode::ConstantPixelSize: return "ConstantPixelSize";
		default: return "IntegerScale";
		}
	}

	static UIScaleMode UIScaleModeFromString(const std::string& name)
	{
		if (name == "ScaleWithScreen")
		{
			return UIScaleMode::ScaleWithScreen;
		}
		if (name == "ConstantPixelSize")
		{
			return UIScaleMode::ConstantPixelSize;
		}
		return UIScaleMode::IntegerScale;
	}

	static const char* UITextAlignHToString(TextAlignH align)
	{
		switch (align)
		{
		case TextAlignH::Center: return "Center";
		case TextAlignH::Right: return "Right";
		default: return "Left";
		}
	}

	static TextAlignH UITextAlignHFromString(const std::string& name)
	{
		if (name == "Center")
		{
			return TextAlignH::Center;
		}
		if (name == "Right")
		{
			return TextAlignH::Right;
		}
		return TextAlignH::Left;
	}

	static const char* UITextAlignVToString(TextAlignV align)
	{
		switch (align)
		{
		case TextAlignV::Middle: return "Middle";
		case TextAlignV::Bottom: return "Bottom";
		default: return "Top";
		}
	}

	static TextAlignV UITextAlignVFromString(const std::string& name)
	{
		if (name == "Middle")
		{
			return TextAlignV::Middle;
		}
		if (name == "Bottom")
		{
			return TextAlignV::Bottom;
		}
		return TextAlignV::Top;
	}

	static const char* UINavigationToString(UINavigation navigation)
	{
		switch (navigation)
		{
		case UINavigation::Explicit: return "Explicit";
		case UINavigation::None: return "None";
		default: return "Auto";
		}
	}

	static UINavigation UINavigationFromString(const std::string& name)
	{
		if (name == "Explicit")
		{
			return UINavigation::Explicit;
		}
		if (name == "None")
		{
			return UINavigation::None;
		}
		return UINavigation::Auto;
	}

	static float AtLeast(float minimum, float value)
	{
		return std::max(minimum, value);
	}

	static const char* PhysicsLayerToString(PhysicsLayer layer)
	{
		switch (layer)
		{
		case PhysicsLayer::World: return "World";
		case PhysicsLayer::Player: return "Player";
		case PhysicsLayer::Npc: return "Npc";
		case PhysicsLayer::Trigger: return "Trigger";
		}

		return "World";
	}

	static PhysicsLayer PhysicsLayerFromString(const std::string& name, PhysicsLayer fallback)
	{
		if (name == "World")
		{
			return PhysicsLayer::World;
		}
		if (name == "Player")
		{
			return PhysicsLayer::Player;
		}
		if (name == "Npc")
		{
			return PhysicsLayer::Npc;
		}
		if (name == "Trigger")
		{
			return PhysicsLayer::Trigger;
		}

		return fallback;
	}

	// The keys every 3D collider has.
	static void SerializeCollider3D(YAML::Emitter& out, PhysicsLayer layer, bool isTrigger)
	{
		out << YAML::Key << "Layer" << YAML::Value << PhysicsLayerToString(layer);
		out << YAML::Key << "IsTrigger" << YAML::Value << isTrigger;
	}

	static void DeserializeCollider3D(const YAML::Node& node, PhysicsLayer& layer, bool& isTrigger)
	{
		layer = PhysicsLayerFromString(node["Layer"].as<std::string>(PhysicsLayerToString(layer)), layer);
		isTrigger = node["IsTrigger"].as<bool>(isTrigger);
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

	void EntitySerializer::Serialize(YAML::Emitter& out, Entity entity, bool includeParent)
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

		// Roots have none (and nothing is written).
		if (includeParent && entity.HasComponent<RelationshipComponent>() && (uint64_t)entity.GetComponent<RelationshipComponent>().Parent != 0)
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Parent" << YAML::Value << (uint64_t)entity.GetComponent<RelationshipComponent>().Parent;
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
			if (sprite.CastShadows == false)
			{
				out << YAML::Key << "CastShadows" << YAML::Value << false;
			}
			// Emission (plan P3), only when the sprite glows.
			if (sprite.EmissionIntensity > 0.0f)
			{
				out << YAML::Key << "EmissionColor" << YAML::Value << sprite.EmissionColor;
				out << YAML::Key << "EmissionIntensity" << YAML::Value << sprite.EmissionIntensity;
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
			out << YAML::Key << "Type" << YAML::Value << (light.Type == Light2DType::Global ? "Global" : (light.Type == Light2DType::Spot ? "Spot" : "Point"));
			out << YAML::Key << "Color" << YAML::Value << light.Color;
			out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
			out << YAML::Key << "Radius" << YAML::Value << light.Radius;
			out << YAML::Key << "Height" << YAML::Value << light.Height;
			if (light.Type == Light2DType::Spot)
			{
				out << YAML::Key << "InnerAngle" << YAML::Value << light.InnerAngle;
				out << YAML::Key << "OuterAngle" << YAML::Value << light.OuterAngle;
			}
			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			auto& light = entity.GetComponent<DirectionalLightComponent>();

			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Color" << YAML::Value << light.Color;
			out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
			out << YAML::Key << "IsMainLight" << YAML::Value << light.IsMainLight;
			out << YAML::Key << "CastShadows" << YAML::Value << light.CastShadows;
			out << YAML::Key << "ShadowStrength" << YAML::Value << light.ShadowStrength;
			out << YAML::Key << "ShadowDistance" << YAML::Value << light.ShadowDistance;
			out << YAML::Key << "ShadowBias" << YAML::Value << light.ShadowBias;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<MeshComponent>())
		{
			auto& mesh = entity.GetComponent<MeshComponent>();

			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap;
			if ((uint64_t)mesh.Model != 0)
			{
				out << YAML::Key << "ModelHandle" << YAML::Value << (uint64_t)mesh.Model;
			}
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
			// Each slot: its material asset's handle (when it has one) and its inline settings, the fallback.
			for (const MeshMaterialSlot& slot : mesh.Materials)
			{
				out << YAML::BeginMap;
				if ((uint64_t)slot.Material != 0)
				{
					out << YAML::Key << "MaterialHandle" << YAML::Value << (uint64_t)slot.Material;
				}
				out << YAML::Key << "Inline" << YAML::Value;
				MaterialSerializer::SerializeData(out, slot.Inline);
				out << YAML::EndMap;
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

		if (entity.HasComponent<BoxColliderComponent>())
		{
			auto& collider = entity.GetComponent<BoxColliderComponent>();

			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
			out << YAML::Key << "Size" << YAML::Value << collider.Size;
			SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
			out << YAML::EndMap;
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			auto& collider = entity.GetComponent<SphereColliderComponent>();

			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
			out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
			SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
			out << YAML::EndMap;
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			auto& collider = entity.GetComponent<CapsuleColliderComponent>();

			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
			out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
			out << YAML::Key << "Height" << YAML::Value << collider.Height;
			SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
			out << YAML::EndMap;
		}

		if (entity.HasComponent<CylinderColliderComponent>())
		{
			auto& collider = entity.GetComponent<CylinderColliderComponent>();

			out << YAML::Key << "CylinderColliderComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
			out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
			out << YAML::Key << "Height" << YAML::Value << collider.Height;
			SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
			out << YAML::EndMap;
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			auto& collider = entity.GetComponent<MeshColliderComponent>();

			out << YAML::Key << "MeshColliderComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Layer" << YAML::Value << PhysicsLayerToString(collider.Layer);
			out << YAML::EndMap;
		}

		if (entity.HasComponent<CharacterBodyComponent>())
		{
			auto& body = entity.GetComponent<CharacterBodyComponent>();

			out << YAML::Key << "CharacterBodyComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Radius" << YAML::Value << body.Radius;
			out << YAML::Key << "Height" << YAML::Value << body.Height;
			out << YAML::Key << "MaxSlopeDegrees" << YAML::Value << body.MaxSlopeDegrees;
			out << YAML::Key << "StepHeight" << YAML::Value << body.StepHeight;
			out << YAML::Key << "StaysOnLedges" << YAML::Value << body.StaysOnLedges;
			out << YAML::Key << "Layer" << YAML::Value << PhysicsLayerToString(body.Layer);
			out << YAML::EndMap;
		}

		// Runtime UI. UIResolvedRectComponent is derived (Scene::ResolveUILayout) and never saved.
		if (entity.HasComponent<UICanvasComponent>())
		{
			auto& canvas = entity.GetComponent<UICanvasComponent>();

			out << YAML::Key << "UICanvasComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "ReferenceResolution" << YAML::Value << canvas.ReferenceResolution;
			out << YAML::Key << "ScaleMode" << YAML::Value << UIScaleModeToString(canvas.ScaleMode);
			out << YAML::Key << "SortOrder" << YAML::Value << canvas.SortOrder;
			out << YAML::Key << "Visible" << YAML::Value << canvas.Visible;
			out << YAML::Key << "BlocksGameplayInput" << YAML::Value << canvas.BlocksGameplayInput;
			// A World canvas's box, only for World canvases (older files and screen canvases stay as they were).
			if (canvas.Space == UICanvasSpace::World)
			{
				out << YAML::Key << "Space" << YAML::Value << "World";
				out << YAML::Key << "WorldSize" << YAML::Value << canvas.WorldSize;
				out << YAML::Key << "WorldPivot" << YAML::Value << canvas.WorldPivot;
			}
			if (canvas.DrawDebug)
			{
				out << YAML::Key << "DrawDebug" << YAML::Value << true;
			}
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIRectComponent>())
		{
			auto& rect = entity.GetComponent<UIRectComponent>();

			out << YAML::Key << "UIRectComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "AnchorMin" << YAML::Value << rect.AnchorMin;
			out << YAML::Key << "AnchorMax" << YAML::Value << rect.AnchorMax;
			out << YAML::Key << "Pivot" << YAML::Value << rect.Pivot;
			out << YAML::Key << "AnchoredPosition" << YAML::Value << rect.AnchoredPosition;
			out << YAML::Key << "SizeDelta" << YAML::Value << rect.SizeDelta;
			out << YAML::Key << "Visible" << YAML::Value << rect.Visible;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIImageComponent>())
		{
			auto& image = entity.GetComponent<UIImageComponent>();

			out << YAML::Key << "UIImageComponent";
			out << YAML::BeginMap;
			if ((uint64_t)image.Texture != 0)
			{
				out << YAML::Key << "TextureHandle" << YAML::Value << (uint64_t)image.Texture;
				// By name, like a sprite's, so slicing the sheet again keeps it; the index is the fallback.
				const Ref<Texture2D> texture = AssetManager::GetLoadedAsset<Texture2D>(image.Texture);
				if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
				{
					out << YAML::Key << "Sprite" << YAML::Value << texture->GetSprite(image.SpriteIndex).Name;
				}
			}
			out << YAML::Key << "SpriteIndex" << YAML::Value << image.SpriteIndex;
			out << YAML::Key << "Color" << YAML::Value << image.Color;
			out << YAML::Key << "NineSlice" << YAML::Value << image.NineSlice;
			out << YAML::Key << "FlipX" << YAML::Value << image.FlipX;
			out << YAML::Key << "FlipY" << YAML::Value << image.FlipY;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UITextComponent>())
		{
			auto& text = entity.GetComponent<UITextComponent>();

			out << YAML::Key << "UITextComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Text" << YAML::Value << text.Text;
			if ((uint64_t)text.Font != 0)
			{
				out << YAML::Key << "FontHandle" << YAML::Value << (uint64_t)text.Font;
			}
			out << YAML::Key << "Color" << YAML::Value << text.Color;
			out << YAML::Key << "Scale" << YAML::Value << text.Scale;
			out << YAML::Key << "AlignH" << YAML::Value << UITextAlignHToString(text.AlignH);
			out << YAML::Key << "AlignV" << YAML::Value << UITextAlignVToString(text.AlignV);
			out << YAML::Key << "Wrap" << YAML::Value << text.Wrap;
			out << YAML::Key << "LineSpacing" << YAML::Value << text.LineSpacing;
			out << YAML::Key << "VisibleCharacters" << YAML::Value << text.VisibleCharacters;
			if (text.ShadowColor.a > 0.0f)
			{
				out << YAML::Key << "ShadowColor" << YAML::Value << text.ShadowColor;
				out << YAML::Key << "ShadowOffset" << YAML::Value << text.ShadowOffset;
			}
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UILayoutGroupComponent>())
		{
			auto& group = entity.GetComponent<UILayoutGroupComponent>();

			out << YAML::Key << "UILayoutGroupComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Direction" << YAML::Value << (group.Direction == UILayoutDirection::Horizontal ? "Horizontal" : "Vertical");
			out << YAML::Key << "Spacing" << YAML::Value << group.Spacing;
			out << YAML::Key << "Padding" << YAML::Value << group.Padding;
			out << YAML::Key << "ChildAlignment" << YAML::Value
				<< (group.ChildAlignment == UILayoutAlignment::Center ? "Center" : (group.ChildAlignment == UILayoutAlignment::End ? "End" : "Start"));
			out << YAML::Key << "StretchCrossAxis" << YAML::Value << group.StretchCrossAxis;
			out << YAML::Key << "FitContent" << YAML::Value << group.FitContent;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIScrollViewComponent>())
		{
			auto& scroll = entity.GetComponent<UIScrollViewComponent>();

			out << YAML::Key << "UIScrollViewComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "ScrollPosition" << YAML::Value << scroll.ScrollPosition;
			out << YAML::Key << "FollowFocus" << YAML::Value << scroll.FollowFocus;
			if ((uint64_t)scroll.Scrollbar != 0)
			{
				out << YAML::Key << "Scrollbar" << YAML::Value << (uint64_t)scroll.Scrollbar;
			}
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIFocusVisibilityComponent>())
		{
			out << YAML::Key << "UIFocusVisibilityComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Show" << YAML::Value
				<< (entity.GetComponent<UIFocusVisibilityComponent>().Show == UIFocusVisibility::Unfocused ? "Unfocused" : "Focused");
			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIButtonComponent>())
		{
			auto& button = entity.GetComponent<UIButtonComponent>();

			out << YAML::Key << "UIButtonComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Interactable" << YAML::Value << button.Interactable;
			out << YAML::Key << "Event" << YAML::Value << button.Event;
			out << YAML::Key << "NormalTint" << YAML::Value << button.NormalTint;
			out << YAML::Key << "FocusedTint" << YAML::Value << button.FocusedTint;
			out << YAML::Key << "PressedTint" << YAML::Value << button.PressedTint;
			out << YAML::Key << "DisabledTint" << YAML::Value << button.DisabledTint;
			out << YAML::Key << "Navigation" << YAML::Value << UINavigationToString(button.Navigation);
			// Explicit navigation's neighbours, only when set.
			const std::pair<const char*, UUID> neighbours[] = { { "Up", button.Up }, { "Down", button.Down }, { "Left", button.Left }, { "Right", button.Right } };
			for (const auto& neighbour : neighbours)
			{
				if ((uint64_t)neighbour.second != 0)
				{
					out << YAML::Key << neighbour.first << YAML::Value << (uint64_t)neighbour.second;
				}
			}
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
			out << YAML::Key << "Space" << YAML::Value << (emitter.Space == ParticleSpace::View ? "View" : "Ground");
			out << YAML::Key << "SortOrder" << YAML::Value << emitter.SortOrder;
			if (emitter.EmissionIntensity > 0.0f)
			{
				out << YAML::Key << "EmissionIntensity" << YAML::Value << emitter.EmissionIntensity;
			}
			out << YAML::Key << "LightRadius" << YAML::Value << emitter.LightRadius;
			out << YAML::Key << "LightIntensity" << YAML::Value << emitter.LightIntensity;
			out << YAML::Key << "MaxLights" << YAML::Value << emitter.MaxLights;
			out << YAML::Key << "DestroyWhenFinished" << YAML::Value << emitter.DestroyWhenFinished;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<NativeScriptComponent>())
		{
			auto& scripts = entity.GetComponent<NativeScriptComponent>();

			out << YAML::Key << "NativeScriptComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Scripts" << YAML::Value << YAML::BeginSeq;

			for (const auto& script : scripts.Scripts)
			{
				if (script.Name.empty())
				{
					HY_CORE_WARN(
						"Entity '{}': a script bound in code without a ScriptRegistry name isn't saved",
						entity.GetName());
					continue;
				}

				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << script.Name;

				if (script.Properties.empty() == false)
				{
					out << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;

					// Sorted by name, so a scene saves the same way every time (the map is unordered).
					std::vector<const std::pair<const std::string, ScriptPropertyValue>*> sorted;
					sorted.reserve(script.Properties.size());
					for (const auto& entry : script.Properties)
					{
						sorted.push_back(&entry);
					}
					std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) { return a->first < b->first; });

					for (const auto* entry : sorted)
					{
						out << YAML::Key << entry->first << YAML::Value;
						WriteScriptPropertyValue(out, entry->second);
					}

					out << YAML::EndMap;
				}

				out << YAML::EndMap;
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

		// Only when loading a scene: the parent is another entity of the same file, and the scene's loader puts
		// the list in order afterwards (Scene::RepairHierarchy). Instantiating a prefab links its entities itself
		// (PrefabSerializer::Instantiate), since they get new UUIDs.
		if (keepUUID)
		{
			if (auto relationshipNode = entityNode["RelationshipComponent"])
			{
				const uint64_t parent = relationshipNode["Parent"].as<uint64_t>(0);
				if (parent != 0)
				{
					entity.AddComponent<RelationshipComponent>(UUID(parent));
				}
			}
		}

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
			sprite.CastShadows = spriteNode["CastShadows"].as<bool>(true);
			sprite.EmissionColor = glm::max(spriteNode["EmissionColor"].as<glm::vec3>(sprite.EmissionColor), glm::vec3(0.0f));
			sprite.EmissionIntensity = AtLeast(0.0f, spriteNode["EmissionIntensity"].as<float>(sprite.EmissionIntensity));
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
			const std::string type = lightNode["Type"].as<std::string>("Point");
			light.Type = type == "Global" ? Light2DType::Global : (type == "Spot" ? Light2DType::Spot : Light2DType::Point);
			light.Color = lightNode["Color"].as<glm::vec3>(light.Color);
			light.Intensity = lightNode["Intensity"].as<float>(light.Intensity);
			light.Radius = lightNode["Radius"].as<float>(light.Radius);
			light.Height = lightNode["Height"].as<float>(light.Height);
			light.InnerAngle = lightNode["InnerAngle"].as<float>(light.InnerAngle);
			light.OuterAngle = std::max(lightNode["OuterAngle"].as<float>(light.OuterAngle), light.InnerAngle);
		}

		if (auto sunNode = entityNode["DirectionalLightComponent"])
		{
			auto& light = entity.AddComponent<DirectionalLightComponent>();
			light.Color = sunNode["Color"].as<glm::vec3>(light.Color);
			light.Intensity = sunNode["Intensity"].as<float>(light.Intensity);
			light.IsMainLight = sunNode["IsMainLight"].as<bool>(light.IsMainLight);
			light.CastShadows = sunNode["CastShadows"].as<bool>(light.CastShadows);
			light.ShadowStrength = sunNode["ShadowStrength"].as<float>(light.ShadowStrength);
			light.ShadowDistance = sunNode["ShadowDistance"].as<float>(light.ShadowDistance);
			light.ShadowBias = sunNode["ShadowBias"].as<float>(light.ShadowBias);
		}

		if (auto meshNode = entityNode["MeshComponent"])
		{
			auto& mesh = entity.AddComponent<MeshComponent>();
			mesh.Model = AssetHandle(meshNode["ModelHandle"].as<uint64_t>(0));
			if ((uint64_t)mesh.Model != 0)
			{
				// Imported now (its materials and textures too), so the renderer, which never loads, finds it.
				AssetManager::GetAsset<Mesh>(mesh.Model);
			}
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
					MeshMaterialSlot slot;
					slot.Material = AssetHandle(materialNode["MaterialHandle"].as<uint64_t>(0));
					// Scenes saved before material assets (M4) have the settings in the slot itself.
					const YAML::Node inlineNode = materialNode["Inline"];
					slot.Inline = MaterialSerializer::DeserializeData(inlineNode.IsDefined() ? inlineNode : materialNode);
					// Loaded now (with its textures), so the renderer, which never loads, finds it.
					if ((uint64_t)slot.Material != 0)
					{
						AssetManager::GetAsset<MaterialAsset>(slot.Material);
					}
					mesh.Materials.push_back(slot);
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

		if (auto boxNode = entityNode["BoxColliderComponent"])
		{
			auto& collider = entity.AddComponent<BoxColliderComponent>();
			collider.Offset = boxNode["Offset"].as<glm::vec3>(collider.Offset);
			collider.Size = boxNode["Size"].as<glm::vec3>(collider.Size);
			collider.Size.x = AtLeast(0.001f, collider.Size.x);
			collider.Size.y = AtLeast(0.001f, collider.Size.y);
			collider.Size.z = AtLeast(0.001f, collider.Size.z);
			DeserializeCollider3D(boxNode, collider.Layer, collider.IsTrigger);
		}

		if (auto sphereNode = entityNode["SphereColliderComponent"])
		{
			auto& collider = entity.AddComponent<SphereColliderComponent>();
			collider.Offset = sphereNode["Offset"].as<glm::vec3>(collider.Offset);
			collider.Radius = AtLeast(0.001f, sphereNode["Radius"].as<float>(collider.Radius));
			DeserializeCollider3D(sphereNode, collider.Layer, collider.IsTrigger);
		}

		if (auto capsuleNode = entityNode["CapsuleColliderComponent"])
		{
			auto& collider = entity.AddComponent<CapsuleColliderComponent>();
			collider.Offset = capsuleNode["Offset"].as<glm::vec3>(collider.Offset);
			collider.Radius = AtLeast(0.001f, capsuleNode["Radius"].as<float>(collider.Radius));
			collider.Height = AtLeast(0.001f, capsuleNode["Height"].as<float>(collider.Height));
			DeserializeCollider3D(capsuleNode, collider.Layer, collider.IsTrigger);
		}

		if (auto cylinderNode = entityNode["CylinderColliderComponent"])
		{
			auto& collider = entity.AddComponent<CylinderColliderComponent>();
			collider.Offset = cylinderNode["Offset"].as<glm::vec3>(collider.Offset);
			collider.Radius = AtLeast(0.001f, cylinderNode["Radius"].as<float>(collider.Radius));
			collider.Height = AtLeast(0.001f, cylinderNode["Height"].as<float>(collider.Height));
			DeserializeCollider3D(cylinderNode, collider.Layer, collider.IsTrigger);
		}

		if (auto meshColliderNode = entityNode["MeshColliderComponent"])
		{
			auto& collider = entity.AddComponent<MeshColliderComponent>();
			collider.Layer = PhysicsLayerFromString(meshColliderNode["Layer"].as<std::string>("World"), collider.Layer);
		}

		if (auto characterNode = entityNode["CharacterBodyComponent"])
		{
			auto& body = entity.AddComponent<CharacterBodyComponent>();
			body.Radius = AtLeast(0.05f, characterNode["Radius"].as<float>(body.Radius));
			body.Height = AtLeast(0.2f, characterNode["Height"].as<float>(body.Height));
			body.MaxSlopeDegrees = std::clamp(characterNode["MaxSlopeDegrees"].as<float>(body.MaxSlopeDegrees), 0.0f, 89.0f);
			body.StepHeight = AtLeast(0.0f, characterNode["StepHeight"].as<float>(body.StepHeight));
			body.StaysOnLedges = characterNode["StaysOnLedges"].as<bool>(body.StaysOnLedges);
			body.Layer = PhysicsLayerFromString(characterNode["Layer"].as<std::string>("Player"), body.Layer);
		}

		// Runtime UI: missing keys keep the defaults.
		if (auto canvasNode = entityNode["UICanvasComponent"])
		{
			auto& canvas = entity.AddComponent<UICanvasComponent>();
			canvas.ReferenceResolution = glm::max(canvasNode["ReferenceResolution"].as<glm::vec2>(canvas.ReferenceResolution), glm::vec2(1.0f));
			canvas.ScaleMode = UIScaleModeFromString(canvasNode["ScaleMode"].as<std::string>("IntegerScale"));
			canvas.SortOrder = canvasNode["SortOrder"].as<int32_t>(canvas.SortOrder);
			canvas.Visible = canvasNode["Visible"].as<bool>(canvas.Visible);
			canvas.BlocksGameplayInput = canvasNode["BlocksGameplayInput"].as<bool>(canvas.BlocksGameplayInput);
			canvas.DrawDebug = canvasNode["DrawDebug"].as<bool>(false);
			canvas.Space = canvasNode["Space"].as<std::string>("Screen") == "World" ? UICanvasSpace::World : UICanvasSpace::Screen;
			canvas.WorldSize = glm::max(canvasNode["WorldSize"].as<glm::vec2>(canvas.WorldSize), glm::vec2(1.0f));
			canvas.WorldPivot = canvasNode["WorldPivot"].as<glm::vec2>(canvas.WorldPivot);
		}

		if (auto rectNode = entityNode["UIRectComponent"])
		{
			auto& rect = entity.AddComponent<UIRectComponent>();
			rect.AnchorMin = rectNode["AnchorMin"].as<glm::vec2>(rect.AnchorMin);
			rect.AnchorMax = rectNode["AnchorMax"].as<glm::vec2>(rect.AnchorMax);
			rect.Pivot = rectNode["Pivot"].as<glm::vec2>(rect.Pivot);
			rect.AnchoredPosition = rectNode["AnchoredPosition"].as<glm::vec2>(rect.AnchoredPosition);
			rect.SizeDelta = rectNode["SizeDelta"].as<glm::vec2>(rect.SizeDelta);
			rect.Visible = rectNode["Visible"].as<bool>(rect.Visible);
		}

		if (auto imageNode = entityNode["UIImageComponent"])
		{
			auto& image = entity.AddComponent<UIImageComponent>();
			// Loaded now, like a sprite's texture, so the renderer (which never loads) finds it.
			Ref<Texture2D> texture;
			if (auto textureNode = imageNode["TextureHandle"])
			{
				image.Texture = AssetHandle(textureNode.as<uint64_t>(0));
				texture = (uint64_t)image.Texture != 0 ? AssetManager::GetAsset<Texture2D>(image.Texture) : nullptr;
			}
			image.SpriteIndex = imageNode["SpriteIndex"].as<uint32_t>(image.SpriteIndex);
			if (auto spriteNameNode = imageNode["Sprite"]; spriteNameNode && texture != nullptr)
			{
				const int32_t found = texture->FindSprite(spriteNameNode.as<std::string>(""));
				if (found >= 0)
				{
					image.SpriteIndex = (uint32_t)found;
				}
			}
			image.Color = imageNode["Color"].as<glm::vec4>(image.Color);
			image.NineSlice = glm::max(imageNode["NineSlice"].as<glm::vec4>(image.NineSlice), glm::vec4(0.0f));
			image.FlipX = imageNode["FlipX"].as<bool>(image.FlipX);
			image.FlipY = imageNode["FlipY"].as<bool>(image.FlipY);
		}

		if (auto textNode = entityNode["UITextComponent"])
		{
			auto& text = entity.AddComponent<UITextComponent>();
			text.Text = textNode["Text"].as<std::string>(text.Text);
			// Loaded now, like a sprite's texture, so the renderer (which never loads) finds it.
			text.Font = AssetHandle(textNode["FontHandle"].as<uint64_t>(0));
			if ((uint64_t)text.Font != 0)
			{
				AssetManager::GetAsset<Font>(text.Font);
			}
			text.Color = textNode["Color"].as<glm::vec4>(text.Color);
			text.Scale = std::max(textNode["Scale"].as<uint32_t>(text.Scale), 1u);
			text.AlignH = UITextAlignHFromString(textNode["AlignH"].as<std::string>("Left"));
			text.AlignV = UITextAlignVFromString(textNode["AlignV"].as<std::string>("Top"));
			text.Wrap = textNode["Wrap"].as<bool>(text.Wrap);
			text.LineSpacing = AtLeast(0.0f, textNode["LineSpacing"].as<float>(text.LineSpacing));
			text.VisibleCharacters = std::max(textNode["VisibleCharacters"].as<int32_t>(text.VisibleCharacters), -1);
			text.ShadowColor = textNode["ShadowColor"].as<glm::vec4>(text.ShadowColor);
			text.ShadowOffset = textNode["ShadowOffset"].as<glm::vec2>(text.ShadowOffset);
		}

		if (auto groupNode = entityNode["UILayoutGroupComponent"])
		{
			auto& group = entity.AddComponent<UILayoutGroupComponent>();
			group.Direction = groupNode["Direction"].as<std::string>("Vertical") == "Horizontal" ? UILayoutDirection::Horizontal : UILayoutDirection::Vertical;
			group.Spacing = AtLeast(0.0f, groupNode["Spacing"].as<float>(group.Spacing));
			group.Padding = glm::max(groupNode["Padding"].as<glm::vec4>(group.Padding), glm::vec4(0.0f));
			const std::string alignment = groupNode["ChildAlignment"].as<std::string>("Start");
			group.ChildAlignment = alignment == "Center" ? UILayoutAlignment::Center : (alignment == "End" ? UILayoutAlignment::End : UILayoutAlignment::Start);
			group.StretchCrossAxis = groupNode["StretchCrossAxis"].as<bool>(group.StretchCrossAxis);
			group.FitContent = groupNode["FitContent"].as<bool>(group.FitContent);
		}

		if (auto scrollNode = entityNode["UIScrollViewComponent"])
		{
			auto& scroll = entity.AddComponent<UIScrollViewComponent>();
			scroll.ScrollPosition = AtLeast(0.0f, scrollNode["ScrollPosition"].as<float>(scroll.ScrollPosition));
			scroll.FollowFocus = scrollNode["FollowFocus"].as<bool>(scroll.FollowFocus);
			scroll.Scrollbar = UUID(scrollNode["Scrollbar"].as<uint64_t>(0));
		}

		if (auto visibilityNode = entityNode["UIFocusVisibilityComponent"])
		{
			entity.AddComponent<UIFocusVisibilityComponent>().Show =
				visibilityNode["Show"].as<std::string>("Focused") == "Unfocused" ? UIFocusVisibility::Unfocused : UIFocusVisibility::Focused;
		}

		if (auto buttonNode = entityNode["UIButtonComponent"])
		{
			auto& button = entity.AddComponent<UIButtonComponent>();
			button.Interactable = buttonNode["Interactable"].as<bool>(button.Interactable);
			button.Event = buttonNode["Event"].as<std::string>(button.Event);
			button.NormalTint = buttonNode["NormalTint"].as<glm::vec4>(button.NormalTint);
			button.FocusedTint = buttonNode["FocusedTint"].as<glm::vec4>(button.FocusedTint);
			button.PressedTint = buttonNode["PressedTint"].as<glm::vec4>(button.PressedTint);
			button.DisabledTint = buttonNode["DisabledTint"].as<glm::vec4>(button.DisabledTint);
			button.Navigation = UINavigationFromString(buttonNode["Navigation"].as<std::string>("Auto"));
			button.Up = UUID(buttonNode["Up"].as<uint64_t>(0));
			button.Down = UUID(buttonNode["Down"].as<uint64_t>(0));
			button.Left = UUID(buttonNode["Left"].as<uint64_t>(0));
			button.Right = UUID(buttonNode["Right"].as<uint64_t>(0));
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
			emitter.Space = emitterNode["Space"].as<std::string>("Ground") == "View" ? ParticleSpace::View : ParticleSpace::Ground;
			emitter.SortOrder = emitterNode["SortOrder"].as<int32_t>(emitter.SortOrder);
			emitter.EmissionIntensity = AtLeast(0.0f, emitterNode["EmissionIntensity"].as<float>(emitter.EmissionIntensity));
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

			if (auto scriptsNode = scriptNode["Scripts"]; scriptsNode && scriptsNode.IsSequence())
			{
				for (const YAML::Node& savedScript : scriptsNode)
				{
					std::string scriptName;

					// Backwards compatibility with the original string-only format.
					if (savedScript.IsScalar())
					{
						scriptName = savedScript.as<std::string>("");
					}
					else if (savedScript.IsMap())
					{
						scriptName = savedScript["Name"].as<std::string>("");
					}

					if (scriptName.empty())
						continue;

					// Bind resolves Instantiate and fills the current reflected defaults.
					scripts.Bind(scriptName);

					NativeScriptComponent::Script& binding = scripts.Scripts.back();

					// Saved values override the defaults. Missing/new fields keep their
					// current C++ defaults from the registry.
					if (savedScript.IsMap())
					{
						const YAML::Node propertiesNode = savedScript["Properties"];

						if (propertiesNode && propertiesNode.IsMap())
						{
							ScriptTypeInfo* typeInfo = ScriptRegistry::FindType(scriptName);

							if (typeInfo != nullptr)
							{
								for (const ScriptProperty& property : typeInfo->Properties)
								{
									if (const YAML::Node valueNode = propertiesNode[property.Name])
									{
										binding.Properties[property.Name] = ReadScriptPropertyValue(valueNode, property);
									}
								}
							}
						}
					}

					if (binding.Instantiate == nullptr)
					{
						HY_CORE_WARN(
							"Entity '{}': script '{}' isn't registered; it is kept but won't run",
							entity.GetName(),
							scriptName);
					}
				}
			}
		}


		return entity;
	}
}
