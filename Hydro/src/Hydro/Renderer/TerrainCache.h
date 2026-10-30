#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Terrain/Terrain.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// One chunk of a terrain, ready to draw: its tops (TR1) in the terrain's own space.
	struct TerrainChunkDraw
	{
		Ref<Mesh> Tops;
		glm::vec3 BoundsMin{ 0.0f };
		glm::vec3 BoundsMax{ 0.0f };
	};

	// What a terrain looks like on the GPU this frame (TerrainCache::Prepare).
	struct TerrainRenderData
	{
		Ref<Texture2D> Weights; // the weight map: RGBA (linear), filtered linearly, clamped
		std::vector<TerrainChunkDraw> Chunks;
	};

	// Terrains on the GPU (Terrain plan, "Component and rendering"): per terrain asset, the weight map as a texture and
	// the meshes of its chunks, rebuilt only where the terrain changed (its revisions), plus the chunks' triangles on
	// the CPU for picking. A part of Renderer (started and stopped by Renderer::Init / Shutdown, after Renderer3D);
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
