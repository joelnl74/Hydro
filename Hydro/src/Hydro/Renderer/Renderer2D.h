#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Renderer/Lighting.h"
#include "Hydro/Renderer/PostProcess.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/RenderGraph.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/Texture2D.h"

namespace Hydro
{
	// The GPU time of one render graph pass last frame, summed over the views that ran it.
	struct PassTiming
	{
		std::string Name;
		float GPUMs = 0.0f;
	};

	// What the views drew in one frame, all views together.
	struct Statistics
	{
		uint32_t DrawCalls = 0;
		uint32_t QuadCount = 0;
		uint32_t LineCount = 0;           // of QuadCount: lines and outlines

		uint32_t BatchCount = 0;          // vertex batches used, summed over the views
		uint32_t FullestBatchQuads = 0;   // how close one batch is to needing another
		uint32_t BatchesAllocated = 0;    // batches kept alive for reuse
		uint32_t MaxQuadsPerBatch = 0;
		uint64_t VertexBytesUploaded = 0;

		uint32_t MaxTextureSlots = 0;     // textures one draw call can sample (slot 0 is the white texture)
		uint32_t TextureSlotsUsed = 0;    // summed over every texture set of every view
		uint32_t TextureFlushes = 0;      // extra draw calls because a draw ran out of texture slots

		uint64_t GPUMemoryUsed = 0;
		uint64_t GPUMemoryFree = 0;

		// GPU times are a couple of frames old; -1 = unsupported (or no lit view).
		float GPUTimeMs = -1.0f;
		const char* GPUName = "";

		// The 2D light buffer, summed over the lit views. The light pass includes the shadows.
		float LightPassGPUMs = -1.0f;
		float ShadowGPUMs = -1.0f;
		float CompositeGPUMs = -1.0f;
		uint32_t LitViewCount = 0;
		uint32_t LightCount = 0;
		uint32_t ShadowCount = 0;

		// Forward lighting, summed over the forward-lit views.
		uint32_t ForwardLitViewCount = 0;
		uint32_t ForwardLightCount = 0;
		uint32_t ForwardLightsDropped = 0; // beyond MaxForwardLights
		uint32_t DebugLineCount = 0;

		uint32_t SunShadowViewCount = 0;
		uint32_t ShadowCasterQuads = 0;    // sprite quads in shadow maps (meshes: Renderer3DStatistics)

		uint32_t HDRViewCount = 0;
		uint32_t BloomViewCount = 0;       // and the levels of the last one (the first level's size)
		uint32_t BloomLevelCount = 0;
		uint32_t BloomWidth = 0;
		uint32_t BloomHeight = 0;
		uint32_t DepthOfFieldViewCount = 0; // and the half-size blur of the last one
		uint32_t DepthOfFieldWidth = 0;
		uint32_t DepthOfFieldHeight = 0;

		uint32_t ViewCount = 0;
		uint32_t ViewportWidth = 0;        // of the first view rendered
		uint32_t ViewportHeight = 0;
		uint32_t FramesInFlight = 0;
		uint32_t FrameIndex = 0;

		uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
		uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
		uint32_t GetTriangleCount() const { return QuadCount * 2; }
	};

	// Queues quads, lines, lights and shadows for RenderViews between BeginScene and EndScene; ViewRenderer draws them.
	// Started and stopped by Renderer, which calls Render once per frame.
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		// Starts queuing for `view`, seen through `viewProjection` (from Camera, which handles Vulkan's clip space). Views
		// are drawn in the order they were first begun; beginning one again adds to its quads and replaces its camera.
		static void BeginScene(RenderView& view, const glm::mat4& viewProjection);
		// `cameraTransform`: the camera's world transform (position and rotation, no scale).
		static void BeginScene(RenderView& view, const Camera& camera, const glm::mat4& cameraTransform);
		static void EndScene();

