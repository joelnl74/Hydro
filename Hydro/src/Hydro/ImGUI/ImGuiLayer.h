#pragma once
#include "Hydro/Core/Layer.h"

#include <vulkan/vulkan.h>

namespace Hydro
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();

	private:
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
}
