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

	// How the ImGui layer starts. Part of ApplicationSpecification, so the application chooses its
	// fonts (the engine itself loads no font files).
	struct ImGuiLayerSpecification
	{
		// Loaded in order; the first one that loads is the default font. A missing file is logged and
		// skipped. Empty, or none loaded: ImGui's built-in font.
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

		// A font loaded from ImGuiLayerSpecification::Fonts, by name; nullptr if it wasn't loaded.
		// Use with ImGui::PushFont / PopFont.
		static ImFont* GetFont(const std::string& name);

	private:
		void LoadFonts();

	private:
		ImGuiLayerSpecification m_Specification;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
}
