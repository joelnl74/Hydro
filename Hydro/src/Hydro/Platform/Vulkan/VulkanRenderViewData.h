#pragma once
// Backend data of a RenderView: its target, camera buffer, quad batches, texture sets, lights, capture
// state and render graph. Shared by Renderer2D (which fills and records it) and RenderView (which owns
// it). Backend-only: never include it from a Renderer/ header.
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Image.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/RenderGraph.h"
#include "Hydro/Renderer/Texture2D.h"
#include "VulkanDescriptorBuilder.h"
#include "VulkanFrameContext.h"
#include "VulkanImageReadback.h"
#include "VulkanRenderTarget.h"
#include "VulkanUniformBuffer.h"
#include "VulkanVertexBuffer.h"

#include <glm/glm.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// Attachment formats: every view's target, and the 2D light buffer. Pipelines are built from these
	// (PipelineSpecification::ColorFormats), so a target and the pipelines drawing into it must agree.
	static constexpr ImageFormat ViewColorFormat = ImageFormat::SRGB;
	static constexpr ImageFormat LightBufferFormat = ImageFormat::RGBA16F;

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
	};

	struct UniformBufferObject
	{
		alignas(16) glm::mat4 mvp;
	};

	// Textures one draw call can sample. The quad shader declares `sampler2D inTextures[32]` and the
	// descriptor set's image array has VulkanDescriptorBuilder::MaxImageArraySize elements; all three agree.
	static constexpr uint32_t MaxTextureSlots = VulkanDescriptorBuilder::MaxImageArraySize;
	static_assert(MaxTextureSlots == 32, "Change inTextures[] and SampleTexture in sprite_common.glsl too");

	// One GPU vertex buffer per frame in flight. While frame N is still being read by
	// the GPU, frame N+1 writes into its own buffer, so the CPU never overwrites data
	// the GPU is using.
	using PerFrameVertexBuffers = std::array<Scope<VulkanVertexBuffer>, MAX_FRAMES_IN_FLIGHT>;

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
	};

	// Everything one RenderView needs: its target, its camera and its quads.
	// TEMPORARY(S1): a box drawn by the shared-depth spike (Renderer2D::DrawSpikeBox). Removed in M4.
	struct SpikeBox
	{
		glm::mat4 Transform{ 1.0f }; // the unit box (base centred on the origin, 1 high along +Z) into the world
		glm::vec4 Color{ 1.0f };
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
		Scope<VulkanUniformBuffer> UniformBuffer;   // one buffer per frame in flight

		// ---- Batches -----------------------------------------------------------------
		// Vertex storage. Index i in each vector belongs to batch i. Batches are never freed, only
		// reused, so after the first heavy frame there are no more allocations.
		std::vector<QuadVertex*> QuadVertexBufferBases;        // CPU staging, [batch]
		std::vector<PerFrameVertexBuffers> QuadVertexBuffers;  // GPU,         [batch][frame]
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
		SpriteDrawMode DrawMode = SpriteDrawMode::Translucent; // the list quads go to outside the overlay
		std::vector<SpikeBox> SpikeBoxes; // TEMPORARY(S1)

		// ---- Lights ------------------------------------------------------------------
		// Queued by DrawLight / DrawGlobalLight, drawn by RecordLightPass. Cleared with the batches.
		std::vector<LightInstance> Lights;
		std::vector<ShadowInstance> Shadows;
		glm::vec3 AmbientLight{ 0.0f };  // the sum of the global lights: what the light buffer is cleared to
		bool HasLights = false;          // a light of any kind was submitted this frame
		// Made the first frame the view is lit, then kept and resized with the view.
		Scope<VulkanRenderTarget> LightTarget;                    // the light buffer: RGBA16F, the size of Target
		Scope<VulkanDescriptorBuilder> LightBufferDescriptorSet;  // the composite's input: LightTarget's image of each frame
		PerFrameVertexBuffers LightVertexBuffers;                 // 4 vertices per light, then 4 per shadow
		std::vector<QuadVertex> LightVertices;                    // CPU staging, reused every frame

		// ---- Capture (RenderView::RequestCapture) --------------------------------------
		std::filesystem::path RequestedCapture;  // empty = none; recorded after the view's next pass
		Scope<VulkanImageReadback> Readback;     // made by the first capture, then kept
		// The file each frame index's copy goes to; written when that frame's fence has signalled.
		std::array<std::filesystem::path, MAX_FRAMES_IN_FLIGHT> CapturePaths;

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
