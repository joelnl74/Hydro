#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/RenderProfile.h>

#include <filesystem>
#include <string>

namespace Hydro
{
	// The look's fields (post-processing plan P4), shared by Render Settings > Look (scene) and the
	// inspector of a selected .hyrender: the environment and post-processing editors, editing a render
	// profile (its file written once an edit is finished, like a material) and making a new one.
	class RenderLookUI
	{
	public:
		// The environment's fields (own environment, ambient, fog). True when something changed.
		static bool DrawEnvironment(SceneEnvironment& environment);
		// Exposure and the bloom settings. True when something changed.
		static bool DrawPostProcess(ScenePostProcess& postProcess);

		// Both sections of a loaded profile; a change applies to it at once (every scene using it) and
		// marks it to be saved. `environment` / `postProcess`: which sections to show.
		static void EditProfile(const Ref<RenderProfile>& profile, bool environment = true, bool postProcess = true);
		// Writes the profile edited last to its .hyrender once no field is active (`force`: now). Call it
		// once per frame from every window that edits profiles.
		static void SaveEditedProfile(bool force);

		// Writes a new profile with `data` in <assets>/RenderProfiles, named after `name` ("<name>.hyrender",
		// "<name> 2.hyrender"...), and loads it. Returns its handle, 0 (logged) when it couldn't be written.
		static AssetHandle CreateProfile(const std::string& name, const RenderProfileData& data);
		// "<folder>/<name>.hyrender", or "<name> 2.hyrender"... when taken; the name made safe for a file.
		static std::filesystem::path MakeUniqueProfilePath(const std::filesystem::path& folder, std::string name);
	};
}
