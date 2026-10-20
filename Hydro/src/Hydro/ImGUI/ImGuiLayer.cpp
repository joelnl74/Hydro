#include "hypch.h"
// Header
#include "ImGuiLayer.h"

// Engine
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Core/Application.h"

#include "Hydro/Platform/Vulkan/VulkanUtils.h"

// Libs
#include <cstdio>
#include <cstring>
#include <vector>

#include "vulkan/vulkan.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "Hydro/Platform/GLFW/imgui_impl_glfw.h"
#include "Hydro/Platform/Vulkan/imgui_impl_vulkan.h"

namespace Hydro
{
	ImGuiLayer::ImGuiLayer(const ImGuiLayerSpecification& specification)
		: m_Specification(specification)
	{
	}

	void ImGuiLayer::LoadFonts()
	{
		ImGuiIO& io = ImGui::GetIO();

		for (const ImGuiFontSpecification& font : m_Specification.Fonts)
		{
			std::error_code ec;
			if (std::filesystem::exists(font.Path, ec) == false)
			{
				HY_CORE_WARN("ImGui font '{}' not found: {}", font.Name, font.Path.string());
				continue;
			}

			ImFontConfig config;
			config.OversampleH = 3;
			config.OversampleV = 1;
			std::snprintf(config.Name, sizeof(config.Name), "%s", font.Name.c_str());

			if (io.Fonts->AddFontFromFileTTF(font.Path.string().c_str(), font.Size, &config) == nullptr)
				HY_CORE_WARN("ImGui font '{}' could not be loaded from {}", font.Name, font.Path.string());
		}

		// The first font added is ImGui's default; with none, ImGui uses its built-in one.
		if (io.Fonts->Fonts.empty() == false)
			io.FontDefault = io.Fonts->Fonts[0];
	}

	ImFont* ImGuiLayer::GetFont(const std::string& name)
	{
		if (ImGui::GetCurrentContext() == nullptr)
			return nullptr;

		for (ImFont* font : ImGui::GetIO().Fonts->Fonts)
		{
			if (font->ConfigData != nullptr && std::strcmp(font->ConfigData->Name, name.c_str()) == 0)
				return font;
		}

		return nullptr;
	}

	void ImGuiLayer::OnAttach()
	{
		// Create descriptor pool for IMGUI.
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 100;
		pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;

		VulkanDevice& device = Renderer::GetVulkanDevice();
		VulkanSwapChain& swapChain = Renderer::GetVulkanSwapChain();

		VK_CHECK_RESULT(vkCreateDescriptorPool(device.GetDevice(), &pool_info, nullptr, &m_DescriptorPool));

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

		// Fonts go into the atlas before its texture is uploaded below.
		LoadFonts();

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

		// Initializes imgui for Vulkan.
		uint32_t minImageCount = std::max(swapChain.GetMinImageCount(), 2u);
		uint32_t imageCount = std::max(swapChain.GetImageCount(), minImageCount);

		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = Renderer::GetRendererContext().GetInstance();
		init_info.PhysicalDevice = device.GetPhysicalDevice().GetVulkanPhysicalDevice();
		init_info.Device = device.GetDevice();
		init_info.QueueFamily = (uint32_t)device.GetPhysicalDevice().GetQueueFamilyIndices().graphics;
		init_info.Queue = device.GetGraphicsQueue();
		init_info.DescriptorPool = m_DescriptorPool;
		init_info.MinImageCount = minImageCount;
		init_info.ImageCount = imageCount;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		ImGui_ImplVulkan_Init(&init_info, swapChain.GetRenderPass());

		// Upload the font atlas (submits and waits), then free ImGui's staging buffer.
		device.SubmitImmediate([](VkCommandBuffer commandBuffer)
		{
			ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
		});

		ImGui_ImplVulkan_DestroyFontUploadObjects();

		// The renderer calls this inside the swapchain render pass every frame.
		Renderer::SetPresentPassCallback([](VkCommandBuffer frameCommandBuffer)
		{
			ImDrawData* drawData = ImGui::GetDrawData();
			if (drawData != nullptr)
				ImGui_ImplVulkan_RenderDrawData(drawData, frameCommandBuffer);
		});
	}

	void ImGuiLayer::OnDetach()
	{
		// The GPU must be done with ImGui's buffers and descriptor sets before they are destroyed.
		Renderer::WaitIdle();
		Renderer::SetPresentPassCallback(nullptr);

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		if (m_DescriptorPool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(Renderer::GetVulkanDevice().GetDevice(), m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
		}
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
		// DisplaySize is set by ImGui_ImplGlfw_NewFrame() from the real window size.
		// (It used to be overwritten here with the window size from startup, which
		// never changed, so after a resize ImGui laid out and clipped for the old size.)

		// Rendering
		ImGui::Render();
	}
}
