#pragma once
#include "Hydro/Core/Layer.h"

#include <vulkan/vulkan.h>

#include <filesystem>
#include <string>
#include <vector>

struct ImFont;

namespace Hydro
{
	// A font the ImGui layer loads when it is attached.
	struct ImGuiFontSpecification
	{
		std::string Name;           // looked up with ImGuiLayer::GetFont; at most 39 characters
		std::filesystem::path Path; // .ttf / .otf, relative to the working directory
		float Size = 16.0f;         // pixels
	};

	// How the ImGui layer starts, part of ApplicationSpecification: the application chooses its fonts.
	struct ImGuiLayerSpecification
	{
		// Loaded in order; the first that loads is the default. Missing files are logged and skipped; none: ImGui's own.
		std::vector<ImGuiFontSpecification> Fonts;
	};

	class ImGuiLayer : public Layer
	{
	public:
		explicit ImGuiLayer(const ImGuiLayerSpecification& specification = ImGuiLayerSpecification());

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();

		// A loaded font by name for ImGui::PushFont / PopFont; nullptr if it wasn't loaded.
		static ImFont* GetFont(const std::string& name);

	private:
		void LoadFonts();

	private:
		ImGuiLayerSpecification m_Specification;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
}
