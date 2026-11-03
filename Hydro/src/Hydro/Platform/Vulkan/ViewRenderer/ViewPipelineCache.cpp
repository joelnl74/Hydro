#include "hypch.h"
#include "ViewPipelineCache.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/QuadBatcher.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/Light2DPass.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptorBuilder.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanGPUData.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanShaderLibrary.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/Renderer3D.h"

#include <chrono>
#include <map>
#include <tuple>

namespace Hydro
{
	// The cache's key: the attachment layout (colour formats in order, depth format) plus what the pipeline does with it.
	struct ViewPipelineKey
	{
		ViewPipelineKind Kind = ViewPipelineKind::Sprite;
		std::vector<ImageFormat> ColorFormats;
		ImageFormat DepthFormat = ImageFormat::None;
		bool WriteEntityID = false;
		bool Lit = false; // the Sprite, Decal and Cutout kinds use their forward-lit shaders

		bool operator<(const ViewPipelineKey& other) const
		{
			return std::tie(Kind, ColorFormats, DepthFormat, WriteEntityID, Lit)
				< std::tie(other.Kind, other.ColorFormats, other.DepthFormat, other.WriteEntityID, other.Lit);
		}
	};

	struct ViewPipelineCacheData
	{
		// Alpha-blended quads into the plain view layout (one sRGB colour attachment); every quad kind starts from it.
		Scope<VulkanPipeline> QuadPipeline;
		// A null entry failed to build (logged once) and is retried after the next shader change.
		std::map<ViewPipelineKey, Scope<VulkanPipeline>> Pipelines;

		PerFrameBuffers LayoutUniformBuffers;
		PerFrameBuffers LayoutLightingBuffers;
		PerFrameBuffers LayoutLightStorageBuffers;
		Scope<VulkanDescriptorBuilder> LayoutDescriptorSet;

		// Owned by the descriptor allocator's layout cache.
		VkDescriptorSetLayout TonemapLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout BloomLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout DepthOfFieldLayout = VK_NULL_HANDLE;

		std::chrono::steady_clock::time_point NextShaderPoll{};
		static constexpr std::chrono::milliseconds ShaderPollInterval{ 250 };
	};

	static ViewPipelineCacheData* s_Data = nullptr;

	// Throws when the shader doesn't compile.
	static VulkanShader& GetOrLoadShader(const char* name, const char* vertexPath, const char* fragmentPath)
	{
		VulkanShader* shader = VulkanShaderLibrary::Get(name);
		return shader != nullptr ? *shader : VulkanShaderLibrary::Load(name, vertexPath, fragmentPath);
	}

	static const char* GetShaderName(ViewPipelineKind kind, bool lit)
	{
		switch (kind)
		{
			case ViewPipelineKind::Cutout: return lit ? SpriteCutoutLitShaderName : SpriteCutoutShaderName;
			case ViewPipelineKind::FadedCutout: return lit ? SpriteFadeLitShaderName : SpriteFadeShaderName;
			case ViewPipelineKind::Composite: return CompositeShaderName;
			case ViewPipelineKind::ShadowCaster: return SpriteShadowCasterShaderName;
			case ViewPipelineKind::Tonemap: return TonemapShaderName;
			case ViewPipelineKind::BloomPrefilter: return BloomPrefilterShaderName;
			case ViewPipelineKind::BloomDown: return BloomDownShaderName;
			case ViewPipelineKind::BloomUp: return BloomUpShaderName;
			case ViewPipelineKind::DofCoc: return DofCocShaderName;
			case ViewPipelineKind::DofPrepare: return DofPrepareShaderName;
			case ViewPipelineKind::DofBlur: return DofBlurShaderName;
			case ViewPipelineKind::Sprite:
			case ViewPipelineKind::Decal: return lit ? SpriteLitShaderName : SpriteShaderName;
			default: return SpriteShaderName;
		}
	}

