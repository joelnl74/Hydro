#include "hypch.h"
// Header
#include "ImGuiLayer.h"

// Engine
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Core/Application.h"

#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Platform/Vulkan/VulkanCommandBuffer.h"

// Libs
#include <vector>

#include "vulkan/vulkan.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "Hydro/Platform/Vulkan/imgui_impl_glfw.h"
#include "Hydro/Platform/Vulkan/imgui_impl_vulkan.h"

namespace Hydro
{
	ImGuiLayer::ImGuiLayer()
	{

	}

	void ImGuiLayer::OnAttach()
	{
		// Create descriptor pool for IMGUI.
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000;
		pool_info.poolSizeCount = std::size(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;

		VkDescriptorPool imguiPool;

		auto rendererContext = Renderer::GetRendererContext();
		auto& device = rendererContext->GetVulkanDevice();

		VK_CHECK_RESULT(vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &imguiPool));

		// Create context.
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls.
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls.
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking.
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows.

		// Initializes imgui for GLFW.
		auto& window = Application::Get().GetWindow();
		auto glfwWindow = static_cast<GLFWwindow*>(window.GetNativeWindow());

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}


		ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

		// Initializes imgui for Vulkan.
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = rendererContext->GetInstance();
		init_info.PhysicalDevice = device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
		init_info.Device = device->GetDevice();
		init_info.Queue = device->GetGraphicsQueue();
		init_info.DescriptorPool = imguiPool;
		init_info.MinImageCount = 3;
		init_info.ImageCount = 3;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		ImGui_ImplVulkan_Init(&init_info, Renderer::GetVulkanSwapChain()->GetRenderPass());

		//execute a gpu command to upload imgui font textures
		VulkanCommandBuffer cmd;
		auto commandBuffer = cmd.Create();
		
		cmd.Begin();
			ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
		cmd.End();

		//clear font textures from cpu data
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}


	void ImGuiLayer::OnDetach()
	{
		//add the destroy the imgui created structures
// _mainDeletionQueue.push_function([=]() {
//
//	vkDestroyDescriptorPool(_device, imguiPool, nullptr);
//	ImGui_ImplVulkan_Shutdown();
//	});
	}

	void ImGuiLayer::OnImGuiRender() {}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}


	void ImGuiLayer::End()
	{
		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		// Rendering
		ImGui::Render();
	}
}
