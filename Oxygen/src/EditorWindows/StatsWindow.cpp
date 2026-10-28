#include "StatsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Core/Time.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Scene/Scene.h>
#include <Hydro/Scene/ParticleScene.h>
#include <Hydro/Scene/Scripting/ScriptableEntity.h>

#include <algorithm>
#include <cstdio>

namespace Hydro
{
	void StatsWindow::RecordFrame(float frameMs)
	{
		m_FrameTimes[m_HistoryOffset] = frameMs;
		m_HistoryOffset = (m_HistoryOffset + 1) % HistorySize;
		m_HistoryCount = std::min(m_HistoryCount + 1, HistorySize);

		// Avg / min / max / 1% low over the history window.
		std::array<float, HistorySize> sorted;
		std::copy_n(m_FrameTimes.begin(), m_HistoryCount, sorted.begin());
		std::sort(sorted.begin(), sorted.begin() + m_HistoryCount);

		float sum = 0.0f;
		for (int i = 0; i < m_HistoryCount; i++)
			sum += sorted[i];

		m_AvgMs = sum / m_HistoryCount;
		m_MinMs = sorted[0];
		m_MaxMs = sorted[m_HistoryCount - 1];

		// "1% low" = the frame rate of the slowest 1% of frames (99th percentile frame time).
		const int p99 = std::min(m_HistoryCount - 1, (int)(m_HistoryCount * 0.99f));
		m_OnePercentLowFps = sorted[p99] > 0.0f ? 1000.0f / sorted[p99] : 0.0f;
	}

