#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Renderer/Lighting.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/RenderGraph.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/Texture2D.h"

namespace Hydro
{
	class RenderFeature;
	struct QuadDraw;

	// The GPU time of one render graph pass last frame (summed over the views that ran it), M7.
	struct PassTiming
	{
		std::string Name;
		float GPUMs = 0.0f;
	};

	// Stats
	struct Statistics
	{
		// Geometry submitted this frame.
		uint32_t DrawCalls = 0;
		uint32_t QuadCount = 0;
		uint32_t LineCount = 0;           // of QuadCount: the quads that are lines (DrawLine, DrawRect, DrawCircle)

		// Batching.
		uint32_t BatchCount = 0;          // vertex batches used this frame (each holds up to MaxQuadsPerBatch quads)
		uint32_t BatchesAllocated = 0;    // batches kept alive for reuse
		uint32_t MaxQuadsPerBatch = 0;
		uint64_t VertexBytesUploaded = 0; // CPU -> GPU vertex upload this frame

		// Textures.
		uint32_t MaxTextureSlots = 0;     // textures one draw call can sample (slot 0 is the white texture)
		uint32_t TextureSlotsUsed = 0;    // slots filled this frame, summed over all texture sets of all views
		uint32_t TextureFlushes = 0;      // extra draw calls because a draw ran out of texture slots

		// GPU memory (from VMA).
		uint64_t GPUMemoryUsed = 0;
		uint64_t GPUMemoryFree = 0;

		// GPU time of the 2D render pass (timestamp queries; a couple of frames old). -1 = unsupported.
		float GPUTimeMs = -1.0f;
		const char* GPUName = "";

		// 2D lighting (GPU times come from the same timestamp queries, summed over the lit views;
		// -1 = unsupported or no lit view). The light pass includes the shadows. The parts are
		// measured separately, so they can slightly overlap the 2D pass total.
		float LightPassGPUMs = -1.0f;
		float ShadowGPUMs = -1.0f;
		float CompositeGPUMs = -1.0f;
		uint32_t LitViewCount = 0;   // views that drew a light buffer this frame
		uint32_t LightCount = 0;     // point lights drawn into light buffers
		uint32_t ShadowCount = 0;    // blob shadows drawn into light buffers

		// Forward lighting (M6), summed over the forward-lit views.
		uint32_t ForwardLitViewCount = 0;
		uint32_t ForwardLightCount = 0;    // point and spot lights uploaded
		uint32_t ForwardLightsDropped = 0; // beyond MaxForwardLights (the farthest from the camera)
		uint32_t DebugLineCount = 0;       // DrawLine3D quads (editor debug lines, M7)

		// Sun shadows (R14), summed over the views that drew a shadow map.
		uint32_t SunShadowViewCount = 0;
		uint32_t ShadowCasterQuads = 0;    // sprite quads drawn into shadow maps (meshes: Renderer3DStatistics)

		// Frame / target.
		uint32_t ViewCount = 0;      // views rendered this frame
		uint32_t ViewportWidth = 0;  // size of the first view rendered this frame
		uint32_t ViewportHeight = 0;
		uint32_t FramesInFlight = 0;
		uint32_t FrameIndex = 0;

		uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
		uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
		uint32_t GetTriangleCount() const { return QuadCount * 2; }
	};

	// Draws batched quads into RenderViews. A part of Renderer: Renderer::Init / Shutdown start and
	// stop it, and Renderer::RenderFrame calls Render once per frame.
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		// Starts queuing quads for `view`, seen through viewProjection. Projections must come from
		// Camera (Camera::Perspective / Camera::Orthographic), which handles Vulkan's clip space.
		// Each view begun this frame gets its own 2D pass in Render, in the order they were begun.
		// Beginning the same view again in a frame adds to its quads and replaces its camera.
		static void BeginScene(RenderView& view, const glm::mat4& viewProjection);
		// cameraTransform is the camera's world transform (position and rotation, no scale).
		static void BeginScene(RenderView& view, const Camera& camera, const glm::mat4& cameraTransform);
		static void EndScene();

