#include "hypch.h"
#include "SceneLightingBuffer.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	static bool s_LightLimitWarned = false;

	void SceneLightingBuffer::Upload(RenderViewData& view, uint32_t frame)
	{
		FrameLightingUniform uniform;
		const SceneLighting& lighting = view.Lighting;
		const bool forward = view.Features.Lighting == LightingMode::Forward && view.LightingSubmitted;
		uint32_t count = 0;

		if (forward)
		{
			Statistics& stats = ViewRenderer::GetFrameStats();
			uniform.CameraPosition = glm::vec4(lighting.CameraPosition, 1.0f);
			if (lighting.HasSun)
			{
				const float length = glm::length(lighting.SunDirection);
				const glm::vec3 towardsSun = length > 0.0f ? -lighting.SunDirection / length : glm::vec3(0.0f, 0.0f, 1.0f);
				uniform.SunDirection = glm::vec4(towardsSun, 1.0f);
				uniform.SunColor = glm::vec4(lighting.SunColor, 0.0f);
			}
			uniform.SkyAmbient = glm::vec4(lighting.SkyAmbient, 0.0f);
			uniform.GroundAmbient = glm::vec4(lighting.GroundAmbient, 0.0f);
			uniform.FogColor = glm::vec4(lighting.FogColor, lighting.Fog ? 1.0f : 0.0f);
			uniform.FogOrigin = glm::vec4(lighting.FogOrigin, 1.0f);
			const RendererSettings& settings = Renderer::GetSettings();
			const float bands = settings.LightFalloff == LightFalloffStyle::Banded ? (float)std::clamp(settings.LightBands, 2u, 32u) : 0.0f;
			uniform.FogParams = glm::vec4(lighting.FogStart, std::max(lighting.FogEnd, lighting.FogStart + 0.01f), bands, 0.0f);

			// Ranked by distance from FogOrigin (where the camera's view meets the ground) less the light's range, so a wide
			// pool by the player outranks a small lamp under the camera, and a tilted camera keeps the lights in front of it.
			std::vector<const ForwardLight*> lights;
			lights.reserve(lighting.Lights.size());
			for (const ForwardLight& light : lighting.Lights)
			{
				lights.push_back(&light);
			}
			if (lights.size() > MaxForwardLights)
			{
				const glm::vec3 focus = lighting.FogOrigin;
				std::partial_sort(lights.begin(), lights.begin() + MaxForwardLights, lights.end(),
					[focus](const ForwardLight* a, const ForwardLight* b)
					{
						return glm::length(a->Position - focus) - a->Range < glm::length(b->Position - focus) - b->Range;
					});
				stats.ForwardLightsDropped += (uint32_t)(lights.size() - MaxForwardLights);
				if (s_LightLimitWarned == false)
				{
					HY_CORE_WARN("Renderer2D: view '{}' has {} point and spot lights; the {} reaching nearest to what the camera looks at "
						"are used", view.DebugName, lights.size(), MaxForwardLights);
					s_LightLimitWarned = true;
				}
				lights.resize(MaxForwardLights);
			}

			std::array<GPULight, MaxForwardLights> gpu{};
			for (const ForwardLight* light : lights)
			{
				GPULight& out = gpu[count++];
				out.PositionRange = glm::vec4(light->Position, std::max(light->Range, 0.01f));
				out.ColorType = glm::vec4(light->Color, light->Type == ForwardLightType::Spot ? 1.0f : 0.0f);
				out.DirectionCosInner = glm::vec4(light->Direction, light->CosInner);
				out.Params = glm::vec4(light->CosOuter, 0.0f, 0.0f, 0.0f);
			}
			if (count > 0)
			{
				view.LightStorageBuffers[frame]->SetData(gpu.data(), sizeof(GPULight) * count);
			}
			uniform.Counts = glm::ivec4((int32_t)count, 1, view.SunShadowsActive ? 1 : 0, view.Features.PixelSnap ? 1 : 0);
			if (view.SunShadowsActive)
			{
				const float mapSize = (float)view.ShadowTarget->GetWidth();
				const float distance = std::max(lighting.ShadowDistance, 1.0f);
				// Receivers are pushed out of their own shadow by about a texel and a half (along the surface normal,
				// or towards the sun for sprites).
				const float texelWorld = 2.0f * distance / mapSize;
				uniform.ShadowMatrix = view.ShadowMatrix;
				uniform.ShadowParams = glm::vec4(std::clamp(lighting.ShadowStrength, 0.0f, 1.0f), 1.0f / mapSize,
					Renderer::GetSettings().SoftSunShadows ? 1.0f : 0.0f, texelWorld * 1.5f * std::max(lighting.ShadowBias, 0.0f));
			}
			stats.ForwardLitViewCount++;
			stats.ForwardLightCount += count;
		}

		view.LightingBuffers[frame]->SetData(&uniform, sizeof(FrameLightingUniform));
	}
}
