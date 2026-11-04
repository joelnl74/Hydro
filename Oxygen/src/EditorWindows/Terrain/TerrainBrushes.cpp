#include "TerrainBrushes.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace Hydro
{
	// Raises `layer` of the weights `w` to at least `target` (0..1); the other layers shrink in proportion.
	static void RaiseLayerTo(float* w, uint32_t layer, uint32_t layerCount, float target)
	{
		if (w[layer] >= target)
		{
			return;
		}
		float others = 0.0f;
		for (uint32_t k = 0; k < layerCount; k++)
		{
			others += k == layer ? 0.0f : w[k];
		}
		const float scale = others > 1e-6f ? (1.0f - target) / others : 0.0f;
		for (uint32_t k = 0; k < layerCount; k++)
		{
			w[k] = k == layer ? target : w[k] * scale;
		}
	}

	// Weights 0..1 to bytes that sum to exactly 255: the rounding error goes to the largest channel.
	static glm::u8vec4 WeightsToBytes(const float* w)
	{
		int bytes[4] = { 0, 0, 0, 0 };
		int sum = 0;
		int largest = 0;
		for (int k = 0; k < 4; k++)
		{
			bytes[k] = (int)std::lround(glm::clamp(w[k], 0.0f, 1.0f) * 255.0f);
			sum += bytes[k];
			largest = bytes[k] > bytes[largest] ? k : largest;
		}
		bytes[largest] = glm::clamp(bytes[largest] + 255 - sum, 0, 255);
		return glm::u8vec4((uint8_t)bytes[0], (uint8_t)bytes[1], (uint8_t)bytes[2], (uint8_t)bytes[3]);
	}

	// The cells whose centres are inside the brush.
	template<typename Visit>
	static void ForCellsUnderBrush(const Terrain& terrain, const glm::vec2& centre, float radius, Visit&& visit)
	{
		const float cell = terrain.GetSettings().CellSize;
		const int minX = std::max(0, (int)std::floor((centre.x - radius) / cell));
		const int minY = std::max(0, (int)std::floor((centre.y - radius) / cell));
		const int maxX = std::min((int)terrain.GetCellCountX() - 1, (int)std::floor((centre.x + radius) / cell));
		const int maxY = std::min((int)terrain.GetCellCountY() - 1, (int)std::floor((centre.y + radius) / cell));
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				if (glm::distance((glm::vec2((float)x, (float)y) + 0.5f) * cell, centre) <= radius)
				{
					visit((uint32_t)x, (uint32_t)y);
				}
			}
		}
	}

	void TerrainBrushes::Paint(Terrain& terrain, const TerrainBrushSettings& brush, std::vector<float>& weights, const glm::vec2& centre,
		bool lower, float deltaTime)
	{
		const TerrainSettings& settings = terrain.GetSettings();
		const uint32_t layerCount = std::min((uint32_t)settings.Layers.size(), MaxTerrainLayers);
		const uint32_t layer = std::min(brush.Layer, layerCount - 1);
		if (lower && layerCount < 2)
		{
			return; // the only layer can't give way to another
		}

		// The texels whose centres ((k + 0.5) / SplatResolution metres) are inside the circle.
		const float resolution = (float)settings.SplatResolution;
		const int width = (int)terrain.GetWeightMapWidth();
		const int height = (int)terrain.GetWeightMapHeight();
		const int minX = std::max(0, (int)std::floor((centre.x - brush.Radius) * resolution));
		const int minY = std::max(0, (int)std::floor((centre.y - brush.Radius) * resolution));
		const int maxX = std::min(width - 1, (int)std::ceil((centre.x + brush.Radius) * resolution));
		const int maxY = std::min(height - 1, (int)std::ceil((centre.y + brush.Radius) * resolution));
		if (minX > maxX || minY > maxY)
		{
			return;
		}

		const float inner = brush.Radius * glm::clamp(brush.Hardness, 0.0f, 0.99f);
		// Full strength reaches the target weight in about a quarter of a second.
		const float rate = glm::clamp(brush.Strength, 0.0f, 1.0f) * 4.0f * deltaTime;
		bool changed = false;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				const glm::vec2 texelCentre((x + 0.5f) / resolution, (y + 0.5f) / resolution);
				const float distance = glm::distance(texelCentre, centre);
				if (distance > brush.Radius)
				{
					continue;
				}
				const float falloff = distance <= inner ? 1.0f : 1.0f - glm::smoothstep(inner, brush.Radius, distance);
				const float amount = glm::clamp(rate * falloff, 0.0f, 1.0f);
				if (amount <= 0.0f)
				{
					continue;
				}

				float* w = &weights[((size_t)y * width + x) * 4];
				const float old = w[layer];
				float others = 0.0f;
				for (uint32_t k = 0; k < layerCount; k++)
				{
					others += k == layer ? 0.0f : w[k];
				}
				if (lower == false)
				{
					// Raise the layer towards 1; the others shrink in proportion.
					const float raised = old + amount * (1.0f - old);
					const float scale = others > 1e-6f ? (1.0f - raised) / others : 0.0f;
					for (uint32_t k = 0; k < layerCount; k++)
					{
						w[k] = k == layer ? raised : w[k] * scale;
					}
				}
				else
				{
					// Lower the layer; what it gives up goes to the others in proportion (to the first other layer when
					// they are all empty).
					const float lowered = old * (1.0f - amount);
					const float freed = old - lowered;
					w[layer] = lowered;
					if (others > 1e-6f)
					{
						for (uint32_t k = 0; k < layerCount; k++)
						{
							w[k] += k == layer ? 0.0f : w[k] / others * freed;
						}
					}
					else
					{
						w[layer == 0 ? 1 : 0] += freed;
					}
				}
				for (uint32_t k = layerCount; k < 4; k++)
				{
					w[k] = 0.0f;
				}

				const glm::u8vec4 value = WeightsToBytes(w);
				if (value != terrain.GetWeight((uint32_t)x, (uint32_t)y))
				{
					terrain.SetWeight((uint32_t)x, (uint32_t)y, value);
					changed = true;
				}
			}
		}

		if (changed)
		{
			terrain.MarkWeightsChanged(TerrainRect{ glm::uvec2((uint32_t)minX, (uint32_t)minY),
				glm::uvec2((uint32_t)maxX, (uint32_t)maxY) });
		}
	}

	void TerrainBrushes::ChangeLevels(Terrain& terrain, const TerrainBrushSettings& brush, std::vector<uint8_t>& touched,
		const glm::vec2& centre, int32_t step, bool flatten, int8_t flattenLevel)
	{
		const float cell = terrain.GetSettings().CellSize;
		const int minX = std::max(0, (int)std::ceil((centre.x - brush.Radius) / cell));
		const int minY = std::max(0, (int)std::ceil((centre.y - brush.Radius) / cell));
		const int maxX = std::min((int)terrain.GetCornerCountX() - 1, (int)std::floor((centre.x + brush.Radius) / cell));
		const int maxY = std::min((int)terrain.GetCornerCountY() - 1, (int)std::floor((centre.y + brush.Radius) / cell));
		bool changed = false;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				if (glm::distance(glm::vec2((float)x, (float)y) * cell, centre) > brush.Radius)
				{
					continue;
				}
				const size_t index = (size_t)y * terrain.GetCornerCountX() + x;
				const int32_t old = terrain.GetLevel((uint32_t)x, (uint32_t)y);
				int32_t level = old;
				if (flatten)
				{
					level = flattenLevel;
				}
				else if (touched[index] == 0)
				{
					level = glm::clamp(old + step, -128, 127);
					touched[index] = 1;
				}
				if (level != old)
				{
					terrain.SetLevel((uint32_t)x, (uint32_t)y, (int8_t)level);
					changed = true;
				}
			}
		}
		if (changed)
		{
			// The cells touching the corners.
			TerrainRect cells;
			cells.Min = glm::uvec2((uint32_t)std::max(minX - 1, 0), (uint32_t)std::max(minY - 1, 0));
			cells.Max = glm::uvec2((uint32_t)std::min(maxX, (int)terrain.GetCellCountX() - 1),
				(uint32_t)std::min(maxY, (int)terrain.GetCellCountY() - 1));
			terrain.MarkCellsChanged(cells);
		}
	}

	void TerrainBrushes::PaintWalls(Terrain& terrain, const TerrainBrushSettings& brush, const glm::vec2& centre)
	{
		bool changed = false;
		TerrainRect rect{ glm::uvec2(UINT32_MAX), glm::uvec2(0) };
		ForCellsUnderBrush(terrain, centre, brush.Radius, [&](uint32_t x, uint32_t y)
		{
			if (terrain.GetWallMaterial(x, y) != brush.WallLayer)
			{
				terrain.SetWallMaterial(x, y, brush.WallLayer);
				rect.Min = glm::min(rect.Min, glm::uvec2(x, y));
				rect.Max = glm::max(rect.Max, glm::uvec2(x, y));
				changed = true;
			}
		});
		if (changed)
		{
			terrain.MarkCellsChanged(rect);
		}
	}

	void TerrainBrushes::PaintRamps(Terrain& terrain, const TerrainBrushSettings& brush, const glm::vec2& strokeStart,
		const glm::vec2& centre, bool clear)
	{
		// The direction is the main axis of the drag so far (from the low side towards the high side).
		TerrainRamp ramp = TerrainRamp::None;
		if (clear == false)
		{
			const glm::vec2 drag = centre - strokeStart;
			if (glm::length(drag) < 0.25f)
			{
				return;
			}
			if (std::abs(drag.x) > std::abs(drag.y))
			{
				ramp = drag.x > 0.0f ? TerrainRamp::East : TerrainRamp::West;
			}
			else
			{
				ramp = drag.y > 0.0f ? TerrainRamp::North : TerrainRamp::South;
			}
		}
		bool changed = false;
		TerrainRect rect{ glm::uvec2(UINT32_MAX), glm::uvec2(0) };
		ForCellsUnderBrush(terrain, centre, brush.Radius, [&](uint32_t x, uint32_t y)
		{
			if (terrain.GetRamp(x, y) != ramp)
			{
				terrain.SetRamp(x, y, ramp);
				rect.Min = glm::min(rect.Min, glm::uvec2(x, y));
				rect.Max = glm::max(rect.Max, glm::uvec2(x, y));
				changed = true;
			}
		});
		if (changed)
		{
			terrain.MarkCellsChanged(rect);
		}
	}

	void TerrainBrushes::PaintPath(Terrain& terrain, const TerrainBrushSettings& brush, const std::vector<glm::vec3>& pathPoints)
	{
		if (pathPoints.size() < 2)
		{
			return;
		}

		// Catmull-Rom through the points, so the road bends instead of kinking.
		std::vector<glm::vec2> curve;
		std::vector<glm::vec2> points;
		points.push_back(glm::vec2(pathPoints.front()));
		for (const glm::vec3& point : pathPoints)
		{
			points.push_back(glm::vec2(point));
		}
		points.push_back(glm::vec2(pathPoints.back()));
		for (size_t k = 1; k + 2 < points.size(); k++)
		{
			const glm::vec2 p0 = points[k - 1];
			const glm::vec2 p1 = points[k];
			const glm::vec2 p2 = points[k + 1];
			const glm::vec2 p3 = points[k + 2];
			const int samples = std::max(2, (int)std::ceil(glm::distance(p1, p2) / 0.1f));
			for (int n = 0; n < samples; n++)
			{
				const float t = (float)n / (float)samples;
				const float t2 = t * t;
				const float t3 = t2 * t;
				curve.push_back(0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
					+ (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
			}
		}
		curve.push_back(glm::vec2(pathPoints.back()));

		const TerrainSettings& settings = terrain.GetSettings();
		const uint32_t layerCount = std::min((uint32_t)settings.Layers.size(), MaxTerrainLayers);
		const uint32_t layer = std::min(brush.Layer, layerCount - 1);
		const float resolution = (float)settings.SplatResolution;
		const float half = 0.5f * brush.PathWidth;
		const float soft = 0.35f; // the painted edge's width; the edge noise makes it ragged
		const float reach = half + brush.PathWander + soft;
		glm::vec2 low = curve.front();
		glm::vec2 high = curve.front();
		for (const glm::vec2& point : curve)
		{
			low = glm::min(low, point);
			high = glm::max(high, point);
		}
		const int width = (int)terrain.GetWeightMapWidth();
		const int height = (int)terrain.GetWeightMapHeight();
		const int minX = std::max(0, (int)std::floor((low.x - reach) * resolution));
		const int minY = std::max(0, (int)std::floor((low.y - reach) * resolution));
		const int maxX = std::min(width - 1, (int)std::ceil((high.x + reach) * resolution));
		const int maxY = std::min(height - 1, (int)std::ceil((high.y + reach) * resolution));
		const float seed = (float)(settings.Seed % 1000u);
		bool changed = false;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				const glm::vec2 p((x + 0.5f) / resolution, (y + 0.5f) / resolution);
				float distance = FLT_MAX;
				for (size_t k = 0; k + 1 < curve.size(); k++)
				{
					const glm::vec2 a = curve[k];
					const glm::vec2 d = curve[k + 1] - a;
					const float length2 = glm::dot(d, d);
					const float t = length2 > 0.0f ? glm::clamp(glm::dot(p - a, d) / length2, 0.0f, 1.0f) : 0.0f;
					distance = std::min(distance, glm::distance(p, a + d * t));
				}
				// The edge wanders in and out slowly along the road.
				const float wander = brush.PathWander * (0.6f * std::sin(1.7f * p.x + 0.9f * p.y + seed)
					+ 0.4f * std::sin(0.6f * p.x - 1.3f * p.y + 2.0f * seed));
				const float target = glm::clamp((half + wander - distance) / soft + 0.5f, 0.0f, 1.0f);
				if (target <= 0.0f)
				{
					continue;
				}
				const glm::u8vec4 bytes = terrain.GetWeight((uint32_t)x, (uint32_t)y);
				float w[4] = { bytes.x / 255.0f, bytes.y / 255.0f, bytes.z / 255.0f, bytes.w / 255.0f };
				RaiseLayerTo(w, layer, layerCount, target);
				const glm::u8vec4 value = WeightsToBytes(w);
				if (value != bytes)
				{
					terrain.SetWeight((uint32_t)x, (uint32_t)y, value);
					changed = true;
				}
			}
		}
		if (changed)
		{
			terrain.MarkWeightsChanged(TerrainRect{ glm::uvec2((uint32_t)minX, (uint32_t)minY),
				glm::uvec2((uint32_t)maxX, (uint32_t)maxY) });
		}
	}
}
