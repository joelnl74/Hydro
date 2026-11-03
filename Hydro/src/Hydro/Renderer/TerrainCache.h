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
	// One chunk of a terrain in its own space: its tops (the terrain shader) and its parts (Standard materials, one
	// sub-mesh per part kind, TerrainMesher::GetPartKind).
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

	// Terrains on the GPU: per terrain, the weight map and chunk meshes, rebuilt only where it changed, plus triangles
	// for picking. Started by Renderer::Init; terrains not drawn for a while are dropped.
	class TerrainCache
	{
	public:
		static void Init();
		static void Shutdown();

		// Brings the terrain's GPU copy up to date and returns it. Uploads wait for the GPU: render submission and editor only.
		static const TerrainRenderData* Prepare(const Ref<Terrain>& terrain);

		// The nearest hit of a ray with the surface, in the terrain's space (`distance` in units of `direction`).
		// Prepares the terrain first. False when it misses.
		static bool Raycast(const Ref<Terrain>& terrain, const glm::vec3& origin, const glm::vec3& direction, float& distance);

		// After the frame is recorded: drops terrains no frame has prepared for a while.
		static void EndFrame();
		// Terrains kept on the GPU.
		static uint32_t GetCachedCount();
	};
}
