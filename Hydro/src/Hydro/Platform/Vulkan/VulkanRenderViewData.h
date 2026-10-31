#pragma once
// Backend data of a RenderView: its target, camera buffer, quad batches, texture sets, lights, capture
// state and render graph. Shared by Renderer2D (which fills and records it) and RenderView (which owns
// it). Backend-only: never include it from a Renderer/ header.
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
	// Attachment formats: every view's target, and the 2D light buffer. Pipelines are built from these
	// (PipelineSpecification::ColorFormats), so a target and the pipelines drawing into it must agree.
	static constexpr ImageFormat ViewColorFormat = ImageFormat::SRGB;
	static constexpr ImageFormat LightBufferFormat = ImageFormat::RGBA16F;
	// The HDR scene colour (post-processing plan P1): linear, values above 1 kept for the tonemap and bloom.
	static constexpr ImageFormat SceneColorFormat = ImageFormat::RGBA16F;

	// Sun shadows (R14): every view's shadow map (Renderer2D's SunShadows feature; Renderer3D draws the mesh
	// casters into it too).
	static constexpr ImageFormat ShadowMapFormat = ImageFormat::DEPTH32F;
	// Rasteriser depth bias of shadow casters (meshes and sprites): away from the sun (forward Z), so a lit
	// surface stays in front of its own entry in the map. The lit shaders add a normal offset on top.
	static constexpr float ShadowCasterDepthBiasConstant = 1.25f;
	static constexpr float ShadowCasterDepthBiasSlope = 1.75f;

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
		// The entity the quad belongs to, written to the Editor view's entity-ID attachment (click-to-select,
		// M7); -1 = none (particles, lines, lights, effects).
		int32_t EntityID = -1;
		// Where the lit shaders look this point up in the sun shadow map (R14). Its own position, except on a
		// sprite that casts a sun-facing shadow (SpriteShadowMode::FaceSun): the same texel of that caster quad,
		// so the sprite is never darkened by its own shadow, only by what stands between it and the sun.
		glm::vec3 ShadowPosition{ 0.0f };
		// Emission (post-processing plan P3): the sprite shaders add the pixel's colour times this after the
		// lighting. 0 = none (Renderer2D::SetEmission).
		glm::vec3 Emission{ 0.0f };
	};
	// Read by the vertex shader through Renderer2D's vertex layout (GetQuadVertexLayout), which packs the
	// attributes in this order without padding.
	static_assert(sizeof(QuadVertex) == 68, "QuadVertex: must match GetQuadVertexLayout");
	static_assert(offsetof(QuadVertex, Color) == 12 && offsetof(QuadVertex, TexCoord) == 28
		&& offsetof(QuadVertex, TexIndex) == 36 && offsetof(QuadVertex, EntityID) == 40
		&& offsetof(QuadVertex, ShadowPosition) == 44 && offsetof(QuadVertex, Emission) == 56, "QuadVertex: must match GetQuadVertexLayout");

	// Textures one draw call can sample. The quad shader declares `sampler2D inTextures[32]` and the
	// descriptor set's image array has VulkanDescriptorBuilder::MaxImageArraySize elements; all three agree.
	static constexpr uint32_t MaxTextureSlots = VulkanDescriptorBuilder::MaxImageArraySize;
	static_assert(MaxTextureSlots == 32, "Change inTextures[] and SampleTexture in sprite_common.glsl too");

	// One GPU vertex buffer per frame in flight. While frame N is still being read by
	// the GPU, frame N+1 writes into its own buffer, so the CPU never overwrites data
	// the GPU is using.

	// Up to MaxTextureSlots textures that one or more draws sample from, and the set 0 descriptor sets
	// (camera + texture array, one per frame in flight) that bind them. Slot 0 is always the white texture.
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

	// A blob shadow queued for a view this frame.
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
		uint32_t Batch = 0;       // vertex batch the quads are in
		uint32_t FirstIndex = 0;  // first index in that batch (6 per quad)
		uint32_t IndexCount = 0;
		uint32_t TextureSet = 0;
		bool Pickable = false;    // its quads write their entity IDs (an entity's sprite, not a particle or line)
	};

	// Everything one RenderView needs: its target, its camera and its quads.
	struct MeshMaterial; // Renderer3D.cpp
	struct TerrainSurfaceGPU; // Renderer3D.cpp

	// One sub-mesh queued by Renderer3D::DrawMesh, recorded by the view's Meshes pass.
	struct MeshDraw
	{
		Ref<Mesh> MeshRef;
		uint32_t SubMesh = 0;
		MeshMaterial* Material = nullptr; // owned by Renderer3D's material cache (kept while used); a terrain's: white, for its shadow
		TerrainSurfaceGPU* TerrainSurface = nullptr; // a terrain surface (DrawTerrainSurface), drawn with terrain.frag
		glm::mat4 Transform{ 1.0f };
		int32_t EntityID = -1;
		float SortDepth = 0.0f; // clip-space depth of the mesh's centre: front to back
		bool CastShadows = true; // drawn into the sun shadow map too (MeshComponent::CastShadows)
	};

	struct RenderViewData
	{
		std::string DebugName;

		// ---- Target ------------------------------------------------------------------
		Scope<VulkanRenderTarget> Target;
		// Requested size (RenderView::SetSize). Applied at the start of the view's next render.
		uint32_t RequestedWidth = 0;
		uint32_t RequestedHeight = 0;
		// ImGui texture handles for the image of each frame in flight. Created lazily (ImGui may not be
		// initialised when the view is). After a resize each one is pointed at its new image when its
		// frame index is recorded next (a frame still in flight may be drawing with the old one).
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> ImGuiTextures{};
		std::array<bool, MAX_FRAMES_IN_FLIGHT> ImGuiTexturesOutdated{};

		// ---- Camera ------------------------------------------------------------------
		glm::mat4 ViewProjection{ 1.0f };
		PerFrameBuffers UniformBuffers;  // the camera (UniformBufferObject), one buffer per frame in flight

		// ---- Forward lighting (M6) -----------------------------------------------------
		// Set 0 bindings 2 and 3 of every texture set: FrameLightingUniform and MaxForwardLights GPULights,
		// written every frame (unlit views write Counts.y = 0).
		PerFrameBuffers LightingBuffers;
		PerFrameBuffers LightStorageBuffers;
		SceneLighting Lighting;        // Renderer2D::SetSceneLighting this frame; cleared with the batches
		bool LightingSubmitted = false;

		// ---- Batches -----------------------------------------------------------------
		// Vertex storage. Index i in each vector belongs to batch i. Batches are never freed, only
		// reused, so after the first heavy frame there are no more allocations.
		std::vector<QuadVertex*> QuadVertexBufferBases;        // CPU staging, [batch]
		std::vector<PerFrameBuffers> QuadVertexBuffers;        // GPU,         [batch][frame]
		std::vector<uint32_t> QuadIndexCounts;                 // indices used, [batch]

		uint32_t BatchIndex = 0;                     // batch currently being filled
		QuadVertex* QuadVertexBufferPtr = nullptr;   // write cursor into current batch

		// ---- Textures and draws ------------------------------------------------------
		// Texture sets are reused like batches (their descriptor sets are created once).
		std::vector<TextureSet> TextureSets;
		uint32_t TextureSetIndex = 0;                // set currently being filled
		// This frame's draw calls in submission order. A new draw starts when the batch is full
		// or the texture set is full. Every quad without shared depth; with it, the translucent ones.
		std::vector<QuadDraw> Draws;
		// Shared depth only (Renderer2D::SetSpriteDrawMode): decals, then cut-out sprites, drawn before Draws.
		std::vector<QuadDraw> DecalDraws;
		std::vector<QuadDraw> CutoutDraws;
		// Draws made between BeginOverlay and EndOverlay. They are recorded after the lighting is
		// composited, so lights and shadows don't darken them (editor outlines, debug shapes).
		std::vector<QuadDraw> OverlayDraws;
		bool InOverlay = false; // quads currently go to OverlayDraws
		// Renderer2D::BeginScreenOverlay: quad corners come in screen space and are taken to the scene's world by
		// ScreenToWorld (inverse view-projection times the screen projection, in double so pixels stay exact).
		bool InScreenSpace = false;
		bool ScreenSpaceValid = false; // the view-projection could be inverted (else screen-space quads are dropped)
		glm::dmat4 ScreenToWorld{ 1.0 };
		// Draws made between BeginDebugLines and EndDebugLines (M7): editor shapes in the scene (bounds, light
		// reach, spot cones), depth tested against it, never writing depth or IDs. Editor views only.
		std::vector<QuadDraw> DebugDraws;
		bool InDebugLines = false;
		SpriteDrawMode DrawMode = SpriteDrawMode::Translucent; // the list quads go to outside the overlay
		int32_t EntityID = -1; // Renderer2D::SetEntityID: what the next quads carry
		glm::vec3 Emission{ 0.0f }; // Renderer2D::SetEmission: what the next quads carry
		std::vector<MeshDraw> MeshDraws;  // Renderer3D::DrawMesh, cleared with the batches
		// Sun shadow casters (R14): quads made between BeginShadowCasters and EndShadowCasters (cut-out sprites,
		// as drawn or turned to face the sun), drawn only into the shadow map.
		std::vector<QuadDraw> ShadowCasterDraws;
		bool InShadowCasters = false;

		// ---- Lights ------------------------------------------------------------------
		// Queued by DrawLight / DrawGlobalLight, drawn by RecordLightPass. Cleared with the batches.
		std::vector<LightInstance> Lights;
		std::vector<ShadowInstance> Shadows;
		glm::vec3 AmbientLight{ 0.0f };  // the sum of the global lights: what the light buffer is cleared to
		bool HasLights = false;          // a light of any kind was submitted this frame
		// Made the first frame the view is lit, then kept and resized with the view.
		Scope<VulkanRenderTarget> LightTarget;                    // the light buffer: RGBA16F, the size of Target
		VkDescriptorSet LightBufferSet = VK_NULL_HANDLE;  // the composite's input this frame (a per-frame set: LightTarget's image)
		PerFrameBuffers LightVertexBuffers;                    // 4 vertices per light, then 4 per shadow
		std::vector<QuadVertex> LightVertices;                    // CPU staging, reused every frame

		// ---- HDR (post-processing plan P1) -----------------------------------------------
		// The scene colour the scene pass draws into when the graph asks for one (RenderGraph::GetSceneColorFormat;
		// the HDR feature), with Target's depth and entity ID; the Tonemap pass writes Target's colour from it.
		// The view's size, resized with it; null without HDR.
		Scope<VulkanRenderTarget> SceneColor;
		// While the HDR scene pass records: SceneColor (pipelines are built for its colour format). Null otherwise.
		VulkanRenderTarget* PassColorTarget = nullptr;
		ScenePostProcess PostProcess;     // Renderer2D::SetPostProcess this frame; reset with the batches
		VkDescriptorSet TonemapSet = VK_NULL_HANDLE; // the Tonemap pass's input this frame (a per-frame set)
		// Bloom (plan P2): the chain, level 0 at half the view's size, each next one half again (RGBA16F), made by
		// the Bloom pass when the view's size or level count changes. After the pass level 0 holds the glow.
		std::vector<Scope<VulkanRenderTarget>> BloomLevels;
		bool BloomDrawn = false; // this frame: the Tonemap pass adds level 0

		// ---- Sun shadows (R14) ------------------------------------------------------------
		// The SunShadows feature's depth map (DEPTH32F, RendererSettings::SunShadowMapSize square), made the
		// first frame the view draws one and kept. Sampled by the lit shaders at set 0 binding 4.
		Scope<VulkanRenderTarget> ShadowTarget;
		bool SunShadowsActive = false; // this frame: the feature is in the graph and the scene has a shadowing sun
		glm::mat4 ShadowMatrix{ 1.0f }; // world -> the shadow map's clip space, this frame

		// ---- Capture (RenderView::RequestCapture) --------------------------------------
		std::filesystem::path RequestedCapture;  // empty = none; recorded after the view's next pass
		Scope<VulkanImageReadback> Readback;     // made by the first capture, then kept
		// The file each frame index's copy goes to; written when that frame's fence has signalled.
		std::array<std::filesystem::path, MAX_FRAMES_IN_FLIGHT> CapturePaths;

		// ---- Picking (RenderView::RequestEntityIDReadback, M7) ---------------------------
		// One pixel of the entity-ID attachment is copied after the view's pass and read when that frame's
		// fence has signalled. A copy is used only when it belongs to the newest request and its generation
		// still matches (bumped on a resize and by RenderView::InvalidateEntityIDRequests).
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

		// ---- Render graph (render features) ---------------------------------------------
		// What the next render should use (RenderView::SetRenderFeatures; SceneRenderer sets it every
		// frame from the scene or the project). The graph is rebuilt only when this changes.
		RenderFeatureSettings Features;
		Scope<RenderGraph> Graph;
		bool EditorView = false; // RenderViewSpecification::EditorView

		bool Queued = false; // begun this frame, so Render() records it
	};

	// What a render graph pass records with (RenderGraph.h declares it; only the backend defines it).
	struct RenderPassContext
	{
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		uint32_t Frame = 0;
		RenderViewData& View;
	};
}
