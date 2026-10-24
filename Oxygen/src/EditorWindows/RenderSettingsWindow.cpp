#include "RenderSettingsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Renderer/Renderer.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Project/Project.h>

namespace Hydro
{
	static bool SameSettings(const RendererSettings& a, const RendererSettings& b)
	{
		return a.ClearColor == b.ClearColor && a.VSync == b.VSync && a.Wireframe == b.Wireframe
			&& a.ShaderHotReload == b.ShaderHotReload
			&& a.Lighting == b.Lighting && a.LightFalloff == b.LightFalloff && a.LightBands == b.LightBands;
	}

	static void HelpMarker(const char* text)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", text);
	}

	void RenderSettingsWindow::OnImguiRender()
	{
		if (m_Open == false)
			return;

		ImGui::SetNextWindowSize(ImVec2(380.0f, 420.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Render Settings", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		// Edit a copy and hand it back only when something changed, so the renderer sees one change per edit.
		RendererSettings settings = Renderer::GetSettings();
		const RendererCapabilities& capabilities = Renderer::GetCapabilities();
		bool changed = false;

		// ---- Output ------------------------------------------------------------
		if (ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen))
		{
			changed |= ImGui::ColorEdit4("Clear color", &settings.ClearColor.x, ImGuiColorEditFlags_Float);
			HelpMarker("Background of the scene viewport (linear RGBA).");

			changed |= ImGui::Checkbox("VSync", &settings.VSync);
			HelpMarker("Wait for the display refresh. Off: uncapped (mailbox, or immediate if the GPU has no mailbox).\nChanging it recreates the swapchain.");
		}

		// ---- Lighting ----------------------------------------------------------
		if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
		{
			changed |= ImGui::Checkbox("2D lighting", &settings.Lighting);
			HelpMarker("Draw views that have lights (Light 2D components) lit. Off: lights are ignored and the scene is drawn as it is.");

			ImGui::BeginDisabled(settings.Lighting == false);

			const char* falloffNames[] = { "Smooth", "Banded" };
			int falloff = (int)settings.LightFalloff;
			if (ImGui::Combo("Light falloff", &falloff, falloffNames, 2))
			{
				settings.LightFalloff = (LightFalloffStyle)falloff;
				changed = true;
			}
			HelpMarker("Smooth: lights fade out continuously.\nBanded: the fade is cut into visible steps (the retro, pixel-art look).");

			ImGui::BeginDisabled(settings.LightFalloff != LightFalloffStyle::Banded);
			int bands = (int)settings.LightBands;
			if (ImGui::SliderInt("Bands", &bands, 2, 32))
			{
				settings.LightBands = (uint32_t)bands;
				changed = true;
			}
			ImGui::EndDisabled();

			ImGui::EndDisabled();

			ImGui::TextDisabled("The light curve is assets/shaders/light.frag. With Shader hot reload on,\nsaving it changes the look of the running editor.");
		}

		// ---- Debug -------------------------------------------------------------
		if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginDisabled(capabilities.Wireframe == false);
			changed |= ImGui::Checkbox("Wireframe", &settings.Wireframe);
			ImGui::EndDisabled();
			HelpMarker(capabilities.Wireframe ? "Draw sprites as outlines (rebuilds the 2D pipeline)."
				: "Not supported: the GPU has no fillModeNonSolid feature.");

			changed |= ImGui::Checkbox("Shader hot reload", &settings.ShaderHotReload);
			HelpMarker("Recompile shaders when their .vert/.frag files change on disk.");
		}

		if (changed)
			Renderer::SetSettings(settings);

		// ---- Device (read-only) ------------------------------------------------
		if (ImGui::CollapsingHeader("Device") && StatsUI::BeginStatTable("##render_device"))
		{
			const Statistics stats = Renderer2D::GetStats();
			StatsUI::Row("GPU", "%s", capabilities.GPUName);
			StatsUI::Row("Frames in flight", "%u", capabilities.FramesInFlight);
			StatsUI::Row("Viewport", "%u x %u", stats.ViewportWidth, stats.ViewportHeight);
			StatsUI::Row("Quads per batch", "%u", stats.MaxQuadsPerBatch);
			StatsUI::Row("Wireframe support", "%s", capabilities.Wireframe ? "yes" : "no");
			StatsUI::Row("Wide lines support", "%s", capabilities.WideLines ? "yes" : "no");
			ImGui::EndTable();
		}

		// ---- Project -----------------------------------------------------------
		ImGui::Separator();
		Ref<Project> project = Project::GetActive();
		if (project == nullptr)
		{
			ImGui::TextDisabled("No project open: settings are not saved.");
		}
		else
		{
			const bool unsaved = SameSettings(Renderer::GetSettings(), project->GetConfig().Renderer) == false;

			if (ImGui::Button("Save to project") && OnSaveProject)
				OnSaveProject();
			ImGui::SameLine();
			ImGui::BeginDisabled(unsaved == false);
			if (ImGui::Button("Revert"))
				Renderer::SetSettings(project->GetConfig().Renderer);
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Defaults"))
				Renderer::SetSettings(RendererSettings());

			if (unsaved)
				ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Unsaved changes (%s)", project->GetProjectFilePath().filename().string().c_str());
			else
				ImGui::TextDisabled("Saved in %s", project->GetProjectFilePath().filename().string().c_str());
		}

		ImGui::End();
	}
}
