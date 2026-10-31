#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Terrain/Terrain.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// One chunk of a terrain, ready to draw, in the terrain's own space: its tops (the terrain surface shader) and its
	// parts (walls, lip faces, fringes, cap stones, decor: Standard materials, one sub-mesh per part whose
	// MaterialSlot says what it is, TerrainMesher::GetPartKind).
	struct TerrainChunkDraw
	{
		Ref<Mesh> Tops;
		Ref<Mesh> Parts;
		glm::vec3 BoundsMin{ 0.0f };
		glm::vec3 BoundsMax{ 0.0f };
	};

	// What a terrain looks like on the GPU this frame (TerrainCache::Prepare).
	struct TerrainRenderData
	{
		Ref<Texture2D> Weights; // the weight map: RGBA (linear), filtered linearly, clamped
		std::vector<TerrainChunkDraw> Chunks;
		std::vector<std::string> Problems; // what the mesher found wrong (an invalid ramp run), for the inspector
	};

	// Terrains on the GPU (Terrain plan, "Component and rendering"): per terrain asset, the weight map as a texture and
	// the meshes of its chunks, rebuilt only where the terrain changed (its revisions), plus the chunks' triangles on
	// the CPU for picking. It keeps one TerrainMesher per terrain (the whole terrain's walls and ramps), made again
	// when the levels, ramps, wall materials or settings change (Terrain::GetCollisionRevision). A part of Renderer (started and stopped by Renderer::Init / Shutdown, after Renderer3D);
	// terrains no frame has drawn for a while are dropped (their GPU objects go through the deletion queue).
	//
	//   const TerrainRenderData* data = TerrainCache::Prepare(terrain); // SceneRenderer, every frame it is drawn
	//   float distance;
	//   if (TerrainCache::Raycast(terrain, origin, direction, distance)) { ... } // the editor's brushes
	class TerrainCache
	{
	public:
		static void Init();
		static void Shutdown();

		// Brings `terrain`'s GPU copy up to date (uploads the weights it changed, rebuilds the chunks it changed) and
		// returns it. Uploads wait for the GPU (Texture2D::SetData, Mesh): render submission and the editor only.
		static const TerrainRenderData* Prepare(const Ref<Terrain>& terrain);

		// The nearest hit of a ray with the terrain's surface, in the terrain's own space (`direction` need not be
		// normalised; `distance` is in units of it). Prepares the terrain first. False when the ray misses.
		static bool Raycast(const Ref<Terrain>& terrain, const glm::vec3& origin, const glm::vec3& direction, float& distance);

		// After the frame is recorded: drops terrains no frame has prepared for a while.
		static void EndFrame();
		// Terrains kept on the GPU.
		static uint32_t GetCachedCount();
	};
}