		// Queue a quad for the view between BeginScene and EndScene. Only writes CPU-side vertex
		// data. When the current batch is full a new batch is started automatically - there is no
		// hard limit on quads per frame.
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		// Textured quad: the texture, repeated tilingFactor times across the quad, multiplied by
		// tintColor. A null texture draws the white texture (a plain coloured quad).
		// One draw call samples up to MaxTextureSlots textures. The quad that needs one more starts a
		// new draw call with its own texture slots ("texture flush"), in the same vertex batch, so
		// there is no limit on textures per frame either. Draw order is kept.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
		// Part of a texture (a sprite of a sprite sheet: Texture2D::GetSpriteUVRect), stretched over the
		// quad and multiplied by tintColor. No tiling. Swap textureRect's Min.x and Max.x to flip it
		// horizontally, Min.y and Max.y to flip it vertically.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& tintColor = glm::vec4(1.0f));
		// The same, but the lit shaders look its pixels up in the sun shadow map (R14) on the quad of
		// `shadowLookupTransform` instead of on itself: the same texel of the quad that casts its shadow (a
		// sprite turned to face the sun, SpriteShadowMode::FaceSun), so it is never darkened by its own shadow.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& tintColor,
			const glm::mat4& shadowLookupTransform);
		// The fast path for many small quads (particles): a quad centred on `position`, `size` wide and
		// high, turned `rotation` radians around Z, without building a matrix. Otherwise like the one above.
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& tintColor);

		// A soft round dot (white, alpha fading from the centre to the edge), made in code: the default
		// particle texture. Valid between Init and Shutdown.
		static const Ref<Texture2D>& GetSoftDotTexture();

		// Lines and outlines, drawn between BeginScene and EndScene. A line is a thin quad (it batches and
		// blends like a sprite, and works on GPUs without wide lines), so it counts in QuadCount and
		// LineCount. Thickness is in pixels of the view, so outlines look the same at any zoom. Colours are
		// straight RGBA. Lines are drawn in submission order with the sprites, and are lit like them;
		// wrap them in BeginOverlay / EndOverlay to draw them on top, after the lighting.
		static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness = 1.5f);
		// The outline of the unit quad (-0.5 to 0.5) moved by `transform`: the border of a sprite or collider.
		static void DrawRect(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.5f);
		static void DrawRect(const glm::vec3& center, const glm::vec2& size, const glm::vec4& color, float thickness = 1.5f);
		// A circle outline in the XY plane, a polygon of `segments` sides (3 to 256).
		static void DrawCircle(const glm::vec3& center, float radius, const glm::vec4& color, float thickness = 1.5f, uint32_t segments = 32);

		// Everything queued between BeginOverlay and EndOverlay is drawn after the view's lighting is
		// composited, on top of the scene and untouched by lights and shadows: selection outlines, collider
		// shapes, gizmos. Overlay quads keep their order among themselves.
		static void BeginOverlay();
		static void EndOverlay();

		// Editor shapes in the scene (M7, the DebugLines feature of the Editor view): quads queued between
		// BeginDebugLines and EndDebugLines are drawn last, depth tested against the scene (hidden behind
		// what is in front of them) without writing depth or entity IDs. Views without the feature (the Game
		// view) drop them.
		static void BeginDebugLines();
		static void EndDebugLines();

		// Sun shadows (R14, the SunShadows render feature): quads queued between BeginShadowCasters and
		// EndShadowCasters are drawn only into the view's sun shadow map, alpha tested like cut-out sprites
		// (pixels under 0.5 cast nothing). SceneRenderer queues the shadows of cut-out standing sprites here,
		// as drawn or turned to face the sun (SpriteShadowMode). Views without the feature, or without a
		// shadow-casting sun this frame, drop them. Meshes cast through Renderer3D (MeshComponent::CastShadows).
		static void BeginShadowCasters();
		static void EndShadowCasters();
		// A line between two points anywhere in 3D: a quad that faces the camera, `thickness` pixels wide at
		// its middle. For debug shapes (bounds, light ranges, cones); DrawLine stays the 2D one (XY).
		static void DrawLine3D(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness = 1.5f);

		// Forward lighting (M6): what the current view is lit with this frame, between BeginScene and EndScene
		// (SceneRenderer gathers it). Used by views whose render features have Lighting = Forward: sprites and
		// meshes are shaded with it in their shaders. The nearest MaxForwardLights point and spot lights to
		// the camera are kept.
		static void SetSceneLighting(const SceneLighting& lighting);

		// Shared depth (RenderFeatureSettings::Depth): how the quads submitted next are drawn, until the next
		// call or EndScene (which goes back to Translucent). Decal and Cutout quads go to their own lists,
		// drawn before the translucent ones; see SpriteDrawMode. Ignored inside an overlay and in a view
		// without shared depth (everything is translucent there, in submission order).
		static void SetSpriteDrawMode(SpriteDrawMode mode);
		// Whether a view with these settings has a shared depth attachment (Depth = Shared and the GPU can
		// use DEPTH32F as one). SceneRenderer chooses sprite draw modes only then.
		static bool HasSharedDepth(const RenderFeatureSettings& settings);

		// The entity the quads submitted next belong to, until the next call or EndScene (which goes back to
		// -1). The Editor view writes it to its entity-ID attachment for click-to-select (M7); -1 (or any
		// negative value) = none: the quads don't change the IDs below them (particles, effects, lines).
		// Overlay quads write it too (HUD sprites); editor lines carry -1.
		static void SetEntityID(int32_t entityID);

		// 2D lighting (RendererSettings::Lighting). Queue lights for the view between BeginScene and
		// EndScene, like quads. A view that gets at least one light is drawn lit: each light is drawn
		// into the view's light buffer, which is then multiplied over the scene. Colours are linear RGB,
		// multiplied by intensity. A light is drawn through the view's camera like the sprites, so give
		// it the Z of the sprites it lights.
		// A point light lights the area around `position` out to `radius` (world units). At most 1024
		// per view and frame; the rest are dropped.
		static void DrawLight(const glm::vec3& position, float radius, const glm::vec3& color, float intensity);
		// A light that reaches everything equally: the ambient colour of the view. Several add up. Without
		// one, the parts no point light reaches are black.
		static void DrawGlobalLight(const glm::vec3& color, float intensity);
		// A soft blob shadow (RendererSettings::Shadows): an ellipse centred on `position`, `size` wide and
		// high in world units, that darkens the light buffer by `strength` (0..1) at its centre and fades to
		// nothing at its edge. It darkens whatever light reaches that spot, ambient included. It does not
		// light a view by itself and does nothing in an unlit one. At most 1024 per view and frame.
		static void DrawShadow(const glm::vec3& position, const glm::vec2& size, float strength);

		// Records one rendering pass per view begun this frame (one vkCmdDrawIndexed per draw) into
		// the frame's command buffer. Called once per frame by Renderer::RenderFrame.
		static void Render(VkCommandBuffer commandBuffer);

		// Throw away this frame's quads without rendering (e.g. while the window is minimized).
		static void DiscardFrame();

		// Stats from the last frame that was rendered (all views together).
		static Statistics GetStats();
		// GPU time per render graph pass, a couple of frames old (empty without timestamp support), M7.
		static const std::vector<PassTiming>& GetPassTimings();

		// Textures one draw call can sample, including the white texture in slot 0.
		static uint32_t GetMaxTextureSlots();

		// What a view's render graph would report for these render features (settings UI). `editorView`:
		// as the Editor window's view.
		static std::vector<RenderValidationMessage> ValidateRenderFeatures(const RenderFeatureSettings& settings, bool editorView);

		// An ImGui texture handle showing `texture` (pass it to ImGui::Image), for editor previews.
		// Call it every frame the texture is shown. Handles come from a small pool: one unused for
		// longer than the frames in flight lets go of its texture and is reused for another, so ImGui's
		// descriptor pool doesn't grow and nothing is rewritten while the GPU may still read it.
		static VkDescriptorSet GetImGuiTexture(const Ref<Texture2D>& texture);

	private:
		// RenderView's constructor and destructor: batches, texture sets, the view list.
		static void RegisterView(RenderViewData& view);
		static void UnregisterView(RenderViewData& view);
		// The render features `settings` ask for, in the order their passes run within a stage.
		// `editorView`: the Editor window's view, which also gets the entity-ID attachment.
		static std::vector<Scope<RenderFeature>> CreateRenderFeatures(const RenderFeatureSettings& settings, bool editorView);
		// Rebuilds the view's render graph when its features changed (and its target when they need
		// other attachments).
		static void UpdateRenderGraph(RenderViewData& view);
		// Records `draws` (one of the view's quad lists) with pipelines built for the view's attachments:
		// `mode` picks the cut-out or the sprite pipeline, `overlay` the overlay's. Nothing when empty.
		static void RecordQuadDraws(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame, const std::vector<QuadDraw>& draws, SpriteDrawMode mode, bool overlay, bool debugLines = false);
		// Uploads the view's forward lighting (FrameLightingUniform, the light buffer) for `frame`.
		static void UploadLighting(RenderViewData& view, uint32_t frame);
		// Sun shadows (R14): whether `view` draws its shadow map this frame (the feature is in its graph and the
		// scene has a shadow-casting sun), making or resizing the map; the matrix it is drawn with.
		static void PrepareSunShadows(RenderViewData& view);
		// The SunShadows pass (BeforeScene): meshes, then the queued sprite casters, into the view's shadow map.
		static void RecordSunShadowPass(RenderPassContext& context);
		// Runs one stage of the view's render graph: each pass in a debug label, timed when it can be.
		static void ExecuteStage(RenderStage stage, RenderPassContext& context);
		// The draw list quads go to now (overlay, or the current sprite draw mode's list).
		static std::vector<QuadDraw>& GetCurrentDraws(RenderViewData& view);
		static void AllocateBatch(RenderViewData& view);
		static void NextBatch(RenderViewData& view);
		static void ResetBatches(RenderViewData& view);
		static void AllocateTextureSet(RenderViewData& view);
		static void NextTextureSet(RenderViewData& view);
		// Slot of `texture` in the view's current texture set; adds it, or starts a new set (and draw)
		// when the set is full.
		static float GetTextureSlot(RenderViewData& view, const Ref<Texture2D>& texture);
		// Starts a new draw at the current batch's write position with the current texture set.
		static void StartDraw(RenderViewData& view);
		static void SubmitQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& color);
		// Corners counter-clockwise from the bottom-left, like QuadVertexPositions.
		// `shadowCorners`: where each corner looks up the sun shadow map (QuadVertex::ShadowPosition); null = the corners themselves.
		static void SubmitQuadCorners(const glm::vec3 (&corners)[4], const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& color,
			const glm::vec3 (*shadowCorners)[4] = nullptr);
		static void ApplyViewResize(RenderViewData& view);
		// Points the ImGui texture handles of frame index `frame` that a resize left outdated at the
		// view's new images (every view, or one).
		static void UpdateImGuiTextures(uint32_t frame);
		static void UpdateImGuiTextures(RenderViewData& view, uint32_t frame);
		static void RecordView(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// Draws the view's lights into its light buffer (before the view's scene pass). False when the view
		// isn't lit: lighting is off, it has no lights, or the light shaders are unavailable.
		static bool RecordLightPass(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// Multiplies the light buffer over the scene. Call it inside the scene pass, after the sprites.
		static void RecordComposite(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// Applies Renderer::GetSettings() that need a rebuild (wireframe), and rebuilds what was built from a
		// shader whose source changed (VulkanShaderLibrary; only with RendererSettings::ShaderHotReload).
		static void ApplySettings();
		static void ReloadShadersIfChanged();
		// Builds a new quad pipeline from the library's "Sprite" shader and swaps it in; the old one is freed
		// through the deletion queue. On failure the current one keeps drawing and false is returned.
		static bool RebuildQuadPipeline(bool wireframe);

		friend class Hydro::RenderView;
	};
}
