#include "hypch.h"
#include "Renderer3D.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Core/Time.h"
#include "Hydro/Platform/Vulkan/VulkanBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanGPUData.h"
#include "Hydro/Platform/Vulkan/VulkanMeshData.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanShaderLibrary.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Renderer/MeshFactory.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/Texture2D.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <tuple>

namespace Hydro
{
	// set 1, binding 0 of mesh.frag (std140).
	struct MaterialUniform
	{
		glm::vec4 Tint{ 1.0f };
		glm::vec4 Emission{ 0.0f }; // rgb = colour * intensity
		glm::vec2 Tiling{ 1.0f };
		float AlphaCutoff = 0.0f;
		float Padding = 0.0f;
	};
	static_assert(sizeof(MaterialUniform) == 48, "MaterialUniform: must match the std140 block in mesh.frag");
	static_assert(offsetof(MaterialUniform, Emission) == 16 && offsetof(MaterialUniform, Tiling) == 32
		&& offsetof(MaterialUniform, AlphaCutoff) == 40, "MaterialUniform: must match the std140 block in mesh.frag");

	// A Standard material on the GPU: its uniform data and a persistent descriptor set (set 1) with the
	// albedo and emissive textures. Immutable: a change to the data is another material.
	struct MeshMaterial
	{
		MaterialData Data;
		Ref<Texture2D> Albedo;   // what the set points at (the loaded asset, or null = white)
		Ref<Texture2D> Emissive;
		Scope<VulkanBuffer> Uniform;
		DescriptorAllocation Set;
		uint64_t LastUsedFrame = 0;

		~MeshMaterial()
		{
			VulkanDescriptorAllocator::FreePersistent(Set);
		}
	};

	struct ShapeEntry
	{
		MeshShape Shape = MeshShape::Box;
		glm::vec3 Size{ 1.0f };
		uint32_t Steps = 0;
		uint32_t Segments = 0;
		Ref<Mesh> MeshRef;
		uint64_t LastUsedFrame = 0;
	};

	struct MeshPipelineKey
	{
		std::vector<ImageFormat> ColorFormats;
		ImageFormat DepthFormat = ImageFormat::None;
		bool WriteEntityID = false;
		bool Wireframe = false;

		bool operator<(const MeshPipelineKey& other) const
		{
			return std::tie(ColorFormats, DepthFormat, WriteEntityID, Wireframe) < std::tie(other.ColorFormats, other.DepthFormat, other.WriteEntityID, other.Wireframe);
		}
	};

	struct Renderer3DData
	{
		std::vector<Scope<MeshMaterial>> Materials;
		std::vector<ShapeEntry> Shapes;
		std::map<MeshPipelineKey, Scope<VulkanPipeline>> Pipelines; // a null entry failed to build (logged once)

		Ref<Texture2D> WhiteTexture;
		VkSampler NearestSampler = VK_NULL_HANDLE;
		VkSampler LinearSampler = VK_NULL_HANDLE;
		VkDescriptorSetLayout MaterialLayout = VK_NULL_HANDLE;

		RenderViewData* CurrentView = nullptr;
		Renderer3DStatistics Stats;     // this frame
		Renderer3DStatistics LastStats; // GetStats
	};

	static Renderer3DData* s_Data3D = nullptr;

	static constexpr const char* MeshShaderName = "Mesh";
	// Shapes and materials no frame has used for this many frames are freed (the GPU is long done with them).
	static constexpr uint64_t UnusedFramesBeforeFree = 120;

