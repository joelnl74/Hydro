#include "hypch.h"
#include "TerrainDecor.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Terrain/TerrainMeshBuilder.h"
#include "Hydro/Terrain/TerrainMesher.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	static constexpr int64_t TagDecor = 7; // after terrain_mesher's tags 1..6
	static constexpr uint32_t MaxDecorPerCell = 16;

	// A sprite's size in metres at 32 pixels per metre (0 when it isn't loaded).
	static glm::vec2 GetSpriteSize(AssetHandle sprite)
	{
		if ((uint64_t)sprite == 0)
		{
			return glm::vec2(0.0f);
		}
		const Ref<Texture2D> texture = AssetManager::GetLoadedAsset<Texture2D>(sprite);
		if (texture == nullptr)
		{
			return glm::vec2(0.0f);
		}
		return glm::vec2((float)texture->GetWidth(), (float)texture->GetHeight()) / TerrainArtPixelsPerMetre;
	}

	bool TerrainDecor::AreSpritesLoaded(const Terrain& terrain)
	{
		for (const TerrainDecorRule& rule : terrain.GetSettings().Decor)
		{
			for (AssetHandle sprite : rule.Sprites)
			{
				if ((uint64_t)sprite != 0 && AssetManager::GetLoadedAsset<Texture2D>(sprite) == nullptr)
				{
					return false;
				}
			}
		}
		return true;
	}

	// One instance: rule `ruleIndex`'s sprite picked by `pick`, scaled by `size01` within the rule's range, at `position`
	// (z = the ground), mirrored or turned by `turn`.
	static void AddInstance(const TerrainDecorRule& rule, uint32_t ruleIndex, const glm::vec3& position, float pick, float size01, float turn,
		TerrainMeshBuilder& parts)
	{
		const uint32_t count = std::min((uint32_t)rule.Sprites.size(), MaxTerrainDecorSprites);
		if (count == 0)
		{
			return;
		}
		const uint32_t sprite = std::min((uint32_t)(pick * (float)count), count - 1);
		const glm::vec2 native = GetSpriteSize(rule.Sprites[sprite]);
		if (native.x <= 0.0f || native.y <= 0.0f)
		{
			return;
		}
		const float scale = rule.Scale.x + (rule.Scale.y - rule.Scale.x) * size01;
		const glm::vec2 size = native * std::max(scale, 0.01f);
		const uint32_t slot = TerrainMesher::GetPartSlot(TerrainPartKind::Decor, ruleIndex * 256 + sprite);
		const glm::vec3 up(0.0f, 0.0f, 1.0f);

		switch (rule.Shape)
		{
			case TerrainDecorShape::Upright:
			{
				// Facing south (towards the field cameras), standing on the ground; lit like the ground under it.
				const float half = 0.5f * size.x;
				const glm::vec3 p0(position.x - half, position.y, position.z - 0.01f);
				const glm::vec3 p1(position.x + half, position.y, position.z - 0.01f);
				const glm::vec3 p2(position.x + half, position.y, position.z + size.y);
				const glm::vec3 p3(position.x - half, position.y, position.z + size.y);
				const bool mirror = turn < 0.5f;
				const float u0 = mirror ? 1.0f : 0.0f;
				const float u1 = mirror ? 0.0f : 1.0f;
				parts.Quad(slot, p0, p1, p2, p3, glm::vec2(u0, 1.0f), glm::vec2(u1, 1.0f), glm::vec2(u1, 0.0f), glm::vec2(u0, 0.0f), &up);
				break;
			}
			case TerrainDecorShape::Flat:
			{
				const float angle = turn * glm::two_pi<float>();
				const glm::vec2 ax = glm::vec2(std::cos(angle), std::sin(angle)) * (0.5f * size.x);
				const glm::vec2 ay = glm::vec2(-std::sin(angle), std::cos(angle)) * (0.5f * size.y);
				const float z = position.z + 0.004f;
				const glm::vec3 p0(position.x - ax.x - ay.x, position.y - ax.y - ay.y, z);
				const glm::vec3 p1(position.x + ax.x - ay.x, position.y + ax.y - ay.y, z);
				const glm::vec3 p2(position.x + ax.x + ay.x, position.y + ax.y + ay.y, z);
				const glm::vec3 p3(position.x - ax.x + ay.x, position.y - ax.y + ay.y, z);
				parts.Quad(slot, p0, p1, p2, p3, glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f), &up);
				break;
			}
			case TerrainDecorShape::Stone:
			{
				const float width = size.x;
				parts.Box(slot, position, glm::vec2(1.0f, 0.0f), turn * glm::pi<float>(), width, width * 0.75f, std::max(size.y * 0.5f, 0.02f));
				break;
			}
		}
	}

	// Whether the top layer at `position` is `layer` and, for a border, another layer wins close by.
	static bool LayerMatches(const Terrain& terrain, const TerrainDecorRule& rule, const glm::vec2& position)
	{
		if (TerrainMesher::GetTopLayerAt(terrain, position) != rule.Layer)
		{
			return false;
		}
		if (rule.Place != TerrainDecorPlace::LayerBorder)
		{
			return true;
		}
		const float reach = 0.12f;
		const glm::vec2 around[4] = { { reach, 0.0f }, { -reach, 0.0f }, { 0.0f, reach }, { 0.0f, -reach } };
		for (const glm::vec2& offset : around)
		{
			if (TerrainMesher::GetTopLayerAt(terrain, position + offset) != rule.Layer)
			{
				return true;
			}
		}
		return false;
	}

	void TerrainDecor::AddChunk(const Terrain& terrain, const TerrainMesher& mesher, uint32_t chunkX, uint32_t chunkY, TerrainMeshBuilder& parts)
	{
		const TerrainSettings& settings = terrain.GetSettings();
		const uint32_t minX = chunkX * TerrainChunkCells;
		const uint32_t minY = chunkY * TerrainChunkCells;
		const uint32_t maxX = std::min(minX + TerrainChunkCells, terrain.GetCellCountX());
		const uint32_t maxY = std::min(minY + TerrainChunkCells, terrain.GetCellCountY());
		const float cell = settings.CellSize;
		const std::vector<TerrainMesher::WallSegment>& segments = mesher.GetWallSegments();

		const uint32_t ruleCount = std::min((uint32_t)settings.Decor.size(), MaxTerrainDecorRules);
		for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++)
		{
			const TerrainDecorRule& rule = settings.Decor[ruleIndex];
			if (rule.Sprites.empty() || rule.Density <= 0.0f)
			{
				continue;
			}

			if (rule.Place == TerrainDecorPlace::Layer || rule.Place == TerrainDecorPlace::LayerBorder)
			{
				// A few candidates per cell, each kept with the chance that gives the density, at hashed positions.
				const float expected = rule.Density * cell * cell;
				const uint32_t candidates = std::min((uint32_t)std::ceil(expected), MaxDecorPerCell);
				const float keep = candidates > 0 ? expected / (float)candidates : 0.0f;
				for (uint32_t j = minY; j < maxY; j++)
				{
					for (uint32_t i = minX; i < maxX; i++)
					{
						for (uint32_t k = 0; k < candidates; k++)
						{
							float r[6];
							for (int n = 0; n < 6; n++)
							{
								r[n] = TerrainMesher::Hash01({ settings.Seed, i, j, ruleIndex, k * 8 + n, TagDecor });
							}
							if (r[0] >= keep)
							{
								continue;
							}
							const glm::vec2 position(((float)i + r[1]) * cell, ((float)j + r[2]) * cell);
							if (LayerMatches(terrain, rule, position) == false)
							{
								continue;
							}
							AddInstance(rule, ruleIndex, glm::vec3(position, mesher.GetHeightAt(position)), r[3], r[4], r[5], parts);
						}
					}
				}
				continue;
			}

			// Along walls: slots every 1 / Density metres of outline distance, so instances don't depend on the cell.
			const float spacing = 1.0f / rule.Density;
			for (uint32_t j = minY; j < maxY; j++)
			{
				for (uint32_t i = minX; i < maxX; i++)
				{
					uint32_t first = 0;
					uint32_t end = 0;
					mesher.GetCellSegments(i, j, first, end);
					for (uint32_t s = first; s < end; s++)
					{
						const TerrainMesher::WallSegment& segment = segments[s];
						const bool top = rule.Place == TerrainDecorPlace::WallTop;
						if ((top && (segment.IsTop == false || segment.Material == TerrainWallHidden))
							|| (rule.Layer != TerrainDecorAnyLayer && segment.Material != rule.Layer))
						{
							continue;
						}
						glm::vec3 a;
						glm::vec3 b;
						mesher.GetSegmentLine(segment, top ? segment.Level : segment.Level - 1, a, b);
						const glm::vec2 edge(b.x - a.x, b.y - a.y);
						const float length = glm::length(edge);
						if (length < 1e-5f)
						{
							continue;
						}
						const glm::vec2 inward(-edge.y / length, edge.x / length); // left of A -> B: the upper shelf
						const int64_t firstSlot = (int64_t)std::ceil(segment.U / spacing - 0.5f);
						const int64_t lastSlot = (int64_t)std::floor((segment.U + length) / spacing - 0.5f);
						for (int64_t n = firstSlot; n <= lastSlot; n++)
						{
							float r[6];
							for (int m = 0; m < 6; m++)
							{
								r[m] = TerrainMesher::Hash01({ settings.Seed, n, segment.Level, ruleIndex, m, TagDecor + 1 });
							}
							const float along = ((float)n + 0.5f) * spacing + (r[0] - 0.5f) * spacing * 0.8f;
							const float f = (along - segment.U) / length;
							if (f < 0.0f || f > 1.0f)
							{
								continue;
							}
							const glm::vec2 onLine(a.x + edge.x * f, a.y + edge.y * f);
							glm::vec2 position;
							if (top)
							{
								position = onLine + inward * (0.06f + r[1] * 0.1f);
							}
							else
							{
								// On the lower ground, only where it is ground (not a ledge of a taller wall).
								position = onLine - inward * (0.04f + r[1] * 0.1f);
								if (std::abs(mesher.GetHeightAt(position) - a.z) > 0.02f)
								{
									continue;
								}
							}
							AddInstance(rule, ruleIndex, glm::vec3(position, top ? a.z : mesher.GetHeightAt(position)), r[2], r[3], r[4], parts);
						}
					}
				}
			}
		}
	}
}
