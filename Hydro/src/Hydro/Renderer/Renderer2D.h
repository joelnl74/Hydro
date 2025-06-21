#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
namespace Hydro
{
	// Stats
	struct Statistics
	{
		uint32_t DrawCalls = 0;
		uint32_t QuadCount = 0;
		uint32_t LineCount = 0;

		uint32_t GetTotalVertexCount() { return QuadCount * 4 + LineCount * 2; }
		uint32_t GetTotalIndexCount() { return QuadCount * 6 + LineCount * 2; }
	};

	class Renderer2D
	{
	public:
		static void Init();
		static void ShutDown();

		static void Begin();
		static void End();
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void CreateCompositeImageView();
		static VkDescriptorSet GetCompositeDescriptorSet();

		static void FlushAndReset();
	};

}
