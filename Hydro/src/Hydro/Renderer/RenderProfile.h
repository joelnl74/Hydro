#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Lighting.h"
#include "Hydro/Renderer/PostProcess.h"

#include <cstdint>
#include <filesystem>

namespace Hydro
{
	// A look: the values a scene's frame is coloured with, apart from the render features. A scene's own or a profile's.
	struct RenderProfileData
	{
		// Ambient and fog under forward lighting. Override off: the Global lights are the ambient, no fog.
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

	// Which render profile a scene uses and for which sections. The default uses none (the scene's own values).
	struct SceneLook
	{
		AssetHandle Profile = 0; // a RenderProfile (.hyrender); 0 = none
		RenderLookSource Environment = RenderLookSource::Scene;
		RenderLookSource PostProcess = RenderLookSource::Scene;
		bool operator==(const SceneLook& other) const { return Profile == other.Profile && Environment == other.Environment && PostProcess == other.PostProcess; }
		bool operator!=(const SceneLook& other) const { return (*this == other) == false; }
	};

	// A .hyrender profile asset shared by every scene that names it, so scenes with the same mood keep the same values.
	// Only data, resolved by the scene every frame, so SetData changes every scene using it at once.
	class RenderProfile : public Asset
	{
	public:
		explicit RenderProfile(const RenderProfileData& data)
			: m_Data(data) {}

		RenderProfile(const RenderProfile&) = delete;
		RenderProfile& operator=(const RenderProfile&) = delete;

		// Reads a .hyrender, unshared (the AssetManager's loader). nullptr (logged) when it can't be read.
		static Ref<RenderProfile> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::RenderProfile; }
		AssetType GetType() const override { return GetStaticType(); }

		const RenderProfileData& GetData() const { return m_Data; }
		void SetData(const RenderProfileData& data) { m_Data = data; }

	private:
		RenderProfileData m_Data;
	};
}
