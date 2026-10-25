#include "hypch.h"

#include "SceneRenderer.h"
#include "Renderer2D.h"
#include "Renderer.h"
#include "Renderer3D.h"
#include <glm/ext/matrix_transform.hpp>

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	SceneRenderer::CameraBasis SceneRenderer::MakeCameraBasis(const glm::mat3& rotation)
	{
		CameraBasis basis;
		basis.Rotation = rotation;

		// The camera's up and forward in the world (it looks down its local -Z).
		const glm::vec3 up = glm::normalize(rotation[1]);
		const glm::vec3 forward = -glm::normalize(rotation[2]);
		basis.Forward = forward;

		// The direction "into the screen" along the ground. Up's ground part points the same way as
		// forward's for a camera without roll, and still exists when the camera looks straight down
		// (forward has no ground part then); a camera looking along the ground has no up on the ground.
		glm::vec2 ground{ up.x, up.y };
		const float upOnGround = glm::length(ground);
		if (upOnGround < 1.0e-4f)
		{
			ground = { forward.x, forward.y };
		}
		const float groundLength = glm::length(ground);
		basis.GroundForward = groundLength < 1.0e-4f ? glm::vec2(0.0f, 1.0f) : ground / groundLength;

		// An Upright quad turns about Z to face the camera (yaw), then leans back about its bottom edge
		// until its up matches the camera's up (lean: 0 for a top-down camera, 90 degrees looking along
		// the ground). Rz(yaw) maps +Y onto GroundForward; Rx(lean) maps +Y onto (0, cos, sin).
		const float yaw = std::atan2(-basis.GroundForward.x, basis.GroundForward.y);
		const float lean = std::atan2(up.z, upOnGround);
		basis.Upright = glm::mat3(glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 0.0f, 1.0f)))
			* glm::mat3(glm::rotate(glm::mat4(1.0f), lean, glm::vec3(1.0f, 0.0f, 0.0f)));
		return basis;
	}

	RenderFeatureSettings SceneRenderer::GetEffectiveRenderFeatures() const
	{
		const SceneRenderSettings& scene = m_Scene->GetRenderSettings();
		return scene.OverrideProject ? scene.Features : Renderer::GetSettings().Features;
	}

	void SceneRenderer::RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay)
	{
		view.SetRenderFeatures(GetEffectiveRenderFeatures());
		const glm::mat4 cameraWorld = glm::inverse(camera.GetView());
		m_Camera = MakeCameraBasis(glm::mat3(cameraWorld));
		m_Camera.Position = glm::vec3(cameraWorld[3]);
		Renderer2D::BeginScene(view, camera.GetViewProjection());
		SubmitMeshes(view);
		SubmitSprites();
		SubmitLights();
		SubmitShadows();
		SubmitUnlitParticles();
		SubmitOverlaySprites();
		SubmitEditorOverlay(overlay);
		Renderer2D::EndScene();
	}

	bool SceneRenderer::RenderRuntime(RenderView& view)
	{
		view.SetRenderFeatures(GetEffectiveRenderFeatures());
		Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();
		if ((bool)cameraEntity == false)
		{
			// Nothing to look through: begin the view anyway so it is cleared, not left stale.
			Renderer2D::BeginScene(view, glm::mat4(1.0f));
			Renderer2D::EndScene();
			return false;
		}

		// The camera's position and rotation; scale would distort the view, so it's ignored.
		glm::mat4 cameraTransform(1.0f);
		if (cameraEntity.HasComponent<TransformComponent>())
		{
			const TransformComponent& transform = cameraEntity.GetComponent<TransformComponent>();
			cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(glm::quat(transform.Rotation));
		}
		m_Camera = MakeCameraBasis(glm::mat3(cameraTransform));
		m_Camera.Position = glm::vec3(cameraTransform[3]);

		const SceneCamera& sceneCamera = cameraEntity.GetComponent<CameraComponent>().Camera;
		Renderer2D::BeginScene(view, Camera(sceneCamera.GetProjection(m_Scene->GetViewportAspectRatio())), cameraTransform);
		SubmitMeshes(view);
		SubmitSprites();
		SubmitLights();
		SubmitShadows();
		SubmitUnlitParticles();
		SubmitOverlaySprites();
		Renderer2D::EndScene();
		return true;
	}

	void SceneRenderer::SubmitSprites()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, SpriteRendererComponent>();

		const RenderFeatureSettings features = GetEffectiveRenderFeatures();
		const bool sharedDepth = Renderer2D::HasSharedDepth(features);
		const bool viewDepthSorting = features.SpriteSorting == SpriteSortMode::ViewDepth;
		const bool cutoutByDefault = features.DefaultSpriteAlpha == SpriteAlphaDefault::Cutout;

		// SpriteSortMode::SortOrder (the 2D preset, today's order kept bit for bit): SortOrder; then Flat
		// before standing (Upright, Billboard, particles); Flat by Z, standing from far to near along the
		// camera's ground direction (Depth, larger = further away); then storage order.
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

		// SpriteSortMode::ViewDepth (3D): the ground decals first (Flat sprites, by SortOrder, then Z, then
		// storage order), then everything standing back to front by its distance along the camera's view
		// direction; SortOrder only breaks ties, so a high number can't pull an effect over a nearer sprite.
		// Cut-out sprites don't depend on this order (the depth buffer sorts them); translucent ones do.
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

		// Collect sort keys. Most scenes are already in order (all SortOrder 0, same Z), so the sort
		// is skipped unless an item is out of order.
		m_SpriteQueue.clear();
		bool inOrder = true;
		uint32_t index = 0;
		for (auto entity : view)
		{
			const auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);
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
				const glm::mat4 quad = GetSpriteQuadTransform(transform.GetTransform(), loaded.get(), sprite.SpriteIndex);
				const glm::vec4 local = sprite.Facing == SpriteFacing::Upright ? glm::vec4(0.0f, -0.5f, 0.0f, 1.0f) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
				const glm::vec3 standsAt = glm::vec3(quad * local);
				item.Standing = true;
				item.Depth = glm::dot(glm::vec2(standsAt), m_Camera.GroundForward);
				item.ViewDepth = glm::dot(standsAt - m_Camera.Position, m_Camera.Forward);
			}
			// Standing sprites take the view's default; flat ones are ground decals unless marked Cut-out.
			const SpriteAlphaMode defaultMode = (item.Standing && cutoutByDefault) ? SpriteAlphaMode::Cutout : SpriteAlphaMode::Translucent;
			item.Cutout = (sprite.AlphaMode == SpriteAlphaMode::Default ? defaultMode : sprite.AlphaMode) == SpriteAlphaMode::Cutout;

			if (inOrder && m_SpriteQueue.empty() == false && isBefore(item, m_SpriteQueue.back()))
			{
				inOrder = false;
			}
			m_SpriteQueue.push_back(item);
		}

		// Lit emitters with live particles join the same order (only while the game runs: there is no
		// ParticleScene while editing).
		const ParticleScene* particles = m_Scene->GetParticleScene();
		if (particles != nullptr)
		{
			auto emitters = m_Scene->m_Registry.view<TransformComponent, ParticleEmitter2DComponent>();
			for (auto entity : emitters)
			{
				const auto [transform, emitter] = emitters.get<TransformComponent, ParticleEmitter2DComponent>(entity);
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

		// Shared depth: cut-out sprites (Cutout, their own or the view's default for standing ones) go to the
		// cut-out list; flat sprites before the first standing item (the ground, roads, decals: with view-depth
		// sorting every flat one) are decals, drawn first in this order; everything else (particles,
		// translucent sprites, flat sprites layered above standing ones in SortOrder sorting) is translucent,
		// drawn after the cut-outs in this order. Without shared depth Renderer2D ignores the modes and draws
		// everything in this order, as before.
		bool reachedStanding = false;

		for (const SpriteDrawItem& item : m_SpriteQueue)
		{
			if (sharedDepth)
			{
				reachedStanding = reachedStanding || item.Standing;
				SpriteDrawMode mode = SpriteDrawMode::Translucent;
				if (item.IsEmitter == false && item.Cutout)
				{
					mode = SpriteDrawMode::Cutout;
				}
				else if (item.Standing == false)
				{
					mode = reachedStanding ? SpriteDrawMode::Translucent : SpriteDrawMode::Decal;
				}
				Renderer2D::SetSpriteDrawMode(mode);
			}

			// An entity's sprite carries its ID (the Editor view writes it for click-to-select); particles don't,
			// so clicks pass through them.
			Renderer2D::SetEntityID(item.IsEmitter ? -1 : (int32_t)(uint32_t)item.Entity);

			if (item.IsEmitter)
			{
				const ParticleEmitter2DComponent& emitter = m_Scene->m_Registry.get<ParticleEmitter2DComponent>(item.Entity);
				SubmitParticles(emitter, *particles->Find(item.Entity), item.Z, m_Camera);
				continue;
			}
			const auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(item.Entity);
			SubmitSprite(transform.GetTransform(), sprite, m_Camera);
		}
		Renderer2D::SetEntityID(-1);

		if (sharedDepth)
		{
			Renderer2D::SetSpriteDrawMode(SpriteDrawMode::Translucent);
		}

		// TODO MESH RENDERING
	}

	// What a mesh slot draws with: its material asset when it is loaded (scene loading and the inspector load
	// it), else its inline settings.
	static MaterialData ResolveMaterial(const MeshMaterialSlot& slot)
	{
		if ((uint64_t)slot.Material != 0)
		{
			const Ref<MaterialAsset> asset = AssetManager::GetLoadedAsset<MaterialAsset>(slot.Material);
			if (asset != nullptr)
			{
				return asset->GetData();
			}
		}
		return slot.Inline;
	}

	void SceneRenderer::SubmitMeshes(RenderView& renderView)
	{
		auto meshes = m_Scene->m_Registry.view<TransformComponent, MeshComponent>();
		if (meshes.begin() == meshes.end())
		{
			return;
		}

		// Meshes are drawn only with shared depth and the Meshes feature; otherwise say so once for this scene.
		const RenderFeatureSettings features = GetEffectiveRenderFeatures();
		if (features.Meshes == false || Renderer2D::HasSharedDepth(features) == false)
		{
			if (m_MeshWarningShown == false)
			{
				HY_CORE_WARN("Scene '{}' has MeshComponents, but its render features don't draw them (Meshes needs to be on, with Depth = Shared)", m_Scene->GetName());
				m_MeshWarningShown = true;
			}
			return;
		}

		Renderer3D::BeginScene(renderView);
		for (auto entity : meshes)
		{
			const auto [transform, meshComponent] = meshes.get<TransformComponent, MeshComponent>(entity);
			const Ref<Mesh> mesh = Renderer3D::GetShapeMesh(meshComponent);
			if (mesh == nullptr)
			{
				continue;
			}

			const glm::mat4 world = transform.GetTransform();
			const uint32_t subMeshCount = (uint32_t)mesh->GetSubMeshes().size();
			for (uint32_t i = 0; i < subMeshCount; i++)
			{
				const uint32_t slot = mesh->GetSubMeshes()[i].MaterialSlot;
				const MaterialData material = slot < meshComponent.Materials.size() ? ResolveMaterial(meshComponent.Materials[slot]) : MaterialData{};
				Renderer3D::DrawMesh(mesh, i, material, world, (int32_t)(uint32_t)entity);
			}
		}
		Renderer3D::EndScene();
	}

	void SceneRenderer::SubmitUnlitParticles()
	{
		const ParticleScene* particles = m_Scene->GetParticleScene();
		if (particles == nullptr)
		{
			return;
		}

		bool began = false;
		auto emitters = m_Scene->m_Registry.view<TransformComponent, ParticleEmitter2DComponent>();
		for (auto entity : emitters)
		{
			const auto [transform, emitter] = emitters.get<TransformComponent, ParticleEmitter2DComponent>(entity);
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
			SubmitParticles(emitter, *runtime, transform.Translation.z, m_Camera);
		}
		if (began)
		{
			Renderer2D::EndOverlay();
		}
	}

	void SceneRenderer::SubmitOverlaySprites()
	{
		// Rare (a few HUD sprites), so collected fresh each frame.
		auto view = m_Scene->m_Registry.view<TransformComponent, SpriteRendererComponent>();
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
			const auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::SetEntityID((int32_t)(uint32_t)entity);
			SubmitSprite(transform.GetTransform(), sprite, m_Camera);
		}
		Renderer2D::SetEntityID(-1);
		Renderer2D::EndOverlay();
	}

	void SceneRenderer::SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime, float z, const CameraBasis& camera)
	{
		// The emitter's texture when it is loaded (whoever assigned it loaded it), else the soft dot.
		// TODO(V4): ParticleBlend::Additive needs the premultiplied quad pipeline; until then it draws alpha.
		const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(emitter.Texture);
		const Ref<Texture2D>& texture = loaded != nullptr ? loaded : Renderer2D::GetSoftDotTexture();
		TextureRect rect;
		if (loaded != nullptr && loaded->GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = loaded->GetSpriteUVRect(emitter.SpriteIndex);
		}

		const ParticleScene::ParticleBuffer& buffer = runtime.Particles;
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
			// A tilted camera: each particle faces it (a billboard), still moving on the ground plane (H10
			// gives particles a height).
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(buffer.Position[i], z))
				* glm::mat4(camera.Rotation)
				* glm::rotate(glm::mat4(1.0f), buffer.Rotation[i], glm::vec3(0.0f, 0.0f, 1.0f))
				* glm::scale(glm::mat4(1.0f), glm::vec3(size, size, 1.0f));
			Renderer2D::DrawQuad(transform, texture, rect, color);
		}
	}

	void SceneRenderer::SubmitLights()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, Light2DComponent>();
		for (auto entity : view)
		{
			const auto [transform, light] = view.get<TransformComponent, Light2DComponent>(entity);

			if (light.Type == Light2DType::Global)
			{
				Renderer2D::DrawGlobalLight(light.Color, light.Intensity);
			}
			else
			{
				// A light with a height sits that far up an Upright sprite: along the camera's up as Upright
				// quads lean (straight up the screen when looking down, up the standing sprite when tilted).
				const glm::vec3 position = transform.Translation + m_Camera.Upright[1] * light.Height;
				Renderer2D::DrawLight(position, light.Radius, light.Color, light.Intensity);
			}
		}
	}

	void SceneRenderer::SubmitShadows()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, Shadow2DComponent>();
		for (auto entity : view)
		{
			const auto [transform, shadow] = view.get<TransformComponent, Shadow2DComponent>(entity);

			const glm::vec3 position = transform.Translation + glm::vec3(shadow.Offset, 0.0f);
			Renderer2D::DrawShadow(position, shadow.Size, shadow.Strength);
		}
	}

	void SceneRenderer::SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite, const CameraBasis& camera)
	{
		// No texture, or one that isn't loaded, draws the white texture, i.e. a plain coloured quad. This
		// never loads from disk: whoever assigned the handle loaded the asset (see the component).
		const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
		if (loaded == nullptr)
		{
			Renderer2D::DrawQuad(ApplyFacing(transform, sprite.Facing, camera), nullptr, sprite.TilingFactor, sprite.Color);
			return;
		}

		const Texture2D& texture = *loaded;
		TextureRect rect;
		const glm::mat4 quadTransform = ApplyFacing(GetSpriteQuadTransform(transform, &texture, sprite.SpriteIndex), sprite.Facing, camera);
		if (texture.GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = texture.GetSpriteUVRect(sprite.SpriteIndex);
		}
		else
		{
			rect.Max = glm::vec2(sprite.TilingFactor);
		}

		if (sprite.FlipX)
		{
			std::swap(rect.Min.x, rect.Max.x);
		}
		if (sprite.FlipY)
		{
			std::swap(rect.Min.y, rect.Max.y);
		}

		Renderer2D::DrawQuad(quadTransform, loaded, rect, sprite.Color);
	}

	glm::mat4 SceneRenderer::ApplyFacing(const glm::mat4& quad, SpriteFacing facing, const CameraBasis& camera)
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

	glm::mat4 SceneRenderer::GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex)
	{
		if (texture == nullptr || texture->GetSpriteMode() != SpriteMode::Multiple)
		{
			return transform;
		}

		// The quad is centred on the entity; move it so the sprite's pivot is there instead
		// (a feet pivot keeps a character standing on its position while frames change).
		const glm::vec2 pivot = texture->GetSprite(spriteIndex).Pivot;
		if (pivot == glm::vec2(0.5f))
		{
			return transform;
		}
		return transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.5f - pivot.x, 0.5f - pivot.y, 0.0f));
	}

	void SceneRenderer::SubmitEditorOverlay(const EditorOverlay& overlay)
	{
		Entity selected = overlay.Selected; // a copy: Entity::GetScene is not const
		const bool drawSelected = overlay.SelectionOutline && selected.IsValid()
			&& selected.GetScene() == m_Scene.get() && selected.HasComponent<TransformComponent>();
		if (drawSelected == false && overlay.Colliders == false)
		{
			return;
		}

		Renderer2D::BeginOverlay();

		if (overlay.Colliders)
		{
			const glm::vec4 sensorColor{ 1.0f, 0.9f, 0.2f, 1.0f };
			const glm::vec4 staticColor{ 0.3f, 0.9f, 0.3f, 1.0f };
			const glm::vec4 dynamicColor{ 0.3f, 0.55f, 1.0f, 1.0f };
			const glm::vec4 kinematicColor{ 0.3f, 0.9f, 0.9f, 1.0f };

			const auto colorFor = [&](entt::entity entity, bool sensor)
			{
				if (sensor)
				{
					return sensorColor;
				}
				if (m_Scene->m_Registry.has<Rigidbody2DComponent>(entity) == false)
				{
					return staticColor;
				}
				switch (m_Scene->m_Registry.get<Rigidbody2DComponent>(entity).Type)
				{
				case Rigidbody2DType::Dynamic: return dynamicColor;
				case Rigidbody2DType::Kinematic: return kinematicColor;
				default: return staticColor;
				}
			};

			auto boxes = m_Scene->m_Registry.view<TransformComponent, BoxCollider2DComponent>();
			for (auto entity : boxes)
			{
				const auto [transform, box] = boxes.get<TransformComponent, BoxCollider2DComponent>(entity);
				const glm::mat4 shape = transform.GetTransform()
					* glm::translate(glm::mat4(1.0f), glm::vec3(box.Offset, 0.0f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(box.Size, 1.0f));
				Renderer2D::DrawRect(shape, colorFor(entity, box.IsSensor), 1.5f);
			}

			auto circles = m_Scene->m_Registry.view<TransformComponent, CircleCollider2DComponent>();
			for (auto entity : circles)
			{
				const auto [transform, circle] = circles.get<TransformComponent, CircleCollider2DComponent>(entity);
				const glm::vec3 centre = glm::vec3(transform.GetTransform() * glm::vec4(circle.Offset, 0.0f, 1.0f));
				const float scale = std::max(std::abs(transform.Scale.x), std::abs(transform.Scale.y));
				Renderer2D::DrawCircle(centre, circle.Radius * scale, colorFor(entity, circle.IsSensor), 1.5f);
			}
		}

		if (drawSelected)
		{
			const glm::vec4 selectionColor{ 1.0f, 0.65f, 0.1f, 1.0f };
			const float thickness = 2.5f;
			const TransformComponent& transform = selected.GetComponent<TransformComponent>();
			const glm::mat4 matrix = transform.GetTransform();

			if (selected.HasComponent<SpriteRendererComponent>())
			{
				const SpriteRendererComponent& sprite = selected.GetComponent<SpriteRendererComponent>();
				const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
				Renderer2D::DrawRect(ApplyFacing(GetSpriteQuadTransform(matrix, loaded.get(), sprite.SpriteIndex), sprite.Facing, m_Camera), selectionColor, thickness);
			}
			else
			{
				// Nothing drawn to outline: a small box on the entity (a point light also shows its reach).
				Renderer2D::DrawRect(transform.Translation, glm::vec2(0.6f), selectionColor, thickness);
				if (selected.HasComponent<Light2DComponent>())
				{
					const Light2DComponent& light = selected.GetComponent<Light2DComponent>();
					if (light.Type == Light2DType::Point)
					{
						Renderer2D::DrawCircle(transform.Translation, light.Radius, selectionColor, 1.5f);
					}
				}
			}
		}

		Renderer2D::EndOverlay();
	}
}
