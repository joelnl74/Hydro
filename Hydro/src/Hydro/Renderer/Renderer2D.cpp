#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/PassTimers.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/QuadBatcher.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"
#include "Hydro/Core/Time.h"
#include "Hydro/Renderer/Renderer.h"

#include <Hydro/Platform/Vulkan/imgui_impl_vulkan.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace Hydro
{
	// One ImGui texture handle (from ImGui's descriptor pool) and the texture it shows.
	struct ImGuiTextureEntry
	{
		VkDescriptorSet Set = VK_NULL_HANDLE;
		Ref<Texture2D> Texture;     // kept alive while the handle may be drawn
		uint64_t LastUsedFrame = 0; // Time::GetFrameCount()
	};

	struct Renderer2DData
	{
		RenderViewData* CurrentView = nullptr; // the view between BeginScene and EndScene
		glm::vec4 QuadVertexPositions[4];       // the unit quad, counter-clockwise from the bottom-left

		Ref<Texture2D> SoftDotTexture;
		Ref<Texture2D> BlobShadowTexture;

		// Sets are never freed (this ImGui version has no RemoveTexture); they are rewritten for another texture.
		std::vector<ImGuiTextureEntry> ImGuiTextures;

		bool LightLimitWarned = false;
		bool ShadowLimitWarned = false;
	};

	static Renderer2DData* s_Data = nullptr;

	// White, alpha falling from the centre to the edge of a size x size image by `falloff(r)`.
	template<typename Falloff>
	static Ref<Texture2D> MakeRoundTexture(uint32_t size, Falloff falloff)
	{
		std::vector<uint32_t> pixels(size * size);
		for (uint32_t y = 0; y < size; y++)
		{
			for (uint32_t x = 0; x < size; x++)
			{
				const float dx = ((float)x + 0.5f) / (float)size * 2.0f - 1.0f;
				const float dy = ((float)y + 0.5f) / (float)size * 2.0f - 1.0f;
				const uint32_t alpha = (uint32_t)std::lround(falloff(std::sqrt(dx * dx + dy * dy)) * 255.0f);
				pixels[y * size + x] = (alpha << 24) | 0x00ffffffu; // RGBA bytes in memory: ff ff ff alpha
			}
		}
		ImageSpecification specification;
		specification.Width = size;
		specification.Height = size;
		specification.Format = ImageFormat::SRGB;
		TextureProperties properties;
		properties.SamplerWrap = TextureWrap::Clamp;
		return CreateRef<Texture2D>(specification, pixels.data(), properties);
	}

	void Renderer2D::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "Renderer2D::Init called twice");
		ViewRenderer::Init();
		s_Data = new Renderer2DData();

		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_Data->SoftDotTexture = MakeRoundTexture(32, [](float r)
		{
			const float falloff = std::max(0.0f, 1.0f - r);
			return falloff * falloff;
		});
		// 1 - smoothstep(0, 1, r), the curve shadow.frag gives the light buffer's blobs, so forward-lit contact shadows read
		// as strongly as 2D ones. The soft dot is half as dark halfway out: only a faint smudge at a sprite's feet.
		s_Data->BlobShadowTexture = MakeRoundTexture(64, [](float distance)
		{
			const float r = std::min(1.0f, distance);
			return 1.0f - r * r * (3.0f - 2.0f * r);
		});
	}

	void Renderer2D::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		// The preview handles belong to ImGui's pool; only the textures they hold are released here.
		s_Data->ImGuiTextures.clear();
		s_Data->BlobShadowTexture.reset();
		s_Data->SoftDotTexture.reset();
		delete s_Data;
		s_Data = nullptr;

		ViewRenderer::Shutdown();
	}

	void Renderer2D::RegisterView(RenderViewData& view)
	{
		ViewRenderer::RegisterView(view);
	}

	void Renderer2D::UnregisterView(RenderViewData& view)
	{
		ViewRenderer::UnregisterView(view);
		if (s_Data != nullptr && s_Data->CurrentView == &view)
		{
			s_Data->CurrentView = nullptr;
		}
	}

	void Renderer2D::BeginScene(RenderView& view, const glm::mat4& viewProjection)
	{
		HY_CORE_ASSERT(s_Data->CurrentView == nullptr, "Renderer2D::BeginScene without EndScene");

		RenderViewData& data = *view.m_Data;
		data.ViewProjection = viewProjection;
		ViewRenderer::QueueView(data);
		s_Data->CurrentView = &data;
	}

	void Renderer2D::BeginScene(RenderView& view, const Camera& camera, const glm::mat4& cameraTransform)
	{
		BeginScene(view, camera.GetProjection() * glm::inverse(cameraTransform));
	}

	void Renderer2D::EndScene()
	{
		// An open overlay and a sprite draw mode end with the scene, so the next BeginScene starts with translucent draws.
		RenderViewData* view = s_Data->CurrentView;
		if (view != nullptr)
		{
			view->Emission = glm::vec3(0.0f);
			view->InScreenSpace = false;
		}
		if (view != nullptr && (view->InOverlay || view->InDebugLines || view->InShadowCasters
			|| view->DrawMode != SpriteDrawMode::Translucent
			|| view->EntityID >= 0))
		{
			view->InOverlay = false;
			view->InDebugLines = false;
			view->InShadowCasters = false;
			view->DrawMode = SpriteDrawMode::Translucent;
			view->EntityID = -1;
			QuadBatcher::StartDraw(*view);
		}

		s_Data->CurrentView = nullptr;
	}

	void Renderer2D::BeginOverlay()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::BeginOverlay outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InOverlay)
		{
			return;
		}

		s_Data->CurrentView->InOverlay = true;
		QuadBatcher::StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::EndOverlay()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndOverlay outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InOverlay == false)
		{
			return;
		}

		s_Data->CurrentView->InOverlay = false;
		QuadBatcher::StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::BeginScreenOverlay(const glm::mat4& screenProjection)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::BeginScreenOverlay outside BeginScene/EndScene");
		RenderViewData* view = s_Data->CurrentView;
		if (view == nullptr || view->InScreenSpace)
		{
			return;
		}

		BeginOverlay();
		view->InScreenSpace = true;
		// In double, so screen pixels stay exact through the inverse.
		const glm::dmat4 viewProjection(view->ViewProjection);
		const double determinant = glm::determinant(viewProjection);
		view->ScreenSpaceValid = std::isfinite(determinant) && std::abs(determinant) > 1.0e-30;
		view->ScreenToWorld = view->ScreenSpaceValid ? glm::inverse(viewProjection) * glm::dmat4(screenProjection) : glm::dmat4(1.0);
	}

	void Renderer2D::EndScreenOverlay()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndScreenOverlay outside BeginScene/EndScene");
		RenderViewData* view = s_Data->CurrentView;
		if (view == nullptr || view->InScreenSpace == false)
		{
			return;
		}

		view->InScreenSpace = false;
		EndOverlay();
	}

	void Renderer2D::BeginDebugLines()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::BeginDebugLines outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InDebugLines)
		{
			return;
		}

		SetEntityID(-1);
		s_Data->CurrentView->InDebugLines = true;
		QuadBatcher::StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::EndDebugLines()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndDebugLines outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InDebugLines == false)
		{
			return;
		}

		s_Data->CurrentView->InDebugLines = false;
		QuadBatcher::StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::BeginShadowCasters()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::BeginShadowCasters outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InShadowCasters)
		{
			return;
		}

		// The shadow map has no ID attachment.
		SetEntityID(-1);
		s_Data->CurrentView->InShadowCasters = true;
		QuadBatcher::StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::EndShadowCasters()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndShadowCasters outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InShadowCasters == false)
		{
			return;
		}

		s_Data->CurrentView->InShadowCasters = false;
		QuadBatcher::StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::SetSceneLighting(const SceneLighting& lighting)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::SetSceneLighting outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}
		s_Data->CurrentView->Lighting = lighting;
		s_Data->CurrentView->LightingSubmitted = true;
	}

	void Renderer2D::SetPostProcess(const ScenePostProcess& postProcess)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::SetPostProcess outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}
		s_Data->CurrentView->PostProcess = postProcess;
	}

	void Renderer2D::SetViewFocus(const glm::vec3& focusPoint, const glm::vec3& viewDirection)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::SetViewFocus outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}
		const float length = glm::length(viewDirection);
		if (length < 1.0e-6f)
		{
			return; // no direction to measure along: no depth of field this frame
		}
		s_Data->CurrentView->FocusPoint = focusPoint;
		s_Data->CurrentView->FocusForward = viewDirection / length;
		s_Data->CurrentView->HasFocus = true;
	}

	const std::vector<PassTiming>& Renderer2D::GetPassTimings()
	{
		return PassTimers::GetPassTimings();
	}

	bool Renderer2D::HasSharedDepth(const RenderFeatureSettings& settings)
	{
		return settings.Depth == DepthMode::Shared && Renderer::GetCapabilities().DepthFormat32F;
	}

	void Renderer2D::SetSpriteDrawMode(SpriteDrawMode mode)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::SetSpriteDrawMode outside BeginScene/EndScene");
		RenderViewData* view = s_Data->CurrentView;
		if (view == nullptr)
		{
			return;
		}

		if (HasSharedDepth(view->Features) == false && mode != SpriteDrawMode::Additive)
		{
			mode = SpriteDrawMode::Translucent;
		}
		if (mode == view->DrawMode)
		{
			return;
		}

		view->DrawMode = mode;
		// Inside an overlay or the shadow casters that list stays current; its End starts the draw in the new mode's list.
		if (view->InOverlay == false && view->InShadowCasters == false)
		{
			QuadBatcher::StartDraw(*view);
		}
	}

	void Renderer2D::SetEntityID(int32_t entityID)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::SetEntityID outside BeginScene/EndScene");
		RenderViewData* view = s_Data->CurrentView;
		if (view == nullptr)
		{
			return;
		}

		const bool wasPickable = view->EntityID >= 0;
		view->EntityID = entityID < 0 ? -1 : entityID;
		// Pickable and other quads use different pipelines (one writes IDs), so only a change of kind starts a new draw.
		if (wasPickable != (view->EntityID >= 0))
		{
			QuadBatcher::StartDraw(*view);
		}
	}

	void Renderer2D::SetEmission(const glm::vec3& emission)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::SetEmission outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}
		// Travels in the vertices, so it doesn't start a new draw.
		s_Data->CurrentView->Emission = glm::max(emission, glm::vec3(0.0f));
	}

	static void SubmitQuadCorners(const glm::vec3 (&corners)[4], const Ref<Texture2D>& texture, const TextureRect& textureRect,
		const glm::vec4& color, const glm::vec3 (*shadowCorners)[4] = nullptr)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawQuad outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}
		QuadBatcher::Submit(*s_Data->CurrentView, corners, texture, textureRect, color, shadowCorners);
	}

	static void SubmitQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect,
		const glm::vec4& color)
	{
		glm::vec3 corners[4];
		for (size_t i = 0; i < 4; i++)
		{
			corners[i] = glm::vec3(transform * s_Data->QuadVertexPositions[i]);
		}
		SubmitQuadCorners(corners, texture, textureRect, color);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitQuad(transform, nullptr, TextureRect{}, color);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		// A rect larger than the texture; the sampler's wrap mode repeats it.
		SubmitQuad(transform, texture, TextureRect{ glm::vec2(0.0f), glm::vec2(tilingFactor) }, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect,
		const glm::vec4& tintColor)
	{
		SubmitQuad(transform, texture, textureRect, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect,
		const glm::vec4& tintColor,
		const glm::mat4& shadowLookupTransform)
	{
		glm::vec3 corners[4];
		glm::vec3 shadowCorners[4];
		for (size_t i = 0; i < 4; i++)
		{
			corners[i] = glm::vec3(transform * s_Data->QuadVertexPositions[i]);
			shadowCorners[i] = glm::vec3(shadowLookupTransform * s_Data->QuadVertexPositions[i]);
		}
		SubmitQuadCorners(corners, texture, textureRect, tintColor, &shadowCorners);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture,
		const TextureRect& textureRect, const glm::vec4& tintColor)
	{
		const float c = std::cos(rotation);
		const float s = std::sin(rotation);
		const glm::vec2 right = glm::vec2(c, s) * (size.x * 0.5f);
		const glm::vec2 up = glm::vec2(-s, c) * (size.y * 0.5f);
		const glm::vec3 corners[4] = {
			{ position.x - right.x - up.x, position.y - right.y - up.y, position.z },
			{ position.x + right.x - up.x, position.y + right.y - up.y, position.z },
			{ position.x + right.x + up.x, position.y + right.y + up.y, position.z },
			{ position.x - right.x + up.x, position.y - right.y + up.y, position.z } };
		SubmitQuadCorners(corners, texture, textureRect, tintColor);
	}

	const Ref<Texture2D>& Renderer2D::GetSoftDotTexture()
	{
		return s_Data->SoftDotTexture;
	}

	const Ref<Texture2D>& Renderer2D::GetBlobShadowTexture()
	{
		return s_Data->BlobShadowTexture;
	}

	void Renderer2D::DrawLight(const glm::vec3& position, float radius, const glm::vec3& color, float intensity)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawLight outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}

		RenderViewData& view = *s_Data->CurrentView;

		// A view with a light is lit, even when this one adds nothing.
		view.HasLights = true;
		if (radius <= 0.0f || intensity <= 0.0f)
		{
			return;
		}

		if (view.Lights.size() >= MaxLightsPerView)
		{
			if (s_Data->LightLimitWarned == false)
			{
				s_Data->LightLimitWarned = true;
				HY_CORE_WARN("Renderer2D: more than {} lights in a view; the rest are not drawn", MaxLightsPerView);
			}
			return;
		}

		LightInstance light;
		light.Position = position;
		light.Radius = radius;
		light.Color = color * intensity;
		view.Lights.push_back(light);
	}

	void Renderer2D::DrawGlobalLight(const glm::vec3& color, float intensity)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawGlobalLight outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}

		RenderViewData& view = *s_Data->CurrentView;
		view.HasLights = true;
		if (intensity > 0.0f)
		{
			view.AmbientLight += color * intensity;
		}
	}

	void Renderer2D::DrawShadow(const glm::vec3& position, const glm::vec2& size, float strength)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawShadow outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || strength <= 0.0f || size.x <= 0.0f || size.y <= 0.0f)
		{
			return;
		}

		RenderViewData& view = *s_Data->CurrentView;
		if (view.Shadows.size() >= MaxShadowsPerView)
		{
			if (s_Data->ShadowLimitWarned == false)
			{
				s_Data->ShadowLimitWarned = true;
				HY_CORE_WARN("Renderer2D: more than {} shadows in a view; the rest are not drawn", MaxShadowsPerView);
			}
			return;
		}

		ShadowInstance shadow;
		shadow.Position = position;
		shadow.Size = size;
		shadow.Strength = std::min(strength, 1.0f);
		view.Shadows.push_back(shadow);
	}

	// From the view-projection; the same at every point for orthographic cameras.
	static float GetWorldUnitsPerPixel(const RenderViewData& view)
	{
		const glm::mat4& matrix = view.ViewProjection;
		// How far clip-space x moves per world unit, whatever the camera's rotation.
		const float clipPerWorld = glm::length(glm::vec2(matrix[0][0], matrix[1][0]));
		const float width = (float)std::max(view.Target->GetWidth(), 1u);
		if (clipPerWorld <= 0.0f)
		{
			return 0.0f;
		}

		// Clip space spans 2 units across the view's width.
		return 2.0f / (clipPerWorld * width);
	}

	void Renderer2D::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawLine outside BeginScene/EndScene");
		// Widths come from the scene's camera, which screen-space quads don't use.
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InScreenSpace)
		{
			return;
		}

		// Extended by its thickness at both ends, so lines meeting at a corner leave no gap. Only x and y set the
		// direction; z is the start's.
		const float width = std::max(thickness, 0.0f) * GetWorldUnitsPerPixel(*s_Data->CurrentView);
		const glm::vec2 delta = glm::vec2(end) - glm::vec2(start);
		const float length = glm::length(delta);
		const float angle = std::atan2(delta.y, delta.x);
		const glm::vec3 middle = 0.5f * (start + end);

		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), middle)
			* glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(length + width, width, 1.0f));

		SubmitQuad(transform, nullptr, TextureRect{}, color);
		ViewRenderer::GetFrameStats().LineCount++;
	}

	void Renderer2D::DrawLine3D(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawLine3D outside BeginScene/EndScene");
		RenderViewData* view = s_Data->CurrentView;
		if (view == nullptr || view->InScreenSpace)
		{
			return;
		}

		const glm::vec3 direction = end - start;
		const float length = glm::length(direction);
		if (length <= 1.0e-6f)
		{
			return;
		}

		// The ray through the line's middle (from the inverse view-projection, so perspective and orthographic alike) gives
		// the side the ribbon spreads along, and the pixel size there.
		const glm::mat4 inverse = glm::inverse(view->ViewProjection);
		const glm::vec3 middle = 0.5f * (start + end);
		const glm::vec4 clip = view->ViewProjection * glm::vec4(middle, 1.0f);
		if (clip.w <= 1.0e-6f)
		{
			return;
		}
		const glm::vec2 ndc = glm::vec2(clip) / clip.w;
		glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
		glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
		nearPoint /= nearPoint.w;
		farPoint /= farPoint.w;
		const glm::vec3 viewDirection = glm::vec3(farPoint - nearPoint);

		glm::vec3 side = glm::cross(direction / length, viewDirection);
		if (glm::length(side) <= 1.0e-6f)
		{
			return; // looking straight along the line
		}
		side = glm::normalize(side);

		const glm::vec4 sideClip = view->ViewProjection * glm::vec4(middle + side, 1.0f);
		const float width = (float)std::max(view->Target->GetWidth(), 1u);
		const float height = (float)std::max(view->Target->GetHeight(), 1u);
		const glm::vec2 pixels = (glm::vec2(sideClip) / sideClip.w - ndc) * glm::vec2(width, height) * 0.5f;
		const float pixelsPerUnit = glm::length(pixels);
		if (pixelsPerUnit <= 1.0e-6f)
		{
			return;
		}
		const glm::vec3 offset = side * (0.5f * std::max(thickness, 0.0f) / pixelsPerUnit);

		const glm::vec3 corners[4] = { start - offset, end - offset, end + offset, start + offset };
		SubmitQuadCorners(corners, nullptr, TextureRect{}, color);
		Statistics& stats = ViewRenderer::GetFrameStats();
		stats.LineCount++;
		stats.DebugLineCount++;
	}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, float thickness)
	{
		glm::vec3 corners[4];
		for (size_t i = 0; i < 4; i++)
		{
			corners[i] = glm::vec3(transform * s_Data->QuadVertexPositions[i]);
		}

		for (size_t i = 0; i < 4; i++)
		{
			DrawLine(corners[i], corners[(i + 1) % 4], color, thickness);
		}
	}

	void Renderer2D::DrawRect(const glm::vec3& center, const glm::vec2& size, const glm::vec4& color, float thickness)
	{
		DrawRect(glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f)), color, thickness);
	}

	void Renderer2D::DrawCircle(const glm::vec3& center, float radius, const glm::vec4& color, float thickness, uint32_t segments)
	{
		segments = std::clamp(segments, 3u, 256u);

		const float step = glm::two_pi<float>() / (float)segments;
		glm::vec3 previous = center + glm::vec3(radius, 0.0f, 0.0f);
		for (uint32_t i = 1; i <= segments; i++)
		{
			const float angle = step * (float)i;
			const glm::vec3 next = center + glm::vec3(std::cos(angle) * radius, std::sin(angle) * radius, 0.0f);
			DrawLine(previous, next, color, thickness);
			previous = next;
		}
	}

	void Renderer2D::Render(VkCommandBuffer commandBuffer)
	{
		HY_CORE_ASSERT(s_Data->CurrentView == nullptr, "Renderer2D::Render inside BeginScene/EndScene");
		s_Data->CurrentView = nullptr;
		ViewRenderer::Render(commandBuffer);
	}

	void Renderer2D::DiscardFrame()
	{
		ViewRenderer::DiscardFrame();
		s_Data->CurrentView = nullptr;
	}

	Statistics Renderer2D::GetStats()
	{
		return ViewRenderer::GetLastFrameStats();
	}

	uint32_t Renderer2D::GetMaxTextureSlots()
	{
		return MaxTextureSlots;
	}

	std::vector<RenderValidationMessage> Renderer2D::ValidateRenderFeatures(const RenderFeatureSettings& settings, bool editorView)
	{
		return ViewRenderer::ValidateRenderFeatures(settings, editorView);
	}

	VkDescriptorSet Renderer2D::GetImGuiTexture(const Ref<Texture2D>& texture)
	{
		if (s_Data == nullptr || texture == nullptr)
		{
			return VK_NULL_HANDLE;
		}

		const uint64_t frame = Time::GetFrameCount();
		for (ImGuiTextureEntry& entry : s_Data->ImGuiTextures)
		{
			if (entry.Texture == texture)
			{
				entry.LastUsedFrame = frame;
				return entry.Set;
			}
		}

		// A handle no frame in flight can still be drawing with is rewritten for this texture.
		const VulkanImageInfo& image = GetTexture2DData(*texture).Image->GetVulkanImageInfo();
		for (ImGuiTextureEntry& entry : s_Data->ImGuiTextures)
		{
			if (entry.LastUsedFrame + MAX_FRAMES_IN_FLIGHT + 1 < frame)
			{
				ViewRenderer::WriteImGuiTexture(entry.Set, image.ImageView, image.Sampler);
				entry.Texture = texture;
				entry.LastUsedFrame = frame;
				return entry.Set;
			}
		}

		ImGuiTextureEntry entry;
		entry.Set = ImGui_ImplVulkan_AddTexture(image.Sampler, image.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		entry.Texture = texture;
		entry.LastUsedFrame = frame;
		s_Data->ImGuiTextures.push_back(entry);
		return entry.Set;
	}
}
