#include "BuildWindow.h"

#include <imgui/imgui.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Core/Time.h>
#include <Hydro/Project/Project.h>

#include <algorithm>

namespace Hydro
{
	// Keeps the log readable and bounded: a full build writes thousands of lines.
	static constexpr size_t MaxLogSize = 256 * 1024;

	const char* BuildWindow::GetKindName(BuildKind kind)
	{
		return kind == BuildKind::Release ? "Release" : "Distribution";
	}

	std::string BuildWindow::GetProjectName() const
	{
		const Ref<Project> project = Project::GetActive();
		return project != nullptr ? project->GetProjectFilePath().stem().u8string() : std::string();
	}

	std::filesystem::path BuildWindow::GetOutputDirectory() const
	{
		return m_WorkspaceRoot / "Builds" / (GetProjectName() + "-" + GetKindName(m_Kind));
	}

	void BuildWindow::OnImguiRender()
	{
		// The build is followed even while the window is closed.
		UpdateBuild();

		if (m_Open == false)
		{
			return;
		}

		if (m_RootsFound == false)
		{
#if defined(HY_HYDRO_ROOT) && defined(HY_WORKSPACE_DIR)
			std::error_code ec;
			if (std::filesystem::exists(std::filesystem::path(HY_HYDRO_ROOT) / "scripts" / "Package.bat", ec))
			{
				m_EngineRoot = HY_HYDRO_ROOT;
				m_WorkspaceRoot = HY_WORKSPACE_DIR;
			}
#endif
			m_RootsFound = true;
		}

		ImGui::SetNextWindowSize(ImVec2(620.0f, 460.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Build", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		const Ref<Project> project = Project::GetActive();
		const bool building = m_State == BuildState::Building;
		if (project == nullptr)
		{
			ImGui::TextDisabled("No project is open.");
			ImGui::End();
			return;
		}
		if (m_EngineRoot.empty())
		{
			ImGui::TextWrapped("The Hydro checkout this editor was built from (scripts/Package.bat) isn't there: builds need it.");
			ImGui::End();
			return;
		}

		ImGui::Text("Project: %s", GetProjectName().c_str());

		ImGui::BeginDisabled(building);
		int kind = (int)m_Kind;
		ImGui::RadioButton("Release", &kind, (int)BuildKind::Release);
		ImGui::SameLine();
		ImGui::RadioButton("Distribution", &kind, (int)BuildKind::Distribution);
		m_Kind = (BuildKind)kind;
		ImGui::EndDisabled();
		ImGui::TextDisabled(m_Kind == BuildKind::Release ? "Optimised, with a console window and logs: for testing."
			: "Optimised, no console window: for players.");
		ImGui::Text("Output: %s", GetOutputDirectory().u8string().c_str());

		ImGui::Spacing();
		if (building)
		{
			if (ImGui::Button("Cancel"))
			{
				m_Process->Kill();
				m_State = BuildState::Cancelled;
				m_Duration = Time::GetUptime() - m_StartTime;
				m_Log += "\n[Build] Cancelled\n";
				m_ScrollToEnd = true;
			}
			ImGui::SameLine();
			ImGui::Text("Building... %.0f s", Time::GetUptime() - m_StartTime);
		}
		else
		{
			if (ImGui::Button("Build"))
			{
				StartBuild();
			}

			ImGui::SameLine();
			if (m_State == BuildState::Succeeded)
			{
				ImGui::Text("Build succeeded (%.0f s): %s.exe", m_Duration, m_BuiltName.c_str());
			}
			else if (m_State == BuildState::Failed)
			{
				ImGui::Text("Build failed (exit code %d, %.0f s): see the output below", m_ExitCode, m_Duration);
			}
			else if (m_State == BuildState::Cancelled)
			{
				ImGui::TextDisabled("Cancelled");
			}

			if (m_State == BuildState::Succeeded)
			{
				if (ImGui::Button("Run"))
				{
					RunGame();
				}
				ImGui::SameLine();
				if (ImGui::Button("Open Folder"))
				{
					OpenFolder();
				}
			}
		}

		ImGui::Separator();
		ImGui::BeginChild("##BuildOutput", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(m_Log.c_str(), m_Log.c_str() + m_Log.size());
		// Follow new output while the view is at the bottom.
		if (m_ScrollToEnd && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f * ImGui::GetTextLineHeight())
		{
			ImGui::SetScrollHereY(1.0f);
		}
		m_ScrollToEnd = false;
		ImGui::EndChild();

		ImGui::End();
	}

	void BuildWindow::StartBuild()
	{
		if (OnBeforeBuild)
		{
			OnBeforeBuild();
		}

		const Ref<Project> project = Project::GetActive();
		std::error_code ec;
		const std::filesystem::path projectFile = std::filesystem::absolute(project->GetProjectFilePath(), ec);
		const std::filesystem::path script = m_EngineRoot / "scripts" / "Package.bat";

		// cmd /s /c "<line>" runs <line> as it is, quotes and all.
		ProcessSpecification specification;
		specification.CommandLine = "cmd.exe /d /s /c \"\"" + script.u8string() + "\" " + GetKindName(m_Kind) + " --workspace \""
			+ m_WorkspaceRoot.u8string() + "\" --project \"" + projectFile.u8string() + "\" --no-pause\"";
		specification.WorkingDirectory = m_WorkspaceRoot;

		m_Log = "> scripts\\Package.bat " + std::string(GetKindName(m_Kind)) + " --workspace " + m_WorkspaceRoot.u8string()
			+ " --project " + projectFile.u8string() + "\n";
		m_Process = CreateScope<Process>(specification);
		m_StartTime = Time::GetUptime();
		m_BuiltDirectory = GetOutputDirectory();
		m_BuiltName = GetProjectName();
		m_ScrollToEnd = true;

		if (m_Process->IsValid() == false)
		{
			m_State = BuildState::Failed;
			m_ExitCode = -1;
			m_Duration = 0.0;
			m_Log += "[Build] Could not start the package script (see the Console)\n";
			return;
		}

		m_State = BuildState::Building;
		HY_INFO("Build started: {} {}", m_BuiltName, GetKindName(m_Kind));
	}

	void BuildWindow::UpdateBuild()
	{
		if (m_Process == nullptr)
		{
			return;
		}

		// Output keeps coming after the script ends, until the pipe is empty.
		std::string output = m_Process->ReadOutput();
		if (output.empty() == false)
		{
			output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
			m_Log += output;
			if (m_Log.size() > MaxLogSize)
			{
				m_Log.erase(0, m_Log.size() - MaxLogSize);
			}
			m_ScrollToEnd = true;
		}

		if (m_State == BuildState::Building && m_Process->IsRunning() == false)
		{
			m_ExitCode = m_Process->GetExitCode();
			m_Duration = Time::GetUptime() - m_StartTime;
			m_State = m_ExitCode == 0 ? BuildState::Succeeded : BuildState::Failed;
			if (m_State == BuildState::Succeeded)
			{
				HY_INFO("Build succeeded: {}", (m_BuiltDirectory / (m_BuiltName + ".exe")).u8string());
			}
			else
			{
				HY_WARN("Build failed (exit code {}): see the Build window", m_ExitCode);
			}
		}
	}

	void BuildWindow::RunGame()
	{
		// The game runs on its own: closing the editor or building again doesn't stop it.
		const std::filesystem::path executable = m_BuiltDirectory / (m_BuiltName + ".exe");
		ProcessSpecification specification;
		specification.CommandLine = "\"" + executable.u8string() + "\"";
		specification.WorkingDirectory = m_BuiltDirectory;
		Process::Launch(specification);
	}

	void BuildWindow::OpenFolder()
	{
		ProcessSpecification specification;
		specification.CommandLine = "explorer.exe \"" + m_BuiltDirectory.u8string() + "\"";
		Process::Launch(specification);
	}
}
