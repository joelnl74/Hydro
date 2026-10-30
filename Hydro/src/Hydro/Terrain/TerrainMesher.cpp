#include "hypch.h"
#include "TerrainMesher.h"

#include "Hydro/Terrain/TerrainDecor.h"
#include "Hydro/Terrain/TerrainMeshBuilder.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_set>

namespace Hydro
{
	// Hash tags, as in scripts/terrain/terrain_mesher.py, so the different uses never correlate.
	static constexpr int64_t TagCross = 1;
	static constexpr int64_t TagWallNoise = 2;
	static constexpr int64_t TagCap = 3;
	static constexpr int64_t TagSplatCluster = 4;
	static constexpr int64_t TagSplatPixel = 5;

	static constexpr uint32_t EdgeH = 0; // corner (i, j) to (i + 1, j)
	static constexpr uint32_t EdgeV = 1; // corner (i, j) to (i, j + 1)

	static constexpr uint64_t KeyCorner = 0;
	static constexpr uint64_t KeyEdgeH = 1;
	static constexpr uint64_t KeyEdgeV = 2;

	static uint64_t MakeKey(uint64_t kind, int64_t i, int64_t j)
	{
		return (kind << 60) | ((uint64_t)(i & 0x3FFFFFFF) << 30) | (uint64_t)(j & 0x3FFFFFFF);
	}

	static uint64_t KeyKind(uint64_t key) { return key >> 60; }
	static int64_t KeyI(uint64_t key) { return (int64_t)((key >> 30) & 0x3FFFFFFF); }
	static int64_t KeyJ(uint64_t key) { return (int64_t)(key & 0x3FFFFFFF); }
	static uint32_t KeyOrient(uint64_t key) { return KeyKind(key) == KeyEdgeH ? EdgeH : EdgeV; }

	// ---- Hashes and noise -----------------------------------------------------------------------------------

	uint32_t TerrainMesher::Hash(std::initializer_list<int64_t> values)
	{
		uint32_t h = 0x811C9DC5u;
		for (int64_t value : values)
		{
			h ^= (uint32_t)((uint64_t)value & 0xFFFFFFFFull);
			h *= 0x01000193u;
			h ^= h >> 15;
			h *= 0x2C1B3C6Du;
			h ^= h >> 12;
			h *= 0x297A2D39u;
			h ^= h >> 15;
		}
		return h;
	}

	float TerrainMesher::Hash01(std::initializer_list<int64_t> values)
	{
		return (float)((double)Hash(values) / 4294967296.0);
	}

	static double HashDouble(std::initializer_list<int64_t> values)
	{
		return (double)TerrainMesher::Hash(values) / 4294967296.0;
	}

	// terrain_mesher.smooth_noise: value noise 0..1 over metres, a lattice every `scale` metres, smoothstep between.
	static double SmoothNoise(uint32_t seed, double x, double y, double scale, int64_t tag)
	{
		const double gx = x / scale;
		const double gy = y / scale;
		const double x0 = std::floor(gx);
		const double y0 = std::floor(gy);
		double fx = gx - x0;
		double fy = gy - y0;
		fx = fx * fx * (3.0 - 2.0 * fx);
		fy = fy * fy * (3.0 - 2.0 * fy);
		const int64_t ix = (int64_t)x0;
		const int64_t iy = (int64_t)y0;
		const double v00 = HashDouble({ seed, ix, iy, tag });
		const double v10 = HashDouble({ seed, ix + 1, iy, tag });
		const double v01 = HashDouble({ seed, ix, iy + 1, tag });
		const double v11 = HashDouble({ seed, ix + 1, iy + 1, tag });
		return (v00 * (1.0 - fx) + v10 * fx) * (1.0 - fy) + (v01 * (1.0 - fx) + v11 * fx) * fy;
	}

	uint32_t TerrainMesher::GetTopLayerAt(const Terrain& terrain, const glm::vec2& position)
	{
		const TerrainSettings& settings = terrain.GetSettings();
		const uint32_t layerCount = std::min((uint32_t)settings.Layers.size(), MaxTerrainLayers);
		if (layerCount <= 1)
		{
			return 0;
		}

		// 1. The art pixel and its centre.
		const int64_t px = (int64_t)std::floor((double)position.x * TerrainArtPixelsPerMetre);
		const int64_t py = (int64_t)std::floor((double)position.y * TerrainArtPixelsPerMetre);
		const double cx = ((double)px + 0.5) / TerrainArtPixelsPerMetre;
		const double cy = ((double)py + 0.5) / TerrainArtPixelsPerMetre;

		// 2. The weights there, filtered between texel centres and clamped like the GPU's sampler.
		const double width = (double)terrain.GetWeightMapWidth();
		const double height = (double)terrain.GetWeightMapHeight();
		const double perMetre = (double)std::max(settings.SplatResolution, 1u);
		const double fx = std::min(std::max(cx * perMetre - 0.5, 0.0), width - 1.0);
		const double fy = std::min(std::max(cy * perMetre - 0.5, 0.0), height - 1.0);
		const uint32_t x0 = (uint32_t)fx;
		const uint32_t y0 = (uint32_t)fy;
		const uint32_t x1 = std::min(x0 + 1, terrain.GetWeightMapWidth() - 1);
		const uint32_t y1 = std::min(y0 + 1, terrain.GetWeightMapHeight() - 1);
		const double ax = fx - (double)x0;
		const double ay = fy - (double)y0;
		const glm::dvec4 w00 = glm::dvec4(terrain.GetWeight(x0, y0));
		const glm::dvec4 w10 = glm::dvec4(terrain.GetWeight(x1, y0));
		const glm::dvec4 w01 = glm::dvec4(terrain.GetWeight(x0, y1));
		const glm::dvec4 w11 = glm::dvec4(terrain.GetWeight(x1, y1));
		const glm::dvec4 weights = ((w00 * (1.0 - ax) + w10 * ax) * (1.0 - ay) + (w01 * (1.0 - ax) + w11 * ax) * ay) / 255.0;

		// 3. Weight plus clustered edge noise; the highest wins (the first on a tie).
		const double cluster = std::max((double)settings.EdgeClusterSize, 0.5);
		const double pixelNoise = std::min(std::max((double)settings.EdgePixelNoise, 0.0), 1.0);
		const double gx = (double)px / cluster;
		const double gy = (double)py / cluster;
		const double cellX = std::floor(gx);
		const double cellY = std::floor(gy);
		double sx = gx - cellX;
		double sy = gy - cellY;
		sx = sx * sx * (3.0 - 2.0 * sx);
		sy = sy * sy * (3.0 - 2.0 * sy);
		const int64_t ix = (int64_t)cellX;
		const int64_t iy = (int64_t)cellY;

		uint32_t best = 0;
		double bestScore = -1.0e9;
		for (uint32_t layer = 0; layer < layerCount; layer++)
		{
			const double v00 = HashDouble({ settings.Seed, ix, iy, layer, TagSplatCluster });
			const double v10 = HashDouble({ settings.Seed, ix + 1, iy, layer, TagSplatCluster });
			const double v01 = HashDouble({ settings.Seed, ix, iy + 1, layer, TagSplatCluster });
			const double v11 = HashDouble({ settings.Seed, ix + 1, iy + 1, layer, TagSplatCluster });
			const double clustered = (v00 * (1.0 - sx) + v10 * sx) * (1.0 - sy) + (v01 * (1.0 - sx) + v11 * sx) * sy;
			const double single = HashDouble({ settings.Seed, px, py, layer, TagSplatPixel });
			const double noise = (1.0 - pixelNoise) * clustered + pixelNoise * single;
			const double score = weights[(int)layer] + (noise - 0.5) * (double)settings.Layers[layer].EdgeRoughness;
			if (score > bestScore)
			{
				bestScore = score;
				best = layer;
			}
		}
		return best;
	}