	// Starts from the pipeline built at Init for that kind of draw, so shaders, vertex layout, descriptor sets and the
	// wireframe setting carry over. Throws when a shader doesn't compile.
	static Scope<VulkanPipeline> Build(const ViewPipelineKey& key)
	{
		const bool hasDepth = key.DepthFormat != ImageFormat::None;

		PipelineSpecification specification;
		switch (key.Kind)
		{
			case ViewPipelineKind::Sprite:
			case ViewPipelineKind::Decal:
			case ViewPipelineKind::Overlay:
			case ViewPipelineKind::Cutout:
			case ViewPipelineKind::FadedCutout:
			case ViewPipelineKind::Additive:
			case ViewPipelineKind::DebugLines:
				specification = s_Data->QuadPipeline->GetSpecification();
				specification.Shader = VulkanShaderLibrary::Get(SpriteShaderName);
				if (key.Lit && (key.Kind == ViewPipelineKind::Sprite || key.Kind == ViewPipelineKind::Decal))
				{
					specification.Shader = &GetOrLoadShader(SpriteLitShaderName, "assets/shaders/vertex.vert",
						"assets/shaders/fragment_lit.frag");
				}
				if (key.Kind == ViewPipelineKind::Cutout)
				{
					specification.Shader = key.Lit
						? &GetOrLoadShader(SpriteCutoutLitShaderName, "assets/shaders/vertex.vert", "assets/shaders/sprite_cutout_lit.frag")
						: &GetOrLoadShader(SpriteCutoutShaderName, "assets/shaders/vertex.vert", "assets/shaders/sprite_cutout.frag");
					// The shader writes alpha 1 where it keeps a pixel: nothing to blend.
					specification.Blend = BlendMode::None;
				}
				if (key.Kind == ViewPipelineKind::FadedCutout)
				{
					specification.Shader = key.Lit
						? &GetOrLoadShader(SpriteFadeLitShaderName, "assets/shaders/vertex.vert", "assets/shaders/sprite_fade_lit.frag")
						: &GetOrLoadShader(SpriteFadeShaderName, "assets/shaders/vertex.vert", "assets/shaders/sprite_fade.frag");
					specification.Blend = BlendMode::None;
				}
				if (key.Kind == ViewPipelineKind::Additive)
				{
					specification.Blend = BlendMode::Additive;
				}
				specification.DepthTest = hasDepth && key.Kind != ViewPipelineKind::Overlay;
				specification.DepthWrite = hasDepth && (key.Kind == ViewPipelineKind::Cutout || key.Kind == ViewPipelineKind::FadedCutout);
				if (hasDepth && key.Kind == ViewPipelineKind::Decal)
				{
					// In front of ground geometry at the same height, and never written, so nothing else is affected.
					specification.DepthBiasConstant = -4.0f;
					specification.DepthBiasSlope = -1.0f;
				}
				break;

			case ViewPipelineKind::ShadowCaster:
				// Both faces (a sprite casts whichever way it is turned), pushed away from the sun against shadow acne.
				specification = s_Data->QuadPipeline->GetSpecification();
				specification.Shader = &GetOrLoadShader(SpriteShadowCasterShaderName, "assets/shaders/shadow_caster.vert",
					"assets/shaders/shadow_caster.frag");
				specification.Blend = BlendMode::None;
				specification.Wireframe = false;
				specification.BackfaceCulling = false;
				specification.DepthTest = true;
				specification.DepthWrite = true;
				specification.DepthBiasConstant = ShadowCasterDepthBiasConstant;
				specification.DepthBiasSlope = ShadowCasterDepthBiasSlope;
				break;

			case ViewPipelineKind::Tonemap:
				// No vertex buffer: composite.vert makes the triangle.
				specification.Shader = &GetOrLoadShader(TonemapShaderName, "assets/shaders/composite.vert", "assets/shaders/tonemap.frag");
				specification.DescriptorSetLayouts = { s_Data->TonemapLayout };
				specification.PushConstantSize = (uint32_t)sizeof(TonemapPushConstants);
				specification.Blend = BlendMode::None;
				specification.BackfaceCulling = false;
				specification.DepthTest = false;
				specification.DepthWrite = false;
				break;

			case ViewPipelineKind::BloomPrefilter:
			case ViewPipelineKind::BloomDown:
			case ViewPipelineKind::BloomUp:
				if (key.Kind == ViewPipelineKind::BloomPrefilter)
				{
					specification.Shader = &GetOrLoadShader(BloomPrefilterShaderName, "assets/shaders/composite.vert",
						"assets/shaders/bloom_prefilter.frag");
				}
				else if (key.Kind == ViewPipelineKind::BloomDown)
				{
					specification.Shader = &GetOrLoadShader(BloomDownShaderName, "assets/shaders/composite.vert",
						"assets/shaders/bloom_down.frag");
				}
				else
				{
					specification.Shader = &GetOrLoadShader(BloomUpShaderName, "assets/shaders/composite.vert",
						"assets/shaders/bloom_up.frag");
				}
				specification.DescriptorSetLayouts = { s_Data->BloomLayout };
				specification.PushConstantSize = (uint32_t)sizeof(BloomPushConstants);
				// Up passes add onto the level they draw into.
				specification.Blend = key.Kind == ViewPipelineKind::BloomUp ? BlendMode::Additive : BlendMode::None;
				specification.BackfaceCulling = false;
				specification.DepthTest = false;
				specification.DepthWrite = false;
				break;

			case ViewPipelineKind::DofCoc:
			case ViewPipelineKind::DofPrepare:
			case ViewPipelineKind::DofBlur:
				if (key.Kind == ViewPipelineKind::DofCoc)
				{
					specification.Shader = &GetOrLoadShader(DofCocShaderName, "assets/shaders/composite.vert",
						"assets/shaders/dof_coc.frag");
				}
				else if (key.Kind == ViewPipelineKind::DofPrepare)
				{
					specification.Shader = &GetOrLoadShader(DofPrepareShaderName, "assets/shaders/composite.vert",
						"assets/shaders/dof_prepare.frag");
				}
				else
				{
					specification.Shader = &GetOrLoadShader(DofBlurShaderName, "assets/shaders/composite.vert",
						"assets/shaders/dof_blur.frag");
				}
				specification.DescriptorSetLayouts = { s_Data->DepthOfFieldLayout };
				specification.PushConstantSize = (uint32_t)sizeof(DepthOfFieldPushConstants);
				specification.Blend = BlendMode::None;
				specification.BackfaceCulling = false;
				specification.DepthTest = false;
				specification.DepthWrite = false;
				break;

			case ViewPipelineKind::Composite:
			{
				const VulkanPipeline* composite = Light2DPass::GetCompositePipeline();
				if (composite == nullptr)
				{
					return nullptr;
				}
				specification = composite->GetSpecification();
				specification.Shader = VulkanShaderLibrary::Get(CompositeShaderName);
				specification.DepthTest = false;
				specification.DepthWrite = false;
				break;
			}
		}

		specification.ColorFormats = key.ColorFormats;
		specification.DepthFormat = key.DepthFormat;
		specification.WriteIntegerAttachments = key.WriteEntityID && key.Kind != ViewPipelineKind::DebugLines;
		return CreateScope<VulkanPipeline>(specification);
	}

