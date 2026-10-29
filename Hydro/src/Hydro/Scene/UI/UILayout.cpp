#include "hypch.h"
#include "UILayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Hydro
{
	UICanvasMetrics UILayout::GetCanvasMetrics(const UICanvasComponent& canvas, const glm::vec2& viewSize)
	{
		const glm::vec2 view = glm::max(viewSize, glm::vec2(1.0f));
		const glm::vec2 reference = glm::max(canvas.ReferenceResolution, glm::vec2(1.0f));
		const float fit = std::min(view.x / reference.x, view.y / reference.y);

		UICanvasMetrics metrics;
		switch (canvas.ScaleMode)
		{
		case UIScaleMode::ScaleWithScreen:
			metrics.Scale = std::max(fit, 1.0e-4f);
			metrics.Size = view / metrics.Scale;
			break;
		case UIScaleMode::ConstantPixelSize:
			metrics.Scale = 1.0f;
			metrics.Size = view;
			break;
		case UIScaleMode::IntegerScale:
		default:
			// Never below 1: a view smaller than the reference crops the canvas instead of shrinking it to nothing.
			metrics.Scale = std::max(std::floor(fit), 1.0f);
			metrics.Size = glm::max(glm::floor(view / metrics.Scale), reference);
			break;
		}

		// Centred; whole view pixels so the canvas's pixels line up with the view's.
		metrics.Offset = glm::floor((view - metrics.Size * metrics.Scale) * 0.5f);
		return metrics;
	}

	void UILayout::ResolveRect(const UIRectComponent& rect, const glm::vec2& parentMin, const glm::vec2& parentMax, glm::vec2& outMin, glm::vec2& outMax)
	{
		const glm::vec2 parentSize = parentMax - parentMin;
		const glm::vec2 anchorMin = parentMin + rect.AnchorMin * parentSize;
		const glm::vec2 anchorMax = parentMin + rect.AnchorMax * parentSize;

		const glm::vec2 size = (anchorMax - anchorMin) + rect.SizeDelta;
		const glm::vec2 pivot = anchorMin + (anchorMax - anchorMin) * rect.Pivot + rect.AnchoredPosition;
		const glm::vec2 min = pivot - size * rect.Pivot;

		outMin = glm::round(min);
		outMax = glm::max(glm::round(min + size), outMin);
	}

	// Two opposite borders (in destination pixels) that fit `span`: shrunk in proportion when they don't, whole pixels.
	static void FitBorders(float span, float& first, float& second)
	{
		span = std::max(span, 0.0f);
		first = std::max(first, 0.0f);
		second = std::max(second, 0.0f);
		const float total = first + second;
		if (total > span && total > 0.0f)
		{
			const float shrink = span / total;
			first *= shrink;
			second *= shrink;
		}
		first = std::min(std::round(first), span);
		second = std::min(std::round(second), span - first);
	}

	uint32_t UILayout::BuildNineSlice(const glm::vec2& destinationMin, const glm::vec2& destinationMax,
		const glm::vec2& sourcePosition, const glm::vec2& sourceSize, const glm::vec4& borders, bool flipX, bool flipY,
		UINineSliceCell (&cells)[9])
	{
		// The borders in the source, cut to fit it (left, bottom, right, top).
		float left = borders.x;
		float right = borders.z;
		float bottom = borders.y;
		float top = borders.w;
		FitBorders(sourceSize.x, left, right);
		FitBorders(sourceSize.y, bottom, top);

		// The same borders in the destination (mirrored when flipped), cut to fit it.
		float destinationLeft = flipX ? right : left;
		float destinationRight = flipX ? left : right;
		float destinationBottom = flipY ? top : bottom;
		float destinationTop = flipY ? bottom : top;
		FitBorders(destinationMax.x - destinationMin.x, destinationLeft, destinationRight);
		FitBorders(destinationMax.y - destinationMin.y, destinationBottom, destinationTop);

		const float columns[4] = { destinationMin.x, destinationMin.x + destinationLeft, destinationMax.x - destinationRight, destinationMax.x };
		const float rows[4] = { destinationMin.y, destinationMin.y + destinationBottom, destinationMax.y - destinationTop, destinationMax.y }; // bottom to top
		const float sourceColumns[4] = { sourcePosition.x, sourcePosition.x + left, sourcePosition.x + sourceSize.x - right, sourcePosition.x + sourceSize.x };
		const float sourceBands[4] = { sourcePosition.y, sourcePosition.y + top, sourcePosition.y + sourceSize.y - bottom, sourcePosition.y + sourceSize.y }; // top to bottom

		uint32_t count = 0;
		for (uint32_t row = 0; row < 3; row++)
		{
			for (uint32_t column = 0; column < 3; column++)
			{
				UINineSliceCell cell;
				cell.DestinationMin = { columns[column], rows[row] };
				cell.DestinationMax = { columns[column + 1], rows[row + 1] };
				if (cell.DestinationMax.x <= cell.DestinationMin.x || cell.DestinationMax.y <= cell.DestinationMin.y)
				{
					continue;
				}

				// The destination's bottom row shows the source's bottom band (rows go up, bands go down).
				const uint32_t sourceColumn = flipX ? 2 - column : column;
				const uint32_t sourceBand = flipY ? row : 2 - row;
				cell.SourceMin = { sourceColumns[sourceColumn], sourceBands[sourceBand] };
				cell.SourceMax = { sourceColumns[sourceColumn + 1], sourceBands[sourceBand + 1] };
				if (flipX)
				{
					std::swap(cell.SourceMin.x, cell.SourceMax.x);
				}
				if (flipY)
				{
					std::swap(cell.SourceMin.y, cell.SourceMax.y);
				}
				cells[count++] = cell;
			}
		}
		return count;
	}
}
