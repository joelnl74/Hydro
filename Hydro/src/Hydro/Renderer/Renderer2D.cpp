#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanShaderLibrary.h"
#include "Hydro/Renderer/Renderer3D.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"
#include "Hydro/Platform/Vulkan/VulkanImageReadback.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

// SHOULD BE IN IMGUI EXTENSION.
#include <Hydro/Platform/Vulkan/imgui_impl_vulkan.h>
// SHOULD BE A IMGUI EXTENSION END BLOCK.

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/ImageWriter.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Core/FileWatcher.h"
#include "Hydro/Core/Time.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <map>
#include <tuple>
#include <array>
#include <cmath>
#include <chrono>

namespace Hydro
{

	// One ImGui texture handle (from ImGui's descriptor pool) and the texture it shows.
	struct ImGuiTextureEntry
	{
		VkDescriptorSet Set = VK_NULL_HANDLE;
		Ref<Texture2D> Texture;     // kept alive while the handle may be drawn
		uint64_t LastUsedFrame = 0; // Time::GetFrameCount() when last handed out
	};

	// What a view pipeline draws; its depth and entity-ID behaviour follow from the kind (BuildViewPipeline).
	enum class ViewPipelineKind : uint8_t
	{
		Sprite = 0, // translucent quads: depth test (no write) when there is depth
		Decal,      // ground decals: like Sprite, with a small depth bias towards the camera
		Cutout,     // cut-out quads: depth test and write, alpha < 0.5 discarded, no blending
		Overlay,    // overlay quads (editor outlines, HUD sprites): no depth test
		Composite,  // the 2D light composite
		DebugLines, // editor lines in the scene (M7): depth test, no depth write, never IDs
		ShadowCaster, // sun shadow casters (R14): depth only, alpha tested, into a view's shadow map
		Tonemap       // HDR views (post-processing plan P1): the scene colour through exposure and PBR Neutral, a fullscreen triangle
	};

	// The pipeline cache's key: the attachment layout (colour formats in order, depth format; one sample per
	// pixel) plus what the pipeline does with it.
	struct ViewPipelineKey
	{
		ViewPipelineKind Kind = ViewPipelineKind::Sprite;
		std::vector<ImageFormat> ColorFormats;
		ImageFormat DepthFormat = ImageFormat::None;
		bool WriteEntityID = false;
		// Forward lit (M6): the Sprite, Decal and Cutout kinds use their lit shaders.
		bool Lit = false;

		bool operator<(const ViewPipelineKey& other) const
		{
			return std::tie(Kind, ColorFormats, DepthFormat, WriteEntityID, Lit) < std::tie(other.Kind, other.ColorFormats, other.DepthFormat, other.WriteEntityID, other.Lit);
		}
	};

	struct Renderer2DData
	{
		// Editor previews (GetImGuiTexture). Sets are never freed (this ImGui version has no
		// RemoveTexture); they are rewritten for another texture once unused for a while.
		std::vector<ImGuiTextureEntry> ImGuiTextures;

		// Quads per batch (= per vertex buffer). 20k quads * 4 verts * 56 bytes = ~4.5 MB per buffer.
		// When a frame needs more, extra batches are allocated on demand.
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;

		glm::vec4 QuadVertexPositions[4];

		// Shader hot reload: VulkanShaderLibrary::ReloadChanged, at most every ShaderPollInterval.
		std::chrono::steady_clock::time_point NextShaderPoll{};
		static constexpr std::chrono::milliseconds ShaderPollInterval{ 250 };

		Scope<VulkanPipeline> QuadPipeline;

		// 2D lighting: lights are drawn into a per-view RGBA16F light buffer (LightPipeline, additive), which
		// is multiplied over the scene (CompositePipeline). Built like QuadPipeline, from the attachment
		// formats (LightBufferFormat, ViewColorFormat). Optional: all of it stays null when the light shaders
		// can't be built, and views are then drawn unlit (see CreateLightingResources).
		Scope<VulkanPipeline> LightPipeline;
		Scope<VulkanDescriptorBuilder> CompositeLayoutDescriptorSet;
		Scope<VulkanPipeline> CompositePipeline;
		bool LightLimitWarned = false;

		// Blob shadows: drawn into the light buffer after the lights (BlendMode::Darken). Optional like the
		// lights, and separately: without the shadow shader the lights still work.
		Scope<VulkanPipeline> ShadowPipeline;
		bool ShadowLimitWarned = false;

		// Pipelines for the views' other attachment layouts (shared depth, the entity ID), built on first use
		// by GetViewPipeline and kept per layout and kind. Cleared after a shader reload or a wireframe change
		// (ClearViewPipelines); a null entry is one that failed to build (logged once). The plain 2D layout
		// (one colour attachment) keeps using the pipelines above.
		std::map<ViewPipelineKey, Scope<VulkanPipeline>> ViewPipelines;


		Scope<VulkanBuffer> QuadIndexBuffer;    // GpuOnly; shared by all batches of all views (every batch starts at vertex 0)

		// Sun shadows (R14): the comparison sampler every view's shadow map is read with (set 0 binding 4),
		// and a 1 x 1 map cleared to "nothing in the way" that the binding points at in views that don't draw
		// one this frame (the lit shaders always declare it, so it must always be valid).
		VkSampler ShadowSampler = VK_NULL_HANDLE;
		Scope<VulkanRenderTarget> EmptyShadowMap;

		// HDR (post-processing plan P1): set 0 of tonemap.frag, the scene colour (one sampled image). Owned by the
		// descriptor allocator's layout cache.
		VkDescriptorSetLayout TonemapLayout = VK_NULL_HANDLE;

		// The pipeline's set 0 layout. Every view's target has the same attachment format (ViewColorFormat)
		// and its descriptor sets are built exactly like LayoutDescriptorSet (BuildViewDescriptorSet), so
		// they can be bound in its place.
		PerFrameBuffers LayoutUniformBuffers;
		PerFrameBuffers LayoutLightingBuffers;
		PerFrameBuffers LayoutLightStorageBuffers;
		Scope<VulkanDescriptorBuilder> LayoutDescriptorSet;
		bool LightLimitWarnedForward = false;

		// GPU time per render graph pass (M7): a second query pool, a begin/end pair per recorded pass, read
		// when the frame's fence has signalled (never waiting). The names are summed over the views.
		static constexpr uint32_t MaxTimedPasses = 48;
		VkQueryPool PassTimestampPool = VK_NULL_HANDLE;
		std::array<std::vector<std::string>, MAX_FRAMES_IN_FLIGHT> TimedPassNames;
		std::vector<PassTiming> LastPassTimings;

		// vkCmdBeginDebugUtilsLabelEXT / End (VK_EXT_debug_utils), null when the instance doesn't have it.
		PFN_vkCmdBeginDebugUtilsLabelEXT BeginLabel = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT EndLabel = nullptr;

		// Slot 0 of every texture set; made in code (no file).
		Ref<Texture2D> WhiteTexture;
		// The default particle texture; made in code.
		Ref<Texture2D> SoftDotTexture;

		// Every RenderView that exists (they register themselves).
		std::vector<RenderViewData*> Views;

		// Views begun this frame, in BeginScene order, and the one DrawQuad writes to.
		std::vector<RenderViewData*> QueuedViews;
		RenderViewData* CurrentView = nullptr;

		// GPU timestamps per frame in flight: queries 0/1 are begin/end of all 2D passes, then for each
		// of the first MaxTimedViews lit views three begin/end pairs (TimerPhase): light buffer pass
		// (including shadows), shadows, composite.
		static constexpr uint32_t MaxTimedViews = 4;
		static constexpr uint32_t TimerPhaseCount = 3;
		static constexpr uint32_t QueriesPerFrame = 2 + MaxTimedViews * TimerPhaseCount * 2;
		VkQueryPool TimestampPool = VK_NULL_HANDLE;
		float TimestampPeriodNs = 0.0f;  // ns per timestamp tick
		std::array<bool, MAX_FRAMES_IN_FLIGHT> TimestampsWritten{};
		float LastGPUTimeMs = -1.0f;
		float LastLightPassGPUMs = -1.0f;
		float LastShadowGPUMs = -1.0f;
		float LastCompositeGPUMs = -1.0f;
		// Which phases of which timed view were written into each frame slot (bit per phase).
		std::array<std::array<uint32_t, MaxTimedViews>, MAX_FRAMES_IN_FLIGHT> TimedPhaseMask{};
		uint32_t TimedViewCount = 0;   // lit views timed so far this frame
		uint32_t CurrentTimerSlot = UINT32_MAX; // timed slot of the view being recorded
		char GPUName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};