	void ViewPipelineCache::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "ViewPipelineCache::Init called twice");
		s_Data = new ViewPipelineCacheData();

		// Required: throws when it doesn't compile. Every other shader is optional.
		VulkanShaderLibrary::Load(SpriteShaderName, "assets/shaders/vertex.vert", "assets/shaders/fragment.frag");

		// The tonemap's five inputs (scene colour, bloom, the depth of field's circle of confusion, blur and half-size
		// colour), a bloom step's source and a depth of field step's two sources.
		std::vector<VkDescriptorSetLayoutBinding> bindings(5);
		for (uint32_t i = 0; i < 5; i++)
		{
			bindings[i].binding = i;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[i].descriptorCount = 1;
			bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		s_Data->BloomLayout = VulkanDescriptorAllocator::GetLayout({ bindings[0] });
		s_Data->DepthOfFieldLayout = VulkanDescriptorAllocator::GetLayout({ bindings[0], bindings[1] });
		s_Data->TonemapLayout = VulkanDescriptorAllocator::GetLayout(bindings);

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
		s_Data->LayoutDescriptorSet = QuadBatcher::BuildDescriptorSet(s_Data->LayoutUniformBuffers, s_Data->LayoutLightingBuffers,
			s_Data->LayoutLightStorageBuffers);

		PipelineSpecification specification;
		specification.Layout = QuadBatcher::GetVertexLayout();
		specification.DescriptorSets = { s_Data->LayoutDescriptorSet.get() };
		specification.Shader = VulkanShaderLibrary::Get(SpriteShaderName);
		specification.ColorFormats = { ViewColorFormat };
		// Drawn in submission order and visible from both sides (a negative scale flips a sprite).
		specification.Blend = BlendMode::Alpha;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;
		s_Data->QuadPipeline = CreateScope<VulkanPipeline>(specification);
	}

	void ViewPipelineCache::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		// Pipelines and buffers free themselves through the deletion queue.
		Clear();
		s_Data->QuadPipeline.reset();
		s_Data->LayoutDescriptorSet.reset();
		s_Data->LayoutUniformBuffers = {};
		s_Data->LayoutLightingBuffers = {};
		s_Data->LayoutLightStorageBuffers = {};

		delete s_Data;
		s_Data = nullptr;
	}

	VulkanPipeline* ViewPipelineCache::Get(ViewPipelineKind kind, const VulkanRenderTarget& target, bool writeEntityID, bool lit,
		const VulkanRenderTarget* colorTarget)
	{
		ViewPipelineKey key;
		key.Kind = kind;
		key.ColorFormats = target.GetPipelineColorFormats(colorTarget);
		key.DepthFormat = target.GetPipelineDepthFormat();
		key.WriteEntityID = writeEntityID && target.GetSpecification().EntityID;
		key.Lit = lit && (kind == ViewPipelineKind::Sprite || kind == ViewPipelineKind::Decal || kind == ViewPipelineKind::Cutout
			|| kind == ViewPipelineKind::FadedCutout);

		// The plain view layout draws with the pipelines built at Init.
		if (key.ColorFormats.size() == 1 && key.ColorFormats[0] == ViewColorFormat && key.DepthFormat == ImageFormat::None
			&& key.Lit == false)
		{
			if (kind == ViewPipelineKind::Sprite || kind == ViewPipelineKind::Decal || kind == ViewPipelineKind::Overlay
				|| kind == ViewPipelineKind::DebugLines)
			{
				return s_Data->QuadPipeline.get();
			}
			if (kind == ViewPipelineKind::Composite)
			{
				return Light2DPass::GetCompositePipeline();
			}
		}

		auto it = s_Data->Pipelines.find(key);
		if (it != s_Data->Pipelines.end())
		{
			return it->second.get();
		}

		Scope<VulkanPipeline> pipeline;
		try
		{
			pipeline = Build(key);
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer2D: a view pipeline could not be built; those draws are skipped until the next shader change: {}",
				e.what());
		}

		VulkanPipeline* result = pipeline.get();
		s_Data->Pipelines.emplace(std::move(key), std::move(pipeline));
		return result;
	}

	void ViewPipelineCache::Clear()
	{
		s_Data->Pipelines.clear();
	}

	void ViewPipelineCache::Clear(const std::string& shaderName)
	{
		for (auto it = s_Data->Pipelines.begin(); it != s_Data->Pipelines.end();)
		{
			if (shaderName == GetShaderName(it->first.Kind, it->first.Lit))
			{
				it = s_Data->Pipelines.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void ViewPipelineCache::ApplySettings()
	{
		// Wireframe is baked into the pipeline. Only rebuild when the GPU can draw it, or VulkanPipeline warns every frame.
		const bool wireframe = Renderer::GetSettings().Wireframe && Renderer::GetCapabilities().Wireframe;
		if (wireframe != s_Data->QuadPipeline->GetSpecification().Wireframe && RebuildQuadPipeline(wireframe))
		{
			// Every cached pipeline copies the quad pipeline's wireframe setting.
			Clear();
		}

		ReloadShadersIfChanged();
	}

	void ViewPipelineCache::ReloadShadersIfChanged()
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
		s_Data->NextShaderPoll = now + ViewPipelineCacheData::ShaderPollInterval;

		for (const std::string& name : VulkanShaderLibrary::ReloadChanged())
		{
			if (name == SpriteShaderName)
			{
				RebuildQuadPipeline(s_Data->QuadPipeline->GetSpecification().Wireframe);
			}
			else
			{
				Light2DPass::OnShaderReloaded(name);
			}
			Clear(name);
			Renderer3D::OnShaderReloaded(name);
			HY_CORE_INFO("Shader '{}' reloaded.", name);
		}
	}

	bool ViewPipelineCache::RebuildQuadPipeline(bool wireframe)
	{
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

		// The old pipeline frees itself through the deletion queue once the frames that bound it are done.
		s_Data->QuadPipeline = std::move(pipeline);
		return true;
	}

	void ViewPipelineCache::RebuildWithShader(Scope<VulkanPipeline>& pipeline, const char* shaderName)
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

	VulkanDescriptorBuilder& ViewPipelineCache::GetLayoutDescriptorSet()
	{
		return *s_Data->LayoutDescriptorSet;
	}

	VkDescriptorSetLayout ViewPipelineCache::GetTonemapLayout()
	{
		return s_Data->TonemapLayout;
	}

	VkDescriptorSetLayout ViewPipelineCache::GetBloomLayout()
	{
		return s_Data->BloomLayout;
	}

	VkDescriptorSetLayout ViewPipelineCache::GetDepthOfFieldLayout()
	{
		return s_Data->DepthOfFieldLayout;
	}
}
