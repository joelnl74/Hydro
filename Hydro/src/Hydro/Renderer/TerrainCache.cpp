#include "hypch.h"
#include "TerrainCache.h"

#include "Hydro/Core/Time.h"
#include "Hydro/Terrain/TerrainMesher.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Hydro
{
	struct TerrainChunkEntry
	{
		uint64_t GeometryRevision = 0; // the terrain's revision it was built from (0 = never)
		std::vector<glm::vec3> Positions; // for Raycast
		std::vector<uint32_t> Indices;
	};

	struct TerrainCacheEntry
	{
		const Terrain* Key = nullptr;
		std::weak_ptr<Terrain> Owner; // the key's terrain is gone (or replaced by a reload) when this expires
		TerrainRenderData Data;
		std::vector<TerrainChunkEntry> Chunks;
		bool StepsReported = false;
		uint64_t LastUsedFrame = 0;
	};

	struct TerrainCacheData
	{
		std::vector<Scope<TerrainCacheEntry>> Entries;
	};

	static TerrainCacheData* s_TerrainCache = nullptr;

	// Terrains no frame has prepared for this many frames are dropped.
	static constexpr uint64_t UnusedTerrainFrames = 300;

	void TerrainCache::Init()
	{
		HY_CORE_ASSERT(s_TerrainCache == nullptr, "TerrainCache::Init called twice");
		s_TerrainCache = new TerrainCacheData();
	}

	void TerrainCache::Shutdown()
	{
		if (s_TerrainCache == nullptr)
		{
			return;
		}

		// Textures and meshes free themselves through the deletion queue, which Renderer::Shutdown flushes next.
		delete s_TerrainCache;
		s_TerrainCache = nullptr;
	}

	static Ref<Texture2D> CreateWeightTexture(const Terrain& terrain)
	{
		ImageSpecification specification;
		specification.Width = terrain.GetWeightMapWidth();
		specification.Height = terrain.GetWeightMapHeight();
		specification.Format = ImageFormat::RGBA; // data, not colour
		TextureProperties properties;
		properties.SamplerFilter = TextureFilter::Linear;
		properties.SamplerWrap = TextureWrap::Clamp;
		return CreateRef<Texture2D>(specification, terrain.GetWeights().data(), properties);
	}

	static void UploadWeights(TerrainCacheEntry& entry, Terrain& terrain)
	{
		TerrainRect texels;
		if (terrain.TakeChangedWeights(texels) == false || entry.Data.Weights == nullptr)
		{
			return;
		}

		const uint32_t width = texels.Max.x - texels.Min.x + 1;
		const uint32_t height = texels.Max.y - texels.Min.y + 1;
		std::vector<uint8_t> pixels((size_t)width * height * 4);
		const std::vector<uint8_t>& weights = terrain.GetWeights();
		const size_t stride = (size_t)terrain.GetWeightMapWidth() * 4;
		for (uint32_t y = 0; y < height; y++)
		{
			const size_t from = (size_t)(texels.Min.y + y) * stride + (size_t)texels.Min.x * 4;
			std::copy(weights.begin() + from, weights.begin() + from + (size_t)width * 4, pixels.begin() + (size_t)y * width * 4);
		}
		entry.Data.Weights->SetData(texels.Min.x, texels.Min.y, width, height, pixels.data());
	}

	static void RebuildChunks(TerrainCacheEntry& entry, const Terrain& terrain)
	{
		const uint32_t chunksX = terrain.GetChunkCountX();
		const uint32_t chunkCount = chunksX * terrain.GetChunkCountY();
		if (entry.Chunks.size() != chunkCount)
		{
			entry.Chunks.assign(chunkCount, TerrainChunkEntry{});
			entry.Data.Chunks.assign(chunkCount, TerrainChunkDraw{});
		}

		bool hasSteps = false;
		for (uint32_t chunk = 0; chunk < chunkCount; chunk++)
		{
			TerrainChunkEntry& cached = entry.Chunks[chunk];
			const uint64_t revision = terrain.GetGeometryRevision(chunk);
			if (cached.GeometryRevision == revision)
			{
				continue;
			}

			bool chunkSteps = false;
			MeshData data = TerrainMesher::BuildChunkTops(terrain, chunk % chunksX, chunk / chunksX, chunkSteps);
			hasSteps |= chunkSteps;

			TerrainChunkDraw& draw = entry.Data.Chunks[chunk];
			draw.BoundsMin = glm::vec3(std::numeric_limits<float>::max());
			draw.BoundsMax = glm::vec3(std::numeric_limits<float>::lowest());
			cached.Positions.clear();
			cached.Positions.reserve(data.Vertices.size());
			for (const MeshVertex& vertex : data.Vertices)
			{
				cached.Positions.push_back(vertex.Position);
				draw.BoundsMin = glm::min(draw.BoundsMin, vertex.Position);
				draw.BoundsMax = glm::max(draw.BoundsMax, vertex.Position);
			}
			cached.Indices = data.Indices;
			// The old mesh (frames in flight may still draw it) goes through the deletion queue.
			draw.Tops = data.Indices.empty() ? nullptr : CreateRef<Mesh>(data, "Terrain chunk " + std::to_string(chunk));
			cached.GeometryRevision = revision;
		}

		if (hasSteps && entry.StepsReported == false)
		{
			HY_CORE_WARN("TerrainCache: a terrain has cells whose corners are at different levels; they are drawn flat until terraces are meshed (Terrain plan TR2)");
			entry.StepsReported = true;
		}
	}

	static TerrainCacheEntry* FindEntry(const Ref<Terrain>& terrain)
	{
		for (Scope<TerrainCacheEntry>& entry : s_TerrainCache->Entries)
		{
			if (entry->Key == terrain.get() && entry->Owner.expired() == false)
			{
				return entry.get();
			}
		}
		return nullptr;
	}

	const TerrainRenderData* TerrainCache::Prepare(const Ref<Terrain>& terrain)
	{
		if (s_TerrainCache == nullptr || terrain == nullptr)
		{
			return nullptr;
		}

		TerrainCacheEntry* entry = FindEntry(terrain);
		if (entry == nullptr)
		{
			Scope<TerrainCacheEntry> created = CreateScope<TerrainCacheEntry>();
			created->Key = terrain.get();
			created->Owner = terrain;
			created->Data.Weights = CreateWeightTexture(*terrain);
			TerrainRect ignored;
			terrain->TakeChangedWeights(ignored); // the new texture has every texel already
			s_TerrainCache->Entries.push_back(std::move(created));
			entry = s_TerrainCache->Entries.back().get();
		}

		const glm::uvec2 weightSize(terrain->GetWeightMapWidth(), terrain->GetWeightMapHeight());
		if (entry->Data.Weights == nullptr || entry->Data.Weights->GetWidth() != weightSize.x || entry->Data.Weights->GetHeight() != weightSize.y)
		{
			entry->Data.Weights = CreateWeightTexture(*terrain);
			TerrainRect ignored;
			terrain->TakeChangedWeights(ignored);
		}
		UploadWeights(*entry, *terrain);
		RebuildChunks(*entry, *terrain);
		entry->LastUsedFrame = Time::GetFrameCount();
		return &entry->Data;
	}

	// Whether a ray meets a box before `limit` (in units of `direction`). (Not "near" / "far": windows.h defines those.)
	static bool RayHitsBox(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& low, const glm::vec3& high, float limit)
	{
		float enter = 0.0f;
		float leave = limit;
		for (int axis = 0; axis < 3; axis++)
		{
			if (std::abs(direction[axis]) < 1e-12f)
			{
				if (origin[axis] < low[axis] || origin[axis] > high[axis])
				{
					return false;
				}
				continue;
			}
			float t0 = (low[axis] - origin[axis]) / direction[axis];
			float t1 = (high[axis] - origin[axis]) / direction[axis];
			if (t0 > t1)
			{
				std::swap(t0, t1);
			}
			enter = std::max(enter, t0);
			leave = std::min(leave, t1);
			if (enter > leave)
			{
				return false;
			}
		}
		return true;
	}

	// Möller-Trumbore, both faces.
	static bool RayHitsTriangle(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t)
	{
		const glm::vec3 edge1 = b - a;
		const glm::vec3 edge2 = c - a;
		const glm::vec3 p = glm::cross(direction, edge2);
		const float determinant = glm::dot(edge1, p);
		if (std::abs(determinant) < 1e-12f)
		{
			return false;
		}
		const float inverse = 1.0f / determinant;
		const glm::vec3 s = origin - a;
		const float u = glm::dot(s, p) * inverse;
		if (u < 0.0f || u > 1.0f)
		{
			return false;
		}
		const glm::vec3 q = glm::cross(s, edge1);
		const float v = glm::dot(direction, q) * inverse;
		if (v < 0.0f || u + v > 1.0f)
		{
			return false;
		}
		t = glm::dot(edge2, q) * inverse;
		return t >= 0.0f;
	}

	bool TerrainCache::Raycast(const Ref<Terrain>& terrain, const glm::vec3& origin, const glm::vec3& direction, float& distance)
	{
		if (Prepare(terrain) == nullptr)
		{
			return false;
		}
		const TerrainCacheEntry* entry = FindEntry(terrain);

		float best = std::numeric_limits<float>::max();
		for (size_t chunk = 0; chunk < entry->Chunks.size(); chunk++)
		{
			const TerrainChunkDraw& draw = entry->Data.Chunks[chunk];
			if (draw.Tops == nullptr || RayHitsBox(origin, direction, draw.BoundsMin - glm::vec3(1e-3f), draw.BoundsMax + glm::vec3(1e-3f), best) == false)
			{
				continue;
			}
			const TerrainChunkEntry& cached = entry->Chunks[chunk];
			for (size_t i = 0; i + 2 < cached.Indices.size(); i += 3)
			{
				float t = 0.0f;
				if (RayHitsTriangle(origin, direction, cached.Positions[cached.Indices[i]], cached.Positions[cached.Indices[i + 1]],
					cached.Positions[cached.Indices[i + 2]], t) && t < best)
				{
					best = t;
				}
			}
		}

		if (best == std::numeric_limits<float>::max())
		{
			return false;
		}
		distance = best;
		return true;
	}

	void TerrainCache::EndFrame()
	{
		if (s_TerrainCache == nullptr)
		{
			return;
		}

		const uint64_t frame = Time::GetFrameCount();
		std::vector<Scope<TerrainCacheEntry>>& entries = s_TerrainCache->Entries;
		entries.erase(std::remove_if(entries.begin(), entries.end(), [frame](const Scope<TerrainCacheEntry>& entry)
		{
			return entry->Owner.expired() || entry->LastUsedFrame + UnusedTerrainFrames < frame;
		}), entries.end());
	}

	uint32_t TerrainCache::GetCachedCount()
	{
		return s_TerrainCache != nullptr ? (uint32_t)s_TerrainCache->Entries.size() : 0;
	}
}
