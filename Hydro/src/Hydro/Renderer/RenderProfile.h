#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Lighting.h"
#include "Hydro/Renderer/PostProcess.h"

#include <cstdint>
#include <filesystem>

namespace Hydro
{
	// A look (post-processing plan P4): the values a scene's frame is coloured with, apart from how it is
	// built (the render features). Plain data: a scene's own values, or a render profile's.
	struct RenderProfileData
	{
		// Ambient and fog under forward lighting. Override false: no environment of its own (the scene's
		// Global lights are the ambient light, no fog).
		SceneEnvironment Environment;
		// Exposure and bloom of HDR views.
		ScenePostProcess PostProcess;

		bool operator==(const RenderProfileData& other) const { return Environment == other.Environment && PostProcess == other.PostProcess; }
		bool operator!=(const RenderProfileData& other) const { return (*this == other) == false; }
	};

	// Where one section of a scene's look comes from.
	enum class RenderLookSource : uint8_t
	{
		Scene = 0, // the scene's own values (Scene::GetEnvironment / GetPostProcess)
		Profile    // the scene's render profile; its own values while the profile isn't loaded
	};

	// Which render profile a scene uses and for which sections (saved in the scene as "Look", copied on
	// Play). The default uses none: the scene's own values, as before profiles.
	struct SceneLook
	{
		AssetHandle Profile = 0; // a RenderProfile (.hyrender); 0 = none
		RenderLookSource Environment = RenderLookSource::Scene;
		RenderLookSource PostProcess = RenderLookSource::Scene;

		bool operator==(const SceneLook& other) const { return Profile == other.Profile && Environment == other.Environment && PostProcess == other.PostProcess; }
		bool operator!=(const SceneLook& other) const { return (*this == other) == false; }
	};

	// A render profile asset (plan P4): a .hyrender file (RenderProfileSerializer) holding one
	// RenderProfileData, shared by every scene that names its handle, so scenes with the same mood (every
	// town at dusk) keep the same values. The AssetManager loads it (AssetManager::GetAsset<RenderProfile>)
	// and keeps it until the project closes or the file changes.
	//
	// Editable like a MaterialAsset: only data, resolved by the scene every frame (Scene::GetActiveEnvironment,
	// GetActivePostProcess), so SetData changes every scene using it at once; the editor then saves the file.
	class RenderProfile : public Asset
	{
	public:
		explicit RenderProfile(const RenderProfileData& data)
			: m_Data(data) {}

		RenderProfile(const RenderProfile&) = delete;
		RenderProfile& operator=(const RenderProfile&) = delete;

		// Reads a .hyrender file, without sharing: every call makes a new asset. nullptr (logged) when the
		// file can't be read. The AssetManager's loader calls this.
		static Ref<RenderProfile> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::RenderProfile; }
		AssetType GetType() const override { return GetStaticType(); }

		const RenderProfileData& GetData() const { return m_Data; }
		void SetData(const RenderProfileData& data) { m_Data = data; }

	private:
		RenderProfileData m_Data;
	};
}
