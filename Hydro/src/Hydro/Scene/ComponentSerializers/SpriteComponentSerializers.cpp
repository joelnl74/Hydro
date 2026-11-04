#include "hypch.h"
#include "ComponentSerializers.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <algorithm>
#include <cfloat>

namespace Hydro
{
	// The saved handle (TextureHandle, AnimationsHandle) when the registry knows it, else the file an older scene names
	// (Texture, Animations; registered now). An unknown handle with no path is kept, so a file that comes back is found.
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

	static const char* SpriteAlphaModeToString(SpriteAlphaMode mode)
	{
		switch (mode)
		{
		case SpriteAlphaMode::Default: return "Default";
		case SpriteAlphaMode::Cutout: return "Cutout";
		case SpriteAlphaMode::Translucent: return "Translucent";
		case SpriteAlphaMode::Additive: return "Additive";
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
		if (name == "Additive")
		{
			return SpriteAlphaMode::Additive;
		}

		return SpriteAlphaMode::Default;
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
		const float a = ComponentSerializers::AtLeast(minimum, values.x);
		const float b = ComponentSerializers::AtLeast(minimum, values.y);
		return { std::min(a, b), std::max(a, b) };
	}

	static uint32_t ReadCount(const YAML::Node& node, uint32_t fallback, uint32_t minimum, uint32_t maximum)
	{
		return std::clamp(node.as<uint32_t>(fallback), minimum, maximum);
	}

	void ComponentSerializers::SaveSpriteRenderer(YAML::Emitter& out, const SpriteRendererComponent& sprite)
	{
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
		// Only when set, to keep scenes that don't use them small.
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
		// Emission, only when the sprite glows.
		if (sprite.EmissionIntensity > 0.0f)
		{
			out << YAML::Key << "EmissionColor" << YAML::Value << sprite.EmissionColor;
			out << YAML::Key << "EmissionIntensity" << YAML::Value << sprite.EmissionIntensity;
		}
		if (sprite.Opacity < 1.0f)
		{
			out << YAML::Key << "Opacity" << YAML::Value << sprite.Opacity;
		}
	}

	void ComponentSerializers::LoadSpriteRenderer(const YAML::Node& spriteNode, SpriteRendererComponent& sprite)
	{
		sprite.Color = spriteNode["Color"].as<glm::vec4>(sprite.Color);
		sprite.TilingFactor = spriteNode["TilingFactor"].as<float>(sprite.TilingFactor);
		sprite.SortOrder = spriteNode["SortOrder"].as<int32_t>(sprite.SortOrder);

		// Loaded now (failures are logged by the manager) so the renderer, which never loads, finds it.
		sprite.Texture = ResolveSavedAsset(spriteNode["TextureHandle"], spriteNode["Texture"]);
		const Ref<Texture2D> texture = (uint64_t)sprite.Texture != 0 ? AssetManager::GetAsset<Texture2D>(sprite.Texture) : nullptr;

		sprite.SpriteIndex = spriteNode["SpriteIndex"].as<uint32_t>(sprite.SpriteIndex);
		sprite.FlipX = spriteNode["FlipX"].as<bool>(sprite.FlipX);
		sprite.FlipY = spriteNode["FlipY"].as<bool>(sprite.FlipY);
		// Missing facing: Flat.
		sprite.Facing = SpriteFacingFromString(spriteNode["Facing"].as<std::string>("Flat"));
		sprite.AlphaMode = SpriteAlphaModeFromString(spriteNode["AlphaMode"].as<std::string>("Default"));
		sprite.Overlay = spriteNode["Overlay"].as<bool>(false);
		sprite.CastShadows = spriteNode["CastShadows"].as<bool>(true);
		sprite.EmissionColor = glm::max(spriteNode["EmissionColor"].as<glm::vec3>(sprite.EmissionColor), glm::vec3(0.0f));
		sprite.EmissionIntensity = AtLeast(0.0f, spriteNode["EmissionIntensity"].as<float>(sprite.EmissionIntensity));
		sprite.Opacity = std::clamp(spriteNode["Opacity"].as<float>(1.0f), 0.0f, 1.0f);
		if (auto spriteNameNode = spriteNode["Sprite"]; spriteNameNode && texture != nullptr)
		{
			const int32_t found = texture->FindSprite(spriteNameNode.as<std::string>(""));
			if (found >= 0)
			{
				sprite.SpriteIndex = (uint32_t)found;
			}
		}
	}

	void ComponentSerializers::SaveSpriteAnimator(YAML::Emitter& out, const SpriteAnimatorComponent& animator)
	{
		// Only the settings; the play state starts fresh.
		if ((uint64_t)animator.Animations != 0)
		{
			out << YAML::Key << "AnimationsHandle" << YAML::Value << (uint64_t)animator.Animations;
		}
		out << YAML::Key << "StartClip" << YAML::Value << animator.StartClip;
		out << YAML::Key << "PlayOnStart" << YAML::Value << animator.PlayOnStart;
		out << YAML::Key << "Speed" << YAML::Value << animator.Speed;
	}

	void ComponentSerializers::LoadSpriteAnimator(const YAML::Node& animatorNode, SpriteAnimatorComponent& animator)
	{
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

	void ComponentSerializers::SaveShadow2D(YAML::Emitter& out, const Shadow2DComponent& shadow)
	{
		out << YAML::Key << "Offset" << YAML::Value << shadow.Offset;
		out << YAML::Key << "Size" << YAML::Value << shadow.Size;
		out << YAML::Key << "Strength" << YAML::Value << shadow.Strength;
	}

	void ComponentSerializers::LoadShadow2D(const YAML::Node& shadowNode, Shadow2DComponent& shadow)
	{
		shadow.Offset = shadowNode["Offset"].as<glm::vec2>(shadow.Offset);
		shadow.Size = shadowNode["Size"].as<glm::vec2>(shadow.Size);
		shadow.Strength = shadowNode["Strength"].as<float>(shadow.Strength);
	}

	void ComponentSerializers::SaveParticleEmitter2D(YAML::Emitter& out, const ParticleEmitter2DComponent& emitter)
	{
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
	}

	void ComponentSerializers::LoadParticleEmitter2D(const YAML::Node& emitterNode, ParticleEmitter2DComponent& emitter)
	{
		// Missing keys keep the defaults; values that would break the simulation are pulled into range.
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
}
