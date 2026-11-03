#include "RenderLookUI.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Renderer/RenderProfileSerializer.h>

namespace Hydro
{
	// The profile changed in memory and not written yet (0: none). Shared because Render Settings and the inspector
	// may both edit profiles.
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
			changed |= ImGui::DragFloatRange2("Fog distance", &environment.FogStart, &environment.FogEnd, 0.25f, 0.0f, 10000.0f,
				"%.1f m", "%.1f m");
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

		// The vendored ImGui has no SeparatorText.
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

		ImGui::Separator();
		ImGui::TextDisabled("Depth of field");
		changed |= ImGui::Checkbox("Depth of field on", &postProcess.DepthOfField);
		HelpMarker("With the DepthOfField render feature. Off: a quick before/after comparison (the feature stays on).\nDistances are metres along the view from where the camera's view meets the ground (as the fog's),\nin front of it (towards the camera) and behind it. The band between stays sharp: keep the player in it.");
		ImGui::BeginDisabled(postProcess.DepthOfField == false);
		changed |= ImGui::DragFloat("Near start", &postProcess.DofNearStart, 0.1f, 0.0f, 200.0f, "%.1f m");
		HelpMarker("Metres in front of the focus point that stay sharp; nearer than that the foreground blurs.");
		changed |= ImGui::DragFloat("Far start", &postProcess.DofFarStart, 0.1f, 0.0f, 200.0f, "%.1f m");
		HelpMarker("Metres behind the focus point that stay sharp; further than that the background blurs.");
		changed |= ImGui::DragFloat("Range", &postProcess.DofRange, 0.1f, 0.1f, 200.0f, "%.1f m");
		HelpMarker("Metres beyond either start over which the blur grows to its largest.");
		changed |= ImGui::SliderFloat("Max radius", &postProcess.DofMaxRadius, 0.0f, 3.0f, "%.2f %%");
		HelpMarker("The largest blur radius, in percent of the view's height (0.6 = about 6 pixels at 1080 lines),\nso it looks the same in any window size. 0: no blur.");
		ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextDisabled("Vignette");
		changed |= ImGui::SliderFloat("Vignette", &postProcess.VignetteIntensity, 0.0f, 1.0f, "%.2f");
		HelpMarker("How dark the frame's corners get (0: none). HDR views only. The Editor view leaves it out\nunless Editor > Effects > Post-Processing is on; the Game view always shows it.");
		ImGui::BeginDisabled(postProcess.VignetteIntensity <= 0.0f);
		changed |= ImGui::SliderFloat("Vignette radius", &postProcess.VignetteRadius, 0.0f, 1.0f, "%.2f");
		HelpMarker("Where the darkening starts: 0 the centre, about 0.7 the middle of an edge, 1 the corners.");
		changed |= ImGui::SliderFloat("Vignette softness", &postProcess.VignetteSoftness, 0.05f, 1.0f, "%.2f");
		HelpMarker("Over how much further the darkening reaches its full strength.");
		ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextDisabled("Colour grading");
		changed |= ImGui::Checkbox("Grading on", &postProcess.Grading);
		HelpMarker("HDR views only. Off: a quick before/after comparison. The defaults change nothing.");
		ImGui::BeginDisabled(postProcess.Grading == false);
		changed |= ImGui::SliderFloat("Temperature", &postProcess.Temperature, -1.0f, 1.0f, "%.2f");
		HelpMarker("White balance: below 0 cooler (bluer), above 0 warmer (more orange). Keeps the brightness.");
		changed |= ImGui::SliderFloat("Tint", &postProcess.Tint, -1.0f, 1.0f, "%.2f");
		HelpMarker("White balance: below 0 greener, above 0 more magenta.");
		changed |= ImGui::SliderFloat("Contrast", &postProcess.Contrast, 0.5f, 2.0f, "%.2f");
		HelpMarker("Around middle grey, in stops (never clips): above 1 darker darks and brighter lights.");
		changed |= ImGui::SliderFloat("Saturation", &postProcess.Saturation, 0.0f, 2.0f, "%.2f");
		HelpMarker("0: grey, 1: as painted, 2: twice as colourful.");
		changed |= ImGui::DragFloat3("Lift", &postProcess.Lift.x, 0.002f, -0.3f, 0.3f, "%.3f");
		HelpMarker("Added to the darks, fading out towards white (R, G, B). A little blue lifts the shadows cool.");
		changed |= ImGui::DragFloat3("Gamma", &postProcess.Gamma.x, 0.005f, 0.3f, 3.0f, "%.3f");
		HelpMarker("The mid-tones (R, G, B): above 1 brighter, below 1 darker.");
		changed |= ImGui::DragFloat3("Gain", &postProcess.Gain.x, 0.005f, 0.0f, 3.0f, "%.3f");
		HelpMarker("Multiplies everything, mostly visible in the lights (R, G, B). A little red and green warms the sun.");
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
