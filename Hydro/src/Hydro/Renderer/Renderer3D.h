#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Scene/Components/Components.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Hydro
{
	class RenderView;
	struct RenderPassContext;

	// Meshes for the last frame (all views), for the Stats window.
	struct Renderer3DStatistics
	{
		uint32_t MeshDraws = 0;       // draw calls (one per sub-mesh drawn)
		uint32_t ShadowDraws = 0;     // of those, into sun shadow maps
		uint32_t Triangles = 0;
		uint32_t CachedMeshes = 0;    // built-in shapes kept on the GPU
		uint32_t CachedMaterials = 0; // Standard materials (one descriptor set each)
	};

	// The 3D part of Renderer (plan M4): static meshes with the Standard material, drawn by the view's Meshes
	// render feature at the Opaque stage, into the shared depth buffer, before the sprites. Started and
	// stopped by Renderer::Init / Shutdown.
	//
	//   Renderer2D::BeginScene(view, ...);   // the camera
	//   Renderer3D::BeginScene(view);
	//   Renderer3D::DrawMesh(mesh, subMesh, material, transform, entityID);
	//   Renderer3D::EndScene();
	//   Renderer2D::EndScene();
	class Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		// Queue meshes for `view`, seen through the camera of its Renderer2D::BeginScene this frame. Only
		// views with shared depth and RenderFeatureSettings::Meshes draw them.
		static void BeginScene(RenderView& view);
		static void EndScene();

		// One sub-mesh with a Standard material. `entityID` is written to the Editor view's entity-ID
		// attachment (-1 = not pickable). The material is cached by its data, so meshes with equal materials
		// share one descriptor set. `castShadows`: also drawn into the view's sun shadow map (R14), when it has one.
		static void DrawMesh(const Ref<Mesh>& mesh, uint32_t subMesh, const MaterialData& material, const glm::mat4& transform, int32_t entityID,
			bool castShadows = true);

		// The built-in shape of `component` (shape, size, steps, segments), made once and kept while used.
		static Ref<Mesh> GetShapeMesh(const MeshComponent& component);

		// The Meshes feature's pass (Renderer2D's render graph). Records the view's queued meshes.
		static void RecordMeshes(RenderPassContext& context);
		// Part of the SunShadows pass (Renderer2D, R14): the view's queued meshes that cast shadows, depth only
		// (cut out like their material), into the view's shadow map, which the pass has begun.
		static void RecordShadowCasters(RenderPassContext& context);
		// After the frame is recorded: frees shapes and materials no frame has used for a while, updates the stats.
		static void EndFrame();
		// A shader of the VulkanShaderLibrary was reloaded: drops the pipelines built from it.
		static void OnShaderReloaded(const std::string& name);

		static Renderer3DStatistics GetStats();
	};
}