		Statistics Stats;          // being accumulated this frame
		Statistics LastFrameStats; // what GetStats() returns
	};

	static Renderer2DData* s_Data = nullptr;

	// The lighting phases timed per lit view (index = which begin/end query pair of the view's slot).
	enum class TimerPhase : uint32_t
	{
		LightBuffer = 0, // the whole light buffer pass, shadows included
		Shadows = 1,
		Composite = 2
	};

	// Writes the begin (top of pipe) or end (bottom of pipe) timestamp of `phase` for the lit view being
	// recorded. Does nothing without timestamp support or past MaxTimedViews lit views. Allowed inside a render pass.
	static void WriteTimer(VkCommandBuffer commandBuffer, uint32_t frame, TimerPhase phase, bool end)
	{
		const uint32_t slot = s_Data->CurrentTimerSlot;
		if (s_Data->TimestampPool == VK_NULL_HANDLE || slot >= Renderer2DData::MaxTimedViews)
		{
			return;
		}

		const uint32_t phaseIndex = (uint32_t)phase;
		const uint32_t query = frame * Renderer2DData::QueriesPerFrame + 2 + (slot * Renderer2DData::TimerPhaseCount + phaseIndex) * 2 + (end ? 1u : 0u);
		vkCmdWriteTimestamp(commandBuffer, end ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_Data->TimestampPool, query);
		if (end)
		{
			s_Data->TimedPhaseMask[frame][slot] |= 1u << phaseIndex;
		}
	}

	// A named region in the command buffer (RenderDoc, Nsight, validation messages), when VK_EXT_debug_utils
	// is on; nothing otherwise.
	static void BeginDebugLabel(VkCommandBuffer commandBuffer, const char* name)
	{
		if (s_Data->BeginLabel == nullptr)
		{
			return;
		}
		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = name;
		s_Data->BeginLabel(commandBuffer, &label);
	}

	static void EndDebugLabel(VkCommandBuffer commandBuffer)
	{
		if (s_Data->EndLabel != nullptr)
		{
			s_Data->EndLabel(commandBuffer);
		}
	}

	static const VulkanImageInfo& GetImageInfo(const Texture2D& texture)
	{
		return GetTexture2DData(texture).Image->GetVulkanImageInfo();
	}

	// Set 0 of the quad and mesh shaders: binding 0 = camera (vertex), binding 1 = texture array (fragment),
	// binding 2 = forward lighting (FrameLightingUniform, vertex and fragment), binding 3 = the point and spot
	// lights (a storage buffer of GPULight, fragment), binding 4 = the sun shadow map (a comparison sampler,
	// fragment, R14). Every texture slot starts as the white texture and the shadow map as the empty one;
	// RecordView writes the real ones and the lighting each frame.
	static Scope<VulkanDescriptorBuilder> BuildViewDescriptorSet(const PerFrameBuffers& uniformBuffers, const PerFrameBuffers& lightingBuffers, const PerFrameBuffers& lightBuffers)
	{
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);
		const VulkanImageInfo& emptyShadowMap = s_Data->EmptyShadowMap->GetDepthImage(0)->GetVulkanImageInfo();

		Scope<VulkanDescriptorBuilder> descriptorSet = CreateScope<VulkanDescriptorBuilder>();
		descriptorSet->Begin();
		descriptorSet->BindBuffer(0, uniformBuffers, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		descriptorSet->BindImage(1, white.ImageView, white.Sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->BindBuffer(2, lightingBuffers, sizeof(FrameLightingUniform), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->BindBuffer(3, lightBuffers, sizeof(GPULight) * MaxForwardLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->BindImage(4, emptyShadowMap.ImageView, s_Data->ShadowSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
		descriptorSet->Build();
		return descriptorSet;
	}

	// Points the texture array of `set`'s descriptor set for `frame` at its textures (unused slots: white).
	static void WriteTextureSet(TextureSet& set, uint32_t frame)
	{
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);

		std::array<VkDescriptorImageInfo, MaxTextureSlots> images{};
		for (uint32_t i = 0; i < MaxTextureSlots; i++)
		{
			const VulkanImageInfo& image = (i < set.SlotCount && set.Slots[i] != nullptr) ? GetImageInfo(*set.Slots[i]) : white;
			images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			images[i].imageView = image.ImageView;
			images[i].sampler = image.Sampler;
		}

		set.DescriptorSet->WriteImages(frame, 1, images.data(), MaxTextureSlots, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	// Points `set`'s shadow-map binding (4) for `frame` at the view's sun shadow map when it draws one this
	// frame, else at the empty one.
	static void WriteShadowMapBinding(TextureSet& set, const RenderViewData& view, uint32_t frame)
	{
		const VulkanRenderTarget& map = (view.SunShadowsActive && view.ShadowTarget != nullptr) ? *view.ShadowTarget : *s_Data->EmptyShadowMap;
		const uint32_t mapFrame = &map == s_Data->EmptyShadowMap.get() ? 0 : frame;
		VkDescriptorImageInfo image{};
		image.sampler = s_Data->ShadowSampler;
		image.imageView = map.GetDepthImage(mapFrame)->GetVulkanImageInfo().ImageView;
		image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		set.DescriptorSet->WriteImages(frame, 4, &image, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	// The vertex layout of QuadVertex (the quad shader's inputs). The light quads use it too.
	static VertexBufferLayout GetQuadVertexLayout()
	{
		return VertexBufferLayout
		{
			{ "inPosition", ShaderDataType::Float3, },
			{ "inColor", ShaderDataType::Float4, },
			{ "inTexCoord", ShaderDataType::Float2, },
			{ "inTexIndex", ShaderDataType::Float, },
			{ "inEntityID", ShaderDataType::Int, },
			{ "inShadowPosition", ShaderDataType::Float3, },
		};
	}

	// Renderer2D's shaders in the VulkanShaderLibrary, by name.
	static constexpr const char* SpriteShaderName = "Sprite";
	static constexpr const char* SpriteCutoutShaderName = "SpriteCutout";
	static constexpr const char* LightShaderName = "Light";
	static constexpr const char* ShadowShaderName = "Shadow";
	static constexpr const char* CompositeShaderName = "Composite";
	// Forward lit (M6): the same vertex shader, lit fragment shaders (lighting.glsl).
	static constexpr const char* SpriteLitShaderName = "SpriteLit";
	static constexpr const char* SpriteCutoutLitShaderName = "SpriteCutoutLit";
	// The SunShadows feature's pass (looked up by name to know whether a view draws a shadow map).
	static constexpr const char* SunShadowPassName = "SunShadows";
	// Sun shadow casters (R14): quads into the shadow map, alpha tested.
	static constexpr const char* SpriteShadowCasterShaderName = "SpriteShadowCaster";
	// HDR (post-processing plan P1): the tonemap, on composite.vert's fullscreen triangle.
	static constexpr const char* TonemapShaderName = "Tonemap";
	// The HDR feature's pass (looked up by name to know whether a view renders through the scene colour).
	static constexpr const char* TonemapPassName = "Tonemap";

	// The shader loaded under `name`, loading it the first time. Throws when it doesn't compile.
	static VulkanShader& GetOrLoadShader(const char* name, const char* vertexPath, const char* fragmentPath)
	{
		VulkanShader* shader = VulkanShaderLibrary::Get(name);
		return shader != nullptr ? *shader : VulkanShaderLibrary::Load(name, vertexPath, fragmentPath);
	}

	// Set 0 of the composite shader: binding 0 = the light buffer (an image array, see BindImage). Only its
	// layout is used (the composite pipeline is built with it); RecordLightPass gives each lit view a
	// per-frame set with that layout, pointing at the view's light buffer.
	static Scope<VulkanDescriptorBuilder> BuildLightBufferDescriptorSet()
	{
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);

		Scope<VulkanDescriptorBuilder> descriptorSet = CreateScope<VulkanDescriptorBuilder>();
		descriptorSet->Begin();
		descriptorSet->BindImage(0, white.ImageView, white.Sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->Build();
		return descriptorSet;
	}

	// Lights are drawn as quads (the quad vertex layout, the view's camera set) and added up in the light buffer.
	static Scope<VulkanPipeline> BuildLightPipeline(VulkanShader& shader)
	{
		PipelineSpecification specification;
		specification.Layout = GetQuadVertexLayout();
		specification.DescriptorSets = { s_Data->LayoutDescriptorSet.get() };
		specification.Shader = &shader;
		specification.ColorFormats = { LightBufferFormat };
		specification.Blend = BlendMode::Additive;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;
		return CreateScope<VulkanPipeline>(specification);
	}

	// Blob shadows are drawn like the lights (quads, the view's camera set) but darken the light buffer.
	static Scope<VulkanPipeline> BuildShadowPipeline(VulkanShader& shader)
	{
		PipelineSpecification specification;
		specification.Layout = GetQuadVertexLayout();
		specification.DescriptorSets = { s_Data->LayoutDescriptorSet.get() };
		specification.Shader = &shader;
		specification.ColorFormats = { LightBufferFormat };
		specification.Blend = BlendMode::Darken;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;
		return CreateScope<VulkanPipeline>(specification);
	}

	// One triangle over the whole view (no vertex buffer), multiplied over what the scene pass has drawn.
	static Scope<VulkanPipeline> BuildCompositePipeline(VulkanShader& shader)
	{
		PipelineSpecification specification;
		specification.DescriptorSets = { s_Data->CompositeLayoutDescriptorSet.get() };
		specification.Shader = &shader;
		specification.ColorFormats = { ViewColorFormat };
		specification.Blend = BlendMode::Multiply2x;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;
		return CreateScope<VulkanPipeline>(specification);
	}

	// Lighting is optional: when a light shader is missing or doesn't compile, this logs it and every view is
	// drawn unlit, instead of the engine failing to start.
	static void CreateLightingResources()
	{
		try
		{
			s_Data->LightPipeline = BuildLightPipeline(VulkanShaderLibrary::Load(LightShaderName, "assets/shaders/vertex.vert", "assets/shaders/light.frag"));

			s_Data->CompositeLayoutDescriptorSet = BuildLightBufferDescriptorSet();
			s_Data->CompositePipeline = BuildCompositePipeline(VulkanShaderLibrary::Load(CompositeShaderName, "assets/shaders/composite.vert", "assets/shaders/composite.frag"));
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer2D: 2D lighting is unavailable, views are drawn unlit: {}", e.what());

			s_Data->CompositePipeline.reset();
			s_Data->CompositeLayoutDescriptorSet.reset();
			s_Data->LightPipeline.reset();
		}
	}

	// Shadows are optional on top of the lights: when the shader is missing or doesn't compile, this logs it
	// and views are lit without shadows. Needs the lights.
	static void CreateShadowResources()
	{
		if (s_Data->LightPipeline == nullptr)
		{
			return;
		}

		try
		{
			s_Data->ShadowPipeline = BuildShadowPipeline(VulkanShaderLibrary::Load(ShadowShaderName, "assets/shaders/vertex.vert", "assets/shaders/shadow.frag"));
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer2D: shadows are unavailable, lit views are drawn without them: {}", e.what());

			s_Data->ShadowPipeline.reset();
		}
	}

	// Rebuilds `pipeline` (when it exists) with the library's current version of `shaderName` (shader hot
	// reload). The old pipeline keeps drawing when the new one can't be built.
	static void RebuildPipelineWithShader(Scope<VulkanPipeline>& pipeline, const char* shaderName)
	{
		VulkanShader* shader = VulkanShaderLibrary::Get(shaderName);
		if (pipeline == nullptr || shader == nullptr)
		{
			return;
		}

		try
		{
			PipelineSpecification specification = pipeline->GetSpecification();
			specification.Shader = shader;
			pipeline = CreateScope<VulkanPipeline>(specification);
		}
		catch (const std::exception& e)
		{
			HY_CORE_WARN("Renderer2D: the '{}' pipeline couldn't be rebuilt; still using the previous one: {}", shaderName, e.what());
		}
	}

	// ---- Shared depth -----------------------------------------------------------

	static constexpr ImageFormat SharedDepthFormat = ImageFormat::DEPTH32F;


	// Drops the cached view pipelines (they free themselves through the deletion queue); they are built
	// again on first use from the current shaders and settings.
	static void ClearViewPipelines()
	{
		s_Data->ViewPipelines.clear();
	}

	// The library shader a view pipeline kind is built from.
	static const char* GetViewPipelineShaderName(ViewPipelineKind kind, bool lit)
	{
		switch (kind)
		{
			case ViewPipelineKind::Cutout: return lit ? SpriteCutoutLitShaderName : SpriteCutoutShaderName;
			case ViewPipelineKind::Composite: return CompositeShaderName;
			case ViewPipelineKind::ShadowCaster: return SpriteShadowCasterShaderName;
			case ViewPipelineKind::Tonemap: return TonemapShaderName;
			case ViewPipelineKind::Sprite:
			case ViewPipelineKind::Decal: return lit ? SpriteLitShaderName : SpriteShaderName;
			default: return SpriteShaderName;
		}
	}

	// Drops only the cached view pipelines built from `shaderName` (its hot reload); the rest are kept.
	static void ClearViewPipelines(const std::string& shaderName)
	{
		for (auto it = s_Data->ViewPipelines.begin(); it != s_Data->ViewPipelines.end();)
		{
			if (shaderName == GetViewPipelineShaderName(it->first.Kind, it->first.Lit))
			{
				it = s_Data->ViewPipelines.erase(it);
			}
			else
			{
				++it;
			}
		}
	}


	// A pipeline for `key`, from the pipeline built at Init for that kind of draw (so shaders, vertex layout,
	// descriptor sets and the wireframe setting carry over). Throws when a shader doesn't compile.
	static Scope<VulkanPipeline> BuildViewPipeline(const ViewPipelineKey& key)
	{
		const bool hasDepth = key.DepthFormat != ImageFormat::None;

		PipelineSpecification specification;
		switch (key.Kind)
		{
			case ViewPipelineKind::Sprite:
			case ViewPipelineKind::Decal:
			case ViewPipelineKind::Overlay:
			case ViewPipelineKind::Cutout:
			case ViewPipelineKind::DebugLines:
				specification = s_Data->QuadPipeline->GetSpecification();
				specification.Shader = VulkanShaderLibrary::Get(SpriteShaderName);
				if (key.Lit && (key.Kind == ViewPipelineKind::Sprite || key.Kind == ViewPipelineKind::Decal))
				{
					specification.Shader = &GetOrLoadShader(SpriteLitShaderName, "assets/shaders/vertex.vert", "assets/shaders/fragment_lit.frag");
				}
				if (key.Kind == ViewPipelineKind::Cutout)
				{
					specification.Shader = key.Lit
						? &GetOrLoadShader(SpriteCutoutLitShaderName, "assets/shaders/vertex.vert", "assets/shaders/sprite_cutout_lit.frag")
						: &GetOrLoadShader(SpriteCutoutShaderName, "assets/shaders/vertex.vert", "assets/shaders/sprite_cutout.frag");
					// Opaque where kept: the shader writes alpha 1, nothing to blend.
					specification.Blend = BlendMode::None;
				}
				specification.DepthTest = hasDepth && key.Kind != ViewPipelineKind::Overlay;
				specification.DepthWrite = hasDepth && key.Kind == ViewPipelineKind::Cutout;
				if (hasDepth && key.Kind == ViewPipelineKind::Decal)
				{
					// In front of ground geometry at the same height (later meshes; the spike box's base), never
					// written, so nothing else is affected.
					specification.DepthBiasConstant = -4.0f;
					specification.DepthBiasSlope = -1.0f;
				}
				break;

			case ViewPipelineKind::ShadowCaster:
				// Depth only, both faces (a sprite casts whichever way it is turned), alpha tested in the shader,
				// pushed away from the sun a little so lit surfaces don't shadow themselves (acne).
				specification = s_Data->QuadPipeline->GetSpecification();
				specification.Shader = &GetOrLoadShader(SpriteShadowCasterShaderName, "assets/shaders/shadow_caster.vert", "assets/shaders/shadow_caster.frag");
				specification.Blend = BlendMode::None;
				specification.Wireframe = false;
				specification.BackfaceCulling = false;
				specification.DepthTest = true;
				specification.DepthWrite = true;
				specification.DepthBiasConstant = ShadowCasterDepthBiasConstant;
				specification.DepthBiasSlope = ShadowCasterDepthBiasSlope;
				break;

			case ViewPipelineKind::Tonemap:
				// No vertex buffer (composite.vert makes the triangle), the scene colour in set 0, the exposure in
				// push constants; overwrites every pixel, never depth tested, never writes IDs.
				specification.Shader = &GetOrLoadShader(TonemapShaderName, "assets/shaders/composite.vert", "assets/shaders/tonemap.frag");
				specification.DescriptorSetLayouts = { s_Data->TonemapLayout };
				specification.PushConstantSize = (uint32_t)sizeof(TonemapPushConstants);
				specification.Blend = BlendMode::None;
				specification.BackfaceCulling = false;
				specification.DepthTest = false;
				specification.DepthWrite = false;
				break;

			case ViewPipelineKind::Composite:
				if (s_Data->CompositePipeline == nullptr)
				{
					return nullptr;
				}
				specification = s_Data->CompositePipeline->GetSpecification();
				specification.Shader = VulkanShaderLibrary::Get(CompositeShaderName);
				specification.DepthTest = false;
				specification.DepthWrite = false;
				break;

		}

		specification.ColorFormats = key.ColorFormats;
		specification.DepthFormat = key.DepthFormat;
		specification.WriteIntegerAttachments = key.WriteEntityID && key.Kind != ViewPipelineKind::DebugLines;
		return CreateScope<VulkanPipeline>(specification);
	}

	// The pipeline that draws `kind` into `target`. `writeEntityID`: the draw writes its entity IDs (only
	// meaningful when the target has the ID attachment). `colorTarget`: the pass was begun with another target's
	// colour (the HDR scene colour, RenderViewData::PassColorTarget). Null when it can't be built (logged once).
	static VulkanPipeline* GetViewPipeline(ViewPipelineKind kind, const VulkanRenderTarget& target, bool writeEntityID, bool lit = false,
		const VulkanRenderTarget* colorTarget = nullptr)
	{
		ViewPipelineKey key;
		key.Kind = kind;
		key.ColorFormats = target.GetPipelineColorFormats(colorTarget);
		key.DepthFormat = target.GetPipelineDepthFormat();
		key.WriteEntityID = writeEntityID && target.GetSpecification().EntityID;
		key.Lit = lit && (kind == ViewPipelineKind::Sprite || kind == ViewPipelineKind::Decal || kind == ViewPipelineKind::Cutout);

		// The plain 2D layout: the pipelines built at Init, exactly as before render features existed.
		if (key.ColorFormats.size() == 1 && key.ColorFormats[0] == ViewColorFormat && key.DepthFormat == ImageFormat::None && key.Lit == false)
		{
			if (kind == ViewPipelineKind::Sprite || kind == ViewPipelineKind::Decal || kind == ViewPipelineKind::Overlay || kind == ViewPipelineKind::DebugLines)
			{
				return s_Data->QuadPipeline.get();
			}
			if (kind == ViewPipelineKind::Composite)
			{
				return s_Data->CompositePipeline.get();
			}
		}

		auto it = s_Data->ViewPipelines.find(key);
		if (it != s_Data->ViewPipelines.end())
		{
			return it->second.get();
		}

		Scope<VulkanPipeline> pipeline;
		try
		{
			pipeline = BuildViewPipeline(key);
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer2D: a view pipeline could not be built; those draws are skipped until the next shader change: {}", e.what());
		}

		VulkanPipeline* result = pipeline.get();
		s_Data->ViewPipelines.emplace(std::move(key), std::move(pipeline));
		return result;
	}

	void Renderer2D::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "Renderer2D::Init called twice");
		s_Data = new Renderer2DData();

		// Counter-clockwise starting bottom-left, matching the texture coords in SubmitQuad.
		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		// The quad shader is required (throws when it doesn't compile); the others are optional.
		VulkanShaderLibrary::Load(SpriteShaderName, "assets/shaders/vertex.vert", "assets/shaders/fragment.frag");

		// The white texture is made in code, so plain coloured quads need no file.
		{
			const uint32_t whitePixel = 0xffffffff;
			ImageSpecification whiteSpecification;
			whiteSpecification.Width = 1;
			whiteSpecification.Height = 1;
			whiteSpecification.Format = ImageFormat::SRGB;
			s_Data->WhiteTexture = CreateRef<Texture2D>(whiteSpecification, &whitePixel);
		}

		// The soft dot: white, alpha (1 - r)^2 from the centre to the edge of a 32 x 32 image.
		{
			constexpr uint32_t size = 32;
			std::vector<uint32_t> pixels(size * size);
			for (uint32_t y = 0; y < size; y++)
			{
				for (uint32_t x = 0; x < size; x++)
				{
					const float dx = ((float)x + 0.5f) / (float)size * 2.0f - 1.0f;
					const float dy = ((float)y + 0.5f) / (float)size * 2.0f - 1.0f;
					const float falloff = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy));
					const uint32_t alpha = (uint32_t)std::lround(falloff * falloff * 255.0f);
					pixels[y * size + x] = (alpha << 24) | 0x00ffffffu; // RGBA bytes in memory: ff ff ff alpha
				}
			}
			ImageSpecification dotSpecification;
			dotSpecification.Width = size;
			dotSpecification.Height = size;
			dotSpecification.Format = ImageFormat::SRGB;
			TextureProperties dotProperties;
			dotProperties.SamplerWrap = TextureWrap::Clamp;
			s_Data->SoftDotTexture = CreateRef<Texture2D>(dotSpecification, pixels.data(), dotProperties);
		}

		// Index buffer. FIX: indices are uint32_t (and bound as VK_INDEX_TYPE_UINT32), so the
		// buffer must be MaxIndices * 4 bytes. It used to be sized with sizeof(uint16_t), so only
		// the first half was uploaded and any draw past MaxQuads/2 read outside the buffer.
		uint32_t* quadIndices = new uint32_t[Renderer2DData::MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < Renderer2DData::MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		// Never changes: GPU-only memory, filled once through a staging buffer.
		BufferSpecification indexSpecification;
		indexSpecification.Usage = BufferUsage::Index;
		indexSpecification.Memory = BufferMemory::GpuOnly;
		indexSpecification.Size = Renderer2DData::MaxIndices * sizeof(uint32_t);
		indexSpecification.DebugName = "Quad indices";
		s_Data->QuadIndexBuffer = CreateScope<VulkanBuffer>(indexSpecification, quadIndices);
		delete[] quadIndices;

		// Sun shadows (R14): the comparison sampler (linear: the hardware compares four texels and blends the
		// results, 2 x 2 PCF per tap) and the empty 1 x 1 map, cleared to the far plane ("nothing in the way").
		{
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.compareEnable = VK_TRUE;
			samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // lit (1) where the surface is no further than what the map holds
			samplerInfo.maxLod = 0.0f;
			samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VK_CHECK_RESULT(vkCreateSampler(Renderer::GetVulkanDevice().GetDevice(), &samplerInfo, nullptr, &s_Data->ShadowSampler));

			RenderTargetSpecification emptySpecification;
			emptySpecification.ColorFormats = {};
			emptySpecification.DepthFormat = ShadowMapFormat;
			emptySpecification.SampledDepth = true;
			emptySpecification.DebugName = "Empty shadow map";
			s_Data->EmptyShadowMap = CreateScope<VulkanRenderTarget>(emptySpecification);
			Renderer::GetVulkanDevice().SubmitImmediate([](VkCommandBuffer commandBuffer)
			{
				for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
				{
					s_Data->EmptyShadowMap->Begin(commandBuffer, frame, RenderTargetBeginInfo::Clear(glm::vec4(0.0f)));
					s_Data->EmptyShadowMap->End(commandBuffer, frame);
				}
			});
		}

		// HDR: the tonemap's input, one sampled image.
		{
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = 0;
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.descriptorCount = 1;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			s_Data->TonemapLayout = VulkanDescriptorAllocator::GetLayout({ binding });
		}

		BufferSpecification layoutSpecification;
		layoutSpecification.Usage = BufferUsage::Uniform;
		layoutSpecification.Memory = BufferMemory::HostVisible;
		layoutSpecification.Size = sizeof(UniformBufferObject);
		layoutSpecification.DebugName = "Pipeline layout camera";
		s_Data->LayoutUniformBuffers = CreatePerFrameBuffers(layoutSpecification);
		layoutSpecification.Size = sizeof(FrameLightingUniform);
		layoutSpecification.DebugName = "Pipeline layout lighting";
		s_Data->LayoutLightingBuffers = CreatePerFrameBuffers(layoutSpecification);
		BufferSpecification layoutLightsSpecification;
		layoutLightsSpecification.Usage = BufferUsage::Storage;
		layoutLightsSpecification.Memory = BufferMemory::HostVisible;
		layoutLightsSpecification.Size = sizeof(GPULight) * MaxForwardLights;
		layoutLightsSpecification.DebugName = "Pipeline layout lights";
		s_Data->LayoutLightStorageBuffers = CreatePerFrameBuffers(layoutLightsSpecification);
		s_Data->LayoutDescriptorSet = BuildViewDescriptorSet(s_Data->LayoutUniformBuffers, s_Data->LayoutLightingBuffers, s_Data->LayoutLightStorageBuffers);

		PipelineSpecification specification;
		specification.Layout = GetQuadVertexLayout();

		specification.DescriptorSets = { s_Data->LayoutDescriptorSet.get() };
		specification.Shader = VulkanShaderLibrary::Get(SpriteShaderName);
		specification.ColorFormats = { ViewColorFormat };
		// Sprites: alpha blended, drawn in submission order, visible from both sides (negative scale flips them).
		specification.Blend = BlendMode::Alpha;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;

		s_Data->QuadPipeline = CreateScope<VulkanPipeline>(specification);

		CreateLightingResources();
		CreateShadowResources();

		// Debug labels for passes (RenderDoc), when the instance has VK_EXT_debug_utils.
		{
			// An instance extension: its commands come from the instance (NULL when it isn't enabled).
			VkInstance instance = Renderer::GetRendererContext().GetInstance();
			s_Data->BeginLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
			s_Data->EndLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
			if (s_Data->BeginLabel == nullptr || s_Data->EndLabel == nullptr)
			{
				s_Data->BeginLabel = nullptr;
				s_Data->EndLabel = nullptr;
			}
		}

		// GPU timing and limits.
		{
			VulkanDevice& device = Renderer::GetVulkanDevice();
			const VkPhysicalDeviceProperties& properties = device.GetPhysicalDevice().GetProperties();
			memcpy(s_Data->GPUName, properties.deviceName, sizeof(s_Data->GPUName));

			// The texture array is a fixed size in the shader. Vulkan guarantees only 16 samplers per
			// stage, but every desktop GPU has far more; say so clearly if this one doesn't.
			const uint32_t samplerLimit = std::min(properties.limits.maxPerStageDescriptorSamplers, properties.limits.maxPerStageDescriptorSampledImages);
			if (samplerLimit < MaxTextureSlots)
			{
				HY_CORE_ERROR("Renderer2D: the GPU allows {} textures per shader stage, the quad shader needs {}", samplerLimit, MaxTextureSlots);
			}

			if (properties.limits.timestampComputeAndGraphics)
			{
				s_Data->TimestampPeriodNs = properties.limits.timestampPeriod;

				VkQueryPoolCreateInfo queryPoolInfo{};
				queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
				queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
				queryPoolInfo.queryCount = Renderer2DData::QueriesPerFrame * MAX_FRAMES_IN_FLIGHT;
				VK_CHECK_RESULT(vkCreateQueryPool(device.GetDevice(), &queryPoolInfo, nullptr, &s_Data->TimestampPool));

				queryPoolInfo.queryCount = Renderer2DData::MaxTimedPasses * 2 * MAX_FRAMES_IN_FLIGHT;
				VK_CHECK_RESULT(vkCreateQueryPool(device.GetDevice(), &queryPoolInfo, nullptr, &s_Data->PassTimestampPool));
			}
		}
	}

	void Renderer2D::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		// Views belong to the application and must be gone by now (layers release them in OnDetach).
		HY_CORE_ASSERT(s_Data->Views.empty(), "A RenderView still exists at Renderer2D::Shutdown");

		// Resources free themselves through the deletion queue, which Renderer::Shutdown flushes next.
		// Released in the reverse order of Init.
		if (s_Data->TimestampPool != VK_NULL_HANDLE)
		{
			VkDevice device = Renderer::GetVulkanDevice().GetDevice();
			Renderer::SubmitResourceFree([device, pool = s_Data->TimestampPool]()
			{
				vkDestroyQueryPool(device, pool, nullptr);
			});
		}
		if (s_Data->PassTimestampPool != VK_NULL_HANDLE)
		{
			VkDevice device = Renderer::GetVulkanDevice().GetDevice();
			Renderer::SubmitResourceFree([device, pool = s_Data->PassTimestampPool]()
			{
				vkDestroyQueryPool(device, pool, nullptr);
			});
		}

		// The preview handles belong to ImGui's pool; only the textures they hold are released here.
		s_Data->ImGuiTextures.clear();

		ClearViewPipelines();

		s_Data->ShadowPipeline.reset();
		s_Data->CompositePipeline.reset();
		s_Data->CompositeLayoutDescriptorSet.reset();
		s_Data->LightPipeline.reset();

		s_Data->QuadPipeline.reset();
		s_Data->LayoutDescriptorSet.reset();
		s_Data->LayoutUniformBuffers = {};
		s_Data->LayoutLightingBuffers = {};
		s_Data->LayoutLightStorageBuffers = {};
		s_Data->QuadIndexBuffer.reset();
		s_Data->EmptyShadowMap.reset();
		{
			VkDevice device = Renderer::GetVulkanDevice().GetDevice();
			Renderer::SubmitResourceFree([device, sampler = s_Data->ShadowSampler]()
			{
				vkDestroySampler(device, sampler, nullptr);
			});
		}
		s_Data->SoftDotTexture.reset();
		s_Data->WhiteTexture.reset();

		delete s_Data;
		s_Data = nullptr;
	}

	// ---- Batching ---------------------------------------------------------------

	void Renderer2D::AllocateBatch(RenderViewData& view)
	{
		view.QuadVertexBufferBases.push_back(new QuadVertex[Renderer2DData::MaxVertices]);

		BufferSpecification specification;
		specification.Usage = BufferUsage::Vertex;
		specification.Memory = BufferMemory::HostVisible;
		specification.Size = Renderer2DData::MaxVertices * sizeof(QuadVertex);
		specification.DebugName = view.DebugName + " quad batch " + std::to_string(view.QuadVertexBuffers.size());
		view.QuadVertexBuffers.push_back(CreatePerFrameBuffers(specification));

		view.QuadIndexCounts.push_back(0);
	}

	void Renderer2D::NextBatch(RenderViewData& view)
	{
		// Current batch is full: move on to the next one, creating it the first time we need it.
		view.BatchIndex++;
		if (view.BatchIndex >= view.QuadVertexBufferBases.size())
		{
			AllocateBatch(view);
		}

		view.QuadIndexCounts[view.BatchIndex] = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[view.BatchIndex];

		// A draw call can't span two vertex buffers.
		StartDraw(view);
	}

	void Renderer2D::AllocateTextureSet(RenderViewData& view)
	{
		TextureSet set;
		set.Slots[0] = s_Data->WhiteTexture;
		set.SlotCount = 1;
		set.DescriptorSet = BuildViewDescriptorSet(view.UniformBuffers, view.LightingBuffers, view.LightStorageBuffers);
		view.TextureSets.push_back(std::move(set));
	}

	void Renderer2D::NextTextureSet(RenderViewData& view)
	{
		// "Flush": the current draw has used every texture slot. Later quads get a new draw call with
		// fresh slots. Their vertices stay in the same batch, and earlier draws are recorded first,
		// so the order sprites were submitted in is kept.
		view.TextureSetIndex++;
		if (view.TextureSetIndex >= view.TextureSets.size())
		{
			AllocateTextureSet(view);
		}

		TextureSet& set = view.TextureSets[view.TextureSetIndex];
		for (uint32_t i = 1; i < set.SlotCount; i++)
		{
			set.Slots[i].reset();
		}
		set.SlotCount = 1;

		StartDraw(view);
		s_Data->Stats.TextureFlushes++;
	}

	float Renderer2D::GetTextureSlot(RenderViewData& view, const Ref<Texture2D>& texture)
	{
		if (texture == nullptr || texture == s_Data->WhiteTexture)
		{
			return 0.0f;
		}

		// At most 31 pointer compares; a texture is usually found within the first few slots.
		TextureSet* set = &view.TextureSets[view.TextureSetIndex];
		for (uint32_t i = 1; i < set->SlotCount; i++)
		{
			if (set->Slots[i] == texture)
			{
				return (float)i;
			}
		}

		if (set->SlotCount == MaxTextureSlots)
		{
			NextTextureSet(view);
			set = &view.TextureSets[view.TextureSetIndex];
		}

		const uint32_t slot = set->SlotCount++;
		set->Slots[slot] = texture;
		return (float)slot;
	}

	void Renderer2D::StartDraw(RenderViewData& view)
	{
		QuadDraw draw;
		draw.Batch = view.BatchIndex;
		draw.FirstIndex = view.QuadIndexCounts[view.BatchIndex];
		draw.IndexCount = 0;
		draw.TextureSet = view.TextureSetIndex;
		draw.Pickable = view.EntityID >= 0;

		// A draw nothing was added to yet is reused rather than recorded empty.
		std::vector<QuadDraw>& draws = GetCurrentDraws(view);
		if (draws.empty() == false && draws.back().IndexCount == 0)
		{
			draws.back() = draw;
		}
		else
		{
			draws.push_back(draw);
		}
	}

	void Renderer2D::ResetBatches(RenderViewData& view)
	{
		for (uint32_t& count : view.QuadIndexCounts)
		{
			count = 0;
		}

		view.BatchIndex = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[0];

		// Drop this frame's references to its textures. A texture whose last Ref this was is freed
		// through the deletion queue, after the frames that sample it.
		for (uint32_t s = 0; s <= view.TextureSetIndex && s < view.TextureSets.size(); s++)
		{
			TextureSet& set = view.TextureSets[s];
			for (uint32_t i = 1; i < set.SlotCount; i++)
			{
				set.Slots[i].reset();
			}
			set.SlotCount = 1;
		}
		view.TextureSetIndex = 0;

		view.Lights.clear();
		view.Shadows.clear();
		view.AmbientLight = glm::vec3(0.0f);
		view.HasLights = false;

		view.Draws.clear();
		view.DecalDraws.clear();
		view.CutoutDraws.clear();
		view.OverlayDraws.clear();
		view.InOverlay = false;
		view.DebugDraws.clear();
		view.InDebugLines = false;
		view.ShadowCasterDraws.clear();
		view.InShadowCasters = false;
		view.PostProcess = ScenePostProcess{};
		view.TonemapSet = VK_NULL_HANDLE;
		view.Lighting = SceneLighting{};
		view.LightingSubmitted = false;
		view.DrawMode = SpriteDrawMode::Translucent;
		view.EntityID = -1;
		view.MeshDraws.clear();
		StartDraw(view);
	}

	std::vector<QuadDraw>& Renderer2D::GetCurrentDraws(RenderViewData& view)
	{
		if (view.InShadowCasters)
		{
			return view.ShadowCasterDraws;
		}
		if (view.InDebugLines)
		{
			return view.DebugDraws;
		}
		if (view.InOverlay)
		{
			return view.OverlayDraws;
		}
		switch (view.DrawMode)
		{
			case SpriteDrawMode::Decal: return view.DecalDraws;
			case SpriteDrawMode::Cutout: return view.CutoutDraws;
			default: return view.Draws;
		}
	}

	// ---- Scenes -----------------------------------------------------------------

	void Renderer2D::BeginScene(RenderView& view, const glm::mat4& viewProjection)
	{
		HY_CORE_ASSERT(s_Data->CurrentView == nullptr, "Renderer2D::BeginScene without EndScene");

		RenderViewData& data = *view.m_Data;
		data.ViewProjection = viewProjection;

		if (data.Queued == false)
		{
			data.Queued = true;
			s_Data->QueuedViews.push_back(&data);
		}

		s_Data->CurrentView = &data;
	}

	void Renderer2D::BeginScene(RenderView& view, const Camera& camera, const glm::mat4& cameraTransform)
	{
		BeginScene(view, camera.GetProjection() * glm::inverse(cameraTransform));
	}

	void Renderer2D::EndScene()
	{
		// An overlay left open and a sprite draw mode end with the scene, so the next BeginScene starts with
		// translucent draws.
		RenderViewData* view = s_Data->CurrentView;
		if (view != nullptr && (view->InOverlay || view->InDebugLines || view->InShadowCasters || view->DrawMode != SpriteDrawMode::Translucent || view->EntityID >= 0))
		{
			view->InOverlay = false;
			view->InDebugLines = false;
			view->InShadowCasters = false;
			view->DrawMode = SpriteDrawMode::Translucent;
			view->EntityID = -1;
			StartDraw(*view);
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
		StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::EndOverlay()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndOverlay outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InOverlay == false)
		{
			return;
		}

		s_Data->CurrentView->InOverlay = false;
		StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::BeginDebugLines()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::BeginDebugLines outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InDebugLines)
		{
			return;
		}

		// Lines carry no entity: they never write IDs.
		SetEntityID(-1);
		s_Data->CurrentView->InDebugLines = true;
		StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::EndDebugLines()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndDebugLines outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InDebugLines == false)
		{
			return;
		}

		s_Data->CurrentView->InDebugLines = false;
		StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::BeginShadowCasters()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::BeginShadowCasters outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InShadowCasters)
		{
			return;
		}

		// Casters carry no entity (the shadow map has no ID attachment).
		SetEntityID(-1);
		s_Data->CurrentView->InShadowCasters = true;
		StartDraw(*s_Data->CurrentView);
	}

	void Renderer2D::EndShadowCasters()
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::EndShadowCasters outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr || s_Data->CurrentView->InShadowCasters == false)
		{
			return;
		}

		s_Data->CurrentView->InShadowCasters = false;
		StartDraw(*s_Data->CurrentView);
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

	const std::vector<PassTiming>& Renderer2D::GetPassTimings()
	{
		return s_Data->LastPassTimings;
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

		if (HasSharedDepth(view->Features) == false)
		{
			mode = SpriteDrawMode::Translucent;
		}
		if (mode == view->DrawMode)
		{
			return;
		}

		view->DrawMode = mode;
		// Inside an overlay (or the shadow casters) that list stays current; its End starts the draw in the new mode's list.
		if (view->InOverlay == false && view->InShadowCasters == false)
		{
			StartDraw(*view);
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
		// Pickable and other quads are drawn with different pipelines (one writes IDs), so a change of kind
		// starts a new draw. Changing from one entity to the next doesn't.
		if (wasPickable != (view->EntityID >= 0))
		{
			StartDraw(*view);
		}
	}


	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitQuad(transform, nullptr, TextureRect{}, color);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		// Tiling is a rect larger than the texture; the sampler's wrap mode repeats it.
		SubmitQuad(transform, texture, TextureRect{ glm::vec2(0.0f), glm::vec2(tilingFactor) }, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& tintColor)
	{
		SubmitQuad(transform, texture, textureRect, tintColor);
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

	// World units one pixel of `view` spans, from its view-projection (orthographic cameras: the same
	// at every point). Used for lines, whose thickness is given in pixels.
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
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}

		// A line is a thin quad (so it batches and blends like a sprite, and needs no wide-line support
		// from the GPU). It is extended by its thickness at both ends, so lines meeting at a corner
		// leave no gap. Only x and y are used for the direction; z is the start's.
		const float width = std::max(thickness, 0.0f) * GetWorldUnitsPerPixel(*s_Data->CurrentView);
		const glm::vec2 delta = glm::vec2(end) - glm::vec2(start);
		const float length = glm::length(delta);
		const float angle = std::atan2(delta.y, delta.x);
		const glm::vec3 middle = 0.5f * (start + end);

		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), middle)
			* glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(length + width, width, 1.0f));

		SubmitQuad(transform, nullptr, TextureRect{}, color);
		s_Data->Stats.LineCount++;
	}

	void Renderer2D::DrawLine3D(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawLine3D outside BeginScene/EndScene");
		RenderViewData* view = s_Data->CurrentView;
		if (view == nullptr)
		{
			return;
		}

		const glm::vec3 direction = end - start;
		const float length = glm::length(direction);
		if (length <= 1.0e-6f)
		{
			return;
		}

		// The camera's view of the line's middle: the ray through it (from the inverse view-projection, so
		// perspective and orthographic alike) gives the side the ribbon spreads along, and the pixel size there.
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
			// Looking straight along the line: nothing to see.
			return;
		}
		side = glm::normalize(side);

		// World units per pixel at the middle: how far one unit along `side` moves on screen.
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
		s_Data->Stats.LineCount++;
		s_Data->Stats.DebugLineCount++;
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

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& tintColor,
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

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& tintColor)
	{
		// The unit quad's corners scaled, turned and moved, without a 4x4 matrix per quad.
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

	void Renderer2D::SubmitQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& color)
	{
		glm::vec3 corners[4];
		for (size_t i = 0; i < 4; i++)
		{
			corners[i] = glm::vec3(transform * s_Data->QuadVertexPositions[i]);
		}
		SubmitQuadCorners(corners, texture, textureRect, color);
	}

	void Renderer2D::SubmitQuadCorners(const glm::vec3 (&corners)[4], const Ref<Texture2D>& texture, const TextureRect& textureRect, const glm::vec4& color,
		const glm::vec3 (*shadowCorners)[4])
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawQuad outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}

		RenderViewData& view = *s_Data->CurrentView;

		constexpr size_t quadVertexCount = 4;
		// Vulkan's v = 0 is the first (top) row of the image, and vertex 0 is the quad's bottom-left
		// corner, so v runs Max.y -> Min.y from bottom to top and the image appears upright.
		const glm::vec2 textureCoords[] = {
			{ textureRect.Min.x, textureRect.Max.y },
			{ textureRect.Max.x, textureRect.Max.y },
			{ textureRect.Max.x, textureRect.Min.y },
			{ textureRect.Min.x, textureRect.Min.y } };

		// Batch full? Start a new one instead of overflowing (or flushing mid-frame).
		if (view.QuadIndexCounts[view.BatchIndex] >= Renderer2DData::MaxIndices)
		{
			NextBatch(view);
		}

		// May start a new draw call when this draw's texture slots are full.
		const float textureIndex = GetTextureSlot(view, texture);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			view.QuadVertexBufferPtr->Position = corners[i];
			view.QuadVertexBufferPtr->Color = color;
			view.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			view.QuadVertexBufferPtr->TexIndex = textureIndex;
			view.QuadVertexBufferPtr->EntityID = view.EntityID;
			view.QuadVertexBufferPtr->ShadowPosition = shadowCorners != nullptr ? (*shadowCorners)[i] : corners[i];
			view.QuadVertexBufferPtr++;
		}

		view.QuadIndexCounts[view.BatchIndex] += 6;
		GetCurrentDraws(view).back().IndexCount += 6;
		s_Data->Stats.QuadCount++;
		if (view.InShadowCasters)
		{
			s_Data->Stats.ShadowCasterQuads++;
		}
	}

	// ---- Frame ------------------------------------------------------------------

	// ---- Captures ---------------------------------------------------------------

	// Writes a read-back view image as a PNG: RGBA8 or BGRA8 (swizzled), alpha made opaque.
	static void SaveCapture(const std::filesystem::path& path, ImageReadbackResult& result)
	{
		const bool rgba = result.Format == VK_FORMAT_R8G8B8A8_SRGB || result.Format == VK_FORMAT_R8G8B8A8_UNORM;
		const bool bgra = result.Format == VK_FORMAT_B8G8R8A8_SRGB || result.Format == VK_FORMAT_B8G8R8A8_UNORM;
		if (rgba == false && bgra == false)
		{
			HY_CORE_ERROR("Capture {}: the view's format ({}) can't be saved as a PNG", path.string(), (int)result.Format);
			return;
		}

		const size_t pixelCount = (size_t)result.Width * result.Height;
		for (size_t i = 0; i < pixelCount; i++)
		{
			uint8_t* pixel = result.Pixels.data() + i * 4;
			if (bgra)
			{
				std::swap(pixel[0], pixel[2]);
			}
			pixel[3] = 255;
		}

		if (ImageWriter::WritePNG(path, result.Width, result.Height, result.Pixels.data()))
		{
			HY_CORE_INFO("Saved capture {} ({} x {})", path.string(), result.Width, result.Height);
		}
	}

	// Called after `frame`'s fence: the copies recorded the last time this frame index ran are done.
	static void ResolveCaptures(uint32_t frame)
	{
		// Every view, not only the ones begun this frame: a copy may belong to a view that is hidden now.
		for (RenderViewData* view : s_Data->Views)
		{
			if (view->Readback == nullptr || view->Readback->IsPending(frame) == false)
			{
				continue;
			}

			ImageReadbackResult result;
			if (view->Readback->TakeResult(frame, result))
			{
				SaveCapture(view->CapturePaths[frame], result);
			}
			view->CapturePaths[frame].clear();
		}
	}

	// Called after `frame`'s fence: a pick copy recorded the last time this frame index ran is done. It is the
	// answer only when it belongs to the newest request and nothing invalidated it since (a resize, a scene change).
	static void ResolvePicks(uint32_t frame)
	{
		for (RenderViewData* view : s_Data->Views)
		{
			if (view->PickReadback == nullptr || view->PickReadback->IsPending(frame) == false)
			{
				continue;
			}

			ImageReadbackResult result;
			if (view->PickReadback->TakeResult(frame, result) == false || result.Pixels.size() < sizeof(int32_t))
			{
				continue;
			}
			if (view->PickSlotGeneration[frame] != view->PickGeneration || view->PickSlotRequest[frame] != view->PickRequest)
			{
				continue; // stale: a newer click, or the view or scene changed
			}

			int32_t id = -1;
			memcpy(&id, result.Pixels.data(), sizeof(int32_t));
			view->PickResult = id;
			view->PickResultReady = true;
		}
	}

	// After the view's pass (the entity-ID image is in SHADER_READ_ONLY_OPTIMAL): copies the requested pixel.
	static void RecordPick(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		if (view.PickRequested == false)
		{
			return;
		}

		const VulkanImage* ids = view.Target->GetEntityIDImage(frame);
		if (ids == nullptr || view.PickX >= view.Target->GetWidth() || view.PickY >= view.Target->GetHeight())
		{
			// No IDs in this view, or the click is outside the (resized) image: nothing is selected.
			view.PickRequested = false;
			return;
		}

		if (view.PickReadback == nullptr)
		{
			view.PickReadback = CreateScope<VulkanImageReadback>();
		}
		// This frame index's last copy hasn't been read yet: try again next frame.
		if (view.PickReadback->IsPending(frame))
		{
			return;
		}

		if (view.PickReadback->Record(commandBuffer, *ids, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frame, view.PickX, view.PickY, 1, 1))
		{
			view.PickSlotRequest[frame] = view.PickRequest;
			view.PickSlotGeneration[frame] = view.PickGeneration;
		}
		view.PickRequested = false;
	}

	// Right after the view's pass: its image is in SHADER_READ_ONLY_OPTIMAL (the pass's final layout).
	static void RecordCapture(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		if (view.RequestedCapture.empty())
		{
			return;
		}

		if (view.Readback == nullptr)
		{
			view.Readback = CreateScope<VulkanImageReadback>();
		}

		// This frame index's last copy hasn't been written yet: try again next frame.
		if (view.Readback->IsPending(frame))
		{
			return;
		}

		if (view.Readback->Record(commandBuffer, view.Target->GetImage(frame), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frame))
		{
			view.CapturePaths[frame] = view.RequestedCapture;
		}
		else
		{
			HY_CORE_ERROR("Capture of view '{}' failed: {}", view.DebugName, view.RequestedCapture.string());
		}
		view.RequestedCapture.clear();
	}

	void Renderer2D::Render(VkCommandBuffer commandBuffer)
	{
		HY_CORE_ASSERT(s_Data->CurrentView == nullptr, "Renderer2D::Render inside BeginScene/EndScene");
		s_Data->CurrentView = nullptr;

		ApplySettings();

		const uint32_t frame = Renderer::GetRenderFrame();

		// Renderer::RenderFrame already waited on this frame-in-flight's fence, so the GPU is done
		// with every view's buffers and descriptor sets for `frame`: safe to overwrite.
		UpdateImGuiTextures(frame);
		// The same fence covers the captures and picks this frame index recorded last time: use them.
		ResolveCaptures(frame);
		ResolvePicks(frame);

		// And its pass timestamps: sum them by pass name, then reset the frame's queries.
		if (s_Data->PassTimestampPool != VK_NULL_HANDLE)
		{
			std::vector<std::string>& names = s_Data->TimedPassNames[frame];
			if (names.empty() == false)
			{
				std::vector<uint64_t> results(names.size() * 4, 0);
				const VkResult result = vkGetQueryPoolResults(Renderer::GetVulkanDevice().GetDevice(), s_Data->PassTimestampPool,
					frame * Renderer2DData::MaxTimedPasses * 2, (uint32_t)names.size() * 2, results.size() * sizeof(uint64_t), results.data(),
					2 * sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
				if (result == VK_SUCCESS || result == VK_NOT_READY)
				{
					const double toMs = (double)s_Data->TimestampPeriodNs / 1.0e6;
					std::vector<PassTiming> timings;
					for (size_t i = 0; i < names.size(); i++)
					{
						const uint64_t begin = results[i * 4], beginReady = results[i * 4 + 1];
						const uint64_t end = results[i * 4 + 2], endReady = results[i * 4 + 3];
						if (beginReady == 0 || endReady == 0 || end < begin)
						{
							continue;
						}
						const float ms = (float)((double)(end - begin) * toMs);
						auto found = std::find_if(timings.begin(), timings.end(), [&](const PassTiming& timing) { return timing.Name == names[i]; });
						if (found == timings.end())
						{
							timings.push_back({ names[i], ms });
						}
						else
						{
							found->GPUMs += ms;
						}
					}
					s_Data->LastPassTimings = std::move(timings);
				}
			}
			names.clear();
			vkCmdResetQueryPool(commandBuffer, s_Data->PassTimestampPool, frame * Renderer2DData::MaxTimedPasses * 2, Renderer2DData::MaxTimedPasses * 2);
		}

		// The fence for this frame index was just waited on, so the timestamps this command
		// buffer wrote last time are available: read them, then reset and write new ones.
		const uint32_t firstQuery = frame * Renderer2DData::QueriesPerFrame;
		s_Data->TimedViewCount = 0;
		s_Data->CurrentTimerSlot = UINT32_MAX;
		if (s_Data->TimestampPool)
		{
			if (s_Data->TimestampsWritten[frame])
			{
				// {value, availability} per query (64-bit). Queries that were not written stay unavailable
				// (VK_NOT_READY), which is fine: only the ones with availability set are used.
				uint64_t results[Renderer2DData::QueriesPerFrame * 2] = {};
				VkResult result = vkGetQueryPoolResults(Renderer::GetVulkanDevice().GetDevice(),
					s_Data->TimestampPool, firstQuery, Renderer2DData::QueriesPerFrame, sizeof(results), results,
					2 * sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
				if (result == VK_SUCCESS || result == VK_NOT_READY)
				{
					const double toMs = (double)s_Data->TimestampPeriodNs / 1.0e6;
					if (results[1] != 0 && results[3] != 0 && results[2] >= results[0])
					{
						s_Data->LastGPUTimeMs = (float)((double)(results[2] - results[0]) * toMs);
					}

					double phaseMs[Renderer2DData::TimerPhaseCount] = {};
					bool anyLit = false;
					for (uint32_t slot = 0; slot < Renderer2DData::MaxTimedViews; slot++)
					{
						for (uint32_t phase = 0; phase < Renderer2DData::TimerPhaseCount; phase++)
						{
							if ((s_Data->TimedPhaseMask[frame][slot] & (1u << phase)) == 0)
							{
								continue;
							}

							const uint32_t query = 2 + (slot * Renderer2DData::TimerPhaseCount + phase) * 2;
							const bool available = results[query * 2 + 1] != 0 && results[(query + 1) * 2 + 1] != 0;
							if (available && results[(query + 1) * 2] >= results[query * 2])
							{
								phaseMs[phase] += (double)(results[(query + 1) * 2] - results[query * 2]) * toMs;
								anyLit = anyLit || phase == 0;
							}
						}
					}

					s_Data->LastLightPassGPUMs = anyLit ? (float)phaseMs[0] : -1.0f;
					s_Data->LastShadowGPUMs = anyLit ? (float)phaseMs[1] : -1.0f;
					s_Data->LastCompositeGPUMs = anyLit ? (float)phaseMs[2] : -1.0f;
				}
			}

			s_Data->TimedPhaseMask[frame] = {};
			vkCmdResetQueryPool(commandBuffer, s_Data->TimestampPool, firstQuery, Renderer2DData::QueriesPerFrame);
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_Data->TimestampPool, firstQuery);
		}

		for (RenderViewData* view : s_Data->QueuedViews)
		{
			RecordView(commandBuffer, *view, frame);
			s_Data->Stats.ViewCount++;
		}

		if (s_Data->TimestampPool)
		{
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_Data->TimestampPool, firstQuery + 1);
			s_Data->TimestampsWritten[frame] = true;
		}

		// Frame-level info for the stats panel.
		GPUMemoryStats memory = VulkanAllocator::GetStats();
		s_Data->Stats.GPUMemoryUsed = memory.Used;
		s_Data->Stats.GPUMemoryFree = memory.Free;
		s_Data->Stats.MaxQuadsPerBatch = Renderer2DData::MaxQuads;
		s_Data->Stats.MaxTextureSlots = MaxTextureSlots;
		s_Data->Stats.FramesInFlight = MAX_FRAMES_IN_FLIGHT;
		s_Data->Stats.FrameIndex = frame;
		s_Data->Stats.GPUTimeMs = s_Data->LastGPUTimeMs;
		s_Data->Stats.LightPassGPUMs = s_Data->LastLightPassGPUMs;
		s_Data->Stats.ShadowGPUMs = s_Data->LastShadowGPUMs;
		s_Data->Stats.CompositeGPUMs = s_Data->LastCompositeGPUMs;
		s_Data->Stats.GPUName = s_Data->GPUName;
		if (s_Data->QueuedViews.empty() == false)
		{
			s_Data->Stats.ViewportWidth = s_Data->QueuedViews.front()->Target->GetWidth();
			s_Data->Stats.ViewportHeight = s_Data->QueuedViews.front()->Target->GetHeight();
		}

		s_Data->LastFrameStats = s_Data->Stats;
		s_Data->Stats = {};
		DiscardFrame();
	}

	void Renderer2D::RecordView(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		ApplyViewResize(view);

		// Pass-list rebuild when the view's render features changed (never per frame otherwise). Before the
		// uploads: what the graph holds decides whether the view draws a sun shadow map this frame.
		UpdateRenderGraph(view);
		PrepareSunShadows(view);

		UniformBufferObject ubo;
		ubo.mvp = view.ViewProjection;
		view.UniformBuffers[frame]->SetData(&ubo, sizeof(UniformBufferObject));
		UploadLighting(view, frame);

		// Point this frame's descriptor set of every texture set used at its textures. Descriptor
		// sets can't change once bound in a command buffer, so this happens before the pass.
		// A texture released while this frame is in flight is freed through the deletion queue
		// after it, so the GPU never samples a destroyed image.
		for (uint32_t s = 0; s <= view.TextureSetIndex; s++)
		{
			TextureSet& set = view.TextureSets[s];
			WriteTextureSet(set, frame);
			WriteShadowMapBinding(set, view, frame);
			s_Data->Stats.TextureSlotsUsed += set.SlotCount;
		}

		// Upload each batch used this frame. Each batch has its own vertex buffer, so uploading
		// batch i+1 never overwrites vertices that batch i's draws still have to read.
		for (uint32_t i = 0; i <= view.BatchIndex; i++)
		{
			const uint32_t indexCount = view.QuadIndexCounts[i];
			if (indexCount == 0)
			{
				continue;
			}

			const uint32_t vertexCount = (indexCount / 6) * 4;
			view.QuadVertexBuffers[i][frame]->SetData(view.QuadVertexBufferBases[i], vertexCount * sizeof(QuadVertex));

			s_Data->Stats.BatchCount++;
			s_Data->Stats.VertexBytesUploaded += (uint64_t)vertexCount * sizeof(QuadVertex);
		}

		RenderPassContext context{ commandBuffer, frame, view };

		BeginDebugLabel(commandBuffer, ("View " + view.DebugName).c_str());

		// BeforeScene passes draw into their own targets (the 2D light buffer) before the view's target is begun.
		ExecuteStage(RenderStage::BeforeScene, context);

		const RenderTargetBeginInfo clear = RenderTargetBeginInfo::Clear(Renderer::GetSettings().ClearColor);
		if (view.SceneColor != nullptr && view.Graph->HasPass(TonemapPassName))
		{
			// HDR (post-processing plan P1): the scene pass draws into the scene colour with the view's depth and
			// entity ID (cleared once, here); PostProcess runs between the passes; the output pass loads depth and
			// IDs again, the Tonemap pass fills the view's colour and the overlay and debug lines go on top.
			view.PassColorTarget = view.SceneColor.get();
			view.Target->Begin(commandBuffer, frame, clear, view.SceneColor.get());
			for (uint8_t stage = (uint8_t)RenderStage::Opaque; stage <= (uint8_t)RenderStage::AfterLighting; stage++)
			{
				ExecuteStage((RenderStage)stage, context);
			}
			view.Target->End(commandBuffer, frame, view.SceneColor.get());
			view.PassColorTarget = nullptr;

			ExecuteStage(RenderStage::PostProcess, context);

			RenderTargetBeginInfo output;
			output.ColorLoad = AttachmentLoadOp::DontCare; // the tonemap writes every pixel
			output.DepthLoad = AttachmentLoadOp::Load;
			output.EntityIDLoad = AttachmentLoadOp::Load;
			view.Target->Begin(commandBuffer, frame, output);
			for (uint8_t stage = (uint8_t)RenderStage::Resolve; stage < (uint8_t)RenderStage::Count; stage++)
			{
				ExecuteStage((RenderStage)stage, context);
			}
			view.Target->End(commandBuffer, frame);
		}
		else
		{
			// One rendering pass per view per frame, so its target is cleared exactly once. Every later stage
			// records into it, in stage order (PostProcess has nothing to run inside a pass). End() moves the
			// image to SHADER_READ_ONLY_OPTIMAL for ImGui, which samples it later in this command buffer.
			view.Target->Begin(commandBuffer, frame, clear);
			for (uint8_t stage = (uint8_t)RenderStage::Opaque; stage < (uint8_t)RenderStage::Count; stage++)
			{
				if ((RenderStage)stage != RenderStage::PostProcess)
				{
					ExecuteStage((RenderStage)stage, context);
				}
			}
			view.Target->End(commandBuffer, frame);
		}

		s_Data->Stats.BatchesAllocated += (uint32_t)view.QuadVertexBufferBases.size();

		// After the pass, so the capture has everything the view drew, overlay included.
		RecordCapture(commandBuffer, view, frame);
		RecordPick(commandBuffer, view, frame);
		EndDebugLabel(commandBuffer);
	}

	void Renderer2D::ExecuteStage(RenderStage stage, RenderPassContext& context)
	{
		for (const RenderPassDeclaration& pass : context.View.Graph->GetPasses())
		{
			if (pass.Stage != stage)
			{
				continue;
			}

			BeginDebugLabel(context.CommandBuffer, pass.Name.c_str());
			// A begin/end timestamp pair per pass, while the frame's slots last.
			uint32_t timer = UINT32_MAX;
			std::vector<std::string>& names = s_Data->TimedPassNames[context.Frame];
			if (s_Data->PassTimestampPool != VK_NULL_HANDLE && names.size() < Renderer2DData::MaxTimedPasses)
			{
				timer = (uint32_t)names.size();
				names.push_back(pass.Name);
				const uint32_t query = (context.Frame * Renderer2DData::MaxTimedPasses + timer) * 2;
				vkCmdWriteTimestamp(context.CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_Data->PassTimestampPool, query);
			}

			pass.Execute(context);

			if (timer != UINT32_MAX)
			{
				const uint32_t query = (context.Frame * Renderer2DData::MaxTimedPasses + timer) * 2 + 1;
				vkCmdWriteTimestamp(context.CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_Data->PassTimestampPool, query);
			}
			EndDebugLabel(context.CommandBuffer);
		}
	}

	void Renderer2D::UploadLighting(RenderViewData& view, uint32_t frame)
	{
		FrameLightingUniform uniform;
		const SceneLighting& lighting = view.Lighting;
		const bool forward = view.Features.Lighting == LightingMode::Forward && view.LightingSubmitted;
		uint32_t count = 0;

		if (forward)
		{
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

			// The nearest MaxForwardLights to the camera.
			std::vector<const ForwardLight*> lights;
			lights.reserve(lighting.Lights.size());
			for (const ForwardLight& light : lighting.Lights)
			{
				lights.push_back(&light);
			}
			if (lights.size() > MaxForwardLights)
			{
				const glm::vec3 camera = lighting.CameraPosition;
				std::partial_sort(lights.begin(), lights.begin() + MaxForwardLights, lights.end(), [camera](const ForwardLight* a, const ForwardLight* b)
				{
					return glm::length(a->Position - camera) < glm::length(b->Position - camera);
				});
				s_Data->Stats.ForwardLightsDropped += (uint32_t)(lights.size() - MaxForwardLights);
				if (s_Data->LightLimitWarnedForward == false)
				{
					HY_CORE_WARN("Renderer2D: view '{}' has {} point and spot lights; the {} nearest to the camera are used", view.DebugName, lights.size(), MaxForwardLights);
					s_Data->LightLimitWarnedForward = true;
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
			uniform.Counts = glm::ivec4((int32_t)count, 1, view.SunShadowsActive ? 1 : 0, 0);
			if (view.SunShadowsActive)
			{
				const float mapSize = (float)view.ShadowTarget->GetWidth();
				const float distance = std::max(lighting.ShadowDistance, 1.0f);
				// One texel of the map in metres, and the receivers' push out of their own shadow: about a
				// texel and a half (along the surface normal, or towards the sun for sprites).
				const float texelWorld = 2.0f * distance / mapSize;
				uniform.ShadowMatrix = view.ShadowMatrix;
				uniform.ShadowParams = glm::vec4(std::clamp(lighting.ShadowStrength, 0.0f, 1.0f), 1.0f / mapSize,
					Renderer::GetSettings().SoftSunShadows ? 1.0f : 0.0f, texelWorld * 1.5f * std::max(lighting.ShadowBias, 0.0f));
			}
			s_Data->Stats.ForwardLitViewCount++;
			s_Data->Stats.ForwardLightCount += count;
		}

		view.LightingBuffers[frame]->SetData(&uniform, sizeof(FrameLightingUniform));
	}

	// How far beyond the shadowed area (towards the sun) the shadow camera starts, so casters outside it (a
	// tall tree or a cliff up the hill) still throw their shadows into it. Metres.
	static constexpr float ShadowCasterReach = 60.0f;

	void Renderer2D::PrepareSunShadows(RenderViewData& view)
	{
		const SceneLighting& lighting = view.Lighting;
		view.SunShadowsActive = view.Graph != nullptr && view.Graph->HasPass(SunShadowPassName) && view.LightingSubmitted
			&& lighting.HasSun && lighting.SunShadows && glm::length(lighting.SunDirection) > 0.0f;
		if (view.SunShadowsActive == false)
		{
			return;
		}

		// The map: made the first time, remade (old images through the deletion queue) when its size setting changes.
		const uint32_t size = std::clamp(Renderer::GetSettings().SunShadowMapSize, 512u, 4096u);
		if (view.ShadowTarget == nullptr)
		{
			RenderTargetSpecification specification;
			specification.Width = size;
			specification.Height = size;
			specification.ColorFormats = {};
			specification.DepthFormat = ShadowMapFormat;
			specification.SampledDepth = true;
			specification.DebugName = view.DebugName + " sun shadow map";
			view.ShadowTarget = CreateScope<VulkanRenderTarget>(specification);
		}
		else
		{
			view.ShadowTarget->Resize(size, size);
		}

		// An orthographic camera looking along the sunlight at what the view's camera looks at (FogOrigin),
		// covering ShadowDistance metres each way, starting ShadowCasterReach metres further towards the sun.
		const float distance = std::max(lighting.ShadowDistance, 1.0f);
		const glm::vec3 travel = glm::normalize(lighting.SunDirection); // where the light goes
		const glm::vec3 up = std::abs(travel.z) > 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
		const glm::vec3 centre = lighting.FogOrigin;
		const float back = distance + ShadowCasterReach;
		const glm::mat4 lightView = glm::lookAt(centre - travel * back, centre, up);
		glm::mat4 projection = Camera::Orthographic(-distance, distance, -distance, distance, 0.0f, back + distance);

		// Texel snapping: move the projection by less than a texel so the world's origin lands on a texel
		// corner. The map then slides over the world in whole texels as the camera moves, and shadow edges
		// stay put instead of crawling (most visible on pixel art).
		const glm::mat4 matrix = projection * lightView;
		const float halfSize = (float)size * 0.5f;
		const glm::vec4 origin = matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const glm::vec2 originTexels = glm::vec2(origin) * halfSize;
		const glm::vec2 offset = (glm::round(originTexels) - originTexels) / halfSize;
		projection[3][0] += offset.x;
		projection[3][1] += offset.y;
		view.ShadowMatrix = projection * lightView;
	}

	void Renderer2D::RecordSunShadowPass(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		if (view.SunShadowsActive == false)
		{
			return;
		}

		// Cleared to the far plane: nothing casts. End leaves the map ready to be sampled by the scene pass.
		view.ShadowTarget->Begin(context.CommandBuffer, context.Frame, RenderTargetBeginInfo::Clear(glm::vec4(0.0f)));
		Renderer3D::RecordShadowCasters(context);

		const bool hasCasters = std::any_of(view.ShadowCasterDraws.begin(), view.ShadowCasterDraws.end(), [](const QuadDraw& draw) { return draw.IndexCount != 0; });
		VulkanPipeline* pipeline = hasCasters ? GetViewPipeline(ViewPipelineKind::ShadowCaster, *view.ShadowTarget, false) : nullptr;
		if (pipeline != nullptr)
		{
			pipeline->Bind(context.CommandBuffer, view.ShadowTarget->GetExtent());
			s_Data->QuadIndexBuffer->BindAsIndexBuffer(context.CommandBuffer);
			uint32_t boundBatch = UINT32_MAX;
			uint32_t boundTextureSet = UINT32_MAX;
			for (const QuadDraw& draw : view.ShadowCasterDraws)
			{
				if (draw.IndexCount == 0)
				{
					continue;
				}
				if (draw.Batch != boundBatch)
				{
					view.QuadVertexBuffers[draw.Batch][context.Frame]->BindAsVertexBuffer(context.CommandBuffer);
					boundBatch = draw.Batch;
				}
				if (draw.TextureSet != boundTextureSet)
				{
					pipeline->BindDescriptorSet(context.CommandBuffer, 0, *view.TextureSets[draw.TextureSet].DescriptorSet);
					boundTextureSet = draw.TextureSet;
				}
				vkCmdDrawIndexed(context.CommandBuffer, draw.IndexCount, 1, draw.FirstIndex, 0, 0);
				s_Data->Stats.DrawCalls++;
			}
		}

		view.ShadowTarget->End(context.CommandBuffer, context.Frame);
		s_Data->Stats.SunShadowViewCount++;
	}

	void Renderer2D::RecordTonemap(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		if (view.SceneColor == nullptr)
		{
			return;
		}
		VulkanPipeline* pipeline = GetViewPipeline(ViewPipelineKind::Tonemap, *view.Target, false);
		if (pipeline == nullptr)
		{
			return;
		}

		// This frame's scene colour (left in SHADER_READ_ONLY_OPTIMAL by the scene pass's End), read texel for texel.
		const VulkanImageInfo& image = view.SceneColor->GetImage(context.Frame).GetVulkanImageInfo();
		view.TonemapSet = VulkanDescriptorAllocator::AllocateFrame(s_Data->TonemapLayout);
		if (view.TonemapSet == VK_NULL_HANDLE)
		{
			return;
		}
		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = image.Sampler;
		imageInfo.imageView = image.ImageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = view.TonemapSet;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 1, &write, 0, nullptr);

		TonemapPushConstants constants;
		constants.ExposureScale = std::exp2(std::clamp(view.PostProcess.Exposure, -16.0f, 16.0f));

		pipeline->Bind(context.CommandBuffer, view.Target->GetExtent());
		pipeline->BindDescriptorSet(context.CommandBuffer, 0, view.TonemapSet);
		pipeline->PushConstants(context.CommandBuffer, &constants, (uint32_t)sizeof(constants));
		vkCmdDraw(context.CommandBuffer, 3, 1, 0, 0);
		s_Data->Stats.DrawCalls++;
		s_Data->Stats.HDRViewCount++;
	}

	void Renderer2D::RecordQuadDraws(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame, const std::vector<QuadDraw>& draws, SpriteDrawMode mode, bool overlay, bool debugLines)
	{
		const bool hasDraws = std::any_of(draws.begin(), draws.end(), [](const QuadDraw& draw) { return draw.IndexCount != 0; });
		if (hasDraws == false)
		{
			return;
		}

		// Built for the view's attachments. With the entity-ID attachment, pickable draws (an entity's sprites)
		// write their IDs and the rest (particles, lines, the overlay) use a variant that leaves the IDs alone.
		ViewPipelineKind kind = ViewPipelineKind::Sprite;
		if (debugLines)
		{
			kind = ViewPipelineKind::DebugLines;
		}
		else if (overlay)
		{
			kind = ViewPipelineKind::Overlay;
		}
		else if (mode == SpriteDrawMode::Cutout)
		{
			kind = ViewPipelineKind::Cutout;
		}
		else if (mode == SpriteDrawMode::Decal)
		{
			kind = ViewPipelineKind::Decal;
		}
		const bool hasEntityID = view.Target->GetSpecification().EntityID;
		// Overlay quads can be pickable too: HUD sprites carry their entity (outlines and lines don't).
		// Forward lit views shade sprites in their shaders (the overlay and debug lines stay unlit).
		const bool lit = view.Features.Lighting == LightingMode::Forward && view.LightingSubmitted && overlay == false && debugLines == false;
		VulkanPipeline* pickablePipeline = GetViewPipeline(kind, *view.Target, debugLines == false, lit, view.PassColorTarget);
		VulkanPipeline* otherPipeline = hasEntityID ? GetViewPipeline(kind, *view.Target, false, lit, view.PassColorTarget) : pickablePipeline;

		// Bound by every pass that draws quads: the pass before may have bound another pipeline (the composite).
		// Viewport/scissor follow the view's size.
		s_Data->QuadIndexBuffer->BindAsIndexBuffer(commandBuffer);
		VulkanPipeline* pipeline = nullptr;

		// One draw call per draw, in submission order. Vertex buffer and descriptor set are only
		// rebound when they change. Indices are 0,1,2,2,3,0 + 4 * quad, so FirstIndex = 6 * quad
		// with vertex offset 0 reads that quad's vertices.
		uint32_t boundBatch = UINT32_MAX;
		uint32_t boundTextureSet = UINT32_MAX;
		for (const QuadDraw& draw : draws)
		{
			VulkanPipeline* drawPipeline = draw.Pickable ? pickablePipeline : otherPipeline;
			if (draw.IndexCount == 0 || drawPipeline == nullptr)
			{
				continue;
			}

			if (drawPipeline != pipeline)
			{
				pipeline = drawPipeline;
				pipeline->Bind(commandBuffer, view.Target->GetExtent());
				// Both variants have the same layout; rebinding keeps it simple and cheap.
				boundTextureSet = UINT32_MAX;
			}

			if (draw.Batch != boundBatch)
			{
				view.QuadVertexBuffers[draw.Batch][frame]->BindAsVertexBuffer(commandBuffer);
				boundBatch = draw.Batch;
			}

			if (draw.TextureSet != boundTextureSet)
			{
				pipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[draw.TextureSet].DescriptorSet);
				boundTextureSet = draw.TextureSet;
			}

			vkCmdDrawIndexed(commandBuffer, draw.IndexCount, 1, draw.FirstIndex, 0, 0);
			s_Data->Stats.DrawCalls++;
		}
	}


	// ---- Render features --------------------------------------------------------

	// A render feature made of functions, so the 2D features below can be written inside Renderer2D
	// (they record with its private functions) without a class each.
	class FunctionRenderFeature : public RenderFeature
	{
	public:
		using ValidateFunction = std::function<void(const RenderFeature&, const RenderGraphContext&, std::vector<RenderValidationMessage>&)>;
		using SetupFunction = std::function<void(RenderGraphBuilder&)>;

		FunctionRenderFeature(const char* name, ValidateFunction validate, SetupFunction setup)
			: m_Name(name), m_Validate(std::move(validate)), m_Setup(std::move(setup))
		{
		}

		const char* GetName() const override { return m_Name; }

		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override
		{
			if (m_Validate)
			{
				m_Validate(*this, context, out);
			}
		}

		void Setup(RenderGraphBuilder& builder) override
		{
			if (m_Setup)
			{
				m_Setup(builder);
			}
		}

	private:
		const char* m_Name;
		ValidateFunction m_Validate;
		SetupFunction m_Setup;
	};

	static void AddValidationMessage(std::vector<RenderValidationMessage>& out, const RenderFeature& feature, RenderValidationSeverity severity, const std::string& text)
	{
		out.push_back({ severity, feature.GetName(), text });
	}

	std::vector<Scope<RenderFeature>> Renderer2D::CreateRenderFeatures(const RenderFeatureSettings& settings, bool editorView)
	{
		std::vector<Scope<RenderFeature>> features;

		// The entity-ID attachment (M1d), on the Editor window's view only: R32_SINT, cleared to -1 when the
		// view's pass begins. An entity's sprites write their ID wherever they draw a pixel (cut-out and
		// translucent alike; pixels with alpha 0 write nothing); particles, lines, the overlay, the light
		// composite and blob shadows never do. Read back for click-to-select in M7.
		if (editorView)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("EntityID",
				[](const RenderFeature& feature, const RenderGraphContext& context, std::vector<RenderValidationMessage>& out)
				{
					if (context.Capabilities.IndependentBlend == false)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Error, "the GPU has no independentBlend; the Editor view has no entity IDs");
					}
				},
				[](RenderGraphBuilder& builder)
				{
					builder.RequireEntityIDAttachment();
					builder.ImportResource("EntityID");
				}));
		}

		// The 2D light buffer: lights and blob shadows into the light buffer before the scene, multiplied
		// over it after the sprites. Its state between the two passes (drawn this frame or not) is per view.
		if (settings.Lighting == LightingMode::LightBuffer2D)
		{
			Ref<bool> drawnThisFrame = CreateRef<bool>(false);
			features.push_back(CreateScope<FunctionRenderFeature>("LightBuffer2D",
				[](const RenderFeature& feature, const RenderGraphContext&, std::vector<RenderValidationMessage>& out)
				{
					if (s_Data->LightPipeline == nullptr || s_Data->CompositePipeline == nullptr)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Warning, "the light shaders are unavailable; views are drawn unlit");
					}
				},
				[drawnThisFrame](RenderGraphBuilder& builder)
				{
					builder.ImportResource("SceneColor");
					builder.AddPass({ "LightBuffer", RenderStage::BeforeScene, {}, { "LightBuffer" },
						[drawnThisFrame](RenderPassContext& context)
						{
							// Skips itself on frame data: lighting switched off live, no lights, no shaders.
							*drawnThisFrame = RecordLightPass(context.CommandBuffer, context.View, context.Frame);
						} });
					builder.AddPass({ "LightComposite", RenderStage::AfterLighting, { "LightBuffer" }, { "SceneColor" },
						[drawnThisFrame](RenderPassContext& context)
						{
							if (*drawnThisFrame)
							{
								RecordComposite(context.CommandBuffer, context.View, context.Frame);
							}
						} });
				}));
		}

		// Shared depth (S1): one DEPTH32F attachment in the view's target, cleared once when the view's pass
		// begins and kept through every later stage (all in the one rendering pass), so cut-out sprites,
		// decals, translucent quads and later meshes test against each other.
		const bool sharedDepth = HasSharedDepth(settings);
		if (settings.Depth == DepthMode::Shared)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("Depth",
				[](const RenderFeature& feature, const RenderGraphContext& context, std::vector<RenderValidationMessage>& out)
				{
					if (context.Capabilities.DepthFormat32F == false)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Error, "the GPU can't use DEPTH32F as a depth attachment");
					}
				},
				[](RenderGraphBuilder& builder)
				{
					builder.RequireDepthAttachment(SharedDepthFormat);
					builder.ImportResource("Depth");
				}));
		}

		// Static meshes (M4, Renderer3D): opaque and cut-out, depth test and write, at Opaque before the sprites'
		// passes, so decals and sprites test against them. Needs shared depth.
		if (settings.Meshes)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("Meshes",
				[](const RenderFeature& feature, const RenderGraphContext& context, std::vector<RenderValidationMessage>& out)
				{
					if (context.Settings.Depth != DepthMode::Shared)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Error, "meshes need Depth = Shared");
					}
				},
				[editorView](RenderGraphBuilder& builder)
				{
					builder.ImportResource("SceneColor");
					builder.ImportResource("Depth");
					builder.ImportResource("MeshTextures");
					std::vector<std::string> writes = { "SceneColor", "Depth" };
					if (editorView)
					{
						writes.push_back("EntityID");
					}
					builder.AddPass({ "Meshes", RenderStage::Opaque, { "MeshTextures" }, writes,
						[](RenderPassContext& context)
						{
							Renderer3D::RecordMeshes(context);
						} });
				}));
		}

		// Sprites, particles and lines in submission order (SceneRenderer sorts them: the SortOrder policy).
		// Without shared depth every quad is translucent, so this is the view's translucent list. With it,
		// decals and then cut-out sprites are drawn at Opaque first (SpriteDrawMode), the rest stays here.
		features.push_back(CreateScope<FunctionRenderFeature>("Sprites",
			[](const RenderFeature& feature, const RenderGraphContext& context, std::vector<RenderValidationMessage>& out)
			{
				if (context.Settings.DefaultSpriteAlpha == SpriteAlphaDefault::Cutout && context.Settings.Depth != DepthMode::Shared)
				{
					AddValidationMessage(out, feature, RenderValidationSeverity::Warning, "cut-out sprites need Depth = Shared; sprites are drawn translucent");
				}
			},
			[sharedDepth, editorView](RenderGraphBuilder& builder)
			{
				builder.ImportResource("SceneColor");
				builder.ImportResource("SpriteTextures");
				// Sprites write the entity ID in the Editor view.
				std::vector<std::string> writes = { "SceneColor" };
				if (editorView)
				{
					writes.push_back("EntityID");
				}
				std::vector<std::string> cutoutWrites = writes;
				cutoutWrites.push_back("Depth");
				if (sharedDepth)
				{
					builder.ImportResource("Depth");
					builder.AddPass({ "Decals", RenderStage::Opaque, { "SpriteTextures", "Depth" }, writes,
						[](RenderPassContext& context)
						{
							RecordQuadDraws(context.CommandBuffer, context.View, context.Frame, context.View.DecalDraws, SpriteDrawMode::Decal, false);
						} });
					builder.AddPass({ "CutoutSprites", RenderStage::Opaque, { "SpriteTextures", "Depth" }, cutoutWrites,
						[](RenderPassContext& context)
						{
							RecordQuadDraws(context.CommandBuffer, context.View, context.Frame, context.View.CutoutDraws, SpriteDrawMode::Cutout, false);
						} });
				}
				std::vector<std::string> reads = { "SpriteTextures" };
				if (sharedDepth)
				{
					reads.push_back("Depth");
				}
				builder.AddPass({ "Sprites", RenderStage::Translucent, reads, writes,
					[](RenderPassContext& context)
					{
						RecordQuadDraws(context.CommandBuffer, context.View, context.Frame, context.View.Draws, SpriteDrawMode::Translucent, false);
					} });
			}));

		// Quads queued between BeginOverlay and EndOverlay: on top, after the lighting, never depth tested.
		features.push_back(CreateScope<FunctionRenderFeature>("Overlay", nullptr,
			[](RenderGraphBuilder& builder)
			{
				builder.ImportResource("SceneColor");
				builder.ImportResource("SpriteTextures");
				builder.AddPass({ "Overlay", RenderStage::Overlay, { "SpriteTextures" }, { "SceneColor" },
					[](RenderPassContext& context)
					{
						RecordQuadDraws(context.CommandBuffer, context.View, context.Frame, context.View.OverlayDraws, SpriteDrawMode::Translucent, true);
					} });
			}));

		// Forward lighting (M6): no passes of its own. The view's set 0 carries its lights every frame
		// (UploadLighting), and the sprite and mesh passes use their lit shaders.
		if (settings.Lighting == LightingMode::Forward)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("ForwardLighting",
				[](const RenderFeature& feature, const RenderGraphContext& context, std::vector<RenderValidationMessage>& out)
				{
					if (context.Settings.Depth != DepthMode::Shared)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Warning, "forward lighting without shared depth: sprites are lit, but sort in painter's order");
					}
				}, nullptr));
		}

		// Sun shadows (R14): one shadow map for the scene's main directional light, drawn before the scene from
		// the meshes that cast shadows and the queued sprite casters, then read by the lit sprite and mesh
		// shaders (set 0 binding 4). A pass that reads its own output later in the same frame, so v1's
		// hand-written transition (VulkanRenderTarget::End with SampledDepth) does the synchronisation, as for
		// the 2D light buffer. Skips itself on frame data (no shadow-casting sun).
		if (settings.Shadows == ShadowMode::Sun)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("SunShadows",
				[](const RenderFeature& feature, const RenderGraphContext& context, std::vector<RenderValidationMessage>& out)
				{
					if (context.Settings.Lighting != LightingMode::Forward)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Error, "sun shadows need Lighting = Forward");
					}
					if (context.Settings.Depth != DepthMode::Shared)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Error, "sun shadows need Depth = Shared");
					}
					if (context.Capabilities.DepthFormat32F == false)
					{
						AddValidationMessage(out, feature, RenderValidationSeverity::Error, "the GPU can't use DEPTH32F as a depth attachment");
					}
				},
				[](RenderGraphBuilder& builder)
				{
					builder.ImportResource("MeshTextures");
					builder.ImportResource("SpriteTextures");
					builder.AddPass({ SunShadowPassName, RenderStage::BeforeScene, { "MeshTextures", "SpriteTextures" }, { "SunShadowMap" },
						[](RenderPassContext& context)
						{
							RecordSunShadowPass(context);
						} });
				}));
		}

		// HDR (post-processing plan P1): the scene renders into an RGBA16F scene colour; the Tonemap pass (first in
		// the output pass) writes the view's sRGB colour from it through exposure and PBR Neutral; the overlay and
		// debug lines are drawn after it, untouched by exposure and the tonemap.
		if (settings.HDR)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("HDR", nullptr,
				[](RenderGraphBuilder& builder)
				{
					builder.RequireSceneColor(SceneColorFormat);
					builder.ImportResource("HDRColor");
					builder.ImportResource("SceneColor");
					builder.AddPass({ TonemapPassName, RenderStage::Resolve, { "HDRColor" }, { "SceneColor" },
						[](RenderPassContext& context)
						{
							RecordTonemap(context);
						} });
				}));
		}

		// Editor debug lines (M7): selection bounds, light reach, spot cones, the sun's direction, depth tested.
		if (editorView)
		{
			features.push_back(CreateScope<FunctionRenderFeature>("DebugLines", nullptr,
				[sharedDepth](RenderGraphBuilder& builder)
				{
					builder.ImportResource("SceneColor");
					std::vector<std::string> reads = {};
					if (sharedDepth)
					{
						builder.ImportResource("Depth");
						reads.push_back("Depth");
					}
					builder.AddPass({ "DebugLines", RenderStage::Debug, reads, { "SceneColor" },
						[](RenderPassContext& context)
						{
							RecordQuadDraws(context.CommandBuffer, context.View, context.Frame, context.View.DebugDraws, SpriteDrawMode::Translucent, false, true);
						} });
				}));
		}

		return features;
	}

	std::vector<RenderValidationMessage> Renderer2D::ValidateRenderFeatures(const RenderFeatureSettings& settings, bool editorView)
	{
		RenderGraphContext context;
		context.Settings = settings;
		context.Capabilities = Renderer::GetCapabilities();
		context.EditorView = editorView;
		return RenderGraph::Validate(CreateRenderFeatures(settings, editorView), context);
	}

	void Renderer2D::UpdateRenderGraph(RenderViewData& view)
	{
		const bool upToDate = view.Graph != nullptr && view.Graph->GetContext().Settings == view.Features
			&& view.Graph->GetContext().EditorView == view.EditorView;
		if (upToDate)
		{
			return;
		}

		// A pass-list rebuild: CPU only. The old graph holds no GPU objects, so it can go at once.
		RenderGraphContext context;
		context.Settings = view.Features;
		context.Capabilities = Renderer::GetCapabilities();
		context.EditorView = view.EditorView;
		view.Graph = CreateScope<RenderGraph>(CreateRenderFeatures(view.Features, view.EditorView), context, view.DebugName);

		// HDR: the scene colour, made or dropped with the feature (old images through the deletion queue).
		const ImageFormat sceneColorFormat = view.Graph->GetSceneColorFormat();
		if (sceneColorFormat == ImageFormat::None)
		{
			view.SceneColor.reset();
		}
		else if (view.SceneColor == nullptr || view.SceneColor->GetSpecification().ColorFormats[0] != sceneColorFormat)
		{
			RenderTargetSpecification specification;
			specification.Width = view.Target->GetWidth();
			specification.Height = view.Target->GetHeight();
			specification.ColorFormats = { sceneColorFormat };
			specification.DebugName = view.DebugName + " scene colour";
			view.SceneColor = CreateScope<VulkanRenderTarget>(specification);
		}

		// A target recreation when the features need other attachments (depth, entity ID). The old images go
		// through the deletion queue.
		const RenderTargetSpecification& current = view.Target->GetSpecification();
		if (view.Graph->GetDepthFormat() != current.DepthFormat || view.Graph->NeedsEntityID() != current.EntityID)
		{
			RenderTargetSpecification specification = current;
			specification.DepthFormat = view.Graph->GetDepthFormat();
			specification.EntityID = view.Graph->NeedsEntityID();
			view.Target = CreateScope<VulkanRenderTarget>(specification);
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				view.ImGuiTexturesOutdated[i] = true;
			}
			UpdateImGuiTextures(view, Renderer::GetRenderFrame());
		}
	}

	void Renderer2D::RegisterView(RenderViewData& view)
	{
		HY_CORE_ASSERT(s_Data != nullptr, "Create RenderViews after Renderer::Init");

		// First batch and texture set. More are created when a frame needs them.
		AllocateBatch(view);
		AllocateTextureSet(view);
		ResetBatches(view);

		s_Data->Views.push_back(&view);
	}

	void Renderer2D::UnregisterView(RenderViewData& view)
	{
		if (s_Data == nullptr)
		{
			return;
		}

		auto& views = s_Data->Views;
		views.erase(std::remove(views.begin(), views.end(), &view), views.end());

		auto& queued = s_Data->QueuedViews;
		queued.erase(std::remove(queued.begin(), queued.end(), &view), queued.end());
		if (s_Data->CurrentView == &view)
		{
			s_Data->CurrentView = nullptr;
		}
	}

	bool Renderer2D::RecordLightPass(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		const RendererSettings& settings = Renderer::GetSettings();
		if (settings.Lighting == false || view.HasLights == false || s_Data->LightPipeline == nullptr || s_Data->CompositePipeline == nullptr)
		{
			return false;
		}

		const uint32_t width = view.Target->GetWidth();
		const uint32_t height = view.Target->GetHeight();

		// The light buffer is made the first time the view is lit, then kept and resized with the view.
		if (view.LightTarget == nullptr)
		{
			RenderTargetSpecification lightSpecification;
			lightSpecification.Width = width;
			lightSpecification.Height = height;
			lightSpecification.ColorFormats = { LightBufferFormat };
			lightSpecification.DebugName = view.DebugName + " light buffer";
			view.LightTarget = CreateScope<VulkanRenderTarget>(lightSpecification);
			BufferSpecification vertexSpecification;
			vertexSpecification.Usage = BufferUsage::Vertex;
			vertexSpecification.Memory = BufferMemory::HostVisible;
			vertexSpecification.Size = (MaxLightsPerView + MaxShadowsPerView) * 4 * sizeof(QuadVertex);
			vertexSpecification.DebugName = view.DebugName + " lights";
			view.LightVertexBuffers = CreatePerFrameBuffers(vertexSpecification);
		}
		else
		{
			view.LightTarget->Resize(width, height);
		}

		// One quad per light, covering its radius. The corners' texture coordinates run -1..1, which light.frag
		// turns into the distance to the light; the "texture index" carries the number of bands (0 = smooth).
		const float bands = settings.LightFalloff == LightFalloffStyle::Banded ? (float)std::min(std::max(settings.LightBands, 2u), 32u) : 0.0f;
		static const glm::vec2 corners[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };

		view.LightVertices.clear();
		for (const LightInstance& light : view.Lights)
		{
			for (const glm::vec2& corner : corners)
			{
				QuadVertex vertex;
				vertex.Position = glm::vec3(light.Position.x + corner.x * light.Radius, light.Position.y + corner.y * light.Radius, light.Position.z);
				vertex.Color = glm::vec4(light.Color, 1.0f);
				vertex.TexCoord = corner;
				vertex.TexIndex = bands;
				view.LightVertices.push_back(vertex);
			}
		}

		// The shadows' quads follow the lights' in the same buffer. TexCoord is -1..1 like a light's;
		// the colour's alpha is the strength.
		const bool drawShadows = settings.Shadows && s_Data->ShadowPipeline != nullptr;
		if (drawShadows)
		{
			for (const ShadowInstance& shadow : view.Shadows)
			{
				for (const glm::vec2& corner : corners)
				{
					QuadVertex vertex;
					vertex.Position = glm::vec3(shadow.Position.x + corner.x * shadow.Size.x * 0.5f, shadow.Position.y + corner.y * shadow.Size.y * 0.5f, shadow.Position.z);
					vertex.Color = glm::vec4(0.0f, 0.0f, 0.0f, shadow.Strength);
					vertex.TexCoord = corner;
					vertex.TexIndex = 0.0f;
					view.LightVertices.push_back(vertex);
				}
			}
		}

		const uint32_t lightCount = (uint32_t)view.Lights.size();
		const uint32_t shadowCount = drawShadows ? (uint32_t)view.Shadows.size() : 0;
		if (lightCount + shadowCount > 0)
		{
			view.LightVertexBuffers[frame]->SetData(view.LightVertices.data(), view.LightVertices.size() * sizeof(QuadVertex));
		}

		// The composite samples this frame's light image, through a per-frame descriptor set (plan M2b): made
		// now, gone when this frame slot is reused, so a resized light buffer needs no rewrite. Every element
		// of the shader's array points at it.
		const VulkanImageInfo& lightImage = view.LightTarget->GetImage(frame).GetVulkanImageInfo();
		std::array<VkDescriptorImageInfo, VulkanDescriptorBuilder::MaxImageArraySize> imageInfos{};
		for (VkDescriptorImageInfo& imageInfo : imageInfos)
		{
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = lightImage.ImageView;
			imageInfo.sampler = lightImage.Sampler;
		}
		view.LightBufferSet = VulkanDescriptorAllocator::AllocateFrame(s_Data->CompositeLayoutDescriptorSet->GetDescriptorSetLayout());
		if (view.LightBufferSet == VK_NULL_HANDLE)
		{
			return false;
		}
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = view.LightBufferSet;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = (uint32_t)imageInfos.size();
		write.pImageInfo = imageInfos.data();
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 1, &write, 0, nullptr);

		// Cleared to the ambient light; every point light is added on top. End() makes the result readable by
		// the scene pass, whose composite samples it next.
		s_Data->CurrentTimerSlot = s_Data->TimedViewCount++;
		WriteTimer(commandBuffer, frame, TimerPhase::LightBuffer, false);
		view.LightTarget->Begin(commandBuffer, frame, RenderTargetBeginInfo::Clear(glm::vec4(view.AmbientLight, 1.0f)));
		if (lightCount + shadowCount > 0)
		{
			s_Data->QuadIndexBuffer->BindAsIndexBuffer(commandBuffer);
			view.LightVertexBuffers[frame]->BindAsVertexBuffer(commandBuffer);
		}

		if (lightCount > 0)
		{
			s_Data->LightPipeline->Bind(commandBuffer, { width, height });
			// The camera set of the view's first texture set (the light shader reads only the camera).
			s_Data->LightPipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[0].DescriptorSet);
			vkCmdDrawIndexed(commandBuffer, lightCount * 6, 1, 0, 0, 0);
			s_Data->Stats.DrawCalls++;
		}

		// Shadows last, so they darken the lights that reach them. Their vertices start after the lights'.
		if (shadowCount > 0)
		{
			WriteTimer(commandBuffer, frame, TimerPhase::Shadows, false);
			s_Data->ShadowPipeline->Bind(commandBuffer, { width, height });
			s_Data->ShadowPipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[0].DescriptorSet);
			vkCmdDrawIndexed(commandBuffer, shadowCount * 6, 1, 0, (int32_t)(lightCount * 4), 0);
			s_Data->Stats.DrawCalls++;
			WriteTimer(commandBuffer, frame, TimerPhase::Shadows, true);
		}
		view.LightTarget->End(commandBuffer, frame);
		WriteTimer(commandBuffer, frame, TimerPhase::LightBuffer, true);

		s_Data->Stats.LitViewCount++;
		s_Data->Stats.LightCount += lightCount;
		s_Data->Stats.ShadowCount += shadowCount;

		return true;
	}

	void Renderer2D::RecordComposite(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		// Built for the view's attachments (depth, entity ID); it never writes entity IDs.
		VulkanPipeline* pipeline = GetViewPipeline(ViewPipelineKind::Composite, *view.Target, false, false, view.PassColorTarget);
		if (pipeline == nullptr)
		{
			return;
		}

		WriteTimer(commandBuffer, frame, TimerPhase::Composite, false);
		pipeline->Bind(commandBuffer, view.Target->GetExtent());
		pipeline->BindDescriptorSet(commandBuffer, 0, view.LightBufferSet);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		s_Data->Stats.DrawCalls++;
		WriteTimer(commandBuffer, frame, TimerPhase::Composite, true);
	}

	void Renderer2D::DiscardFrame()
	{
		for (RenderViewData* view : s_Data->QueuedViews)
		{
			view->Queued = false;
			ResetBatches(*view);
		}

		s_Data->QueuedViews.clear();
		s_Data->CurrentView = nullptr;
		s_Data->Stats = {};
	}

	Statistics Renderer2D::GetStats()
	{
		return s_Data->LastFrameStats;
	}

	uint32_t Renderer2D::GetMaxTextureSlots()
	{
		return MaxTextureSlots;
	}

	static void WriteImGuiTexture(VkDescriptorSet set, VkImageView view, VkSampler sampler);

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

		// A handle no frame in flight can still be drawing with: rewrite it for this texture.
		const VulkanImageInfo& image = GetTexture2DData(*texture).Image->GetVulkanImageInfo();
		for (ImGuiTextureEntry& entry : s_Data->ImGuiTextures)
		{
			if (entry.LastUsedFrame + MAX_FRAMES_IN_FLIGHT + 1 < frame)
			{
				WriteImGuiTexture(entry.Set, image.ImageView, image.Sampler);
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

	static void WriteImGuiTexture(VkDescriptorSet set, VkImageView view, VkSampler sampler)
	{
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();

		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = sampler;
		imageInfo.imageView = view;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Binding 0 / combined image sampler is the layout ImGui_ImplVulkan_AddTexture uses.
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	void Renderer2D::ApplyViewResize(RenderViewData& view)
	{
		const uint32_t width = view.RequestedWidth;
		const uint32_t height = view.RequestedHeight;

		if (width == view.Target->GetWidth() && height == view.Target->GetHeight())
		{
			return;
		}

		// The old images and framebuffers go through the deletion queue: the other frame in flight
		// may still be drawing into or sampling them, so there is no need to wait for the GPU.
		view.Target->Resize(width, height);
		if (view.SceneColor != nullptr)
		{
			view.SceneColor->Resize(width, height);
		}
		// A click made on the old size points at another pixel now.
		view.PickGeneration++;

		// Each frame index's ImGui texture handle still points at that index's old image, which the
		// frame in flight may be drawing with. Point it at the new one when that index is recorded
		// next (UpdateImGuiTextures); this frame's index now. Updating in place (instead of
		// AddTexture again) means no descriptor sets leak from ImGui's pool.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			view.ImGuiTexturesOutdated[i] = true;
		}
		UpdateImGuiTextures(view, Renderer::GetRenderFrame());
	}

	void Renderer2D::UpdateImGuiTextures(uint32_t frame)
	{
		// Every view, not just the ones begun this frame: ImGui may show a view that isn't drawn.
		for (RenderViewData* view : s_Data->Views)
		{
			UpdateImGuiTextures(*view, frame);
		}
	}

	void Renderer2D::UpdateImGuiTextures(RenderViewData& view, uint32_t frame)
	{
		// Only call this after `frame`'s fence and before the present pass is recorded: the handle
		// must not be in use by a command buffer that is pending or being recorded.
		if (view.ImGuiTexturesOutdated[frame] == false)
		{
			return;
		}

		view.ImGuiTexturesOutdated[frame] = false;
		if (view.ImGuiTextures[frame] != VK_NULL_HANDLE)
		{
			const VulkanImageInfo& image = view.Target->GetImage(frame).GetVulkanImageInfo();
			WriteImGuiTexture(view.ImGuiTextures[frame], image.ImageView, image.Sampler);
		}
	}

	void Renderer2D::ApplySettings()
	{
		const RendererSettings& settings = Renderer::GetSettings();

		// The wireframe flag is baked into the pipeline, so a change means a rebuild.
		// Only rebuild when the GPU can do it; otherwise VulkanPipeline would warn every frame.
		const bool wireframe = settings.Wireframe && Renderer::GetCapabilities().Wireframe;
		if (wireframe != s_Data->QuadPipeline->GetSpecification().Wireframe && RebuildQuadPipeline(wireframe))
		{
			// Every cached view pipeline copies the quad pipeline's wireframe setting.
			ClearViewPipelines();
		}

		ReloadShadersIfChanged();
	}

	void Renderer2D::ReloadShadersIfChanged()
	{
		if (Renderer::GetSettings().ShaderHotReload == false)
		{
			return;
		}

		// A few checks per second is instant enough and keeps file-system calls off most frames.
		const auto now = std::chrono::steady_clock::now();
		if (now < s_Data->NextShaderPoll)
		{
			return;
		}

		s_Data->NextShaderPoll = now + Renderer2DData::ShaderPollInterval;

		// Only what was built from a changed shader is rebuilt; the rest keeps drawing.
		for (const std::string& name : VulkanShaderLibrary::ReloadChanged())
		{
			if (name == SpriteShaderName)
			{
				RebuildQuadPipeline(s_Data->QuadPipeline->GetSpecification().Wireframe);
			}
			else if (name == LightShaderName)
			{
				RebuildPipelineWithShader(s_Data->LightPipeline, LightShaderName);
			}
			else if (name == ShadowShaderName)
			{
				RebuildPipelineWithShader(s_Data->ShadowPipeline, ShadowShaderName);
			}
			else if (name == CompositeShaderName)
			{
				RebuildPipelineWithShader(s_Data->CompositePipeline, CompositeShaderName);
			}
			ClearViewPipelines(name);
			Renderer3D::OnShaderReloaded(name);
			HY_CORE_INFO("Shader '{}' reloaded.", name);
		}
	}

	bool Renderer2D::RebuildQuadPipeline(bool wireframe)
	{
		// Built first, so a pipeline that can't be built leaves the old one drawing.
		Scope<VulkanPipeline> pipeline;
		try
		{
			PipelineSpecification specification = s_Data->QuadPipeline->GetSpecification();
			specification.Shader = VulkanShaderLibrary::Get(SpriteShaderName);
			specification.Wireframe = wireframe;
			pipeline = CreateScope<VulkanPipeline>(specification);
		}
		catch (const std::exception& e)
		{
			HY_CORE_WARN("Quad pipeline rebuild failed; still using the previous one: {}", e.what());
			return false;
		}

		// No need to wait for the GPU: the old pipeline frees itself through the deletion queue once the
		// frames in flight that bound it are done.
		s_Data->QuadPipeline = std::move(pipeline);
		return true;
	}
}
