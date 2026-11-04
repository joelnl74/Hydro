#include "hypch.h"

#include "SceneSpriteRenderer.h"
#include "Renderer2D.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SpriteBounds.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	SceneCameraBasis SceneCameraBasis::Make(const glm::mat3& rotation, UprightSpriteMode upright)
	{
		SceneCameraBasis basis;
		basis.Rotation = rotation;

		// The camera's up and forward in the world (it looks down its local -Z).
		const glm::vec3 up = glm::normalize(rotation[1]);
		const glm::vec3 forward = -glm::normalize(rotation[2]);
		basis.Forward = forward;

		// "Into the screen" along the ground. Up's ground part still exists when the camera looks straight down
		// (forward has none then); a camera looking along the ground has no up on the ground.
		glm::vec2 ground{ up.x, up.y };
		const float upOnGround = glm::length(ground);
		if (upOnGround < 1.0e-4f)
		{
			ground = { forward.x, forward.y };
		}
		const float groundLength = glm::length(ground);
		basis.GroundForward = groundLength < 1.0e-4f ? glm::vec2(0.0f, 1.0f) : ground / groundLength;

		// An Upright quad turns about Z to face the camera (yaw), then leans up about its bottom edge (0 flat, 90 standing).
		// LeanToCamera leans to the camera's up; Vertical stands straight once the camera tilts 30 degrees.
		const float yaw = std::atan2(-basis.GroundForward.x, basis.GroundForward.y);
		const float cameraLean = std::atan2(up.z, upOnGround);
		const float standsUpAt = glm::radians(30.0f);
		const float lean = upright == UprightSpriteMode::Vertical
			? glm::half_pi<float>() * std::min(1.0f, cameraLean / standsUpAt) : cameraLean;
		basis.Upright = glm::mat3(glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 0.0f, 1.0f)))
			* glm::mat3(glm::rotate(glm::mat4(1.0f), lean, glm::vec3(1.0f, 0.0f, 0.0f)));
		return basis;
	}

	void SceneSpriteRenderer::BeginFrame(Scene& scene, const RenderFeatureSettings& features, const SceneCameraBasis& camera)
	{
		m_Scene = &scene;
		m_Features = features;
		m_Camera = camera;
		PrepareSunShadowCasting();
	}

	void SceneSpriteRenderer::SubmitSprites()
	{
		auto view = m_Scene->m_Registry.view<WorldTransformComponent, SpriteRendererComponent>();

		const RenderFeatureSettings features = m_Features;
		const bool sharedDepth = Renderer2D::HasSharedDepth(features);
		const bool viewDepthSorting = features.SpriteSorting == SpriteSortMode::ViewDepth;
		const bool cutoutByDefault = features.DefaultSpriteAlpha == SpriteAlphaDefault::Cutout;

		// SortOrder sorting (the 2D preset): SortOrder; then flat before standing; flat by Z, standing from far to near
		// along the camera's ground direction; then storage order.
		const auto drawsBefore = [](const SpriteDrawItem& a, const SpriteDrawItem& b)
		{
			if (a.SortOrder != b.SortOrder)
			{
				return a.SortOrder < b.SortOrder;
			}
			if (a.Standing != b.Standing)
			{
				return a.Standing == false;
			}
			if (a.Standing)
			{
				if (a.Depth != b.Depth)
				{
					return a.Depth > b.Depth;
				}
			}
			else if (a.Z != b.Z)
			{
				return a.Z < b.Z;
			}
			return a.Index < b.Index;
		};

		// View-depth sorting (3D): flat decals first, then everything standing back to front along the view direction.
		// SortOrder only breaks ties, so a high number can't pull an effect over a nearer sprite.
		const auto drawsBeforeByViewDepth = [](const SpriteDrawItem& a, const SpriteDrawItem& b)
		{
			if (a.Standing != b.Standing)
			{
				return a.Standing == false;
			}
			if (a.Standing)
			{
				if (a.ViewDepth != b.ViewDepth)
				{
					return a.ViewDepth > b.ViewDepth;
				}
			}
			else
			{
				if (a.SortOrder != b.SortOrder)
				{
					return a.SortOrder < b.SortOrder;
				}
				if (a.Z != b.Z)
				{
					return a.Z < b.Z;
				}
			}
			if (a.SortOrder != b.SortOrder)
			{
				return a.SortOrder < b.SortOrder;
			}
			return a.Index < b.Index;
		};
		const auto isBefore = [&](const SpriteDrawItem& a, const SpriteDrawItem& b)
		{
			return viewDepthSorting ? drawsBeforeByViewDepth(a, b) : drawsBefore(a, b);
		};

		// Most scenes are already in order, so the sort is skipped unless an item is out of order.
		m_SpriteQueue.clear();
		bool inOrder = true;
		uint32_t index = 0;
		for (auto entity : view)
		{
			const auto [transform, sprite] = view.get<WorldTransformComponent, SpriteRendererComponent>(entity);
			if (sprite.Overlay)
			{
				continue; // SubmitOverlaySprites
			}

			SpriteDrawItem item;
			item.SortOrder = sprite.SortOrder;
			item.Z = transform.Translation.z;
			item.Index = index++;
			item.Entity = entity;
			if (sprite.Facing != SpriteFacing::Flat)
			{
				// Where it stands: the bottom edge of an Upright quad, the centre of a Billboard.
				const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
				const glm::mat4 quad = GetSpriteQuadTransform(transform.Matrix, loaded.get(), sprite.SpriteIndex);
				const glm::vec4 local = sprite.Facing == SpriteFacing::Upright ? glm::vec4(0.0f, -0.5f, 0.0f, 1.0f) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
				const glm::vec3 standsAt = glm::vec3(quad * local);
				item.Standing = true;
				item.Depth = glm::dot(glm::vec2(standsAt), m_Camera.GroundForward);
				item.ViewDepth = glm::dot(standsAt - m_Camera.Position, m_Camera.Forward);
			}
			// Standing sprites take the view's default; flat ones are ground decals unless marked Cut-out.
			const SpriteAlphaMode defaultMode = (item.Standing && cutoutByDefault) ? SpriteAlphaMode::Cutout : SpriteAlphaMode::Translucent;
			item.Cutout = (sprite.AlphaMode == SpriteAlphaMode::Default ? defaultMode : sprite.AlphaMode) == SpriteAlphaMode::Cutout;
			item.Additive = sprite.AlphaMode == SpriteAlphaMode::Additive;

			if (inOrder && m_SpriteQueue.empty() == false && isBefore(item, m_SpriteQueue.back()))
			{
				inOrder = false;
			}
			m_SpriteQueue.push_back(item);
		}

		// Lit emitters join the same order (only while the game runs: there is no ParticleScene while editing).
		const ParticleScene* particles = m_Scene->GetParticleScene();
		if (particles != nullptr)
		{
			auto emitters = m_Scene->m_Registry.view<WorldTransformComponent, ParticleEmitter2DComponent>();
			for (auto entity : emitters)
			{
				const auto [transform, emitter] = emitters.get<WorldTransformComponent, ParticleEmitter2DComponent>(entity);
				const ParticleScene::EmitterRuntime* runtime = particles->Find(entity);
				if (emitter.Lighting == ParticleLighting::Unlit || runtime == nullptr || runtime->Particles.Count == 0)
				{
					continue;
				}

				SpriteDrawItem item;
				item.SortOrder = emitter.SortOrder;
				item.Z = transform.Translation.z;
				item.Index = index++;
				item.Entity = entity;
				item.IsEmitter = true;
				item.Additive = emitter.Blend == ParticleBlend::Additive;
				// Particles are billboards (see SubmitParticles), sorted by where the emitter is.
				item.Standing = true;
				item.Depth = glm::dot(glm::vec2(transform.Translation), m_Camera.GroundForward);
				item.ViewDepth = glm::dot(transform.Translation - m_Camera.Position, m_Camera.Forward);

				if (inOrder && m_SpriteQueue.empty() == false && isBefore(item, m_SpriteQueue.back()))
				{
					inOrder = false;
				}
				m_SpriteQueue.push_back(item);
			}
		}

		if (inOrder == false)
		{
			std::sort(m_SpriteQueue.begin(), m_SpriteQueue.end(), isBefore);
		}

		// Shared depth: cut-outs go to the cut-out list, flat sprites before the first standing item are decals, and the
		// rest is translucent, drawn after the cut-outs in this order. Without shared depth only Additive is kept.
		bool reachedStanding = false;

		for (const SpriteDrawItem& item : m_SpriteQueue)
		{
			reachedStanding = reachedStanding || item.Standing;
			if (item.Additive)
			{
				Renderer2D::SetSpriteDrawMode(SpriteDrawMode::Additive);
			}
			else if (sharedDepth)
			{
				SpriteDrawMode mode = SpriteDrawMode::Translucent;
				if (item.IsEmitter == false && item.Cutout)
				{
					// A cut-out sprite being faded (Opacity below 1) goes to the fade variant's list.
					mode = view.get<SpriteRendererComponent>(item.Entity).Opacity < 1.0f ? SpriteDrawMode::FadedCutout : SpriteDrawMode::Cutout;
				}
				else if (item.Standing == false)
				{
					mode = reachedStanding ? SpriteDrawMode::Translucent : SpriteDrawMode::Decal;
				}
				Renderer2D::SetSpriteDrawMode(mode);
			}
			else
			{
				Renderer2D::SetSpriteDrawMode(SpriteDrawMode::Translucent);
			}

			// Sprites carry their entity ID for click-to-select; particles don't, so clicks pass through them.
			Renderer2D::SetEntityID(item.IsEmitter ? -1 : (int32_t)(uint32_t)item.Entity);

			if (item.IsEmitter)
			{
				const ParticleEmitter2DComponent& emitter = m_Scene->m_Registry.get<ParticleEmitter2DComponent>(item.Entity);
				SubmitParticles(emitter, *particles->Find(item.Entity),
					m_Scene->m_Registry.get<WorldTransformComponent>(item.Entity).Translation, m_Camera);
				continue;
			}
			const auto [transform, sprite] = view.get<WorldTransformComponent, SpriteRendererComponent>(item.Entity);
			// Cut-out standing sprites cast sun shadows (the depth buffer holds them, so the shadow matches what is drawn).
			const bool castsShadow = m_SunShadows.Sprites && sharedDepth && item.Cutout && item.Standing && sprite.CastShadows;
			SubmitSprite(transform.Matrix, sprite, m_Camera, castsShadow);
		}
		Renderer2D::SetEntityID(-1);
		Renderer2D::SetEmission(glm::vec3(0.0f));
		Renderer2D::SetSpriteDrawMode(SpriteDrawMode::Translucent);

		// The casters queued above, in one block (one draw call per texture set).
		if (m_ShadowCasters.empty() == false)
		{
			Renderer2D::BeginShadowCasters();
			for (const ShadowCasterQuad& caster : m_ShadowCasters)
			{
				Renderer2D::DrawQuad(caster.Transform, caster.Texture, caster.Rect, caster.Color);
			}
			Renderer2D::EndShadowCasters();
			m_ShadowCasters.clear();
		}
	}

	void SceneSpriteRenderer::PrepareSunShadowCasting()
	{
		m_SunShadows = SunShadowCasting{};
		m_ShadowCasters.clear();

		const RenderFeatureSettings features = m_Features;
		if (features.Shadows != ShadowMode::Sun || features.Lighting != LightingMode::Forward || features.SpriteShadows == SpriteShadowMode::None
			|| Renderer2D::HasSharedDepth(features) == false)
		{
			return;
		}

		Entity sun = m_Scene->GetMainLight();
		if ((bool)sun == false || sun.HasComponent<WorldTransformComponent>() == false || sun.GetComponent<DirectionalLightComponent>().CastShadows == false)
		{
			return;
		}

		m_SunShadows.Sprites = true;
		m_SunShadows.Mode = features.SpriteShadows;

		// FaceSun: the caster stands up, turned about Z so the light hits it face on, the way round closest to how
		// the camera sees the sprite (so its left side casts on the left).
		const glm::vec3 travel = sun.GetComponent<WorldTransformComponent>().Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec2 ground{ travel.x, travel.y };
		const float groundLength = glm::length(ground);
		ground = groundLength < 1.0e-4f ? m_Camera.GroundForward : ground / groundLength;
		if (glm::dot(ground, m_Camera.GroundForward) < 0.0f)
		{
			ground = -ground;
		}
		const float yaw = std::atan2(-ground.x, ground.y);
		m_SunShadows.SunUpright = glm::mat3(glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 0.0f, 1.0f)))
			* glm::mat3(glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));
	}

	void SceneSpriteRenderer::SubmitUnlitParticles()
	{
		const ParticleScene* particles = m_Scene->GetParticleScene();
		if (particles == nullptr)
		{
			return;
		}

		bool began = false;
		auto emitters = m_Scene->m_Registry.view<WorldTransformComponent, ParticleEmitter2DComponent>();
		for (auto entity : emitters)
		{
			const auto [transform, emitter] = emitters.get<WorldTransformComponent, ParticleEmitter2DComponent>(entity);
			const ParticleScene::EmitterRuntime* runtime = particles->Find(entity);
			if (emitter.Lighting != ParticleLighting::Unlit || runtime == nullptr || runtime->Particles.Count == 0)
			{
				continue;
			}
			if (began == false)
			{
				Renderer2D::BeginOverlay();
				began = true;
			}
			SubmitParticles(emitter, *runtime, transform.Translation, m_Camera);
		}
		if (began)
		{
			Renderer2D::SetEmission(glm::vec3(0.0f));
			Renderer2D::EndOverlay();
		}
	}

	void SceneSpriteRenderer::SubmitOverlaySprites()
	{
		// Rare (a few HUD sprites), so collected fresh each frame.
		auto view = m_Scene->m_Registry.view<WorldTransformComponent, SpriteRendererComponent>();
		std::vector<std::pair<int32_t, entt::entity>> overlay;
		for (auto entity : view)
		{
			const SpriteRendererComponent& sprite = view.get<SpriteRendererComponent>(entity);
			if (sprite.Overlay)
			{
				overlay.emplace_back(sprite.SortOrder, entity);
			}
		}
		if (overlay.empty())
		{
			return;
		}

		// Stable: equal SortOrders keep the scene's storage order.
		std::stable_sort(overlay.begin(), overlay.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		Renderer2D::BeginOverlay();
		for (const auto& item : overlay)
		{
			const entt::entity entity = item.second;
			const auto [transform, sprite] = view.get<WorldTransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::SetEntityID((int32_t)(uint32_t)entity);
			SubmitSprite(transform.Matrix, sprite, m_Camera);
		}
		Renderer2D::SetEntityID(-1);
		Renderer2D::SetEmission(glm::vec3(0.0f));
		Renderer2D::EndOverlay();
	}

	void SceneSpriteRenderer::SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime,
		const glm::vec3& origin, const SceneCameraBasis& camera)
	{
		// The emitter's texture when it is loaded, else the soft dot. Lit additive emitters draw additive (SubmitSprites);
		// unlit ones go through the overlay, which has no additive pipeline: TODO they still draw alpha.
		const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(emitter.Texture);
		const Ref<Texture2D>& texture = loaded != nullptr ? loaded : Renderer2D::GetSoftDotTexture();
		TextureRect rect;
		if (loaded != nullptr && loaded->GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = loaded->GetSpriteUVRect(emitter.SpriteIndex);
		}

		// Each particle's own colour times the emitter's intensity.
		Renderer2D::SetEmission(glm::vec3(std::max(emitter.EmissionIntensity, 0.0f)));

		const ParticleScene::ParticleBuffer& buffer = runtime.Particles;
		const float z = origin.z;
		// View space: a particle's offset from the emitter goes along the camera's right and up, so a burst
		// sprays in the plane facing the camera (ParticleSpace).
		const bool viewSpace = emitter.Space == ParticleSpace::View && camera.IsTopDown() == false;
		for (uint32_t i = 0; i < buffer.Count; i++)
		{
			const float t = std::clamp(buffer.Age[i] / buffer.Lifetime[i], 0.0f, 1.0f);
			const glm::vec4 color = emitter.StartColor + (emitter.EndColor - emitter.StartColor) * t;
			const float size = buffer.StartSize[i] * (1.0f + (emitter.EndSizeScale - 1.0f) * t);
			if (camera.IsTopDown())
			{
				Renderer2D::DrawQuad(glm::vec3(buffer.Position[i], z), glm::vec2(size), buffer.Rotation[i], texture, rect, color);
				continue;
			}
			// A tilted camera: each particle faces it, moving on the ground plane (Ground) or facing the camera (View).
			const glm::vec3 position = viewSpace
				? origin + camera.Rotation[0] * (buffer.Position[i].x - origin.x) + camera.Rotation[1] * (buffer.Position[i].y - origin.y)
				: glm::vec3(buffer.Position[i], z);
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
				* glm::mat4(camera.Rotation)
				* glm::rotate(glm::mat4(1.0f), buffer.Rotation[i], glm::vec3(0.0f, 0.0f, 1.0f))
				* glm::scale(glm::mat4(1.0f), glm::vec3(size, size, 1.0f));
			Renderer2D::DrawQuad(transform, texture, rect, color);
		}
	}

	Ref<Texture2D> SceneSpriteRenderer::GetSpriteImage(const SpriteRendererComponent& sprite, TextureRect& rect)
	{
		// No texture, or one not loaded, draws a plain coloured quad. Never loads: whoever assigned the handle loaded it.
		Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
		rect = TextureRect{};
		if (loaded != nullptr && loaded->GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = loaded->GetSpriteUVRect(sprite.SpriteIndex);
		}
		else
		{
			// The whole image, repeated TilingFactor times (the wrap mode repeats it).
			rect.Max = glm::vec2(sprite.TilingFactor);
		}

		if (loaded != nullptr && sprite.FlipX)
		{
			std::swap(rect.Min.x, rect.Max.x);
		}
		if (loaded != nullptr && sprite.FlipY)
		{
			std::swap(rect.Min.y, rect.Max.y);
		}
		return loaded;
	}

	void SceneSpriteRenderer::SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite,
		const SceneCameraBasis& camera, bool shadowCaster)
	{
		TextureRect rect;
		const Ref<Texture2D> texture = GetSpriteImage(sprite, rect);
		const glm::mat4 quad = GetSpriteQuadTransform(transform, texture.get(), sprite.SpriteIndex);
		const glm::mat4 drawn = ApplyFacing(quad, sprite.Facing, camera);
		// Emission travels in the vertices; the casters queued below are drawn later, after it is reset.
		Renderer2D::SetEmission(sprite.EmissionColor * std::max(sprite.EmissionIntensity, 0.0f));
		// Opacity in the alpha: blended for translucent sprites, the fade amount for faded cut-outs.
		// Shadow casters keep the sprite's own colour, so the shadow stays.
		glm::vec4 color = sprite.Color;
		color.a *= std::clamp(sprite.Opacity, 0.0f, 1.0f);
		if (shadowCaster == false || m_SunShadows.Sprites == false)
		{
			Renderer2D::DrawQuad(drawn, texture, rect, color);
			return;
		}

		if (m_SunShadows.Mode == SpriteShadowMode::AsDrawn)
		{
			Renderer2D::DrawQuad(drawn, texture, rect, color);
			m_ShadowCasters.push_back({ drawn, texture, rect, sprite.Color });
			return;
		}

		// FaceSun: the caster stands on the sprite's feet (the middle of its bottom edge; a Billboard's centre)
		// turned to the sun, and the drawn sprite looks its pixels up on it, so it isn't shadowed by itself.
		const bool upright = sprite.Facing == SpriteFacing::Upright;
		const glm::vec3 pivot = glm::vec3(quad * (upright ? glm::vec4(0.0f, -0.5f, 0.0f, 1.0f) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		const glm::mat4 caster = glm::translate(glm::mat4(1.0f), pivot) * glm::mat4(m_SunShadows.SunUpright) * glm::translate(glm::mat4(1.0f), -pivot) * quad;
		Renderer2D::DrawQuad(drawn, texture, rect, color, caster);
		m_ShadowCasters.push_back({ caster, texture, rect, sprite.Color });
	}

	glm::mat4 SceneSpriteRenderer::ApplyFacing(const glm::mat4& quad, SpriteFacing facing, const SceneCameraBasis& camera)
	{
		if (facing == SpriteFacing::Flat || camera.IsTopDown())
		{
			return quad;
		}

		// Turn the flat quad about the point it stands on: the middle of its bottom edge (Upright) or its
		// centre (Billboard). Both leave a top-down view unchanged.
		const bool upright = facing == SpriteFacing::Upright;
		const glm::vec3 pivot = glm::vec3(quad * (upright ? glm::vec4(0.0f, -0.5f, 0.0f, 1.0f) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		const glm::mat3& rotation = upright ? camera.Upright : camera.Rotation;
		return glm::translate(glm::mat4(1.0f), pivot) * glm::mat4(rotation) * glm::translate(glm::mat4(1.0f), -pivot) * quad;
	}

	glm::mat4 SceneSpriteRenderer::GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex)
	{
		return SpriteBounds::GetQuad(transform, texture, spriteIndex);
	}
}
