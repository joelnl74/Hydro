#include "RenderLookUI.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Renderer/RenderProfileSerializer.h>

namespace Hydro
{
	// The profile changed in memory and not written yet (0: none). One for the whole editor: Render Settings
	// and the inspector may both edit profiles.
	static AssetHandle s_UnsavedProfile = 0;

	static void HelpMarker(const char* text)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", text);
		}
	}

	bool RenderLookUI::DrawEnvironment(SceneEnvironment& environment)
	{
		ImGui::PushID("Environment");
		bool changed = ImGui::Checkbox("Own environment", &environment.Override);
		HelpMarker("Forward lighting only. On: this sky and ground ambient and fog.\nOff: the scene's Global lights are the ambient light and there is no fog.");
		if (environment.Override)
		{
			changed |= ImGui::ColorEdit3("Sky ambient", &environment.SkyAmbient.x, ImGuiColorEditFlags_Float);
			HelpMarker("What surfaces facing up get (linear RGB). Facing sideways: half sky, half ground.");
			changed |= ImGui::ColorEdit3("Ground ambient", &environment.GroundAmbient.x, ImGuiColorEditFlags_Float);
			changed |= ImGui::Checkbox("Fog", &environment.Fog);
			ImGui::BeginDisabled(environment.Fog == false);
			changed |= ImGui::ColorEdit3("Fog color", &environment.FogColor.x, ImGuiColorEditFlags_Float);
			changed |= ImGui::DragFloatRange2("Fog distance", &environment.FogStart, &environment.FogEnd, 0.25f, 0.0f, 10000.0f, "%.1f m", "%.1f m");
			HelpMarker("Metres from what the camera looks at: where fog starts, and where it hides everything.");
			ImGui::EndDisabled();
		}
		ImGui::PopID();
		return changed;
	}

	bool RenderLookUI::DrawPostProcess(ScenePostProcess& postProcess)
	{
		ImGui::PushID("PostProcess");
		bool changed = ImGui::DragFloat("Exposure", &postProcess.Exposure, 0.05f, -8.0f, 8.0f, "%.2f EV");
		HelpMarker("HDR views only (render features: HDR). In stops: +1 twice as bright, -1 half. 0 = as lit.");

		// A plain separator and label: the vendored ImGui (1.89 WIP) has no SeparatorText.
		ImGui::Separator();
		ImGui::TextDisabled("Bloom");
		changed |= ImGui::Checkbox("Bloom on", &postProcess.Bloom);
		HelpMarker("With the Bloom render feature. Off: a quick before/after comparison (the feature stays on).");
		ImGui::BeginDisabled(postProcess.Bloom == false);
		changed |= ImGui::DragFloat("Threshold", &postProcess.BloomThreshold, 0.01f, 0.0f, 16.0f, "%.2f");
		HelpMarker("Linear brightness where the glow starts. Above 1: only emission and very bright lights glow.");
		changed |= ImGui::SliderFloat("Knee", &postProcess.BloomKnee, 0.0f, 1.0f, "%.2f");
		HelpMarker("How softly the glow fades in below the threshold (part of the threshold). 0: a hard cut.");
		changed |= ImGui::DragFloat("Intensity", &postProcess.BloomIntensity, 0.01f, 0.0f, 8.0f, "%.2f");
		HelpMarker("How much glow is added. Keep it low: a halo around lamps, not a haze over the scene.");
		changed |= ImGui::SliderFloat("Radius", &postProcess.BloomRadius, 0.5f, 2.0f, "%.2f");
		HelpMarker("How far the glow spreads.");
		ImGui::EndDisabled();
		if (ImGui::Button("Reset post-processing"))
		{
			postProcess = ScenePostProcess{};
			changed = true;
		}
		ImGui::PopID();
		return changed;
	}

	void RenderLookUI::EditProfile(const Ref<RenderProfile>& profile, bool environment, bool postProcess)
	{
		if (profile == nullptr)
		{
			return;
		}

		RenderProfileData data = profile->GetData();
		bool changed = false;
		if (environment)
		{
			changed |= DrawEnvironment(data.Environment);
		}
		if (environment && postProcess)
		{
			ImGui::Separator();
		}
		if (postProcess)
		{
			changed |= DrawPostProcess(data.PostProcess);
		}

		if (changed)
		{
			// Every scene using it changes now; the file is written once the edit is finished.
			if (s_UnsavedProfile != profile->GetHandle())
			{
				SaveEditedProfile(true);
			}
			profile->SetData(data);
			s_UnsavedProfile = profile->GetHandle();
		}
	}

	void RenderLookUI::SaveEditedProfile(bool force)
	{
		if ((uint64_t)s_UnsavedProfile == 0)
		{
			return;
		}
		// Not while a field is being dragged or typed in: a slider would write the file every frame.
		if (force == false && ImGui::IsAnyItemActive())
		{
			return;
		}

		const AssetHandle handle = s_UnsavedProfile;
		s_UnsavedProfile = 0;
		const Ref<RenderProfile> profile = AssetManager::GetLoadedAsset<RenderProfile>(handle);
		const std::filesystem::path path = AssetManager::GetAbsolutePath(handle);
		if (profile == nullptr || path.empty())
		{
			return;
		}
		RenderProfileSerializer::Serialize(path, profile->GetData());
	}

	std::filesystem::path RenderLookUI::MakeUniqueProfilePath(const std::filesystem::path& folder, std::string name)
	{
		for (char& c : name)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
			{
				c = '_';
			}
		}
		if (name.empty())
		{
			name = "New Render Profile";
		}

		std::error_code error;
		std::filesystem::path path = folder / std::filesystem::u8path(name + RenderProfileSerializer::FileExtension);
		for (int number = 2; std::filesystem::exists(path, error); number++)
		{
			path = folder / std::filesystem::u8path(name + " " + std::to_string(number) + RenderProfileSerializer::FileExtension);
		}
		return path;
	}

	AssetHandle RenderLookUI::CreateProfile(const std::string& name, const RenderProfileData& data)
	{
		if (AssetManager::IsProjectOpen() == false)
		{
			return 0;
		}

		const std::filesystem::path folder = AssetManager::GetAssetDirectory() / "RenderProfiles";
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		const std::filesystem::path path = MakeUniqueProfilePath(folder, name);
		if (RenderProfileSerializer::Serialize(path, data) == false)
		{
			return 0;
		}

		const AssetHandle handle = AssetManager::FindHandle(path);
		if ((uint64_t)handle == 0 || AssetManager::GetAsset<RenderProfile>(handle) == nullptr)
		{
			return 0;
		}
		HY_INFO("Created {}", path.filename().u8string());
		return handle;
	}
}
