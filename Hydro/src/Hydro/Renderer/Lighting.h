#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// A scene's look under forward lighting, saved in the scene. Without it (Override off) its Global lights are the
	// ambient and there is no fog, so a 2D scene switched to the 3D preset keeps its mood.
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

	// The lights one view is lit with: those reaching nearest to the camera's ground focus are kept (warned once).
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

	// Everything a view is lit with this frame, gathered by SceneRenderer and uploaded by Renderer2D::SetSceneLighting.
	struct SceneLighting
	{
		glm::vec3 CameraPosition{ 0.0f };
		bool HasSun = false;
		glm::vec3 SunDirection{ 0.0f, 0.0f, -1.0f }; // where the light travels
		glm::vec3 SunColor{ 0.0f };                   // times the intensity
		// Drawn when the view has the SunShadows feature; the map covers ShadowDistance metres around FogOrigin.
		bool SunShadows = false;
		float ShadowStrength = 1.0f;
		float ShadowDistance = 24.0f;
		float ShadowBias = 1.0f;
		glm::vec3 SkyAmbient{ 0.0f };
		glm::vec3 GroundAmbient{ 0.0f };
		bool Fog = false;
		glm::vec3 FogColor{ 0.0f };
		// Fog is measured from the ground point the camera looks at, so a camera far above sees as clearly as a tilted one.
		glm::vec3 FogOrigin{ 0.0f };
		float FogStart = 0.0f;
		float FogEnd = 1.0f;
		std::vector<ForwardLight> Lights;
	};
}
