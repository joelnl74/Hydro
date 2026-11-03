#include "EditorLaunch.h"
#include "EditorPaths.h"

#include <Hydro/Core/Log.h>
#include <Hydro/Project/Project.h>
#include <Hydro/Scene/SceneSerializer.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace Hydro
{
	EditorLaunch::EditorLaunch(const EditorLaunchOptions& options, EditorContext& context, GameViewport& gameViewport)
		: m_Options(options), m_Context(context), m_GameViewport(gameViewport)
	{
	}


	EditorLaunchOptions EditorLaunchOptions::Parse(const ApplicationCommandLine& commandLine)
	{
		EditorLaunchOptions options;
		options.Project = commandLine.Get("--project");
		options.Scene = commandLine.Get("--scene");
		options.Play = commandLine.Has("--play");
		const std::string frames = commandLine.Get("--frames");
		if (frames.empty() == false)
		{
			options.Frames = (uint32_t)std::max(0, std::atoi(frames.c_str()));
		}
		options.Capture = commandLine.Get("--capture");
		options.Validate = commandLine.Has("--validate");
		options.Exit = commandLine.Has("--exit") || options.Capture.empty() == false || options.Validate;
		return options;
	}

	void EditorLaunch::Apply()
	{
		if (m_Options.Project.empty() == false)
		{
			std::error_code ec;
			if (std::filesystem::exists(m_Options.Project, ec))
			{
				OnOpenProject(m_Options.Project);
			}
			else
			{
				HY_ERROR("--project: {} doesn't exist", m_Options.Project.string());
			}
		}
		if (m_Options.Scene.empty() == false)
		{
			const std::filesystem::path scene = FindScene(m_Options.Scene);
			if (scene.empty())
			{
				HY_ERROR("--scene: no scene '{}' in the project", m_Options.Scene);
			}
			else
			{
				OnOpenScene(scene);
			}
		}

		if (m_Options.Validate)
		{
			const bool clean = ValidateProject();
			Application::Get().Quit(clean ? 0 : 1);
			return;
		}
		if (m_Options.Play)
		{
			OnPlay();
		}
		if (m_Options.IsAutomated())
		{
			// The Game view is drawn whatever the docking says, and at the capture size.
			if (m_Options.Capture.empty() == false)
			{
				m_GameViewport.SetFullscreen(true);
				m_Context.Captures.FixedSize = true;
			}
			m_Step = Step::Waiting;
			m_FramesLeft = m_Options.Frames;
			HY_INFO("Unattended run: {} frames{}{}", m_Options.Frames,
				m_Options.Capture.empty() ? "" : ", then capture " + m_Options.Capture.string(),
				m_Options.Exit ? ", then exit" : "");
		}
	}

	std::filesystem::path EditorLaunch::FindScene(const std::string& nameOrPath)
	{
		std::error_code ec;
		const std::filesystem::path given = nameOrPath;
		if (given.extension().string() == SceneSerializer::FileExtension && std::filesystem::exists(given, ec))
		{
			return given;
		}
		if (Project::GetActive() == nullptr)
		{
			return {};
		}
		const std::filesystem::path inProject = EditorPaths::GetScenesDirectory() / (given.stem().string() + SceneSerializer::FileExtension);
		if (std::filesystem::exists(inProject, ec))
		{
			return inProject;
		}
		return {};
	}

	bool EditorLaunch::ValidateProject()
	{
		if (Project::GetActive() == nullptr)
		{
			HY_ERROR("--validate: no project is open");
			return false;
		}

		std::vector<LogMessage> messages;
		const uint64_t before = Log::GetMessagesSince(0, messages);
		uint32_t scenes = 0;
		std::error_code ec;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(Project::GetActive()->GetAssetDirectory(), ec))
		{
			if (entry.is_regular_file() == false || entry.path().extension().string() != SceneSerializer::FileExtension)
			{
				continue;
			}
			// Into a scratch scene, like OpenScene; the serializer logs every missing asset and script.
			Ref<Scene> scene = CreateRef<Scene>("Validate", true);
			SceneSerializer serializer(scene);
			if (serializer.Deserialize(entry.path()))
			{
				HY_INFO("Validated {}: {} entities", entry.path().filename().string(), scene->m_Registry.size<IDComponent>());
			}
			else
			{
				HY_ERROR("Validate: {} failed to load", entry.path().filename().string());
			}
			scenes++;
		}

		messages.clear();
		Log::GetMessagesSince(before, messages);
		uint32_t problems = 0;
		for (const LogMessage& message : messages)
		{
			if (message.Level >= LogLevel::Warn)
			{
				problems++;
			}
		}
		HY_INFO("Validated {} scenes: {} warnings or errors", scenes, problems);
		return problems == 0;
	}

	void EditorLaunch::Update()
	{
		switch (m_Step)
		{
			case Step::Waiting:
			{
				if (m_FramesLeft > 0)
				{
					m_FramesLeft--;
					return;
				}
				if (m_Options.Capture.empty())
				{
					m_Step = Step::Done;
					return;
				}
				// Through the normal capture request, so the view is drawn at the capture size first.
				std::error_code ec;
				std::filesystem::create_directories(m_Options.Capture.parent_path(), ec);
				m_Context.Captures.Game = true;
				m_Context.Captures.GamePath = m_Options.Capture;
				m_Step = Step::Capturing;
				return;
			}
			case Step::Capturing:
			{
				// The request is taken when the Game view is drawn, and the file is written after that frame.
				if (m_Context.Captures.Game == false && m_GameViewport.IsCapturePending() == false)
				{
					HY_INFO("Captured the Game view to {}", m_Options.Capture.string());
					m_Step = Step::Done;
				}
				return;
			}
			case Step::Done:
			{
				if (m_Options.Exit)
				{
					Application::Get().Quit(0);
				}
				m_Step = Step::None;
				return;
			}
			case Step::None:
			{
				return;
			}
		}
	}
}