	static VkSampler CreateSampler(VkFilter filter)
	{
		VkSamplerCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = filter;
		info.minFilter = filter;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.maxLod = 0.0f;
		info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		VkSampler sampler = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateSampler(Renderer::GetVulkanDevice().GetDevice(), &info, nullptr, &sampler));
		return sampler;
	}

	void Renderer3D::Init()
	{
		HY_CORE_ASSERT(s_Data3D == nullptr, "Renderer3D::Init called twice");
		s_Data3D = new Renderer3DData();

		const uint32_t whitePixel = 0xffffffff;
		ImageSpecification whiteSpecification;
		whiteSpecification.Width = 1;
		whiteSpecification.Height = 1;
		whiteSpecification.Format = ImageFormat::SRGB;
		s_Data3D->WhiteTexture = CreateRef<Texture2D>(whiteSpecification, &whitePixel);

		// Materials sample with their own filter (TextureFilter), always repeating (tiling).
		s_Data3D->NearestSampler = CreateSampler(VK_FILTER_NEAREST);
		s_Data3D->LinearSampler = CreateSampler(VK_FILTER_LINEAR);

		// Set 1 of mesh.frag: the material's uniform data, albedo and emissive textures.
		std::vector<VkDescriptorSetLayoutBinding> bindings(3);
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		for (uint32_t i = 1; i < 3; i++)
		{
			bindings[i].binding = i;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[i].descriptorCount = 1;
			bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		s_Data3D->MaterialLayout = VulkanDescriptorAllocator::GetLayout(bindings);
	}

	void Renderer3D::Shutdown()
	{
		if (s_Data3D == nullptr)
		{
			return;
		}

		// Everything frees itself through the deletion queue, which Renderer::Shutdown flushes next.
		s_Data3D->Pipelines.clear();
		s_Data3D->Materials.clear();
		s_Data3D->Shapes.clear();
		s_Data3D->WhiteTexture.reset();
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		Renderer::SubmitResourceFree([device, nearest = s_Data3D->NearestSampler, linear = s_Data3D->LinearSampler]()
		{
			vkDestroySampler(device, nearest, nullptr);
			vkDestroySampler(device, linear, nullptr);
		});

		delete s_Data3D;
		s_Data3D = nullptr;
	}

	void Renderer3D::BeginScene(RenderView& view)
	{
		HY_CORE_ASSERT(s_Data3D->CurrentView == nullptr, "Renderer3D::BeginScene without EndScene");
		s_Data3D->CurrentView = view.m_Data.get();
	}

	void Renderer3D::EndScene()
	{
		s_Data3D->CurrentView = nullptr;
	}

	static const VulkanImageInfo& GetTextureImage(const Ref<Texture2D>& texture)
	{
		const Ref<Texture2D>& shown = texture != nullptr ? texture : s_Data3D->WhiteTexture;
		return GetTexture2DData(*shown).Image->GetVulkanImageInfo();
	}

	static Scope<MeshMaterial> CreateMaterial(const MaterialData& data, const Ref<Texture2D>& albedo, const Ref<Texture2D>& emissive)
	{
		Scope<MeshMaterial> material = CreateScope<MeshMaterial>();
		material->Data = data;
		material->Albedo = albedo;
		material->Emissive = emissive;

		MaterialUniform uniform;
		uniform.Tint = data.Tint;
		uniform.Emission = glm::vec4(data.EmissionColor * data.EmissionIntensity, 0.0f);
		uniform.Tiling = data.Tiling;
		uniform.AlphaCutoff = data.AlphaCutoff;

		// Written once (materials are immutable), so one buffer is enough for every frame in flight.
		BufferSpecification specification;
		specification.Usage = BufferUsage::Uniform;
		specification.Memory = BufferMemory::HostVisible;
		specification.Size = sizeof(MaterialUniform);
		specification.DebugName = "Material";
		material->Uniform = CreateScope<VulkanBuffer>(specification, &uniform);

		material->Set = VulkanDescriptorAllocator::AllocatePersistent(s_Data3D->MaterialLayout);
		if (material->Set.Set == VK_NULL_HANDLE)
		{
			return nullptr;
		}

		const VkSampler sampler = data.Filter == TextureFilter::Linear ? s_Data3D->LinearSampler : s_Data3D->NearestSampler;
		const VulkanImageInfo& albedoImage = GetTextureImage(albedo);
		const VulkanImageInfo& emissiveImage = GetTextureImage(emissive);

		VkDescriptorBufferInfo bufferInfo{ material->Uniform->GetVulkanBuffer(), 0, sizeof(MaterialUniform) };
		VkDescriptorImageInfo images[2]{};
		images[0] = { sampler, albedoImage.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		images[1] = { sampler, emissiveImage.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		VkWriteDescriptorSet writes[3]{};
		for (uint32_t i = 0; i < 3; i++)
		{
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = material->Set.Set;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
		}
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &bufferInfo;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &images[0];
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2].pImageInfo = &images[1];
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 3, writes, 0, nullptr);
		return material;
	}

	// The cached material for `data`. A texture that was (re)loaded since the material was made gives it a
	// new set; the old one is freed through the deletion queue.
	static MeshMaterial* GetMaterial(const MaterialData& data)
	{
		const Ref<Texture2D> albedo = (uint64_t)data.Albedo != 0 ? AssetManager::GetLoadedAsset<Texture2D>(data.Albedo) : nullptr;
		const Ref<Texture2D> emissive = (uint64_t)data.Emissive != 0 ? AssetManager::GetLoadedAsset<Texture2D>(data.Emissive) : nullptr;
		const uint64_t frame = Time::GetFrameCount();

		std::vector<Scope<MeshMaterial>>& materials = s_Data3D->Materials;
		for (auto it = materials.begin(); it != materials.end(); ++it)
		{
			MeshMaterial& material = **it;
			if (material.Data != data)
			{
				continue;
			}
			if (material.Albedo == albedo && material.Emissive == emissive)
			{
				material.LastUsedFrame = frame;
				return &material;
			}
			// A texture was loaded or reloaded since: make a new one. Textures change between frames, never
			// while a frame is being submitted, so no draw of this frame points at the stale one; frames in
			// flight still may, so its set and buffer are freed through the deletion queue.
			materials.erase(it);
			break;
		}

		Scope<MeshMaterial> material = CreateMaterial(data, albedo, emissive);
		if (material == nullptr)
		{
			return nullptr;
		}
		material->LastUsedFrame = frame;
		materials.push_back(std::move(material));
		return materials.back().get();
	}

	void Renderer3D::DrawMesh(const Ref<Mesh>& mesh, uint32_t subMesh, const MaterialData& material, const glm::mat4& transform, int32_t entityID)
	{
		HY_CORE_ASSERT(s_Data3D->CurrentView != nullptr, "Renderer3D::DrawMesh outside BeginScene/EndScene");
		RenderViewData* view = s_Data3D->CurrentView;
		if (view == nullptr || mesh == nullptr || subMesh >= mesh->GetSubMeshes().size())
		{
			return;
		}

		MeshMaterial* resolved = GetMaterial(material);
		if (resolved == nullptr)
		{
			return;
		}

		// Sorted front to back within a material when recorded (fewer pixels shaded twice).
		const glm::vec3 centre = (mesh->GetBoundsMin() + mesh->GetBoundsMax()) * 0.5f;
		const glm::vec4 clip = view->ViewProjection * transform * glm::vec4(centre, 1.0f);

		MeshDraw draw;
		draw.MeshRef = mesh;
		draw.SubMesh = subMesh;
		draw.Material = resolved;
		draw.Transform = transform;
		draw.EntityID = entityID < 0 ? -1 : entityID;
		draw.SortDepth = clip.w != 0.0f ? clip.z / clip.w : clip.z;
		view->MeshDraws.push_back(std::move(draw));
	}

	Ref<Mesh> Renderer3D::GetShapeMesh(const MeshComponent& component)
	{
		// Only the parameters the shape uses are part of its key.
		const glm::vec3 size = glm::max(component.Size, glm::vec3(0.001f));
		const uint32_t steps = component.Shape == MeshShape::Stairs ? std::max(component.Steps, 1u) : 0u;
		const uint32_t segments = component.Shape == MeshShape::Cylinder ? std::max(component.Segments, 3u) : 0u;
		const uint64_t frame = Time::GetFrameCount();

		for (ShapeEntry& entry : s_Data3D->Shapes)
		{
			if (entry.Shape == component.Shape && entry.Size == size && entry.Steps == steps && entry.Segments == segments)
			{
				entry.LastUsedFrame = frame;
				return entry.MeshRef;
			}
		}

		MeshData data;
		const char* name = "Box";
		switch (component.Shape)
		{
			case MeshShape::Box: data = MeshFactory::Box(size); name = "Box"; break;
			case MeshShape::Plane: data = MeshFactory::Plane(size); name = "Plane"; break;
			case MeshShape::Ramp: data = MeshFactory::Ramp(size); name = "Ramp"; break;
			case MeshShape::Stairs: data = MeshFactory::Stairs(size, steps); name = "Stairs"; break;
			case MeshShape::Cylinder: data = MeshFactory::Cylinder(size, segments); name = "Cylinder"; break;
		}

		// GPU-only buffers: the upload waits for the GPU once, the first time this shape and size are drawn.
		ShapeEntry entry;
		entry.Shape = component.Shape;
		entry.Size = size;
		entry.Steps = steps;
		entry.Segments = segments;
		entry.MeshRef = CreateRef<Mesh>(data, std::string("Shape ") + name);
		entry.LastUsedFrame = frame;
		s_Data3D->Shapes.push_back(entry);
		return entry.MeshRef;
	}

	static VulkanPipeline* GetPipeline(const RenderViewData& view, bool writeEntityID)
	{
		MeshPipelineKey key;
		key.ColorFormats = view.Target->GetPipelineColorFormats();
		key.DepthFormat = view.Target->GetPipelineDepthFormat();
		key.WriteEntityID = writeEntityID && view.Target->GetSpecification().EntityID;
		key.Wireframe = Renderer::GetSettings().Wireframe && Renderer::GetCapabilities().Wireframe;

		auto it = s_Data3D->Pipelines.find(key);
		if (it != s_Data3D->Pipelines.end())
		{
			return it->second.get();
		}

		Scope<VulkanPipeline> pipeline;
		try
		{
			VulkanShader* shader = VulkanShaderLibrary::Get(MeshShaderName);
			if (shader == nullptr)
			{
				shader = &VulkanShaderLibrary::Load(MeshShaderName, "assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
			}

			PipelineSpecification specification;
			specification.Shader = shader;
			specification.Layout = GetMeshVertexLayout();
			// Set 0: the view's camera (the same layout as the quad shader's set 0; mesh.vert reads binding 0).
			specification.DescriptorSetLayouts = { view.TextureSets[0].DescriptorSet->GetDescriptorSetLayout(), s_Data3D->MaterialLayout };
			specification.ColorFormats = key.ColorFormats;
			specification.DepthFormat = key.DepthFormat;
			specification.PushConstantSize = (uint32_t)sizeof(ObjectPushConstants);
			specification.BackfaceCulling = true;
			specification.DepthTest = true;
			specification.DepthWrite = true;
			specification.Blend = BlendMode::None;
			specification.Wireframe = key.Wireframe;
			specification.WriteIntegerAttachments = key.WriteEntityID;
			pipeline = CreateScope<VulkanPipeline>(specification);
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer3D: the mesh pipeline could not be built; meshes are skipped until the next shader change: {}", e.what());
		}

		VulkanPipeline* result = pipeline.get();
		s_Data3D->Pipelines.emplace(std::move(key), std::move(pipeline));
		return result;
	}

	void Renderer3D::RecordMeshes(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		if (view.MeshDraws.empty() || view.Target->GetPipelineDepthFormat() == ImageFormat::None)
		{
			return;
		}

		// By pipeline (pickable or not), then material, then front to back.
		std::sort(view.MeshDraws.begin(), view.MeshDraws.end(), [](const MeshDraw& a, const MeshDraw& b)
		{
			const bool aPickable = a.EntityID >= 0;
			const bool bPickable = b.EntityID >= 0;
			if (aPickable != bPickable)
			{
				return aPickable;
			}
			if (a.Material != b.Material)
			{
				return a.Material < b.Material;
			}
			return a.SortDepth < b.SortDepth;
		});

		const bool hasEntityID = view.Target->GetSpecification().EntityID;
		VulkanPipeline* pickablePipeline = GetPipeline(view, true);
		VulkanPipeline* otherPipeline = hasEntityID ? GetPipeline(view, false) : pickablePipeline;

		VkCommandBuffer commandBuffer = context.CommandBuffer;
		VulkanPipeline* boundPipeline = nullptr;
		const MeshMaterial* boundMaterial = nullptr;
		const Mesh* boundMesh = nullptr;
		for (const MeshDraw& draw : view.MeshDraws)
		{
			VulkanPipeline* pipeline = draw.EntityID >= 0 ? pickablePipeline : otherPipeline;
			if (pipeline == nullptr)
			{
				continue;
			}

			if (pipeline != boundPipeline)
			{
				pipeline->Bind(commandBuffer, view.Target->GetExtent());
				// Texture set 0's descriptor set has the view's camera at binding 0 (written in RecordView).
				pipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[0].DescriptorSet);
				boundPipeline = pipeline;
				boundMaterial = nullptr;
			}
			if (draw.Material != boundMaterial)
			{
				pipeline->BindDescriptorSet(commandBuffer, 1, draw.Material->Set.Set);
				boundMaterial = draw.Material;
			}
			if (draw.MeshRef.get() != boundMesh)
			{
				const MeshBuffers& buffers = draw.MeshRef->GetBuffers();
				buffers.Vertices->BindAsVertexBuffer(commandBuffer);
				buffers.Indices->BindAsIndexBuffer(commandBuffer);
				boundMesh = draw.MeshRef.get();
			}

			ObjectPushConstants constants;
			constants.Transform = draw.Transform;
			constants.EntityID = draw.EntityID;
			pipeline->PushConstants(commandBuffer, &constants, (uint32_t)sizeof(constants));

			const SubMesh& subMesh = draw.MeshRef->GetSubMeshes()[draw.SubMesh];
			vkCmdDrawIndexed(commandBuffer, subMesh.IndexCount, 1, subMesh.FirstIndex, 0, 0);
			s_Data3D->Stats.MeshDraws++;
			s_Data3D->Stats.Triangles += subMesh.IndexCount / 3;
		}
	}

	void Renderer3D::EndFrame()
	{
		if (s_Data3D == nullptr)
		{
			return;
		}

		// Unused for a while (a slider dragged through many sizes or tints leaves many): their GPU objects
		// go through the deletion queue.
		const uint64_t frame = Time::GetFrameCount();
		const auto unused = [frame](uint64_t lastUsed) { return lastUsed + UnusedFramesBeforeFree < frame; };
		std::vector<Scope<MeshMaterial>>& materials = s_Data3D->Materials;
		materials.erase(std::remove_if(materials.begin(), materials.end(), [&](const Scope<MeshMaterial>& material) { return unused(material->LastUsedFrame); }), materials.end());
		std::vector<ShapeEntry>& shapes = s_Data3D->Shapes;
		shapes.erase(std::remove_if(shapes.begin(), shapes.end(), [&](const ShapeEntry& entry) { return unused(entry.LastUsedFrame); }), shapes.end());

		s_Data3D->Stats.CachedMeshes = (uint32_t)shapes.size();
		s_Data3D->Stats.CachedMaterials = (uint32_t)materials.size();
		s_Data3D->LastStats = s_Data3D->Stats;
		s_Data3D->Stats = {};
	}

	void Renderer3D::OnShaderReloaded(const std::string& name)
	{
		if (s_Data3D != nullptr && name == MeshShaderName)
		{
			s_Data3D->Pipelines.clear();
		}
	}

	Renderer3DStatistics Renderer3D::GetStats()
	{
		return s_Data3D != nullptr ? s_Data3D->LastStats : Renderer3DStatistics{};
	}
}