	// ---- Analysis -------------------------------------------------------------------------------------------

	TerrainMesher::TerrainMesher(const Terrain& terrain)
		: m_Terrain(terrain), m_CellsX(terrain.GetCellCountX()), m_CellsY(terrain.GetCellCountY())
	{
		FindRampRuns();
		CollectWallSegments();
		ComputeOutlineDistances();
	}

	int32_t TerrainMesher::Level(int64_t i, int64_t j) const
	{
		return (int32_t)m_Terrain.GetLevel((uint32_t)i, (uint32_t)j);
	}

	float TerrainMesher::LevelZ(float level) const
	{
		const TerrainSettings& settings = m_Terrain.GetSettings();
		return (level - (float)settings.BaseLevel) * settings.StepHeight;
	}

	uint32_t TerrainMesher::CellCycle(uint32_t i, uint32_t j, BoundaryPoint* points) const
	{
		// The cell's boundary counter-clockwise from its south-west corner. A crossing point is only there when its
		// edge's corners differ.
		const struct { uint64_t Kind; int64_t I; int64_t J; } items[8] = {
			{ KeyCorner, i, j }, { KeyEdgeH, i, j },
			{ KeyCorner, i + 1, j }, { KeyEdgeV, i + 1, j },
			{ KeyCorner, i + 1, j + 1 }, { KeyEdgeH, i, j + 1 },
			{ KeyCorner, i, j + 1 }, { KeyEdgeV, i, j }
		};
		uint32_t count = 0;
		for (uint8_t slot = 0; slot < 8; slot++)
		{
			const auto& item = items[slot];
			BoundaryPoint point;
			point.Key = MakeKey(item.Kind, item.I, item.J);
			point.Slot = slot;
			if (item.Kind == KeyCorner)
			{
				point.Lo = point.Hi = Level(item.I, item.J);
			}
			else
			{
				const int32_t a = Level(item.I, item.J);
				const int32_t b = item.Kind == KeyEdgeH ? Level(item.I + 1, item.J) : Level(item.I, item.J + 1);
				if (a == b)
				{
					continue;
				}
				point.Lo = std::min(a, b);
				point.Hi = std::max(a, b);
			}
			points[count++] = point;
		}
		return count;
	}

	float TerrainMesher::EdgeVariation(uint32_t orient, int64_t i, int64_t j) const
	{
		// The calmer of the two cells the edge borders, by their wall layer's Variation.
		const std::vector<TerrainWallLayer>& layers = m_Terrain.GetSettings().WallLayers;
		if (layers.empty())
		{
			return 1.0f;
		}
		const int64_t cells[2][2] = { { i, j }, { orient == EdgeH ? i : i - 1, orient == EdgeH ? j - 1 : j } };
		float result = 1.0f;
		bool any = false;
		for (const auto& cell : cells)
		{
			if (cell[0] < 0 || cell[1] < 0 || cell[0] >= (int64_t)m_CellsX || cell[1] >= (int64_t)m_CellsY)
			{
				continue;
			}
			const uint8_t material = m_Terrain.GetWallMaterial((uint32_t)cell[0], (uint32_t)cell[1]);
			const float value = material < layers.size() ? layers[material].Variation : 1.0f;
			result = any ? std::min(result, value) : value;
			any = true;
		}
		return any ? result : 1.0f;
	}

	float TerrainMesher::CrossingParam(uint32_t orient, int64_t i, int64_t j) const
	{
		// Keyed by the canonical edge ID only: a smooth field sampled at the edge's midpoint (outlines wander instead of
		// zigzagging) plus a little per-edge noise.
		const TerrainSettings& settings = m_Terrain.GetSettings();
		const double jitter = std::min(std::max((double)settings.Jitter * (double)EdgeVariation(orient, i, j), 0.0), 0.45);
		const double mx = ((double)i + (orient == EdgeH ? 0.5 : 0.0)) * (double)settings.CellSize;
		const double my = ((double)j + (orient == EdgeV ? 0.5 : 0.0)) * (double)settings.CellSize;
		const double smooth = SmoothNoise(settings.Seed, mx, my, (double)settings.JitterScale, TagCross * 16 + orient);
		const double single = HashDouble({ settings.Seed, i, j, orient, TagCross });
		const double v = (double)settings.JitterSmooth * smooth + (1.0 - (double)settings.JitterSmooth) * single;
		return (float)(0.5 + jitter * (2.0 * v - 1.0));
	}

