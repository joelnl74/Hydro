#pragma once
#include "EditorWindows/EditorStyle.h"

#include <filesystem>

namespace Hydro
{
	// The Oxygen editor's own settings: how the editor looks, not the game or the engine. Kept per
	// user in EditorSettings.yaml next to imgui.ini (EditorPaths::GetEditorSettingsPath), not in the
	// project. Edited live in the Editor Settings window; File > Save there writes them.
	struct EditorSettings
	{
		EditorStyle Style = EditorStyle::FromTheme(EditorTheme::Hydro);
	};

	// Reads and writes EditorSettings as YAML. Missing keys keep their defaults, so the file still
	// loads after a setting is added.
	class EditorSettingsSerializer
	{
	public:
		static bool Save(const EditorSettings& settings, const std::filesystem::path& path);
		// Returns false (settings unchanged) when the file is missing or unreadable.
		static bool Load(const std::filesystem::path& path, EditorSettings& settings);
	};
}