		// No limit on quads or textures per frame: a full batch or texture set starts a new one, keeping draw order.
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		// A null texture draws a plain coloured quad.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f,
			const glm::vec4& tintColor = glm::vec4(1.0f));
		// Part of a texture (Texture2D::GetSpriteUVRect), stretched over the quad. Swap Min and Max of an axis to flip it.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect,
			const glm::vec4& tintColor = glm::vec4(1.0f));
		// Looks its pixels up in the sun shadow map on `shadowLookupTransform`'s quad instead of its own: the sun-facing
		// quad that casts its shadow (SpriteShadowMode::FaceSun), so it is never darkened by its own shadow.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect,
			const glm::vec4& tintColor,
			const glm::mat4& shadowLookupTransform);
		// The fast path for many small quads (particles): centred on `position`, turned `rotation` radians around Z.
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture,
			const TextureRect& textureRect, const glm::vec4& tintColor);

		// A soft round dot made in code: the default particle texture.
		static const Ref<Texture2D>& GetSoftDotTexture();
		// A soft blob made in code, with shadow.frag's curve: contact shadows in forward-lit views.
		static const Ref<Texture2D>& GetBlobShadowTexture();

		// A line is a thin quad, so it batches, blends and is lit like a sprite. Thickness is in the view's pixels.
		static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness = 1.5f);
		// The outline of the unit quad moved by `transform`: the border of a sprite or collider.
		static void DrawRect(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.5f);
		static void DrawRect(const glm::vec3& center, const glm::vec2& size, const glm::vec4& color, float thickness = 1.5f);
		// In the XY plane, a polygon of 3 to 256 sides.
		static void DrawCircle(const glm::vec3& center, float radius, const glm::vec4& color, float thickness = 1.5f,
			uint32_t segments = 32);
		// A camera-facing ribbon between two points in 3D, for debug shapes (bounds, light ranges, cones).
		static void DrawLine3D(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness = 1.5f);

		// Drawn on top after the lighting, untouched by lights, shadows and the tonemap: outlines, gizmos, HUD sprites.
		static void BeginOverlay();
		static void EndOverlay();
		// An overlay placed through `screenProjection` instead of the scene's camera (runtime UI). Quads only; lines are
		// skipped. EndScene ends it too.
		static void BeginScreenOverlay(const glm::mat4& screenProjection);
		static void EndScreenOverlay();
		// The Editor view's debug lines: drawn last, depth tested, never writing depth or IDs. Other views drop them.
		static void BeginDebugLines();
		static void EndDebugLines();
		// Drawn only into the view's sun shadow map, alpha tested like cut-out sprites. Dropped without a shadowing sun.
		static void BeginShadowCasters();
		static void EndShadowCasters();

		// What the current view is lit with this frame (forward-lit views). The lights nearest what the camera looks at are kept.
		static void SetSceneLighting(const SceneLighting& lighting);
		// The current view's post-processing this frame (HDR views).
		static void SetPostProcess(const ScenePostProcess& postProcess);
		// Where the current view's camera looks (a point on the ground, a unit view direction): the depth of field's focus.
		static void SetViewFocus(const glm::vec3& focusPoint, const glm::vec3& viewDirection);

		// How the next quads are drawn, until the next call or EndScene (see SpriteDrawMode). Without shared depth only
		// Additive is kept; everything else is translucent there. Ignored inside an overlay.
		static void SetSpriteDrawMode(SpriteDrawMode mode);
		// Depth = Shared and the GPU can use DEPTH32F as a depth attachment.
		static bool HasSharedDepth(const RenderFeatureSettings& settings);

		// The entity the next quads belong to, written to the Editor view's ID attachment; negative = none.
		static void SetEntityID(int32_t entityID);
		// Linear colour the next quads add after the lighting (texture times tint times this); above 1 it blooms.
		static void SetEmission(const glm::vec3& emission);

		// 2D lighting: a view that gets a light is drawn lit. A point light reaches `radius` world units; at most 1024 per view.
		static void DrawLight(const glm::vec3& position, float radius, const glm::vec3& color, float intensity);
		// Reaches everything equally: the view's ambient colour. Several add up.
		static void DrawGlobalLight(const glm::vec3& color, float intensity);
		// An ellipse darkening the light buffer by `strength` at its centre, fading to nothing at its edge. Lit views only.
		static void DrawShadow(const glm::vec3& position, const glm::vec2& size, float strength);

		// Records every view begun this frame. Called once per frame by Renderer::RenderFrame.
		static void Render(VkCommandBuffer commandBuffer);
		// Drops this frame's quads without rendering (the window is minimized).
		static void DiscardFrame();

		static Statistics GetStats();
		static const std::vector<PassTiming>& GetPassTimings();
		// Textures one draw call can sample, including the white texture in slot 0.
		static uint32_t GetMaxTextureSlots();

		// What a view's render graph would report for these features (settings UI).
		static std::vector<RenderValidationMessage> ValidateRenderFeatures(const RenderFeatureSettings& settings, bool editorView);

		// An ImGui texture handle for an editor preview; call it every frame the texture is shown. Handles come from a
		// small pool and are reused once unused for longer than the frames in flight.
		static VkDescriptorSet GetImGuiTexture(const Ref<Texture2D>& texture);

	private:
		static void RegisterView(RenderViewData& view);
		static void UnregisterView(RenderViewData& view);

		friend class Hydro::RenderView;
	};
}
