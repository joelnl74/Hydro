#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/RenderProfile.h>

#include <filesystem>
#include <string>

namespace Hydro
{
	// The look's fields, shared by Render Settings > Look and the inspector of a .hyrender: the environment and
	// post-processing editors, editing a render profile and making a new one.
	class RenderLookUI
	{
	public:
		// The environment's fields (ambient, fog). True when something changed.
		static bool DrawEnvironment(SceneEnvironment& environment);
		// Exposure and bloom. True when something changed.
		static bool DrawPostProcess(ScenePostProcess& postProcess);

		// Both sections of a loaded profile; an edit applies at once to every scene using it and marks it to be saved.
		static void EditProfile(const Ref<RenderProfile>& profile, bool environment = true, bool postProcess = true);
		// Writes the profile edited last to its .hyrender once no field is active (`force`: now). Call it once per
		// frame from every window that edits profiles.
		static void SaveEditedProfile(bool force);

		// Writes a new profile with `data` in <assets>/RenderProfiles and loads it. 0 (logged) when it can't be written.
		static AssetHandle CreateProfile(const std::string& name, const RenderProfileData& data);
		// "<folder>/<name>.hyrender", or "<name> 2.hyrender" when taken; the name is made safe for a file.
		static std::filesystem::path MakeUniqueProfilePath(const std::filesystem::path& folder, std::string name);
	};
}
