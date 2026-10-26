#include "RenderSettingsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Renderer/Renderer.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Project/Project.h>
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
		HelpMarker("2D: painter's order and the 2D light buffer (today's frame).\n3D: shared depth, view-depth sorting, forward lights, cut-out sprites, meshes, sun shadows, HDR.\nChanging a setting below makes it Custom.");

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
		changed |= ImGui::Checkbox("HDR", &features.HDR);
		HelpMarker("The scene renders into a 16-bit float colour, so lanterns and emission can be brighter than white;\nthe scene's exposure and the PBR Neutral tonemap bring it to the screen (Post-processing (scene)).\nThe HUD and debug lines are drawn after the tonemap. Off: the 8-bit colour path.");

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

		// ---- Post-processing (scene) -------------------------------------------
		if (ImGui::CollapsingHeader("Post-processing (scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (editorScene == nullptr)
			{
				ImGui::TextDisabled("No scene open.");
			}
			else
			{
				// Like the environment: edits go to the editor scene and the running copy.
				ScenePostProcess postProcess = editorScene->GetPostProcess();
				bool postProcessChanged = ImGui::DragFloat("Exposure", &postProcess.Exposure, 0.05f, -8.0f, 8.0f, "%.2f EV");
				HelpMarker("HDR views only (render features: HDR). In stops: +1 twice as bright, -1 half. 0 = as lit.");
				if (ImGui::Button("Reset##PostProcess"))
				{
					postProcess = ScenePostProcess{};
					postProcessChanged = true;
				}

				if (postProcessChanged)
				{
					editorScene->SetPostProcess(postProcess);
					if (activeScene != nullptr && activeScene != editorScene)
					{
						activeScene->SetPostProcess(postProcess);
					}
				}
			}
		}

		// ---- Environment (scene) -----------------------------------------------
		if (ImGui::CollapsingHeader("Environment (scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (editorScene == nullptr)
			{
				ImGui::TextDisabled("No scene open.");
			}
			else
			{
				// Like the render features: edits go to the editor scene and the running copy.
				SceneEnvironment environment = editorScene->GetEnvironment();
				bool environmentChanged = ImGui::Checkbox("Own environment", &environment.Override);
				HelpMarker("Forward lighting only. On: this scene's sky and ground ambient and fog.\nOff: its Global lights are the ambient light and there is no fog.");
				if (environment.Override)
				{
					environmentChanged |= ImGui::ColorEdit3("Sky ambient", &environment.SkyAmbient.x, ImGuiColorEditFlags_Float);
					HelpMarker("What surfaces facing up get (linear RGB). Facing sideways: half sky, half ground.");
					environmentChanged |= ImGui::ColorEdit3("Ground ambient", &environment.GroundAmbient.x, ImGuiColorEditFlags_Float);
					environmentChanged |= ImGui::Checkbox("Fog", &environment.Fog);
					ImGui::BeginDisabled(environment.Fog == false);
					environmentChanged |= ImGui::ColorEdit3("Fog color", &environment.FogColor.x, ImGuiColorEditFlags_Float);
					environmentChanged |= ImGui::DragFloatRange2("Fog distance", &environment.FogStart, &environment.FogEnd, 0.25f, 0.0f, 10000.0f, "%.1f m", "%.1f m");
					HelpMarker("Metres from the camera: where fog starts, and where it hides everything.");
					ImGui::EndDisabled();
				}

				if (environmentChanged)
				{
					editorScene->SetEnvironment(environment);
					if (activeScene != nullptr && activeScene != editorScene)
					{
						activeScene->SetEnvironment(environment);
					}
				}
			}
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
