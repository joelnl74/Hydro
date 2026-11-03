#pragma once
#include "EditorContext.h"

#include <filesystem>
#include <string>

namespace Hydro
{
	// The Capture menu (in the Editor and Game windows' menu bars) and its keys. The views take the requests when they
	// are drawn.
	class CaptureControls
	{
	public:
		static void DrawMenu(CaptureRequests& requests);
		// F9: the Game view (also in full screen); Shift+F9: the Editor view.
		static void UpdateKeys(CaptureRequests& requests);
		// <project>/.hydro/captures/<scene>_<view>_<yyyymmdd-hhmmss-mmm>.png
		static std::filesystem::path MakePath(const Scene* scene, const std::string& viewName);
	};
}