	glm::vec3 TerrainMesher::EdgePoint(uint32_t orient, int64_t i, int64_t j, int32_t level) const
	{
		// The crossing at `level`, moved along the edge towards its lower corner by the wall's lean and noise below
		// the edge's top. Each course steps out by the lean plus a random 0..noise: the wall never undercuts.
		const TerrainSettings& settings = m_Terrain.GetSettings();
		const int64_t ib = orient == EdgeH ? i + 1 : i;
		const int64_t jb = orient == EdgeH ? j : j + 1;
		const int32_t la = Level(i, j);
		const int32_t lb = Level(ib, jb);
		const int32_t high = std::max(la, lb);
		double t = (double)CrossingParam(orient, i, j);
		if (high > level)
		{
			const double noise = (double)settings.WallNoise * (double)EdgeVariation(orient, i, j);
			double offset = 0.0;
			for (int32_t course = level; course < high; course++)
			{
				offset += (double)settings.WallLean / TerrainArtPixelsPerMetre * (double)settings.StepHeight;
				if (noise > 0.0)
				{
					offset += noise / TerrainArtPixelsPerMetre * HashDouble({ settings.Seed, i, j, orient, course, TagWallNoise });
				}
			}
			const double dt = offset / (double)settings.CellSize;
			t = la < lb ? t - dt : t + dt;
			t = std::min(std::max(t, 0.05), 0.95);
		}
		const double x0 = (double)i * settings.CellSize;
		const double y0 = (double)j * settings.CellSize;
		const double x1 = (double)ib * settings.CellSize;
		const double y1 = (double)jb * settings.CellSize;
		return glm::vec3((float)(x0 + (x1 - x0) * t), (float)(y0 + (y1 - y0) * t), LevelZ((float)level));
	}

