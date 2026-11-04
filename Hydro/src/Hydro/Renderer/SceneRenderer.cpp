#include "hypch.h"

#include "SceneRenderer.h"
#include "SceneEditorRenderer.h"
#include "Renderer2D.h"
#include "Renderer.h"
#include "Renderer3D.h"
#include "TerrainCache.h"
#include "Hydro/Terrain/TerrainMesher.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/MeshBounds.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	RenderFeatureSettings SceneRenderer::GetEffectiveRenderFeatures() const
	{
		const SceneRenderSettings& scene = m_Scene->GetRenderSettings();
		return scene.OverrideProject ? scene.Features : Renderer::GetSettings().Features;
	}

	void SceneRenderer::RenderEditor(RenderView& view, const EditorCamera& camera)
	{
		RenderEditor(view, camera, EditorOverlay());
	}

	void SceneRenderer::RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay)
	{
		// Application systems run after the scene update and may have moved things: draw where they are now.
		m_Scene->ResolveTransforms();
		m_Scene->ResolveUILayout();
		view.SetRenderFeatures(GetEffectiveRenderFeatures());
		const glm::mat4 cameraWorld = glm::inverse(camera.GetView());
		m_Camera = SceneCameraBasis::Make(glm::mat3(cameraWorld), GetEffectiveRenderFeatures().UprightSprites);
		m_Camera.Position = glm::vec3(cameraWorld[3]);
		m_Sprites.BeginFrame(*m_Scene, GetEffectiveRenderFeatures(), m_Camera);
		Renderer2D::BeginScene(view, camera.GetViewProjection());
		// The Editor view shows depth of field, bloom, vignette and fog only when asked, so it stays easy to edit in.
		ScenePostProcess postProcess = m_Scene->GetActivePostProcess();
		if (overlay.PostProcessing == false)
		{
			postProcess.DepthOfField = false;
			postProcess.Bloom = false;
			postProcess.VignetteIntensity = 0.0f;
		}
		m_Fog = overlay.Fog;
		Renderer2D::SetPostProcess(postProcess);
		Renderer2D::SetViewFocus(m_Camera.GetGroundFocus(), m_Camera.Forward);
		SubmitMeshes(view);
		m_Sprites.SubmitSprites();
		SubmitLights();
		SubmitShadows();
		m_Sprites.SubmitUnlitParticles();
		m_Sprites.SubmitOverlaySprites();
		SceneEditorRenderer::Submit(*m_Scene, overlay, m_Camera);
		Renderer2D::EndScene();
	}

	bool SceneRenderer::RenderRuntime(RenderView& view)
	{
		// Application systems run after the scene update and may have moved things: draw where they are now.
		m_Scene->ResolveTransforms();
		m_Scene->ResolveUILayout();
		view.SetRenderFeatures(GetEffectiveRenderFeatures());
		Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();
		if ((bool)cameraEntity == false)
		{
			// Begin the view anyway so it is cleared, not left stale. The UI doesn't need a camera.
			Renderer2D::BeginScene(view, glm::mat4(1.0f));
			m_UI.Submit(*m_Scene);
			Renderer2D::EndScene();
			return false;
		}

		// The camera's position and rotation; scale would distort the view, so it's ignored.
		glm::mat4 cameraTransform(1.0f);
		if (cameraEntity.HasComponent<WorldTransformComponent>())
		{
			const WorldTransformComponent& transform = cameraEntity.GetComponent<WorldTransformComponent>();
			cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(transform.Rotation);
		}
		const SceneCamera& sceneCamera = cameraEntity.GetComponent<CameraComponent>().Camera;
		if (GetEffectiveRenderFeatures().PixelSnap)
		{
			SnapCameraToPixels(cameraTransform, sceneCamera, view.GetHeight());
		}
		m_Camera = SceneCameraBasis::Make(glm::mat3(cameraTransform), GetEffectiveRenderFeatures().UprightSprites);
		m_Camera.Position = glm::vec3(cameraTransform[3]);
		m_Sprites.BeginFrame(*m_Scene, GetEffectiveRenderFeatures(), m_Camera);
		m_Fog = true;

		Renderer2D::BeginScene(view, Camera(sceneCamera.GetProjection(m_Scene->GetViewportAspectRatio())), cameraTransform);
		Renderer2D::SetPostProcess(m_Scene->GetActivePostProcess());
		// The depth of field focuses at SceneCamera::FocusDistance when set, else on the ground focus.
		// The fog keeps measuring from the ground (FogOrigin).
		const glm::vec3 focus = sceneCamera.FocusDistance > 0.0f ? m_Camera.Position + m_Camera.Forward * sceneCamera.FocusDistance
			: m_Camera.GetGroundFocus();
		Renderer2D::SetViewFocus(focus, m_Camera.Forward);
		SubmitMeshes(view);
		m_Sprites.SubmitSprites();
		SubmitLights();
		SubmitShadows();
		m_Sprites.SubmitUnlitParticles();
		m_Sprites.SubmitOverlaySprites();
		m_UI.Submit(*m_Scene);
		Renderer2D::EndScene();
		return true;
	}

	void SceneRenderer::SnapCameraToPixels(glm::mat4& cameraTransform, const SceneCamera& camera, uint32_t viewHeight)
	{
		if (viewHeight == 0)
		{
			return;
		}
		const glm::vec3 right = glm::normalize(glm::vec3(cameraTransform[0]));
		const glm::vec3 up = glm::normalize(glm::vec3(cameraTransform[1]));
		const glm::vec3 forward = -glm::normalize(glm::vec3(cameraTransform[2]));
		const glm::vec3 position = glm::vec3(cameraTransform[3]);

		// One screen pixel in metres where the camera looks (at its focus point for a perspective camera).
		// Nearer and farther things still slide by fractions of a pixel (parallax).
		float pixel = 0.0f;
		if (camera.Projection == ProjectionType::Orthographic)
		{
			pixel = camera.OrthographicSize / (float)viewHeight;
		}
		else
		{
			float distance = camera.FocusDistance;
			if (distance <= 0.0f && forward.z < -0.05f)
			{
				distance = -position.z / forward.z;
			}
			if (distance <= 0.0f)
			{
				return;
			}
			pixel = 2.0f * distance * std::tan(glm::radians(camera.PerspectiveFOV) * 0.5f) / (float)viewHeight;
		}
		if (pixel <= 1e-6f)
		{
			return;
		}

		// Round across and up the screen to whole pixels; along the view it is kept, so the camera doesn't zoom.
		// The image then moves by whole pixels and every texel keeps its shape.
		const float across = glm::dot(position, right);
		const float upward = glm::dot(position, up);
		const float snappedAcross = std::round(across / pixel) * pixel;
		const float snappedUpward = std::round(upward / pixel) * pixel;
		const glm::vec3 snapped = position + right * (snappedAcross - across) + up * (snappedUpward - upward);
		cameraTransform[3] = glm::vec4(snapped, 1.0f);
	}

	// A mesh slot's material asset when it is loaded, else its inline settings.
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

	// A slot the component doesn't have: a model's own material for it (the asset its import made), else white.
	static MaterialData ResolveModelMaterial(const Mesh& mesh, uint32_t slot)
	{
		if (slot < mesh.GetMaterials().size())
		{
			const AssetHandle handle = mesh.GetMaterials()[slot].DefaultMaterial;
			const Ref<MaterialAsset> asset = (uint64_t)handle != 0 ? AssetManager::GetLoadedAsset<MaterialAsset>(handle) : nullptr;
			if (asset != nullptr)
			{
				return asset->GetData();
			}
		}
		return MaterialData{};
	}

	void SceneRenderer::SubmitMeshes(RenderView& renderView)
	{
		auto meshes = m_Scene->m_Registry.view<WorldTransformComponent, MeshComponent>();
		auto terrains = m_Scene->m_Registry.view<WorldTransformComponent, TerrainComponent>();
		if (meshes.begin() == meshes.end() && terrains.begin() == terrains.end())
		{
			return;
		}

		// Meshes are drawn only with shared depth and the Meshes feature; otherwise say so once for this scene.
		const RenderFeatureSettings features = GetEffectiveRenderFeatures();
		if (features.Meshes == false || Renderer2D::HasSharedDepth(features) == false)
		{
			if (m_MeshWarningShown == false)
			{
				HY_CORE_WARN("Scene '{}' has MeshComponents or TerrainComponents, but its render features don't draw them (Meshes needs to be on, with Depth = Shared)",
					m_Scene->GetName());
				m_MeshWarningShown = true;
			}
			return;
		}

		Renderer3D::BeginScene(renderView);
		for (auto entity : meshes)
		{
			const auto [transform, meshComponent] = meshes.get<WorldTransformComponent, MeshComponent>(entity);
			// A model that isn't loaded (missing, or failed to import: logged when it was loaded) draws nothing.
			const Ref<Mesh> mesh = (uint64_t)meshComponent.Model != 0 ? AssetManager::GetLoadedAsset<Mesh>(meshComponent.Model)
				: Renderer3D::GetShapeMesh(meshComponent);
			if (mesh == nullptr)
			{
				continue;
			}

			const glm::mat4 world = transform.Matrix;
			const uint32_t subMeshCount = (uint32_t)mesh->GetSubMeshes().size();
			for (uint32_t i = 0; i < subMeshCount; i++)
			{
				const uint32_t slot = mesh->GetSubMeshes()[i].MaterialSlot;
				const MaterialData material = slot < meshComponent.Materials.size() ? ResolveMaterial(meshComponent.Materials[slot]) : ResolveModelMaterial(*mesh, slot);
				Renderer3D::DrawMesh(mesh, i, material, world, (int32_t)(uint32_t)entity, meshComponent.CastShadows, meshComponent.Opacity);
			}
		}
		SubmitTerrains();
		Renderer3D::EndScene();
	}

	void SceneRenderer::SubmitTerrains()
	{
		auto terrains = m_Scene->m_Registry.view<WorldTransformComponent, TerrainComponent>();
		for (auto entity : terrains)
		{
			const auto [transform, terrainComponent] = terrains.get<WorldTransformComponent, TerrainComponent>(entity);
			// A terrain that isn't loaded (missing, or failed to read: logged when it was loaded) draws nothing.
			const Ref<Terrain> terrain = (uint64_t)terrainComponent.Terrain != 0 ? AssetManager::GetLoadedAsset<Terrain>(terrainComponent.Terrain) : nullptr;
			const TerrainRenderData* data = terrain != nullptr ? TerrainCache::Prepare(terrain) : nullptr;
			if (data == nullptr)
			{
				continue;
			}

			const TerrainSettings& settings = terrain->GetSettings();
			TerrainSurfaceMaterial material;
			material.Weights = data->Weights;
			material.WeightMapSize = glm::vec2((float)terrain->GetWeightMapWidth(), (float)terrain->GetWeightMapHeight()) / (float)std::max(settings.SplatResolution, 1u);
			material.LayerCount = (uint32_t)std::min<size_t>(settings.Layers.size(), MaxTerrainLayers);
			for (uint32_t i = 0; i < material.LayerCount; i++)
			{
				const TerrainLayer& layer = settings.Layers[i];
				const Ref<MaterialAsset> asset = (uint64_t)layer.Material != 0 ? AssetManager::GetLoadedAsset<MaterialAsset>(layer.Material) : nullptr;
				material.Layers[i] = asset != nullptr ? asset->GetData() : MaterialData{};
				material.EdgeRoughness[i] = layer.EdgeRoughness;
				material.Blend[i] = layer.Blend;
				material.HeightBlend[i] = layer.HeightBlend;
			}
			material.EdgeClusterSize = settings.EdgeClusterSize;
			material.EdgePixelNoise = settings.EdgePixelNoise;
			material.Seed = settings.Seed;

			// The parts' materials (TerrainMesher::GetPartKind): walls and their lip faces (20 % darker), fringes,
			// cap stones, decor sprites (cut out).
			auto loadedMaterial = [](AssetHandle handle)
			{
				const Ref<MaterialAsset> asset = (uint64_t)handle != 0 ? AssetManager::GetLoadedAsset<MaterialAsset>(handle) : nullptr;
				return asset != nullptr ? asset->GetData() : MaterialData{};
			};
			auto partMaterial = [&](uint32_t slot)
			{
				const uint32_t index = TerrainMesher::GetPartIndex(slot);
				switch (TerrainMesher::GetPartKind(slot))
				{
					case TerrainPartKind::Wall:
					case TerrainPartKind::LipFace:
					{
						MaterialData wall = index < settings.WallLayers.size() ? loadedMaterial(settings.WallLayers[index].Material) : MaterialData{};
						if (TerrainMesher::GetPartKind(slot) == TerrainPartKind::LipFace)
						{
							wall.Tint = glm::vec4(glm::vec3(wall.Tint) * 0.8f, wall.Tint.a);
						}
						return wall;
					}
					case TerrainPartKind::Fringe:
						return index < settings.Layers.size() ? loadedMaterial(settings.Layers[index].Fringe) : MaterialData{};
					case TerrainPartKind::CapStones:
						return loadedMaterial(settings.CapStoneMaterial);
					case TerrainPartKind::Decor:
					{
						MaterialData sprite;
						const uint32_t rule = index / 256;
						const uint32_t variant = index % 256;
						if (rule < settings.Decor.size() && variant < settings.Decor[rule].Sprites.size())
						{
							sprite.Albedo = settings.Decor[rule].Sprites[variant];
							sprite.AlphaCutoff = settings.Decor[rule].Shape == TerrainDecorShape::Stone ? 0.0f : 0.5f;
						}
						return sprite;
					}
				}
				return MaterialData{};
			};

			// Only the position: terrains are axis-aligned and unscaled.
			const glm::mat4 world = glm::translate(glm::mat4(1.0f), transform.Translation);
			const int32_t entityID = (int32_t)(uint32_t)entity;
			for (const TerrainChunkDraw& chunk : data->Chunks)
			{
				if (chunk.Tops != nullptr)
				{
					Renderer3D::DrawTerrainSurface(chunk.Tops, 0, material, world, entityID, terrainComponent.CastShadows);
				}
				if (chunk.Parts != nullptr)
				{
					const std::vector<SubMesh>& subMeshes = chunk.Parts->GetSubMeshes();
					for (uint32_t i = 0; i < (uint32_t)subMeshes.size(); i++)
					{
						Renderer3D::DrawMesh(chunk.Parts, i, partMaterial(subMeshes[i].MaterialSlot), world, entityID,
							terrainComponent.CastShadows);
					}
				}
			}
		}
	}

	void SceneRenderer::SubmitLights()
	{
		const RenderFeatureSettings features = GetEffectiveRenderFeatures();
		if (features.Lighting == LightingMode::Forward)
		{
			SubmitForwardLighting();
			return;
		}

		auto view = m_Scene->m_Registry.view<WorldTransformComponent, Light2DComponent>();
		for (auto entity : view)
		{
			const auto [transform, light] = view.get<WorldTransformComponent, Light2DComponent>(entity);

			if (light.Type == Light2DType::Global)
			{
				Renderer2D::DrawGlobalLight(light.Color, light.Intensity);
			}
			else if (light.Type == Light2DType::Spot)
			{
				// Forward lighting only: the light buffer has no cone.
				continue;
			}
			else
			{
				// A light with a height sits that far up an Upright sprite: along the camera's up as Upright quads lean.
				const glm::vec3 position = transform.Translation + m_Camera.Upright[1] * light.Height;
				Renderer2D::DrawLight(position, light.Radius, light.Color, light.Intensity);
			}
		}
	}

	glm::vec3 SceneRenderer::GetForwardDirection(const WorldTransformComponent& transform)
	{
		const glm::vec3 direction = transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		const float length = glm::length(direction);
		return length > 0.0f ? direction / length : glm::vec3(0.0f, 0.0f, -1.0f);
	}

	void SceneRenderer::SubmitForwardLighting()
	{
		SceneLighting lighting;
		lighting.CameraPosition = m_Camera.Position;
		// Measured from where the view meets the ground, so a top-down editor camera high above isn't fogged out.
		// The depth of field measures from the same point (SetViewFocus).
		lighting.FogOrigin = m_Camera.GetGroundFocus();

		Entity sun = m_Scene->GetMainLight();
		if ((bool)sun && sun.HasComponent<WorldTransformComponent>())
		{
			const DirectionalLightComponent& light = sun.GetComponent<DirectionalLightComponent>();
			lighting.HasSun = true;
			lighting.SunDirection = GetForwardDirection(sun.GetComponent<WorldTransformComponent>());
			lighting.SunColor = light.Color * light.Intensity;
			// Drawn only when the view has the SunShadows feature (Renderer2D checks its graph).
			lighting.SunShadows = light.CastShadows && GetEffectiveRenderFeatures().Shadows == ShadowMode::Sun;
			lighting.ShadowStrength = light.ShadowStrength;
			lighting.ShadowDistance = light.ShadowDistance;
			lighting.ShadowBias = light.ShadowBias;
		}

		// Ambient: the scene's environment, else its Global lights (sky and ground alike).
		glm::vec3 globalAmbient{ 0.0f };
		auto lights = m_Scene->m_Registry.view<WorldTransformComponent, Light2DComponent>();
		for (auto entity : lights)
		{
			const auto [transform, light] = lights.get<WorldTransformComponent, Light2DComponent>(entity);
			if (light.Type == Light2DType::Global)
			{
				globalAmbient += light.Color * light.Intensity;
				continue;
			}

			// Z is up: Height lifts the light straight above its entity (a lantern's glass above its post).
			ForwardLight forward;
			forward.Position = transform.Translation + glm::vec3(0.0f, 0.0f, light.Height);
			forward.Range = light.Radius;
			forward.Color = light.Color * light.Intensity;
			if (light.Type == Light2DType::Spot)
			{
				const float inner = glm::radians(std::clamp(light.InnerAngle, 0.0f, 89.0f));
				const float outer = glm::radians(std::clamp(std::max(light.OuterAngle, light.InnerAngle), 0.1f, 89.5f));
				forward.Type = ForwardLightType::Spot;
				forward.Direction = GetForwardDirection(transform);
				forward.CosInner = std::cos(inner);
				forward.CosOuter = std::cos(std::max(outer, inner + 0.001f));
			}
			lighting.Lights.push_back(forward);
		}

		const SceneEnvironment environment = m_Scene->GetActiveEnvironment();
		if (environment.Override)
		{
			lighting.SkyAmbient = environment.SkyAmbient;
			lighting.GroundAmbient = environment.GroundAmbient;
			lighting.Fog = environment.Fog && m_Fog;
			lighting.FogColor = environment.FogColor;
			lighting.FogStart = environment.FogStart;
			lighting.FogEnd = environment.FogEnd;
		}
		else
		{
			lighting.SkyAmbient = globalAmbient;
			lighting.GroundAmbient = globalAmbient;
		}

		Renderer2D::SetSceneLighting(lighting);
	}

	void SceneRenderer::SubmitShadows()
	{
		auto view = m_Scene->m_Registry.view<WorldTransformComponent, Shadow2DComponent>();
		const RenderFeatureSettings features = GetEffectiveRenderFeatures();
		const bool forward = features.Lighting == LightingMode::Forward;
		if (forward)
		{
			// Forward lighting has no light buffer to darken: blob shadows are soft dark decals on the ground
			// under their owners (depth tested, drawn before the cut-out sprites).
			Renderer2D::SetSpriteDrawMode(SpriteDrawMode::Decal);
		}

		for (auto entity : view)
		{
			const auto [transform, shadow] = view.get<WorldTransformComponent, Shadow2DComponent>(entity);

			const glm::vec3 position = transform.Translation + glm::vec3(shadow.Offset, 0.0f);
			if (forward)
			{
				const glm::mat4 quad = glm::translate(glm::mat4(1.0f), position + glm::vec3(0.0f, 0.0f, 0.01f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(shadow.Size, 1.0f));
				Renderer2D::DrawQuad(quad, Renderer2D::GetBlobShadowTexture(), 1.0f,
					glm::vec4(0.0f, 0.0f, 0.0f, std::clamp(shadow.Strength, 0.0f, 1.0f)));
				continue;
			}
			Renderer2D::DrawShadow(position, shadow.Size, shadow.Strength);
		}

		if (forward)
		{
			Renderer2D::SetSpriteDrawMode(SpriteDrawMode::Translucent);
		}
	}
}
