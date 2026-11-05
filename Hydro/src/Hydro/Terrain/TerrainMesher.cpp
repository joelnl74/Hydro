#include "hypch.h"
#include "TerrainMesher.h"

#include "Hydro/Terrain/TerrainDecor.h"
#include "Hydro/Terrain/TerrainMeshBuilder.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	TerrainMesher::TerrainMesher(const Terrain& terrain)
		: m_Terrain(terrain), m_Analysis(terrain), m_CellsX(terrain.GetCellCountX()), m_CellsY(terrain.GetCellCountY())
	{
	}

	static glm::vec2 TopUV(const glm::vec3& p)
	{
		return glm::vec2(p.x, p.y);
	}

	static constexpr uint32_t TopsSlot = 0;

	void TerrainMesher::AddTops(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainCollisionBuilder* collision) const
	{
		BoundaryPoint cycle[8];
		const uint32_t count = m_Analysis.CellCycle(i, j, cycle);
		TerrainAnalysis::ForEachBand(cycle, count, [&](int32_t t, const BoundaryPoint* polygon, uint32_t n)
		{
			if (n < 3)
			{
				return;
			}
			glm::vec3 points[10];
			for (uint32_t k = 0; k < n; k++)
			{
				points[k] = m_Analysis.PointAt(polygon[k], t);
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
		// Region(>= Level)'s outline extruded down one level, counter-clockwise seen from outside (the lower ground is
		// right of A -> B). UVs in metres: u along the outline, v = -z (v = 0 at the image's top, as in MeshFactory).
		const glm::vec3 at = m_Analysis.WallPointAt(segment.A, segment.Level);
		const glm::vec3 bt = m_Analysis.WallPointAt(segment.B, segment.Level);
		const glm::vec3 ab = m_Analysis.WallPointAt(segment.A, segment.Level - 1);
		const glm::vec3 bb = m_Analysis.WallPointAt(segment.B, segment.Level - 1);
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
		const glm::vec3 shading = WallShading(at, bt, ab, bb);
		parts->Quad(GetPartSlot(TerrainPartKind::Wall, std::min<uint32_t>(segment.Material, MaxTerrainWallLayers - 1)), ab, bb, bt, at,
			glm::vec2(ua, -ab.z), glm::vec2(ub, -bb.z), glm::vec2(ub, -bt.z), glm::vec2(ua, -at.z), &shading);
	}

	glm::vec3 TerrainMesher::WallShading(const glm::vec3& at, const glm::vec3& bt, const glm::vec3& ab, const glm::vec3& bb) const
	{
		// The face's outward normal (right of A -> B, tilted by a slope), turned WallLight of the way towards up.
		const glm::vec3 along = bt - at;
		const glm::vec3 down = 0.5f * (ab + bb) - 0.5f * (at + bt);
		glm::vec3 n = glm::cross(down, along);
		const float length = glm::length(n);
		n = length > 1e-8f ? n / length : glm::vec3(0.0f, 0.0f, 1.0f);
		const float light = glm::clamp(m_Terrain.GetSettings().WallLight, 0.0f, 1.0f);
		const glm::vec3 shading = glm::mix(n, glm::vec3(0.0f, 0.0f, 1.0f), light);
		const float shadingLength = glm::length(shading);
		return shadingLength > 1e-8f ? shading / shadingLength : glm::vec3(0.0f, 0.0f, 1.0f);
	}

	void TerrainMesher::AddWallEdges(const WallSegment& segment, TerrainMeshBuilder& tops, TerrainMeshBuilder& parts) const
	{
		// The top of a wall: the lip (the shelf sticks out over the wall, with a short darker face), the fringe hanging
		// from it where the layer at the edge has one, and coping stones on wall layers that have them.
		const TerrainSettings& settings = m_Terrain.GetSettings();
		const uint32_t material = std::min<uint32_t>(segment.Material, MaxTerrainWallLayers - 1);
		const glm::vec3 at = m_Analysis.WallPointAt(segment.A, segment.Level);
		const glm::vec3 bt = m_Analysis.WallPointAt(segment.B, segment.Level);
		const float zt = at.z;
		const float ua = segment.U;
		const float ub = ua + std::hypot(bt.x - at.x, bt.y - at.y);
		const glm::vec2 da = m_Analysis.EdgeDownDirection(segment.A);
		const glm::vec2 db = m_Analysis.EdgeDownDirection(segment.B);
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
				const glm::vec3 shading = WallShading(ao, bo, aod, bod);
				parts.Quad(GetPartSlot(TerrainPartKind::LipFace, material), aod, bod, bo, ao, glm::vec2(ua, lipFace - zt),
					glm::vec2(ub, lipFace - zt), glm::vec2(ub, -zt), glm::vec2(ua, -zt), &shading);
			}
		}

		// The layer just inside the shelf's edge decides the fringe (grass gets one, flagstones none).
		const glm::vec2 edge(bt.x - at.x, bt.y - at.y);
		const float length = glm::length(edge);
		if (length > 1e-6f)
		{
			const glm::vec2 inward(-edge.y / length, edge.x / length); // left of A -> B: into the shelf
			const glm::vec2 middle = 0.5f * glm::vec2(at.x + bt.x, at.y + bt.y) + inward * 0.06f;
			const uint32_t layer = TerrainAnalysis::GetTopLayerAt(m_Terrain, middle);
			if (layer < settings.Layers.size() && (uint64_t)settings.Layers[layer].Fringe != 0
				&& settings.Layers[layer].FringeLength > 0.0f)
			{
				const float fringe = settings.Layers[layer].FringeLength / TerrainArtPixelsPerMetre;
				const float offset = 0.004f;
				const glm::vec3 af(ao.x + da.x * offset, ao.y + da.y * offset, zt + 0.002f);
				const glm::vec3 bf(bo.x + db.x * offset, bo.y + db.y * offset, zt + 0.002f);
				// On a sloped wall the strip lies along the slope instead of hanging into it.
				const float sa = m_Analysis.EdgeSlope(segment.A);
				const float sb = m_Analysis.EdgeSlope(segment.B);
				const glm::vec3 afd(af.x + da.x * sa * fringe, af.y + da.y * sa * fringe, zt - fringe);
				const glm::vec3 bfd(bf.x + db.x * sb * fringe, bf.y + db.y * sb * fringe, zt - fringe);
				parts.Quad(GetPartSlot(TerrainPartKind::Fringe, layer), afd, bfd, bf, af, glm::vec2(ua, 1.0f), glm::vec2(ub, 1.0f),
					glm::vec2(ub, 0.0f), glm::vec2(ua, 0.0f));
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
				r[k] = TerrainAnalysis::Hash01({ settings.Seed, n, segment.Level, k, TerrainAnalysis::TagCap });
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
		const int64_t ib = orient == TerrainAnalysis::EdgeH ? i + 1 : i;
		const int64_t jb = orient == TerrainAnalysis::EdgeH ? j : j + 1;
		const int32_t la = m_Analysis.Level(i, j);
		const int32_t lb = m_Analysis.Level(ib, jb);
		profile.clear();
		profile.emplace_back(0.0f, m_Analysis.LevelZ((float)la));
		if (la != lb)
		{
			const float x0 = (float)i * cell;
			const float y0 = (float)j * cell;
			const int32_t step = lb > la ? 1 : -1;
			for (int32_t level = la; level != lb + step; level += step)
			{
				const glm::vec3 p = m_Analysis.EdgePoint(orient, i, j, level);
				profile.emplace_back(std::hypot(p.x - x0, p.y - y0) / cell, m_Analysis.LevelZ((float)level));
			}
		}
		profile.emplace_back(1.0f, m_Analysis.LevelZ((float)lb));
	}

	void TerrainMesher::AddRamp(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainMeshBuilder* parts,
		TerrainCollisionBuilder* collision) const
	{
		const float cell = m_Terrain.GetSettings().CellSize;
		const TerrainAnalysis::RampCell& ramp = m_Analysis.GetRamp(i, j);
		const glm::uvec2 corners[4] = { { i, j }, { i + 1, j }, { i + 1, j + 1 }, { i, j + 1 } };
		glm::vec3 points[4];
		for (int k = 0; k < 4; k++)
		{
			points[k] = glm::vec3((float)corners[k].x * cell, (float)corners[k].y * cell, m_Analysis.LevelZ(ramp.Heights[k]));
		}
		if (tops != nullptr)
		{
			tops->Quad(TopsSlot, points[0], points[1], points[2], points[3], TopUV(points[0]), TopUV(points[1]), TopUV(points[2]),
				TopUV(points[3]));
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
			{ 0, 1, TerrainAnalysis::EdgeH, { (int)i, (int)j }, { 0, -1 } },
			{ 1, 2, TerrainAnalysis::EdgeV, { (int)i + 1, (int)j }, { 1, 0 } },
			{ 3, 2, TerrainAnalysis::EdgeH, { (int)i, (int)j + 1 }, { 0, 1 } },
			{ 0, 3, TerrainAnalysis::EdgeV, { (int)i, (int)j }, { -1, 0 } }
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
			const float r0 = m_Analysis.LevelZ(ramp.Heights[side.c0]);
			const float r1 = m_Analysis.LevelZ(ramp.Heights[side.c1]);
			const TerrainAnalysis::RampCell& other = m_Analysis.GetRamp((uint32_t)ni, (uint32_t)nj);
			if (other.Valid)
			{
				// The neighbour's heights at the two shared corners.
				auto heightAt = [&](const glm::uvec2& corner)
				{
					const glm::uvec2 own[4] = { { (uint32_t)ni, (uint32_t)nj }, { (uint32_t)ni + 1, (uint32_t)nj },
						{ (uint32_t)ni + 1, (uint32_t)nj + 1 }, { (uint32_t)ni, (uint32_t)nj + 1 } };
					for (int k = 0; k < 4; k++)
					{
						if (own[k] == corner)
						{
							return m_Analysis.LevelZ(other.Heights[k]);
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
			const float x1 = side.Orient == TerrainAnalysis::EdgeH ? x0 + cell : x0;
			const float y1 = side.Orient == TerrainAnalysis::EdgeH ? y0 : y0 + cell;
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
				if (m_Analysis.GetRamp(i, j).Valid)
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
				uint32_t first = 0;
				uint32_t end = 0;
				m_Analysis.GetCellSegments(i, j, first, end);
				for (uint32_t s = first; s < end; s++)
				{
					const WallSegment& segment = m_Analysis.GetWallSegments()[s];
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
				if (m_Analysis.GetRamp(i, j).Valid)
				{
					AddRamp(i, j, &tops, &parts, nullptr);
				}
			}
		}
		TerrainDecor::AddChunk(m_Terrain, m_Analysis, chunkX, chunkY, parts);

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
				if (m_Analysis.GetRamp(i, j).Valid)
				{
					AddRamp(i, j, nullptr, nullptr, &collision);
				}
				else
				{
					AddTops(i, j, nullptr, &collision);
				}
			}
		}
		for (const WallSegment& segment : m_Analysis.GetWallSegments())
		{
			AddWall(segment, nullptr, &collision);
		}
	}
}
