#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// A scene's look under forward lighting (plan M6), saved in the scene (SceneSerializer, "Environment")
	// and copied on Play. Without it (Override false) the scene's Global lights are its ambient light (sky
	// and ground alike) and there is no fog, so a 2D scene switched to the 3D preset keeps its mood.
	struct SceneEnvironment
	{
		bool Override = false;
		// Hemisphere ambient: surfaces facing up get the sky colour, facing down the ground colour.
		glm::vec3 SkyAmbient{ 0.45f, 0.5f, 0.6f };   // linear RGB
		glm::vec3 GroundAmbient{ 0.22f, 0.2f, 0.18f };
		bool Fog = false;
		glm::vec3 FogColor{ 0.4f, 0.42f, 0.5f };
		float FogStart = 25.0f; // metres from what the camera looks at (its point on the ground) where fog starts
		float FogEnd = 70.0f;   // and where it is complete

		bool operator==(const SceneEnvironment& other) const
		{
			return Override == other.Override && SkyAmbient == other.SkyAmbient && GroundAmbient == other.GroundAmbient && Fog == other.Fog
				&& FogColor == other.FogColor && FogStart == other.FogStart && FogEnd == other.FogEnd;
		}
		bool operator!=(const SceneEnvironment& other) const { return (*this == other) == false; }
	};

	// Point and spot lights one view is lit with: the nearest to the camera are kept (Renderer2D warns once).
	constexpr uint32_t MaxForwardLightsPerView = 64;

	enum class ForwardLightType : uint32_t
	{
		Point = 0,
		Spot = 1
	};

	// A point or spot light as the forward shaders see it: world space, Z up.
	struct ForwardLight
	{
		ForwardLightType Type = ForwardLightType::Point;
		glm::vec3 Position{ 0.0f };
		float Range = 5.0f;
		glm::vec3 Color{ 1.0f }; // linear RGB, already times the intensity
		glm::vec3 Direction{ 0.0f, 0.0f, -1.0f }; // spot: where it points
		float CosInner = 1.0f;
		float CosOuter = 1.0f;
	};

	// Everything a view is lit with this frame: SceneRenderer gathers it from the scene and hands it to
	// Renderer2D::SetSceneLighting, which uploads it into the view's set 0 (VulkanGPUData.h,
	// FrameLightingUniform and the light storage buffer).
	struct SceneLighting
	{
		glm::vec3 CameraPosition{ 0.0f };
		bool HasSun = false;
		glm::vec3 SunDirection{ 0.0f, 0.0f, -1.0f }; // where the light travels
		glm::vec3 SunColor{ 0.0f };                   // times the intensity
		glm::vec3 SkyAmbient{ 0.0f };
		glm::vec3 GroundAmbient{ 0.0f };
		bool Fog = false;
		glm::vec3 FogColor{ 0.0f };
		// Fog distance is measured from here, not from the camera: the point on the ground the camera looks at.
		// So a camera far above the scene (the editor's top-down view) sees it as clearly as the tilted one.
		glm::vec3 FogOrigin{ 0.0f };
		float FogStart = 0.0f;
		float FogEnd = 1.0f;
		std::vector<ForwardLight> Lights;
	};
}