	void StatsWindow::OnImguiRender(const Ref<Scene>& scene)
	{
		// Keep sampling while the window is closed so the history is ready when reopened.
		const float frameMs = Time::GetFrameTimings().DeltaMs;
		if (m_Paused == false && frameMs > 0.0f)
			RecordFrame(frameMs);

		if (m_Open == false)
			return;

		ImGui::SetNextWindowSize(ImVec2(380.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Stats", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		DrawSummary();

		ImGui::Spacing();
		if (ImGui::BeginTabBar("##stats_tabs"))
		{
			if (ImGui::BeginTabItem("Frame"))
			{
				DrawFrameTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Rendering"))
			{
				m_RendererPanel.Draw();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Scene"))
			{
				DrawSceneTab(scene);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	void StatsWindow::DrawSummary()
	{
		if (m_HistoryCount == 0)
		{
			ImGui::TextDisabled("Collecting samples...");
			return;
		}

		const float currentMs = m_FrameTimes[(m_HistoryOffset + HistorySize - 1) % HistorySize];
		const float avgFps = m_AvgMs > 0.0f ? 1000.0f / m_AvgMs : 0.0f;

		// Headline: average FPS over the window (steadier than a single frame), coloured by target.
		const ImVec4 color = avgFps >= 55.0f ? ImVec4(0.45f, 0.85f, 0.45f, 1.0f)
		                   : avgFps >= 30.0f ? ImVec4(0.95f, 0.80f, 0.30f, 1.0f)
		                                     : ImVec4(0.95f, 0.40f, 0.35f, 1.0f);
		ImGui::SetWindowFontScale(1.8f);
		ImGui::TextColored(color, "%.0f FPS", avgFps);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%.2f ms avg  |  %.2f ms last", m_AvgMs, currentMs);

		if (StatsUI::BeginStatTable("##summary"))
		{
			StatsUI::Row("Min / max frame", "%.2f / %.2f ms", m_MinMs, m_MaxMs);
			StatsUI::Row("Best / worst FPS", "%.0f / %.0f", m_MinMs > 0 ? 1000.0f / m_MinMs : 0.0f, m_MaxMs > 0 ? 1000.0f / m_MaxMs : 0.0f);
			StatsUI::Row("1% low", "%.0f FPS", m_OnePercentLowFps);

			const FrameTimings& timings = Time::GetFrameTimings();
			const int seconds = (int)timings.UptimeSeconds;
			StatsUI::Row("Uptime", "%02d:%02d:%02d  (%llu frames)", seconds / 3600, (seconds / 60) % 60, seconds % 60,
				(unsigned long long)timings.FrameCount);
			ImGui::EndTable();
		}

		// Frame-time graph. Spikes here are stutters.
		char overlay[64];
		snprintf(overlay, sizeof(overlay), "frame time (last %d frames)", m_HistoryCount);
		const float scaleMax = std::max(m_MaxMs * 1.2f, 20.0f);
		ImGui::PlotLines("##frametimes", m_FrameTimes.data(), HistorySize, m_HistoryOffset, overlay, 0.0f, scaleMax, ImVec2(-1.0f, 70.0f));

		ImGui::Checkbox("Pause", &m_Paused);
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			m_FrameTimes.fill(0.0f);
			m_HistoryOffset = 0;
			m_HistoryCount = 0;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(graph max %.0f ms)", scaleMax);
	}

	void StatsWindow::DrawFrameTab()
	{
		const FrameTimings& t = Time::GetFrameTimings();
		const float frame = t.FrameMs > 0.0f ? t.FrameMs : 1.0f;

		ImGui::TextDisabled("CPU time per phase (last frame)");
		if (StatsUI::BeginStatTable("##cpu_phases"))
		{
			char text[32];
			auto phase = [&](const char* label, float ms)
			{
				snprintf(text, sizeof(text), "%.3f ms", ms);
				StatsUI::BarRow(label, ms / frame, text);
			};

			phase("Events", t.GetPhaseMs(FramePhase::Events));
			phase("UI build (ImGui)", t.GetPhaseMs(FramePhase::ImGui));
			phase("Update (scene)", t.GetPhaseMs(FramePhase::Update));
			phase("Render 2D (CPU)", t.GetPhaseMs(FramePhase::Render));
			phase("Present", t.GetPhaseMs(FramePhase::Present));
			phase("Extra ImGui windows", t.GetPhaseMs(FramePhase::PlatformWindows));
			StatsUI::Row("Total", "%.3f ms", t.FrameMs);
			ImGui::EndTable();
		}

		ImGui::Spacing();
		ImGui::TextDisabled("GPU");
		Statistics stats = Renderer2D::GetStats();
		if (StatsUI::BeginStatTable("##gpu_time"))
		{
			if (stats.GPUTimeMs >= 0.0f)
				StatsUI::Row("2D pass", "%.3f ms", stats.GPUTimeMs);
			else
				StatsUI::Row("2D pass", "n/a");

			// Lighting parts (the light pass includes the shadows; parts are timed separately, so they
			// may overlap the 2D pass total a little).
			if (stats.LitViewCount > 0)
			{
				if (stats.LightPassGPUMs >= 0.0f)
				{
					StatsUI::Row("  Light pass", "%.3f ms", stats.LightPassGPUMs);
					StatsUI::Row("    Shadows", "%.3f ms", stats.ShadowGPUMs);
					StatsUI::Row("  Composite", "%.3f ms", stats.CompositeGPUMs);
				}
				else
				{
					StatsUI::Row("  Light pass", "n/a");
				}

				StatsUI::Row("  Lit views", "%u", stats.LitViewCount);
				StatsUI::Row("  Lights", "%u", stats.LightCount);
				StatsUI::Row("  Shadows drawn", "%u", stats.ShadowCount);
			}
			ImGui::EndTable();
		}

		// Rough bottleneck hint: Present blocks on the swapchain fence / vsync, so when it
		// dominates the frame the CPU is waiting on the GPU or the display, not the other way around.
		ImGui::Spacing();
		const bool waitingOnPresent = t.GetPhaseMs(FramePhase::Present) > 0.5f * frame;
		ImGui::TextWrapped("Limited by: %s", waitingOnPresent
			? "GPU or VSync (most of the frame is spent waiting in Present)"
			: "CPU (Update / UI / Render 2D take most of the frame)");
	}

	void StatsWindow::DrawSceneTab(const Ref<Scene>& scene)
	{
		if (scene == nullptr)
		{
			ImGui::TextDisabled("No scene loaded.");
			return;
		}

		entt::registry& registry = scene->m_Registry;
		if (StatsUI::BeginStatTable("##scene"))
		{
			StatsUI::Row("Entities", "%zu", (size_t)registry.alive());
			StatsUI::Row("Sprites", "%zu", (size_t)registry.view<SpriteRendererComponent>().size());
			StatsUI::Row("Transforms", "%zu", (size_t)registry.view<TransformComponent>().size());
			size_t scriptCount = 0;
			auto scripts = registry.view<NativeScriptComponent>();
			for (auto entity : scripts)
				scriptCount += scripts.get<NativeScriptComponent>(entity).Scripts.size();
			StatsUI::Row("Native scripts", "%zu", scriptCount);
			StatsUI::Row("Particle emitters", "%zu", (size_t)registry.view<ParticleEmitter2DComponent>().size());
			if (const ParticleScene* particles = scene->GetParticleScene())
			{
				const ParticleScene::Statistics& stats = particles->GetStats();
				StatsUI::Row("  Live particles", "%u of %u", stats.LiveParticles, ParticleScene::MaxLiveParticles);
				StatsUI::Row("  Running emitters", "%u", stats.Emitters);
				StatsUI::Row("  Reserved slots", "%u", stats.ReservedParticles);
				StatsUI::Row("  Dropped (last update)", "%u", stats.DroppedParticles);
				StatsUI::Row("  Update", "%.3f ms", stats.UpdateMs);
			}
			else
			{
				StatsUI::Row("  Live particles", "only while playing");
			}
			ImGui::EndTable();
		}
	}
}
