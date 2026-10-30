#include "RenderSettingsWindow.h"
#include "StatsUI.h"
#include "RenderLookUI.h"
#include "Utilities/AssetPicker.h"

#include <imgui/imgui.h>
#include <Hydro/Renderer/Renderer.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Project/Project.h>
#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Scene/Scene.h>

namespace Hydro
{
	static bool SameSettings(const RendererSettings& a, const RendererSettings& b)
	{
		return a.ClearColor == b.ClearColor && a.VSync == b.VSync && a.Wireframe == b.Wireframe
			&& a.ShaderHotReload == b.ShaderHotReload
			&& a.Lighting == b.Lighting && a.LightFalloff == b.LightFalloff && a.LightBands == b.LightBands && a.Shadows == b.Shadows
			&& a.SunShadowMapSize == b.SunShadowMapSize && a.SoftSunShadows == b.SoftSunShadows
			&& a.Features == b.Features;
	}

	static void HelpMarker(const char* text)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", text);
	}

	// Warning / error colours that stay readable on light and dark themes (as the Console's).
	static ImVec4 GetValidationColor(RenderValidationSeverity severity)
	{
		const ImVec4& background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		const bool light = 0.2126f * background.x + 0.7152f * background.y + 0.0722f * background.z > 0.5f;
		if (severity == RenderValidationSeverity::Error)
		{
			return light ? ImVec4(0.80f, 0.15f, 0.12f, 1.0f) : ImVec4(1.00f, 0.42f, 0.38f, 1.0f);
		}
		return light ? ImVec4(0.70f, 0.45f, 0.00f, 1.0f) : ImVec4(1.00f, 0.80f, 0.30f, 1.0f);
	}

	// A combo over `names`; true when the choice changed.
	template<typename T>
	static bool EnumCombo(const char* label, T& value, const char* const* names, int count)
	{
		int index = (int)value;
		if (ImGui::Combo(label, &index, names, count))
		{
			value = (T)index;
			return true;
		}
		return false;
	}

	// The preset, every render feature setting and what the render graph would report for them.
	static bool DrawRenderFeatures(const char* id, RenderFeatureSettings& features)
	{
		ImGui::PushID(id);
		bool changed = false;

		const char* presetName = features.GetPresetName();
		if (ImGui::BeginCombo("Preset", presetName))
		{
			if (ImGui::Selectable("2D", features == RenderFeatureSettings::Preset2D()))
			{
				features = RenderFeatureSettings::Preset2D();
				changed = true;
			}
			if (ImGui::Selectable("3D", features == RenderFeatureSettings::Preset3D()))
			{
				features = RenderFeatureSettings::Preset3D();
				changed = true;
			}
			ImGui::EndCombo();
		}
		HelpMarker("2D: painter's order and the 2D light buffer (today's frame).\n3D: shared depth, view-depth sorting, forward lights, cut-out sprites, meshes, sun shadows, HDR, bloom.\nChanging a setting below makes it Custom.");

		static const char* depthNames[] = { "Off", "Shared" };
		changed |= EnumCombo("Depth", features.Depth, depthNames, 2);
		static const char* sortNames[] = { "Sort order", "View depth" };
		changed |= EnumCombo("Sprite sorting", features.SpriteSorting, sortNames, 2);
		static const char* lightingNames[] = { "None", "2D light buffer", "Forward" };
		changed |= EnumCombo("Lighting mode", features.Lighting, lightingNames, 3);
		static const char* alphaNames[] = { "Translucent", "Cut-out" };
		changed |= EnumCombo("Default sprite alpha", features.DefaultSpriteAlpha, alphaNames, 2);
		changed |= ImGui::Checkbox("Meshes", &features.Meshes);
		static const char* shadowNames[] = { "Off", "Sun" };
		changed |= EnumCombo("Shadows", features.Shadows, shadowNames, 2);
		HelpMarker("Sun: a shadow map for the main Directional Light (its Cast Shadows). Meshes cast with their Cast shadows;\nevery lit sprite and mesh receives. Needs forward lighting and shared depth.");
		ImGui::BeginDisabled(features.Shadows == ShadowMode::Off);
		static const char* spriteShadowNames[] = { "None", "As drawn", "Face the sun" };
		changed |= EnumCombo("Sprite shadows", features.SpriteShadows, spriteShadowNames, 3);
		HelpMarker("How cut-out Upright and Billboard sprites cast.\nNone: they don't (they still receive).\nAs drawn: the quad the camera sees; thin when the sun comes from the side.\nFace the sun: the quad turned about its feet to face the sun, so the shadow is always\nas wide as the sprite, and the sprite is never darkened by its own shadow.");
		ImGui::EndDisabled();
		static const char* uprightNames[] = { "Lean to the camera", "Stand vertical" };
		changed |= EnumCombo("Upright sprites", features.UprightSprites, uprightNames, 2);
		HelpMarker("How Upright sprites (trees, props, characters) stand when the camera is tilted.\nLean to the camera: they lean back with the camera's tilt and face it fully, so they look\npropped against the hill behind them.\nStand vertical: they stand straight up and only turn left and right, like standees in a model\n(Octopath Traveler). Under 30 degrees of tilt they still lean, so a top-down view shows them.\nThe battle stage places its actors for leaning sprites.");
		changed |= ImGui::Checkbox("HDR", &features.HDR);
		HelpMarker("The scene renders into a 16-bit float colour, so lanterns and emission can be brighter than white;\nthe scene's exposure and the tonemap bring it to the screen (Look (scene)).\nThe HUD and debug lines are drawn after the tonemap. Off: the 8-bit colour path.");
		ImGui::BeginDisabled(features.HDR == false);
		changed |= ImGui::Checkbox("Bloom", &features.Bloom);
		ImGui::EndDisabled();
		HelpMarker("A soft glow around what is brighter than the scene's bloom threshold (lantern flames, lit windows,\nemissive materials). Needs HDR. Strength and threshold: Look (scene).");

		// Errors leave the feature out (the frame still draws); warnings keep it.
		const std::vector<RenderValidationMessage> messages = Renderer2D::ValidateRenderFeatures(features, false);
		if (messages.empty())
		{
			ImGui::TextDisabled("No validation messages.");
		}
		for (const RenderValidationMessage& message : messages)
		{
			const char* severity = message.Severity == RenderValidationSeverity::Error ? "Error" : "Warning";
			ImGui::PushStyleColor(ImGuiCol_Text, GetValidationColor(message.Severity));
			ImGui::TextWrapped("%s, %s: %s", severity, message.Feature.c_str(), message.Text.c_str());
			ImGui::PopStyleColor();
		}

		ImGui::PopID();
		return changed;
	}

	// Applies a look change to the edited scene and the running copy.
	static void SetLook(Scene& editorScene, Scene* activeScene, const SceneLook& look)
	{
		editorScene.SetLook(look);
		if (activeScene != nullptr && activeScene != &editorScene)
		{
			activeScene->SetLook(look);
		}
	}

	void RenderSettingsWindow::DrawLook(Scene& editorScene, Scene* activeScene)
	{
		SceneLook look = editorScene.GetLook();
		const Ref<RenderProfile> profile = (uint64_t)look.Profile != 0 ? AssetManager::GetAsset<RenderProfile>(look.Profile) : nullptr;

		// The profile: picked, dropped from the Project window, or cleared.
		ImGui::TextUnformatted("Render profile");
		HelpMarker("A shared look (.hyrender): environment and post-processing that several scenes can use.\nEditing it changes every scene that uses it. Below, choose which sections come from it.");
		const std::string detail = (uint64_t)look.Profile != 0 && profile == nullptr
			? "The profile is missing or couldn't be loaded (see the console); the scene's own values are drawn." : std::string();
		const AssetPickerResult picked = AssetPicker::DrawField("##RenderProfile", look.Profile, AssetType::RenderProfile, detail);
		if (picked.Changed)
		{
			// Loading logs why a file couldn't be loaded; a handle that doesn't load is not taken.
			if ((uint64_t)picked.Handle == 0 || AssetManager::GetAsset<RenderProfile>(picked.Handle) != nullptr)
			{
				const bool hadProfile = (uint64_t)look.Profile != 0;
				look.Profile = picked.Handle;
				// A first profile is used for both sections; clearing it goes back to the scene's own values.
				if ((uint64_t)look.Profile == 0)
				{
					look.Environment = RenderLookSource::Scene;
					look.PostProcess = RenderLookSource::Scene;
				}
				else if (hadProfile == false)
				{
					look.Environment = RenderLookSource::Profile;
					look.PostProcess = RenderLookSource::Profile;
				}
				SetLook(editorScene, activeScene, look);
			}
		}

		// Turns what the scene shows now into a new profile it then uses.
		if (AssetManager::IsProjectOpen() && ImGui::Button("Save as profile"))
		{
			RenderProfileData data;
			data.Environment = editorScene.GetActiveEnvironment();
			data.PostProcess = editorScene.GetActivePostProcess();
			const AssetHandle created = RenderLookUI::CreateProfile(editorScene.GetName(), data);
			if ((uint64_t)created != 0)
			{
				look.Profile = created;
				look.Environment = RenderLookSource::Profile;
				look.PostProcess = RenderLookSource::Profile;
				SetLook(editorScene, activeScene, look);
			}
		}
		HelpMarker("Writes the environment and post-processing shown now to RenderProfiles/<scene>.hyrender\nand makes this scene use it for both.");
		if ((uint64_t)look.Profile == 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("No profile: the scene's own values.");
		}

		// One section: where it comes from, then its fields (the profile's or the scene's own).
		static const char* sourceNames[] = { "Scene", "Profile" };
		auto drawSource = [&](const char* label, RenderLookSource& source)
		{
			ImGui::BeginDisabled((uint64_t)look.Profile == 0);
			const bool changed = EnumCombo(label, source, sourceNames, 2);
			ImGui::EndDisabled();
			HelpMarker("Scene: this scene's own values (kept in the scene file).\nProfile: the render profile's (shared); the scene's own values come back when you switch back.");
			if (changed)
			{
				SetLook(editorScene, activeScene, look);
			}
		};

		ImGui::Separator();
		ImGui::TextDisabled("Environment");
		drawSource("Environment from", look.Environment);
		if (look.Environment == RenderLookSource::Profile && profile != nullptr)
		{
			ImGui::TextDisabled("Shared: edits change every scene using %s.", AssetManager::GetAbsolutePath(look.Profile).filename().u8string().c_str());
			RenderLookUI::EditProfile(profile, true, false);
		}
		else
		{
			if (look.Environment == RenderLookSource::Profile)
			{
				ImGui::TextDisabled("Profile missing: the scene's own values are drawn.");
			}
			// Like the render features: edits go to the editor scene and the running copy.
			SceneEnvironment environment = editorScene.GetEnvironment();
			if (RenderLookUI::DrawEnvironment(environment))
			{
				editorScene.SetEnvironment(environment);
				if (activeScene != nullptr && activeScene != &editorScene)
				{
					activeScene->SetEnvironment(environment);
				}
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("Post-processing");
		drawSource("Post-processing from", look.PostProcess);
		if (look.PostProcess == RenderLookSource::Profile && profile != nullptr)
		{
			ImGui::TextDisabled("Shared: edits change every scene using %s.", AssetManager::GetAbsolutePath(look.Profile).filename().u8string().c_str());
			RenderLookUI::EditProfile(profile, false, true);
		}
		else
		{
			if (look.PostProcess == RenderLookSource::Profile)
			{
				ImGui::TextDisabled("Profile missing: the scene's own values are drawn.");
			}
			ScenePostProcess postProcess = editorScene.GetPostProcess();
			if (RenderLookUI::DrawPostProcess(postProcess))
			{
				editorScene.SetPostProcess(postProcess);
				if (activeScene != nullptr && activeScene != &editorScene)
				{
					activeScene->SetPostProcess(postProcess);
				}
			}
		}
	}

	void RenderSettingsWindow::OnImguiRender(Scene* editorScene, Scene* activeScene)
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

			changed |= ImGui::Checkbox("Shadows", &settings.Shadows);
			HelpMarker("Draw blob shadows (Shadow 2D components) in lit views. They darken the light buffer.");

			ImGui::EndDisabled();

			// Sun shadows (render features: Shadows = Sun).
			static const char* mapSizeNames[] = { "1024", "2048", "4096" };
			static const uint32_t mapSizes[] = { 1024, 2048, 4096 };
			int mapSize = settings.SunShadowMapSize <= 1024 ? 0 : (settings.SunShadowMapSize <= 2048 ? 1 : 2);
			if (ImGui::Combo("Sun shadow map", &mapSize, mapSizeNames, 3))
			{
				settings.SunShadowMapSize = mapSizes[mapSize];
				changed = true;
			}
			HelpMarker("Texels across the sun's shadow map. It covers twice the sun's Shadow Distance:\n2048 over 48 m is about 2 cm per texel, finer than a 32-per-metre art pixel.");
			changed |= ImGui::Checkbox("Soft sun shadows", &settings.SoftSunShadows);
			HelpMarker("On: filtered shadow edges.\nOff: one texel per sample, crisp steps that suit pixel art.");

			ImGui::TextDisabled("The light curve is assets/shaders/light_falloff.glsl (both lighting modes).\nWith Shader hot reload on, saving it changes the look of the running editor.");
		}

		// ---- Render features (project default) ----------------------------------
		if (ImGui::CollapsingHeader("Rendering (project)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled("Every scene uses these unless it overrides them below.");
			changed |= DrawRenderFeatures("ProjectFeatures", settings.Features);
		}

		// ---- Render features (scene override) ----------------------------------
		if (ImGui::CollapsingHeader("Rendering (scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (editorScene == nullptr)
			{
				ImGui::TextDisabled("No scene open.");
			}
			else
			{
				// Edits go to the editor scene (what is saved) and, while playing, to the running copy too,
				// so the change is visible at once. Saved with the scene (File > Save Scene).
				SceneRenderSettings sceneSettings = editorScene->GetRenderSettings();
				bool sceneChanged = ImGui::Checkbox("Override the project", &sceneSettings.OverrideProject);
				HelpMarker("Give this scene its own render features (a 3D area in a 2D project, say).");
				if (sceneSettings.OverrideProject)
				{
					sceneChanged |= DrawRenderFeatures("SceneFeatures", sceneSettings.Features);
				}
				else
				{
					ImGui::TextDisabled("Uses the project's render features (%s).", settings.Features.GetPresetName());
				}

				if (sceneChanged)
				{
					editorScene->SetRenderSettings(sceneSettings);
					if (activeScene != nullptr && activeScene != editorScene)
					{
						activeScene->SetRenderSettings(sceneSettings);
					}
				}
			}
		}

		// ---- Look (scene) ------------------------------------------------------
		if (ImGui::CollapsingHeader("Look (scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (editorScene == nullptr)
			{
				ImGui::TextDisabled("No scene open.");
			}
			else
			{
				DrawLook(*editorScene, activeScene);
			}
		}
		RenderLookUI::SaveEditedProfile(false);

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
			StatsUI::Row("Vulkan", "%u.%u.%u", capabilities.VulkanVersionMajor, capabilities.VulkanVersionMinor, capabilities.VulkanVersionPatch);
			StatsUI::Row("Frames in flight", "%u", capabilities.FramesInFlight);
			StatsUI::Row("Viewport", "%u x %u", stats.ViewportWidth, stats.ViewportHeight);
			StatsUI::Row("Quads per batch", "%u", stats.MaxQuadsPerBatch);
			StatsUI::Row("Wireframe support", "%s", capabilities.Wireframe ? "yes" : "no");
			StatsUI::Row("Wide lines support", "%s", capabilities.WideLines ? "yes" : "no");
			StatsUI::Row("Independent blend", "%s", capabilities.IndependentBlend ? "yes" : "no");
			StatsUI::Row("Dynamic rendering", "%s", capabilities.DynamicRendering ? "yes" : "no");
			StatsUI::Row("Synchronization2", "%s", capabilities.Synchronization2 ? "yes" : "no");
			StatsUI::Row("Sampler anisotropy", "%s (max %.0f)", capabilities.SamplerAnisotropy ? "yes" : "no", capabilities.MaxSamplerAnisotropy);
			StatsUI::Row("Descriptor indexing", "%s", capabilities.DescriptorIndexing ? "supported (not enabled)" : "no");
			StatsUI::Row("Push constants", "%u bytes", capabilities.MaxPushConstantsSize);
			StatsUI::Row("Sampled images / stage", "%u", capabilities.MaxSampledImagesPerStage);
			StatsUI::Row("Depth formats", "D32F %s, D24S8 %s", capabilities.DepthFormat32F ? "yes" : "no", capabilities.DepthFormat24Stencil8 ? "yes" : "no");
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
