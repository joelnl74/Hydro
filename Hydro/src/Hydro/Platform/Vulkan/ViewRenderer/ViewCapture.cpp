#include "hypch.h"
#include "ViewCapture.h"

#include "Hydro/Platform/Vulkan/VulkanImageReadback.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Renderer/ImageWriter.h"

#include <utility>

namespace Hydro
{
	// RGBA8 or BGRA8 (swizzled), alpha made opaque.
	static void SaveCapture(const std::filesystem::path& path, ImageReadbackResult& result)
	{
		const bool rgba = result.Format == VK_FORMAT_R8G8B8A8_SRGB || result.Format == VK_FORMAT_R8G8B8A8_UNORM;
		const bool bgra = result.Format == VK_FORMAT_B8G8R8A8_SRGB || result.Format == VK_FORMAT_B8G8R8A8_UNORM;
		if (rgba == false && bgra == false)
		{
			HY_CORE_ERROR("Capture {}: the view's format ({}) can't be saved as a PNG", path.string(), (int)result.Format);
			return;
		}

		const size_t pixelCount = (size_t)result.Width * result.Height;
		for (size_t i = 0; i < pixelCount; i++)
		{
			uint8_t* pixel = result.Pixels.data() + i * 4;
			if (bgra)
			{
				std::swap(pixel[0], pixel[2]);
			}
			pixel[3] = 255;
		}

		if (ImageWriter::WritePNG(path, result.Width, result.Height, result.Pixels.data()))
		{
			HY_CORE_INFO("Saved capture {} ({} x {})", path.string(), result.Width, result.Height);
		}
	}

	static void ResolveCapture(RenderViewData& view, uint32_t frame)
	{
		if (view.Readback == nullptr || view.Readback->IsPending(frame) == false)
		{
			return;
		}

		ImageReadbackResult result;
		if (view.Readback->TakeResult(frame, result))
		{
			SaveCapture(view.CapturePaths[frame], result);
		}
		view.CapturePaths[frame].clear();
	}

	void ViewCapture::Resolve(const std::vector<RenderViewData*>& views, uint32_t frame)
	{
		// Every view, not only the ones begun this frame: a copy may belong to a view that is hidden now.
		for (RenderViewData* view : views)
		{
			ResolveCapture(*view, frame);
		}
	}

	static void RecordCapture(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		if (view.RequestedCapture.empty())
		{
			return;
		}

		if (view.Readback == nullptr)
		{
			view.Readback = CreateScope<VulkanImageReadback>();
		}

		// This frame index's last copy hasn't been written yet: try again next frame.
		if (view.Readback->IsPending(frame))
		{
			return;
		}

		if (view.Readback->Record(commandBuffer, view.Target->GetImage(frame), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frame))
		{
			view.CapturePaths[frame] = view.RequestedCapture;
		}
		else
		{
			HY_CORE_ERROR("Capture of view '{}' failed: {}", view.DebugName, view.RequestedCapture.string());
		}
		view.RequestedCapture.clear();
	}

	void ViewCapture::Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		RecordCapture(commandBuffer, view, frame);
	}
}
