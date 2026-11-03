#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Material.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Terrain/Terrain.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Hydro
{
	class RenderView;
	struct RenderPassContext;

	// A terrain's top surface material (terrain.frag picks a layer per art pixel), made by SceneRenderer every frame.
	// Plain data: Renderer3D caches its GPU state by value, like Standard materials.
	struct TerrainSurfaceMaterial
	{
		Ref<Texture2D> Weights;          // the weight map (TerrainCache), sampled linearly and clamped
		glm::vec2 WeightMapSize{ 1.0f }; // metres the weight map covers (texels / SplatResolution)
		uint32_t LayerCount = 1;         // 1..MaxTerrainLayers
		MaterialData Layers[MaxTerrainLayers]; // albedo, tint, tiling and filter of each layer
		float EdgeRoughness[MaxTerrainLayers] = { 0.9f, 0.9f, 0.9f, 0.9f };
		float Blend[MaxTerrainLayers] = { 0.0f, 0.0f, 0.0f, 0.0f };       // TerrainLayer::Blend
		float HeightBlend[MaxTerrainLayers] = { 0.0f, 0.0f, 0.0f, 0.0f }; // TerrainLayer::HeightBlend
		float EdgeClusterSize = 2.5f;    // art pixels
		float EdgePixelNoise = 0.35f;
		uint32_t Seed = 0;

		bool operator==(const TerrainSurfaceMaterial& other) const;
		bool operator!=(const TerrainSurfaceMaterial& other) const { return (*this == other) == false; }
	};

	// Meshes for the last frame (all views), for the Stats window.
	struct Renderer3DStatistics
	{
		uint32_t MeshDraws = 0;       // draw calls (one per sub-mesh drawn)
		uint32_t ShadowDraws = 0;     // of those, into sun shadow maps
		uint32_t Triangles = 0;
		uint32_t CachedMeshes = 0;    // built-in shapes kept on the GPU
		uint32_t CachedMaterials = 0; // Standard and terrain surface materials (one descriptor set each)
		uint32_t TerrainDraws = 0;    // of the mesh draws, terrain surfaces
	};

	// Queues meshes and terrain surfaces for a view and records them for MeshPass and ShadowMapPass. Call between
	// Renderer2D::BeginScene and EndScene (its camera); started and stopped by Renderer.
	class Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		// Only views with shared depth and RenderFeatureSettings::Meshes draw them.
		static void BeginScene(RenderView& view);
		static void EndScene();

		// Materials are cached by their data, so equal ones share a descriptor set. `entityID` -1 = not pickable;
		// `opacity` below 1 dithers the mesh away (0 = gone) while its shadow stays.
		static void DrawMesh(const Ref<Mesh>& mesh, uint32_t subMesh, const MaterialData& material, const glm::mat4& transform,
			int32_t entityID, bool castShadows = true, float opacity = 1.0f);

		// A TerrainCache chunk mesh (UVs are positions in the terrain's space); it casts like an opaque mesh.
		static void DrawTerrainSurface(const Ref<Mesh>& mesh, uint32_t subMesh, const TerrainSurfaceMaterial& material,
			const glm::mat4& transform, int32_t entityID, bool castShadows = true);

		// Made once per shape and parameters, and kept while used.
		static Ref<Mesh> GetShapeMesh(const MeshComponent& component);

		// MeshPass: the view's queued meshes.
		static void RecordMeshes(RenderPassContext& context);
		// ShadowMapPass, inside the map it has begun: the casting meshes, depth only, cut out like their material.
		static void RecordShadowCasters(RenderPassContext& context);
		// Frees shapes and materials no frame has used for a while and updates the stats.
		static void EndFrame();
		// A shader of the VulkanShaderLibrary was reloaded: drops the pipelines built from it.
		static void OnShaderReloaded(const std::string& name);

		static Renderer3DStatistics GetStats();
	};
}