	glm::vec2 TerrainMesher::EdgeDownDirection(uint32_t orient, int64_t i, int64_t j) const
	{
		// Along the edge from its higher corner to its lower one: the outward of a wall there.
		glm::vec2 d = orient == EdgeH ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f);
		const int32_t la = Level(i, j);
		const int32_t lb = orient == EdgeH ? Level(i + 1, j) : Level(i, j + 1);
		return la < lb ? -d : d;
	}

	glm::vec3 TerrainMesher::PointAt(const BoundaryPoint& point, int32_t level) const
	{
		if (KeyKind(point.Key) == KeyCorner)
		{
			const float cell = m_Terrain.GetSettings().CellSize;
			return glm::vec3((float)KeyI(point.Key) * cell, (float)KeyJ(point.Key) * cell, LevelZ((float)level));
		}
		return EdgePoint(KeyOrient(point.Key), KeyI(point.Key), KeyJ(point.Key), level);
	}

	static bool SameSide(const TerrainMesher::BoundaryPoint& a, const TerrainMesher::BoundaryPoint& b)
	{
		for (uint32_t side = 0; side < 4; side++)
		{
			const uint32_t s0 = 2 * side;
			const uint32_t s1 = 2 * side + 1;
			const uint32_t s2 = (2 * side + 2) % 8;
			const bool aIn = a.Slot == s0 || a.Slot == s1 || a.Slot == s2;
			const bool bIn = b.Slot == s0 || b.Slot == s1 || b.Slot == s2;
			if (aIn && bIn)
			{
				return true;
			}
		}
		return false;
	}

	// Region(>= level) in a cell: the cycle's points at that level or higher, in order.
	static uint32_t Region(const TerrainMesher::BoundaryPoint* cycle, uint32_t count, int32_t level, TerrainMesher::BoundaryPoint* out)
	{
		uint32_t n = 0;
		for (uint32_t k = 0; k < count; k++)
		{
			if (cycle[k].Hi >= level)
			{
				out[n++] = cycle[k];
			}
		}
		return n;
	}

	// The wall chords of Region(>= level): consecutive region points not on the same side of the cell.
	static void Chords(const TerrainMesher::BoundaryPoint* cycle, uint32_t count, int32_t level,
		std::vector<std::pair<TerrainMesher::BoundaryPoint, TerrainMesher::BoundaryPoint>>& out)
	{
		out.clear();
		TerrainMesher::BoundaryPoint region[8];
		const uint32_t n = Region(cycle, count, level, region);
		if (n == count || n == 0)
		{
			return;
		}
		for (uint32_t k = 0; k < n; k++)
		{
			const TerrainMesher::BoundaryPoint& a = region[k];
			const TerrainMesher::BoundaryPoint& b = region[(k + 1) % n];
			if (SameSide(a, b) == false)
			{
				out.emplace_back(a, b);
			}
		}
	}

	void TerrainMesher::FindRampRuns()
	{
		// A run is consecutive cells with the same direction; its height goes linearly from the low edge (all corners
		// one level) to the high edge (all corners one higher level). The surface comes only from the run's two ends:
		// the levels on a ramp's side corners belong to the neighbouring ground.
		m_RampCells.assign((size_t)m_CellsX * m_CellsY, RampCell{});
		std::vector<uint8_t> seen((size_t)m_CellsX * m_CellsY, 0);
		const int64_t nx = (int64_t)m_CellsX;
		const int64_t ny = (int64_t)m_CellsY;
		auto rampAt = [&](int64_t i, int64_t j) { return (uint8_t)m_Terrain.GetRamp((uint32_t)i, (uint32_t)j); };

		for (int64_t j = 0; j < ny; j++)
		{
			for (int64_t i = 0; i < nx; i++)
			{
				const uint8_t d = rampAt(i, j);
				if (d == (uint8_t)TerrainRamp::None || d > (uint8_t)TerrainRamp::West || seen[(size_t)j * nx + i] != 0)
				{
					continue;
				}
				const int64_t ax = d == (uint8_t)TerrainRamp::East ? 1 : (d == (uint8_t)TerrainRamp::West ? -1 : 0);
				const int64_t ay = d == (uint8_t)TerrainRamp::North ? 1 : (d == (uint8_t)TerrainRamp::South ? -1 : 0);

				// Back to the run's first cell (the low end), then forward.
				int64_t si = i;
				int64_t sj = j;
				while (si - ax >= 0 && si - ax < nx && sj - ay >= 0 && sj - ay < ny && rampAt(si - ax, sj - ay) == d)
				{
					si -= ax;
					sj -= ay;
				}
				std::vector<glm::i64vec2> cells;
				int64_t ci = si;
				int64_t cj = sj;
				while (ci >= 0 && ci < nx && cj >= 0 && cj < ny && rampAt(ci, cj) == d)
				{
					cells.emplace_back(ci, cj);
					ci += ax;
					cj += ay;
				}
				for (const glm::i64vec2& c : cells)
				{
					seen[(size_t)c.y * nx + c.x] = 1;
				}

				// The two corners of a cell's low or high edge.
				auto edgeCorners = [d](const glm::i64vec2& c, bool high, glm::i64vec2 out[2])
				{
					if (d == (uint8_t)TerrainRamp::North)
					{
						const int64_t row = high ? c.y + 1 : c.y;
						out[0] = { c.x, row };
						out[1] = { c.x + 1, row };
					}
					else if (d == (uint8_t)TerrainRamp::South)
					{
						const int64_t row = high ? c.y : c.y + 1;
						out[0] = { c.x, row };
						out[1] = { c.x + 1, row };
					}
					else if (d == (uint8_t)TerrainRamp::East)
					{
						const int64_t col = high ? c.x + 1 : c.x;
						out[0] = { col, c.y };
						out[1] = { col, c.y + 1 };
					}
					else
					{
						const int64_t col = high ? c.x : c.x + 1;
						out[0] = { col, c.y };
						out[1] = { col, c.y + 1 };
					}
				};

				glm::i64vec2 low[2];
				glm::i64vec2 high[2];
				edgeCorners(cells.front(), false, low);
				edgeCorners(cells.back(), true, high);
				const int32_t low0 = Level(low[0].x, low[0].y);
				const int32_t low1 = Level(low[1].x, low[1].y);
				const int32_t high0 = Level(high[0].x, high[0].y);
				const int32_t high1 = Level(high[1].x, high[1].y);
				if (low0 != low1 || high0 != high1 || high0 <= low0)
				{
					m_Problems.push_back("Ramp at cell (" + std::to_string(cells.front().x) + ", " + std::to_string(cells.front().y) + "): low edge at levels "
						+ std::to_string(low0) + " and " + std::to_string(low1) + ", high edge at " + std::to_string(high0) + " and "
						+ std::to_string(high1) + "; drawn as a cliff");
					continue;
				}

				const float count = (float)cells.size();
				for (size_t n = 0; n < cells.size(); n++)
				{
					const float h0 = (float)low0 + (float)(high0 - low0) * (float)n / count;
					const float h1 = (float)low0 + (float)(high0 - low0) * (float)(n + 1) / count;
					RampCell& ramp = m_RampCells[(size_t)cells[n].y * nx + cells[n].x];
					ramp.Valid = true;
					glm::i64vec2 lowCorners[2];
					glm::i64vec2 highCorners[2];
					edgeCorners(cells[n], false, lowCorners);
					edgeCorners(cells[n], true, highCorners);
					// Corners SW, SE, NE, NW of the cell.
					const glm::i64vec2 corners[4] = { cells[n], cells[n] + glm::i64vec2(1, 0), cells[n] + glm::i64vec2(1, 1), cells[n] + glm::i64vec2(0, 1) };
					for (int k = 0; k < 4; k++)
					{
						const bool isLow = corners[k] == lowCorners[0] || corners[k] == lowCorners[1];
						ramp.Heights[k] = isLow ? h0 : h1;
					}
				}
			}
		}
	}

	void TerrainMesher::CollectWallSegments()
	{
		m_FirstSegment.assign((size_t)m_CellsX * m_CellsY + 1, 0);
		std::vector<std::pair<BoundaryPoint, BoundaryPoint>> chords;
		std::vector<std::pair<BoundaryPoint, BoundaryPoint>> upper;
		BoundaryPoint cycle[8];
		for (uint32_t j = 0; j < m_CellsY; j++)
		{
			for (uint32_t i = 0; i < m_CellsX; i++)
			{
				const size_t cell = (size_t)j * m_CellsX + i;
				m_FirstSegment[cell] = (uint32_t)m_Segments.size();
				if (GetRamp(i, j).Valid)
				{
					continue;
				}
				const uint32_t count = CellCycle(i, j, cycle);
				int32_t lmin = cycle[0].Lo;
				int32_t lmax = cycle[0].Hi;
				for (uint32_t k = 0; k < count; k++)
				{
					lmin = std::min(lmin, cycle[k].Lo);
					lmax = std::max(lmax, cycle[k].Hi);
				}
				if (lmin == lmax)
				{
					continue;
				}
				const uint8_t material = m_Terrain.GetWallMaterial(i, j);
				for (int32_t t = lmin + 1; t <= lmax; t++)
				{
					Chords(cycle, count, t, chords);
					Chords(cycle, count, t + 1, upper);
					for (const auto& [a, b] : chords)
					{
						bool covered = false;
						for (const auto& [ua, ub] : upper)
						{
							covered = covered || (ua.Key == a.Key && ub.Key == b.Key);
						}
						WallSegment segment;
						segment.Level = t;
						segment.A = a;
						segment.B = b;
						segment.Cell = glm::uvec2(i, j);
						segment.Material = material;
						segment.IsTop = covered == false;
						m_Segments.push_back(segment);
					}
				}
			}
		}
		m_FirstSegment[(size_t)m_CellsX * m_CellsY] = (uint32_t)m_Segments.size();
	}

	void TerrainMesher::ComputeOutlineDistances()
	{
		// Distance along the outline at each crossing point (metres), so wall textures run on without jumps. Chains
		// are walked per level, highest first; a point keeps the first value it gets (terrain_mesher.wall_u_table).
		std::unordered_map<uint64_t, float> u;
		std::map<int32_t, std::vector<uint32_t>, std::greater<int32_t>> byLevel;
		for (uint32_t s = 0; s < (uint32_t)m_Segments.size(); s++)
		{
			byLevel[m_Segments[s].Level].push_back(s);
		}

		std::vector<uint8_t> done(m_Segments.size(), 0);
		for (auto& [level, segments] : byLevel)
		{
			std::unordered_map<uint64_t, uint32_t> start;
			std::unordered_map<uint64_t, uint32_t> end;
			for (uint32_t s : segments)
			{
				start[m_Segments[s].A.Key] = s;
				end[m_Segments[s].B.Key] = s;
			}
			std::vector<uint32_t> order = segments;
			std::stable_sort(order.begin(), order.end(), [this](uint32_t x, uint32_t y)
			{
				const WallSegment& a = m_Segments[x];
				const WallSegment& b = m_Segments[y];
				if (a.Cell.y != b.Cell.y)
				{
					return a.Cell.y < b.Cell.y;
				}
				if (a.Cell.x != b.Cell.x)
				{
					return a.Cell.x < b.Cell.x;
				}
				return a.A.Key < b.A.Key; // (orient, i, j), like the reference's key tuples
			});

			for (uint32_t s : order)
			{
				if (done[s] != 0)
				{
					continue;
				}
				// Back to the chain's start (or around the loop).
				uint32_t first = s;
				uint32_t guard = 0;
				for (auto found = end.find(m_Segments[first].A.Key); found != end.end() && found->second != s && guard < 100000;
					found = end.find(m_Segments[first].A.Key))
				{
					first = found->second;
					guard++;
				}

				uint32_t current = first;
				auto known = u.find(m_Segments[current].A.Key);
				double run = known != u.end() ? (double)known->second : 0.0;
				while (done[current] == 0)
				{
					done[current] = 1;
					WallSegment& segment = m_Segments[current];
					auto atA = u.find(segment.A.Key);
					if (atA != u.end())
					{
						run = (double)atA->second;
					}
					else
					{
						u.emplace(segment.A.Key, (float)run);
					}
					const glm::vec3 pa = PointAt(segment.A, level);
					const glm::vec3 pb = PointAt(segment.B, level);
					run += std::hypot((double)pb.x - (double)pa.x, (double)pb.y - (double)pa.y);
					u.emplace(segment.B.Key, (float)run); // keeps an existing value
					auto next = start.find(segment.B.Key);
					if (next == start.end())
					{
						break;
					}
					current = next->second;
				}
			}
		}

		for (WallSegment& segment : m_Segments)
		{
			auto found = u.find(segment.A.Key);
			segment.U = found != u.end() ? found->second : 0.0f;
		}
	}

	bool TerrainMesher::ChunkHasWalls(uint32_t chunkX, uint32_t chunkY) const
	{
		const uint32_t minX = chunkX * TerrainChunkCells;
		const uint32_t minY = chunkY * TerrainChunkCells;
		const uint32_t maxX = std::min(minX + TerrainChunkCells, m_CellsX);
		const uint32_t maxY = std::min(minY + TerrainChunkCells, m_CellsY);
		for (uint32_t j = minY; j < maxY; j++)
		{
			const size_t row = (size_t)j * m_CellsX;
			if (m_FirstSegment[row + minX] != m_FirstSegment[row + maxX])
			{
				return true;
			}
		}
		return false;
	}

	// ---- Meshing --------------------------------------------------------------------------------------------

	// The visible tops of a cell: per level t, Region(>= t) - Region(>= t + 1), as convex caps (terrain_mesher.bands).
	template<typename Emit>
	static void ForEachBand(const TerrainMesher::BoundaryPoint* cycle, uint32_t count, Emit&& emit)
	{
		int32_t lmin = cycle[0].Lo;
		int32_t lmax = cycle[0].Hi;
		for (uint32_t k = 0; k < count; k++)
		{
			lmin = std::min(lmin, cycle[k].Lo);
			lmax = std::max(lmax, cycle[k].Hi);
		}
		TerrainMesher::BoundaryPoint outer[8];
		TerrainMesher::BoundaryPoint inner[8];
		TerrainMesher::BoundaryPoint polygon[10];
		for (int32_t t = lmin; t <= lmax; t++)
		{
			const uint32_t outerCount = Region(cycle, count, t, outer);
			const uint32_t innerCount = Region(cycle, count, t + 1, inner);
			if (innerCount == 0)
			{
				emit(t, outer, outerCount);
				continue;
			}
			for (uint32_t k = 0; k < innerCount; k++)
			{
				const TerrainMesher::BoundaryPoint& a = inner[k];
				const TerrainMesher::BoundaryPoint& b = inner[(k + 1) % innerCount];
				// The cycle's points strictly after a and before b, counter-clockwise, that are in the outer region.
				uint32_t from = 0;
				while (cycle[from].Key != a.Key)
				{
					from++;
				}
				uint32_t n = 0;
				polygon[n++] = a;
				for (uint32_t c = (from + 1) % count; cycle[c].Key != b.Key; c = (c + 1) % count)
				{
					if (cycle[c].Hi >= t)
					{
						polygon[n++] = cycle[c];
					}
				}
				if (n > 1)
				{
					polygon[n++] = b;
					emit(t, polygon, n);
				}
			}
		}
	}

	static glm::vec2 TopUV(const glm::vec3& p)
	{
		return glm::vec2(p.x, p.y);
	}

	static constexpr uint32_t TopsSlot = 0;

	void TerrainMesher::AddTops(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainCollisionBuilder* collision) const
	{
		BoundaryPoint cycle[8];
		const uint32_t count = CellCycle(i, j, cycle);
		ForEachBand(cycle, count, [&](int32_t t, const BoundaryPoint* polygon, uint32_t n)
		{
			if (n < 3)
			{
				return;
			}
			glm::vec3 points[10];
			for (uint32_t k = 0; k < n; k++)
			{
				points[k] = PointAt(polygon[k], t);
			}
			for (uint32_t k = 1; k + 1 < n; k++)
			{
				if (tops != nullptr)
				{
					tops->Triangle(TopsSlot, points[0], points[k], points[k + 1], TopUV(points[0]), TopUV(points[k]), TopUV(points[k + 1]));
				}
				if (collision != nullptr)
				{
					collision->Triangle(points[0], points[k], points[k + 1]);
				}
			}
		});
	}

	void TerrainMesher::AddWall(const WallSegment& segment, TerrainMeshBuilder* parts, TerrainCollisionBuilder* collision) const
	{
		// Region(>= Level)'s outline extruded down one level. Outward (the lower ground) is on the right of A -> B, so
		// the quad is counter-clockwise seen from outside. UVs in metres: u along the outline, v = -z (images have v = 0 at the
		// top, as in MeshFactory).
		const glm::vec3 at = PointAt(segment.A, segment.Level);
		const glm::vec3 bt = PointAt(segment.B, segment.Level);
		const glm::vec3 ab = PointAt(segment.A, segment.Level - 1);
		const glm::vec3 bb = PointAt(segment.B, segment.Level - 1);
		if (collision != nullptr)
		{
			collision->Triangle(ab, bb, bt);
			collision->Triangle(ab, bt, at);
		}
		if (parts == nullptr || segment.Material == TerrainWallHidden)
		{
			return;
		}
		const float ua = segment.U;
		const float ub = ua + std::hypot(bt.x - at.x, bt.y - at.y);
		parts->Quad(GetPartSlot(TerrainPartKind::Wall, std::min<uint32_t>(segment.Material, MaxTerrainWallLayers - 1)), ab, bb, bt, at,
			glm::vec2(ua, -ab.z), glm::vec2(ub, -bb.z), glm::vec2(ub, -bt.z), glm::vec2(ua, -at.z));
	}

	void TerrainMesher::AddWallEdges(const WallSegment& segment, TerrainMeshBuilder& tops, TerrainMeshBuilder& parts) const
	{
		// The top of a wall: the lip (the shelf sticks out over the wall, with a short darker face), the fringe hanging
		// from it where the layer at the edge has one, and coping stones on wall layers that have them.
		const TerrainSettings& settings = m_Terrain.GetSettings();
		const uint32_t material = std::min<uint32_t>(segment.Material, MaxTerrainWallLayers - 1);
		const glm::vec3 at = PointAt(segment.A, segment.Level);
		const glm::vec3 bt = PointAt(segment.B, segment.Level);
		const float zt = at.z;
		const float ua = segment.U;
		const float ub = ua + std::hypot(bt.x - at.x, bt.y - at.y);
		const glm::vec2 da = EdgeDownDirection(KeyOrient(segment.A.Key), KeyI(segment.A.Key), KeyJ(segment.A.Key));
		const glm::vec2 db = EdgeDownDirection(KeyOrient(segment.B.Key), KeyI(segment.B.Key), KeyJ(segment.B.Key));
		const float lip = std::max(settings.LipWidth, 0.0f) / TerrainArtPixelsPerMetre;
		const float lipFace = std::max(settings.LipFaceHeight, 0.0f) / TerrainArtPixelsPerMetre;

		glm::vec3 ao = at;
		glm::vec3 bo = bt;
		if (lip > 0.0f)
		{
			ao = glm::vec3(at.x + da.x * lip, at.y + da.y * lip, zt);
			bo = glm::vec3(bt.x + db.x * lip, bt.y + db.y * lip, zt);
			tops.Quad(TopsSlot, ao, bo, bt, at, TopUV(ao), TopUV(bo), TopUV(bt), TopUV(at));
			if (lipFace > 0.0f)
			{
				const glm::vec3 aod(ao.x, ao.y, zt - lipFace);
				const glm::vec3 bod(bo.x, bo.y, zt - lipFace);
				parts.Quad(GetPartSlot(TerrainPartKind::LipFace, material), aod, bod, bo, ao, glm::vec2(ua, lipFace - zt), glm::vec2(ub, lipFace - zt),
					glm::vec2(ub, -zt), glm::vec2(ua, -zt));
			}
		}

		// The layer just inside the shelf's edge decides the fringe (grass gets one, flagstones none).
		const glm::vec2 edge(bt.x - at.x, bt.y - at.y);
		const float length = glm::length(edge);
		if (length > 1e-6f)
		{
			const glm::vec2 inward(-edge.y / length, edge.x / length); // left of A -> B: into the shelf
			const glm::vec2 middle = 0.5f * glm::vec2(at.x + bt.x, at.y + bt.y) + inward * 0.06f;
			const uint32_t layer = GetTopLayerAt(m_Terrain, middle);
			if (layer < settings.Layers.size() && (uint64_t)settings.Layers[layer].Fringe != 0 && settings.Layers[layer].FringeLength > 0.0f)
			{
				const float fringe = settings.Layers[layer].FringeLength / TerrainArtPixelsPerMetre;
				const float offset = 0.004f;
				const glm::vec3 af(ao.x + da.x * offset, ao.y + da.y * offset, zt + 0.002f);
				const glm::vec3 bf(bo.x + db.x * offset, bo.y + db.y * offset, zt + 0.002f);
				const glm::vec3 afd(af.x, af.y, zt - fringe);
				const glm::vec3 bfd(bf.x, bf.y, zt - fringe);
				parts.Quad(GetPartSlot(TerrainPartKind::Fringe, layer), afd, bfd, bf, af, glm::vec2(ua, 1.0f), glm::vec2(ub, 1.0f), glm::vec2(ub, 0.0f),
					glm::vec2(ua, 0.0f));
			}
		}

		if (segment.Material < settings.WallLayers.size() && settings.WallLayers[segment.Material].CapStones)
		{
			AddCapStones(segment, at, bt, parts);
		}
	}

	void TerrainMesher::AddCapStones(const WallSegment& segment, const glm::vec3& at, const glm::vec3& bt, TerrainMeshBuilder& parts) const
	{
		// A tight row of flat stones along the wall's top, in slots every CapPitch metres of outline distance (so they
		// don't depend on the cell or chunk), with small size and position jitter, a little rotation, a few missing.
		const TerrainSettings& settings = m_Terrain.GetSettings();
		const glm::vec2 edge(bt.x - at.x, bt.y - at.y);
		const float length = glm::length(edge);
		const float pitch = std::max(settings.CapPitch, 0.02f);
		if (length < 1e-6f)
		{
			return;
		}
		const glm::vec2 along = edge / length;
		const glm::vec2 inward(-along.y, along.x);
		const float ua = segment.U;
		const int64_t first = (int64_t)std::ceil(ua / pitch - 0.5f);
		const int64_t last = (int64_t)std::floor((ua + length) / pitch - 0.5f);
		for (int64_t n = first; n <= last; n++)
		{
			float r[6];
			for (int k = 0; k < 6; k++)
			{
				r[k] = Hash01({ settings.Seed, n, segment.Level, k, TagCap });
			}
			if (r[0] < settings.CapMissing)
			{
				continue;
			}
			const float centre = ((float)n + 0.5f) * pitch + (r[1] - 0.5f) * settings.CapPositionJitter;
			const float f = (centre - ua) / length;
			if (f < 0.0f || f > 1.0f)
			{
				continue;
			}
			const float stoneLength = pitch - settings.CapJoint - r[2] * settings.CapSizeJitter;
			const float depth = settings.CapDepth * (1.0f - 0.5f * settings.CapSizeJitter / pitch * r[3]);
			const float height = settings.CapHeight.x + r[4] * (settings.CapHeight.y - settings.CapHeight.x);
			const float yaw = (r[5] - 0.5f) * 2.0f * glm::radians(settings.CapYawJitter);
			const float inset = depth * 0.3f + (r[3] - 0.5f) * settings.CapPositionJitter;
			const glm::vec3 base(at.x + edge.x * f + inward.x * inset, at.y + edge.y * f + inward.y * inset, at.z);
			parts.Box(GetPartSlot(TerrainPartKind::CapStones, 0), base, along, yaw, stoneLength, depth, height);
		}
	}

	void TerrainMesher::EdgeProfile(uint32_t orient, int64_t i, int64_t j, std::vector<glm::vec2>& profile) const
	{
		const float cell = m_Terrain.GetSettings().CellSize;
		const int64_t ib = orient == EdgeH ? i + 1 : i;
		const int64_t jb = orient == EdgeH ? j : j + 1;
		const int32_t la = Level(i, j);
		const int32_t lb = Level(ib, jb);
		profile.clear();
		profile.emplace_back(0.0f, LevelZ((float)la));
		if (la != lb)
		{
			const float x0 = (float)i * cell;
			const float y0 = (float)j * cell;
			const int32_t step = lb > la ? 1 : -1;
			for (int32_t level = la; level != lb + step; level += step)
			{
				const glm::vec3 p = EdgePoint(orient, i, j, level);
				profile.emplace_back(std::hypot(p.x - x0, p.y - y0) / cell, LevelZ((float)level));
			}
		}
		profile.emplace_back(1.0f, LevelZ((float)lb));
	}

	void TerrainMesher::AddRamp(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainMeshBuilder* parts, TerrainCollisionBuilder* collision) const
	{
		const float cell = m_Terrain.GetSettings().CellSize;
		const RampCell& ramp = GetRamp(i, j);
		const glm::uvec2 corners[4] = { { i, j }, { i + 1, j }, { i + 1, j + 1 }, { i, j + 1 } };
		glm::vec3 points[4];
		for (int k = 0; k < 4; k++)
		{
			points[k] = glm::vec3((float)corners[k].x * cell, (float)corners[k].y * cell, LevelZ(ramp.Heights[k]));
		}
		if (tops != nullptr)
		{
			tops->Quad(TopsSlot, points[0], points[1], points[2], points[3], TopUV(points[0]), TopUV(points[1]), TopUV(points[2]), TopUV(points[3]));
		}
		if (collision != nullptr)
		{
			collision->Triangle(points[0], points[1], points[2]);
			collision->Triangle(points[0], points[2], points[3]);
		}

		// Side walls: every cell edge whose other side doesn't continue the same slope. A ramp replaces only the wall
		// across its slope; its sides meet the neighbouring ground through tapering pieces.
		struct Side
		{
			int c0;          // corner index (SW, SE, NE, NW) at the edge's first end
			int c1;
			uint32_t Orient;
			glm::ivec2 Edge; // the edge's ID
			glm::ivec2 Out;  // towards the neighbouring cell
		};
		const Side sides[4] = {
			{ 0, 1, EdgeH, { (int)i, (int)j }, { 0, -1 } },
			{ 1, 2, EdgeV, { (int)i + 1, (int)j }, { 1, 0 } },
			{ 3, 2, EdgeH, { (int)i, (int)j + 1 }, { 0, 1 } },
			{ 0, 3, EdgeV, { (int)i, (int)j }, { -1, 0 } }
		};
		const uint8_t material = m_Terrain.GetWallMaterial(i, j);
		const uint32_t slot = GetPartSlot(TerrainPartKind::Wall, std::min<uint32_t>(material, MaxTerrainWallLayers - 1));
		std::vector<glm::vec2> neighbour;
		for (const Side& side : sides)
		{
			const int64_t ni = (int64_t)i + side.Out.x;
			const int64_t nj = (int64_t)j + side.Out.y;
			if (ni < 0 || nj < 0 || ni >= (int64_t)m_CellsX || nj >= (int64_t)m_CellsY)
			{
				continue;
			}
			const float r0 = LevelZ(ramp.Heights[side.c0]);
			const float r1 = LevelZ(ramp.Heights[side.c1]);
			const RampCell& other = GetRamp((uint32_t)ni, (uint32_t)nj);
			if (other.Valid)
			{
				// The neighbour's heights at the two shared corners.
				auto heightAt = [&](const glm::uvec2& corner)
				{
					const glm::uvec2 own[4] = { { (uint32_t)ni, (uint32_t)nj }, { (uint32_t)ni + 1, (uint32_t)nj }, { (uint32_t)ni + 1, (uint32_t)nj + 1 },
						{ (uint32_t)ni, (uint32_t)nj + 1 } };
					for (int k = 0; k < 4; k++)
					{
						if (own[k] == corner)
						{
							return LevelZ(other.Heights[k]);
						}
					}
					return 0.0f;
				};
				neighbour = { glm::vec2(0.0f, heightAt(corners[side.c0])), glm::vec2(1.0f, heightAt(corners[side.c1])) };
			}
			else
			{
				EdgeProfile(side.Orient, side.Edge.x, side.Edge.y, neighbour);
			}

			const float x0 = (float)side.Edge.x * cell;
			const float y0 = (float)side.Edge.y * cell;
			const float x1 = side.Orient == EdgeH ? x0 + cell : x0;
			const float y1 = side.Orient == EdgeH ? y0 : y0 + cell;
			auto at = [&](float t, float z) { return glm::vec3(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z); };
			auto rampZ = [&](float t) { return r0 + (r1 - r0) * t; };

			for (size_t k = 0; k + 1 < neighbour.size(); k++)
			{
				const float t0 = neighbour[k].x;
				const float n0 = neighbour[k].y;
				const float t1 = neighbour[k + 1].x;
				const float n1 = neighbour[k + 1].y;
				if (t1 - t0 < 1e-6f)
				{
					continue;
				}
				glm::vec4 pieces[2] = { glm::vec4(t0, n0, t1, n1), glm::vec4(0.0f) };
				int pieceCount = 1;
				const float d0 = rampZ(t0) - n0;
				const float d1 = rampZ(t1) - n1;
				if (d0 * d1 < 0.0f)
				{
					const float f = d0 / (d0 - d1);
					const float tm = t0 + (t1 - t0) * f;
					const float nm = n0 + (n1 - n0) * f;
					pieces[0] = glm::vec4(t0, n0, tm, nm);
					pieces[1] = glm::vec4(tm, nm, t1, n1);
					pieceCount = 2;
				}
				for (int p = 0; p < pieceCount; p++)
				{
					const float a = pieces[p].x;
					const float na = pieces[p].y;
					const float b = pieces[p].z;
					const float nb = pieces[p].w;
					const float ra = rampZ(a);
					const float rb = rampZ(b);
					const float middle = (ra - na) + (rb - nb);
					if (std::abs(middle) < 1e-6f)
					{
						continue;
					}
					glm::vec3 q[4] = { at(a, std::min(ra, na)), at(b, std::min(rb, nb)), at(b, std::max(rb, nb)), at(a, std::max(ra, na)) };
					const float uA = x0 + y0 + (x1 - x0 + y1 - y0) * a;
					const float uB = x0 + y0 + (x1 - x0 + y1 - y0) * b;
					glm::vec2 uv[4] = { { uA, -q[0].z }, { uB, -q[1].z }, { uB, -q[2].z }, { uA, -q[3].z } };
					// Seen from the lower side: towards the neighbour where the ramp is higher, else into the ramp.
					const glm::vec2 facing = middle > 0.0f ? glm::vec2(side.Out) : -glm::vec2(side.Out);
					const glm::vec3 n = glm::cross(q[1] - q[0], q[2] - q[0]);
					if (n.x * facing.x + n.y * facing.y < 0.0f)
					{
						std::swap(q[0], q[1]);
						std::swap(q[2], q[3]);
						std::swap(uv[0], uv[1]);
						std::swap(uv[2], uv[3]);
					}
					if (collision != nullptr)
					{
						collision->Triangle(q[0], q[1], q[2]);
						collision->Triangle(q[0], q[2], q[3]);
					}
					if (parts != nullptr && material != TerrainWallHidden)
					{
						parts->Quad(slot, q[0], q[1], q[2], q[3], uv[0], uv[1], uv[2], uv[3]);
					}
				}
			}
		}
	}

	TerrainChunkMesh TerrainMesher::BuildChunk(uint32_t chunkX, uint32_t chunkY) const
	{
		const uint32_t minX = chunkX * TerrainChunkCells;
		const uint32_t minY = chunkY * TerrainChunkCells;
		const uint32_t maxX = std::min(minX + TerrainChunkCells, m_CellsX);
		const uint32_t maxY = std::min(minY + TerrainChunkCells, m_CellsY);

		TerrainMeshBuilder tops;
		TerrainMeshBuilder parts;
		for (uint32_t j = minY; j < maxY; j++)
		{
			for (uint32_t i = minX; i < maxX; i++)
			{
				if (GetRamp(i, j).Valid)
				{
					continue;
				}
				AddTops(i, j, &tops, nullptr);
			}
		}
		for (uint32_t j = minY; j < maxY; j++)
		{
			for (uint32_t i = minX; i < maxX; i++)
			{
				const size_t cell = (size_t)j * m_CellsX + i;
				for (uint32_t s = m_FirstSegment[cell]; s < m_FirstSegment[cell + 1]; s++)
				{
					const WallSegment& segment = m_Segments[s];
					AddWall(segment, &parts, nullptr);
					if (segment.IsTop && segment.Material != TerrainWallHidden)
					{
						AddWallEdges(segment, tops, parts);
					}
				}
			}
		}
		for (uint32_t j = minY; j < maxY; j++)
		{
			for (uint32_t i = minX; i < maxX; i++)
			{
				if (GetRamp(i, j).Valid)
				{
					AddRamp(i, j, &tops, &parts, nullptr);
				}
			}
		}
		TerrainDecor::AddChunk(m_Terrain, *this, chunkX, chunkY, parts);

		TerrainChunkMesh mesh;
		tops.AppendTo(mesh.Tops, "Terrain Tops");
		parts.AppendTo(mesh.Parts, "Terrain Parts");
		return mesh;
	}

	void TerrainMesher::BuildCollision(std::vector<glm::vec3>& positions, std::vector<uint32_t>& indices) const
	{
		positions.clear();
		indices.clear();
		TerrainCollisionBuilder collision{ positions, indices, {} };
		for (uint32_t j = 0; j < m_CellsY; j++)
		{
			for (uint32_t i = 0; i < m_CellsX; i++)
			{
				if (GetRamp(i, j).Valid)
				{
					AddRamp(i, j, nullptr, nullptr, &collision);
				}
				else
				{
					AddTops(i, j, nullptr, &collision);
				}
			}
		}
		for (const WallSegment& segment : m_Segments)
		{
			AddWall(segment, nullptr, &collision);
		}
	}

	const std::vector<TerrainMesher::WallSegment>& TerrainMesher::GetWallSegments() const
	{
		return m_Segments;
	}

	void TerrainMesher::GetCellSegments(uint32_t cellX, uint32_t cellY, uint32_t& first, uint32_t& end) const
	{
		const size_t cell = (size_t)cellY * m_CellsX + cellX;
		first = m_FirstSegment[cell];
		end = m_FirstSegment[cell + 1];
	}

	void TerrainMesher::GetSegmentLine(const WallSegment& segment, int32_t level, glm::vec3& a, glm::vec3& b) const
	{
		a = PointAt(segment.A, level);
		b = PointAt(segment.B, level);
	}

	float TerrainMesher::GetHeightAt(const glm::vec2& position) const
	{
		const float cell = m_Terrain.GetSettings().CellSize;
		const float fx = std::min(std::max(position.x / cell, 0.0f), (float)m_CellsX - 1e-4f);
		const float fy = std::min(std::max(position.y / cell, 0.0f), (float)m_CellsY - 1e-4f);
		const uint32_t i = (uint32_t)fx;
		const uint32_t j = (uint32_t)fy;
		if (GetRamp(i, j).Valid)
		{
			const RampCell& ramp = GetRamp(i, j);
			const float u = fx - (float)i;
			const float v = fy - (float)j;
			const float h = (ramp.Heights[0] * (1.0f - u) + ramp.Heights[1] * u) * (1.0f - v) + (ramp.Heights[3] * (1.0f - u) + ramp.Heights[2] * u) * v;
			return LevelZ(h);
		}

		// The highest band whose polygon holds the point (bands are convex and don't overlap).
		BoundaryPoint cycle[8];
		const uint32_t count = CellCycle(i, j, cycle);
		float best = LevelZ((float)Level(i, j));
		bool found = false;
		ForEachBand(cycle, count, [&](int32_t t, const BoundaryPoint* polygon, uint32_t n)
		{
			if (n < 3 || found)
			{
				return;
			}
			bool inside = true;
			for (uint32_t k = 0; k < n && inside; k++)
			{
				const glm::vec3 p = PointAt(polygon[k], t);
				const glm::vec3 q = PointAt(polygon[(k + 1) % n], t);
				const float side = (q.x - p.x) * (position.y - p.y) - (q.y - p.y) * (position.x - p.x);
				inside = side >= -1e-5f;
			}
			if (inside)
			{
				best = LevelZ((float)t);
				found = true;
			}
		});
		return best;
	}
}

