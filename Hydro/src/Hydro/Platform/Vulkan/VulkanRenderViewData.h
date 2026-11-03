#pragma once
// A RenderView's GPU state, owned by RenderView, filled by Renderer2D and QuadBatcher, recorded by ViewRenderer.
// Backend only: never include it from a Renderer/ header.
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Image.h"
#include "Hydro/Renderer/Lighting.h"
#include "Hydro/Renderer/PostProcess.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/RenderGraph.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/Texture2D.h"
#include "VulkanDescriptorBuilder.h"
#include "VulkanFrameContext.h"
#include "VulkanImageReadback.h"
#include "VulkanRenderTarget.h"
#include "VulkanBuffer.h"
#include "VulkanGPUData.h"

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// Attachment formats of every view's target and the 2D light buffer; pipelines are built from them, so both agree.
	static constexpr ImageFormat ViewColorFormat = ImageFormat::SRGB;
	static constexpr ImageFormat LightBufferFormat = ImageFormat::RGBA16F;
	// The HDR scene colour: linear, values above 1 kept for the tonemap and bloom.
	static constexpr ImageFormat SceneColorFormat = ImageFormat::RGBA16F;

	// Every view's sun shadow map (ShadowMapPass; Renderer3D draws the mesh casters into it too).
	static constexpr ImageFormat ShadowMapFormat = ImageFormat::DEPTH32F;
	// Rasteriser depth bias of shadow casters, away from the sun, so a lit surface stays in front of its own entry in the
	// map. The lit shaders add a normal offset on top.
	static constexpr float ShadowCasterDepthBiasConstant = 1.25f;
	static constexpr float ShadowCasterDepthBiasSlope = 1.75f;

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
		// Written to the Editor view's entity-ID attachment for click-to-select; -1 = none (particles, lines, lights).
		int32_t EntityID = -1;
		// Where the lit shaders look this point up in the sun shadow map: its own position, or the same texel of the
		// sun-facing caster quad (SpriteShadowMode::FaceSun), so a sprite is never darkened by its own shadow.
		glm::vec3 ShadowPosition{ 0.0f };
		// Added after the lighting, times the pixel's colour (Renderer2D::SetEmission). 0 = none.
		glm::vec3 Emission{ 0.0f };
	};
	// Read by the vertex shader through QuadBatcher::GetVertexLayout, which packs the attributes in this order without padding.
	static_assert(sizeof(QuadVertex) == 68, "QuadVertex: must match GetQuadVertexLayout");
	static_assert(offsetof(QuadVertex, Color) == 12 && offsetof(QuadVertex, TexCoord) == 28
		&& offsetof(QuadVertex, TexIndex) == 36 && offsetof(QuadVertex, EntityID) == 40
		&& offsetof(QuadVertex, ShadowPosition) == 44 && offsetof(QuadVertex, Emission) == 56,
		"QuadVertex: must match GetQuadVertexLayout");

	// Textures one draw call can sample: the shader's `inTextures[32]` and the descriptor set's image array agree.
	static constexpr uint32_t MaxTextureSlots = VulkanDescriptorBuilder::MaxImageArraySize;
	static_assert(MaxTextureSlots == 32, "Change inTextures[] and SampleTexture in sprite_common.glsl too");

	// Up to MaxTextureSlots textures some draws sample, and the set 0 descriptor sets (one per frame in flight) that bind
	// them. Slot 0 is always the white texture.
	struct TextureSet
	{
		std::array<Ref<Texture2D>, MaxTextureSlots> Slots;
		uint32_t SlotCount = 1;
		Scope<VulkanDescriptorBuilder> DescriptorSet;
	};

	// A point light queued for a view this frame. Global lights only add to RenderViewData::AmbientLight.
	struct LightInstance
	{
		glm::vec3 Position{ 0.0f };
		float Radius = 0.0f;
		glm::vec3 Color{ 1.0f }; // linear RGB, already multiplied by the intensity
	};

	struct ShadowInstance
	{
		glm::vec3 Position{ 0.0f };
		glm::vec2 Size{ 1.0f };
		float Strength = 0.5f;
	};

	// Point lights and blob shadows per view and frame (4 vertices each). More are dropped.
	static constexpr uint32_t MaxLightsPerView = 1024;
	static constexpr uint32_t MaxShadowsPerView = 1024;

	// One vkCmdDrawIndexed: a run of quads in one vertex batch that use one texture set.
	struct QuadDraw
	{
		uint32_t Batch = 0;
		uint32_t FirstIndex = 0;  // 6 per quad
		uint32_t IndexCount = 0;
		uint32_t TextureSet = 0;
		bool Pickable = false;    // writes entity IDs (an entity's sprite, not a particle or line)
		bool Additive = false;    // added, not blended, among the translucent draws
	};

	struct MeshMaterial; // Renderer3D.cpp
	struct TerrainSurfaceGPU; // Renderer3D.cpp

	// One sub-mesh queued by Renderer3D, recorded by MeshPass.
	struct MeshDraw
	{
		Ref<Mesh> MeshRef;
		uint32_t SubMesh = 0;
		MeshMaterial* Material = nullptr; // owned by Renderer3D's material cache; a terrain's is white, for its shadow
		TerrainSurfaceGPU* TerrainSurface = nullptr; // a terrain surface, drawn with terrain.frag
		glm::mat4 Transform{ 1.0f };
		int32_t EntityID = -1;
		float SortDepth = 0.0f; // clip-space depth of the mesh's centre: front to back
		bool CastShadows = true;
		float Opacity = 1.0f; // below 1 dithered away (its shadow stays)
	};

	// Everything one RenderView needs: its target, its camera and what was queued for it this frame.
	struct RenderViewData
	{
		std::string DebugName;

		// ---- Target ------------------------------------------------------------------
		Scope<VulkanRenderTarget> Target;
		// RenderView::SetSize, applied at the start of the view's next render.
		uint32_t RequestedWidth = 0;
		uint32_t RequestedHeight = 0;
		// ImGui handles for each frame in flight's image, made lazily. After a resize each is pointed at its new image
		// when its frame index is recorded next.
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> ImGuiTextures{};
		std::array<bool, MAX_FRAMES_IN_FLIGHT> ImGuiTexturesOutdated{};

		// ---- Camera ------------------------------------------------------------------
		glm::mat4 ViewProjection{ 1.0f };
		// Renderer2D::SetViewFocus this frame: the depth of field measures from here. HasFocus false: no depth of field.
		glm::vec3 FocusPoint{ 0.0f };
		glm::vec3 FocusForward{ 0.0f, 0.0f, -1.0f };
		bool HasFocus = false;
		PerFrameBuffers UniformBuffers;  // the camera (UniformBufferObject)

		// ---- Forward lighting ----------------------------------------------------------
		// Set 0 bindings 2 and 3 of every texture set, written every frame by SceneLightingBuffer.
		PerFrameBuffers LightingBuffers;
		PerFrameBuffers LightStorageBuffers;
		SceneLighting Lighting;        // Renderer2D::SetSceneLighting this frame
		bool LightingSubmitted = false;

		// ---- Batches -----------------------------------------------------------------
		// Index i in each vector belongs to batch i. Batches are reused, never freed, so a heavy frame allocates once.
		std::vector<QuadVertex*> QuadVertexBufferBases;        // CPU staging, [batch]
		std::vector<PerFrameBuffers> QuadVertexBuffers;        // GPU,         [batch][frame]
		std::vector<uint32_t> QuadIndexCounts;                 // indices used, [batch]

		uint32_t BatchIndex = 0;                     // batch being filled
		QuadVertex* QuadVertexBufferPtr = nullptr;   // write cursor into it

		// ---- Textures and draws ------------------------------------------------------
		std::vector<TextureSet> TextureSets;         // reused like batches
		uint32_t TextureSetIndex = 0;                // set being filled
		// Translucent draws in submission order (every quad without shared depth).
		std::vector<QuadDraw> Draws;
		// Shared depth only (Renderer2D::SetSpriteDrawMode): decals, then cut-out sprites, drawn before Draws.
		std::vector<QuadDraw> DecalDraws;
		std::vector<QuadDraw> CutoutDraws;
		std::vector<QuadDraw> FadedCutoutDraws; // drawn right after CutoutDraws
		// Between BeginOverlay and EndOverlay: drawn after the lighting, so lights and shadows don't darken them.
		std::vector<QuadDraw> OverlayDraws;
		bool InOverlay = false;
		// Renderer2D::BeginScreenOverlay: corners come in screen space and ScreenToWorld (in double, so pixels stay exact)
		// takes them to the scene's world.
		bool InScreenSpace = false;
		bool ScreenSpaceValid = false; // the view-projection could be inverted (else screen-space quads are dropped)
		glm::dmat4 ScreenToWorld{ 1.0 };
		// Between BeginDebugLines and EndDebugLines: depth tested, never writing depth or IDs. Editor views only.
		std::vector<QuadDraw> DebugDraws;
		bool InDebugLines = false;
		SpriteDrawMode DrawMode = SpriteDrawMode::Translucent; // the list quads go to outside the overlay
		int32_t EntityID = -1; // what the next quads carry
		glm::vec3 Emission{ 0.0f }; // what the next quads carry
		std::vector<MeshDraw> MeshDraws;
		// Between BeginShadowCasters and EndShadowCasters: drawn only into the shadow map.
		std::vector<QuadDraw> ShadowCasterDraws;
		bool InShadowCasters = false;

		// ---- Lights ------------------------------------------------------------------
		// Queued by DrawLight / DrawGlobalLight / DrawShadow, drawn by Light2DPass.
		std::vector<LightInstance> Lights;
		std::vector<ShadowInstance> Shadows;
		glm::vec3 AmbientLight{ 0.0f };  // the sum of the global lights: what the light buffer is cleared to
		bool HasLights = false;          // a light of any kind was submitted this frame
		// Made the first frame the view is lit, then kept and resized with the view.
		Scope<VulkanRenderTarget> LightTarget;                    // RGBA16F, the size of Target
		VkDescriptorSet LightBufferSet = VK_NULL_HANDLE;  // the composite's input this frame (a per-frame set)
		PerFrameBuffers LightVertexBuffers;                    // 4 vertices per light, then 4 per shadow
		std::vector<QuadVertex> LightVertices;                    // CPU staging, reused every frame

		// ---- HDR -----------------------------------------------------------------------
		// The scene pass draws here with Target's depth and IDs; the Tonemap pass writes Target's colour from it. Null without HDR.
		Scope<VulkanRenderTarget> SceneColor;
		// SceneColor while the HDR scene pass records (pipelines are built for its format), else null.
		VulkanRenderTarget* PassColorTarget = nullptr;
		ScenePostProcess PostProcess;     // Renderer2D::SetPostProcess this frame
		VkDescriptorSet TonemapSet = VK_NULL_HANDLE; // the Tonemap pass's input this frame
		// Level 0 at half the view's size, each next half again (RGBA16F). After the pass level 0 holds the glow.
		std::vector<Scope<VulkanRenderTarget>> BloomLevels;
		bool BloomDrawn = false; // this frame: the Tonemap pass adds level 0
		// Depth of field (#135): the signed circle of confusion (R16F, full size, in pixels, negative in front of the focus),
		// the scene colour at half size with it in alpha, and that blurred (alpha: how far a blurred foreground reaches).
		Scope<VulkanRenderTarget> DofCoc;
		Scope<VulkanRenderTarget> DofHalf;
		Scope<VulkanRenderTarget> DofBlur;
		bool DofDrawn = false; // this frame: the Tonemap pass mixes in DofBlur by DofCoc

		// ---- Sun shadows ---------------------------------------------------------------
		// DEPTH32F, RendererSettings::SunShadowMapSize square, made the first frame the view draws one. Set 0 binding 4.
		Scope<VulkanRenderTarget> ShadowTarget;
		bool SunShadowsActive = false; // this frame: the pass is in the graph and the scene has a shadowing sun
		glm::mat4 ShadowMatrix{ 1.0f }; // world to the shadow map's clip space, this frame

		// ---- Capture (RenderView::RequestCapture) --------------------------------------
		std::filesystem::path RequestedCapture;  // empty = none; recorded after the view's next pass
		Scope<VulkanImageReadback> Readback;
		// The file each frame index's copy goes to, written when that frame's fence has signalled.
		std::array<std::filesystem::path, MAX_FRAMES_IN_FLIGHT> CapturePaths;

		// ---- Picking (RenderView::RequestEntityIDReadback) -------------------------------
		// A copy counts only for the newest request and an unchanged generation (bumped on a resize and a scene change).
		bool PickRequested = false;
		uint32_t PickX = 0;
		uint32_t PickY = 0;
		uint64_t PickRequest = 0;    // the newest request's number
		uint64_t PickGeneration = 0;
		std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> PickSlotRequest{};
		std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> PickSlotGeneration{};
		Scope<VulkanImageReadback> PickReadback;
		bool PickResultReady = false;
		int32_t PickResult = -1;

		// ---- Render graph --------------------------------------------------------------
		// RenderView::SetRenderFeatures (SceneRenderer sets it every frame); the graph is rebuilt only when it changes.
		RenderFeatureSettings Features;
		Scope<RenderGraph> Graph;
		bool EditorView = false;

		bool Queued = false; // begun this frame, so ViewRenderer records it
	};

	// What a render graph pass records with (RenderGraph.h declares it; only the backend defines it).
	struct RenderPassContext
	{
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		uint32_t Frame = 0;
		RenderViewData& View;
	};
}
