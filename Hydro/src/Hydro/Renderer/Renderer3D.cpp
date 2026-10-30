#include "hypch.h"
#include "Renderer3D.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Core/Time.h"
#include "Hydro/Platform/Vulkan/VulkanBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanGPUData.h"
#include "Hydro/Platform/Vulkan/VulkanMeshData.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
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

	// set 1, binding 0 of terrain.frag (std140). Layers past LayerCount are never picked.
	struct TerrainSurfaceUniform
	{
		glm::vec4 LayerTint[MaxTerrainLayers];
		glm::vec4 LayerTiling[MaxTerrainLayers]; // xy: repeats per metre
		glm::vec4 Roughness{ 0.0f };             // per layer, weight units
		glm::vec4 WeightMap{ 1.0f };             // xy: 1 / metres the weight map covers, z: layer count
		glm::vec4 Noise{ 0.0f };                 // x: cluster size (art pixels), y: share of per-pixel noise
		glm::uvec4 Seed{ 0u };                   // x: the terrain's seed
	};
	static_assert(sizeof(TerrainSurfaceUniform) == 192, "TerrainSurfaceUniform: must match the std140 block in terrain.frag");

	// A terrain surface material on the GPU: its uniform data and a persistent descriptor set (set 1 of terrain.frag)
	// with the weight map and the layers' albedos. Immutable, like MeshMaterial.
	struct TerrainSurfaceGPU
	{
		TerrainSurfaceMaterial Data;
		Ref<Texture2D> Albedos[MaxTerrainLayers]; // what the set points at (the loaded assets, or null = white)
		Scope<VulkanBuffer> Uniform;
		DescriptorAllocation Set;
		uint64_t LastUsedFrame = 0;

		~TerrainSurfaceGPU()
		{
			VulkanDescriptorAllocator::FreePersistent(Set);
		}
	};

	bool TerrainSurfaceMaterial::operator==(const TerrainSurfaceMaterial& other) const
	{
		if (Weights != other.Weights || WeightMapSize != other.WeightMapSize || LayerCount != other.LayerCount || EdgeClusterSize != other.EdgeClusterSize
			|| EdgePixelNoise != other.EdgePixelNoise || Seed != other.Seed)
		{
			return false;
		}
		for (uint32_t i = 0; i < MaxTerrainLayers; i++)
		{
			if (Layers[i] != other.Layers[i] || EdgeRoughness[i] != other.EdgeRoughness[i])
			{
				return false;
			}
		}
		return true;
	}

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
		bool ShadowCaster = false; // depth only, into a sun shadow map (R14)
		bool TerrainSurface = false; // terrain.frag with the terrain surface's set 1

		bool operator<(const MeshPipelineKey& other) const
		{
			return std::tie(ColorFormats, DepthFormat, WriteEntityID, Wireframe, ShadowCaster, TerrainSurface)
				< std::tie(other.ColorFormats, other.DepthFormat, other.WriteEntityID, other.Wireframe, other.ShadowCaster, other.TerrainSurface);
		}
	};

	struct Renderer3DData
	{
		std::vector<Scope<MeshMaterial>> Materials;
		std::vector<Scope<TerrainSurfaceGPU>> TerrainSurfaces;
		std::vector<ShapeEntry> Shapes;
		std::map<MeshPipelineKey, Scope<VulkanPipeline>> Pipelines; // a null entry failed to build (logged once)

		Ref<Texture2D> WhiteTexture;
		VkSampler NearestSampler = VK_NULL_HANDLE;
		VkSampler LinearSampler = VK_NULL_HANDLE;
		VkSampler LinearClampSampler = VK_NULL_HANDLE; // terrain weight maps
		VkDescriptorSetLayout MaterialLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout TerrainSurfaceLayout = VK_NULL_HANDLE;

		RenderViewData* CurrentView = nullptr;
		Renderer3DStatistics Stats;     // this frame
		Renderer3DStatistics LastStats; // GetStats
	};

	static Renderer3DData* s_Data3D = nullptr;

	static constexpr const char* MeshShaderName = "Mesh";
	static constexpr const char* MeshShadowCasterShaderName = "MeshShadowCaster";
	static constexpr const char* TerrainSurfaceShaderName = "TerrainSurface";
	// Shapes and materials no frame has used for this many frames are freed (the GPU is long done with them).
	static constexpr uint64_t UnusedFramesBeforeFree = 120;

	static VkSampler CreateSampler(VkFilter filter, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
	{
		VkSamplerCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = filter;
		info.minFilter = filter;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		info.addressModeU = addressMode;
		info.addressModeV = addressMode;
		info.addressModeW = addressMode;
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

		// Set 1 of terrain.frag: the surface's uniform data, the weight map and the layers' albedos (one binding each:
		// the shader picks a layer per pixel through a switch, so no non-uniform indexing is needed).
		s_Data3D->LinearClampSampler = CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		std::vector<VkDescriptorSetLayoutBinding> terrainBindings(2 + MaxTerrainLayers);
		for (uint32_t i = 0; i < (uint32_t)terrainBindings.size(); i++)
		{
			terrainBindings[i].binding = i;
			terrainBindings[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			terrainBindings[i].descriptorCount = 1;
			terrainBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		s_Data3D->TerrainSurfaceLayout = VulkanDescriptorAllocator::GetLayout(terrainBindings);
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
		s_Data3D->TerrainSurfaces.clear();
		s_Data3D->Shapes.clear();
		s_Data3D->WhiteTexture.reset();
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		Renderer::SubmitResourceFree([device, nearest = s_Data3D->NearestSampler, linear = s_Data3D->LinearSampler, clamp = s_Data3D->LinearClampSampler]()
		{
			vkDestroySampler(device, nearest, nullptr);
			vkDestroySampler(device, linear, nullptr);
			vkDestroySampler(device, clamp, nullptr);
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

	void Renderer3D::DrawMesh(const Ref<Mesh>& mesh, uint32_t subMesh, const MaterialData& material, const glm::mat4& transform, int32_t entityID,
		bool castShadows)
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
		draw.CastShadows = castShadows;
		view->MeshDraws.push_back(std::move(draw));
	}

	static Scope<TerrainSurfaceGPU> CreateTerrainSurface(const TerrainSurfaceMaterial& data, const Ref<Texture2D> (&albedos)[MaxTerrainLayers])
	{
		Scope<TerrainSurfaceGPU> surface = CreateScope<TerrainSurfaceGPU>();
		surface->Data = data;
		for (uint32_t i = 0; i < MaxTerrainLayers; i++)
		{
			surface->Albedos[i] = albedos[i];
		}

		TerrainSurfaceUniform uniform;
		for (uint32_t i = 0; i < MaxTerrainLayers; i++)
		{
			uniform.LayerTint[i] = data.Layers[i].Tint;
			uniform.LayerTiling[i] = glm::vec4(data.Layers[i].Tiling, 0.0f, 0.0f);
			uniform.Roughness[(int)i] = data.EdgeRoughness[i];
		}
		const glm::vec2 size = glm::max(data.WeightMapSize, glm::vec2(1e-4f));
		uniform.WeightMap = glm::vec4(1.0f / size.x, 1.0f / size.y, (float)std::min(std::max(data.LayerCount, 1u), MaxTerrainLayers), 0.0f);
		uniform.Noise = glm::vec4(std::max(data.EdgeClusterSize, 0.5f), glm::clamp(data.EdgePixelNoise, 0.0f, 1.0f), 0.0f, 0.0f);
		uniform.Seed = glm::uvec4(data.Seed, 0u, 0u, 0u);

		BufferSpecification specification;
		specification.Usage = BufferUsage::Uniform;
		specification.Memory = BufferMemory::HostVisible;
		specification.Size = sizeof(TerrainSurfaceUniform);
		specification.DebugName = "TerrainSurface";
		surface->Uniform = CreateScope<VulkanBuffer>(specification, &uniform);

		surface->Set = VulkanDescriptorAllocator::AllocatePersistent(s_Data3D->TerrainSurfaceLayout);
		if (surface->Set.Set == VK_NULL_HANDLE)
		{
			return nullptr;
		}

		VkDescriptorBufferInfo bufferInfo{ surface->Uniform->GetVulkanBuffer(), 0, sizeof(TerrainSurfaceUniform) };
		VkDescriptorImageInfo images[1 + MaxTerrainLayers]{};
		images[0] = { s_Data3D->LinearClampSampler, GetTextureImage(data.Weights).ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		for (uint32_t i = 0; i < MaxTerrainLayers; i++)
		{
			const VkSampler sampler = data.Layers[i].Filter == TextureFilter::Linear ? s_Data3D->LinearSampler : s_Data3D->NearestSampler;
			images[1 + i] = { sampler, GetTextureImage(albedos[i]).ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		}

		VkWriteDescriptorSet writes[2 + MaxTerrainLayers]{};
		for (uint32_t i = 0; i < 2 + MaxTerrainLayers; i++)
		{
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = surface->Set.Set;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			if (i == 0)
			{
				writes[i].pBufferInfo = &bufferInfo;
			}
			else
			{
				writes[i].pImageInfo = &images[i - 1];
			}
		}
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 2 + MaxTerrainLayers, writes, 0, nullptr);
		return surface;
	}

	// The cached terrain surface for `data`; a layer texture (re)loaded since gives it a new set, like GetMaterial.
	static TerrainSurfaceGPU* GetTerrainSurface(const TerrainSurfaceMaterial& data)
	{
		Ref<Texture2D> albedos[MaxTerrainLayers];
		for (uint32_t i = 0; i < MaxTerrainLayers; i++)
		{
			albedos[i] = (uint64_t)data.Layers[i].Albedo != 0 ? AssetManager::GetLoadedAsset<Texture2D>(data.Layers[i].Albedo) : nullptr;
		}
		const uint64_t frame = Time::GetFrameCount();

		std::vector<Scope<TerrainSurfaceGPU>>& surfaces = s_Data3D->TerrainSurfaces;
		for (auto it = surfaces.begin(); it != surfaces.end(); ++it)
		{
			TerrainSurfaceGPU& surface = **it;
			if (surface.Data != data)
			{
				continue;
			}
			bool same = true;
			for (uint32_t i = 0; i < MaxTerrainLayers; i++)
			{
				same = same && surface.Albedos[i] == albedos[i];
			}
			if (same)
			{
				surface.LastUsedFrame = frame;
				return &surface;
			}
			surfaces.erase(it);
			break;
		}

		Scope<TerrainSurfaceGPU> surface = CreateTerrainSurface(data, albedos);
		if (surface == nullptr)
		{
			return nullptr;
		}
		surface->LastUsedFrame = frame;
		surfaces.push_back(std::move(surface));
		return surfaces.back().get();
	}

	void Renderer3D::DrawTerrainSurface(const Ref<Mesh>& mesh, uint32_t subMesh, const TerrainSurfaceMaterial& material, const glm::mat4& transform,
		int32_t entityID, bool castShadows)
	{
		HY_CORE_ASSERT(s_Data3D->CurrentView != nullptr, "Renderer3D::DrawTerrainSurface outside BeginScene/EndScene");
		RenderViewData* view = s_Data3D->CurrentView;
		if (view == nullptr || mesh == nullptr || subMesh >= mesh->GetSubMeshes().size() || material.Weights == nullptr)
		{
			return;
		}

		TerrainSurfaceGPU* surface = GetTerrainSurface(material);
		// The shadow pass draws it with the Standard caster pipeline: a white, opaque material.
		MeshMaterial* shadowMaterial = GetMaterial(MaterialData{});
		if (surface == nullptr || shadowMaterial == nullptr)
		{
			return;
		}

		const glm::vec3 centre = (mesh->GetBoundsMin() + mesh->GetBoundsMax()) * 0.5f;
		const glm::vec4 clip = view->ViewProjection * transform * glm::vec4(centre, 1.0f);

		MeshDraw draw;
		draw.MeshRef = mesh;
		draw.SubMesh = subMesh;
		draw.Material = shadowMaterial;
		draw.TerrainSurface = surface;
		draw.Transform = transform;
		draw.EntityID = entityID < 0 ? -1 : entityID;
		draw.SortDepth = clip.w != 0.0f ? clip.z / clip.w : clip.z;
		draw.CastShadows = castShadows;
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

	// The mesh pipeline for `target` (the view's, or its shadow map for a shadow caster).
	static VulkanPipeline* GetPipeline(const RenderViewData& view, const VulkanRenderTarget& target, bool writeEntityID, bool shadowCaster = false,
		bool terrainSurface = false)
	{
		MeshPipelineKey key;
		// In the HDR scene pass the view's target draws into the scene colour (post-processing plan P1).
		key.ColorFormats = target.GetPipelineColorFormats(&target == view.Target.get() ? view.PassColorTarget : nullptr);
		key.DepthFormat = target.GetPipelineDepthFormat();
		key.WriteEntityID = writeEntityID && target.GetSpecification().EntityID && shadowCaster == false;
		key.Wireframe = Renderer::GetSettings().Wireframe && Renderer::GetCapabilities().Wireframe && shadowCaster == false;
		key.ShadowCaster = shadowCaster;
		key.TerrainSurface = terrainSurface && shadowCaster == false;

		auto it = s_Data3D->Pipelines.find(key);
		if (it != s_Data3D->Pipelines.end())
		{
			return it->second.get();
		}

		Scope<VulkanPipeline> pipeline;
		try
		{
			const char* shaderName = shadowCaster ? MeshShadowCasterShaderName : (key.TerrainSurface ? TerrainSurfaceShaderName : MeshShaderName);
			VulkanShader* shader = VulkanShaderLibrary::Get(shaderName);
			if (shader == nullptr)
			{
				if (shadowCaster)
				{
					shader = &VulkanShaderLibrary::Load(MeshShadowCasterShaderName, "assets/shaders/mesh_shadow.vert", "assets/shaders/mesh_shadow.frag");
				}
				else if (key.TerrainSurface)
				{
					// The mesh vertex shader as it is: the tops' UVs are their positions in the terrain's space.
					shader = &VulkanShaderLibrary::Load(TerrainSurfaceShaderName, "assets/shaders/mesh.vert", "assets/shaders/terrain.frag");
				}
				else
				{
					shader = &VulkanShaderLibrary::Load(MeshShaderName, "assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
				}
			}

			PipelineSpecification specification;
			specification.Shader = shader;
			specification.Layout = GetMeshVertexLayout();
			// Set 0: the view's camera (the same layout as the quad shader's set 0; mesh.vert reads binding 0).
			specification.DescriptorSetLayouts = { view.TextureSets[0].DescriptorSet->GetDescriptorSetLayout(),
				key.TerrainSurface ? s_Data3D->TerrainSurfaceLayout : s_Data3D->MaterialLayout };
			specification.ColorFormats = key.ColorFormats;
			specification.DepthFormat = key.DepthFormat;
			specification.PushConstantSize = (uint32_t)sizeof(ObjectPushConstants);
			specification.BackfaceCulling = true;
			specification.DepthTest = true;
			specification.DepthWrite = true;
			specification.Blend = BlendMode::None;
			specification.Wireframe = key.Wireframe;
			specification.WriteIntegerAttachments = key.WriteEntityID;
			if (shadowCaster)
			{
				// Both faces cast (open shapes, leaf cards, a roof seen from below), pushed away from the sun a little.
				specification.BackfaceCulling = false;
				specification.DepthBiasConstant = ShadowCasterDepthBiasConstant;
				specification.DepthBiasSlope = ShadowCasterDepthBiasSlope;
			}
			pipeline = CreateScope<VulkanPipeline>(specification);
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer3D: the {} pipeline could not be built; it is skipped until the next shader change: {}",
				key.TerrainSurface ? "terrain surface" : "mesh", e.what());
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

		// By pipeline (pickable or not, Standard or terrain surface), then material, then front to back.
		std::sort(view.MeshDraws.begin(), view.MeshDraws.end(), [](const MeshDraw& a, const MeshDraw& b)
		{
			const bool aPickable = a.EntityID >= 0;
			const bool bPickable = b.EntityID >= 0;
			if (aPickable != bPickable)
			{
				return aPickable;
			}
			const bool aTerrain = a.TerrainSurface != nullptr;
			const bool bTerrain = b.TerrainSurface != nullptr;
			if (aTerrain != bTerrain)
			{
				return bTerrain;
			}
			if (a.TerrainSurface != b.TerrainSurface)
			{
				return a.TerrainSurface < b.TerrainSurface;
			}
			if (a.Material != b.Material)
			{
				return a.Material < b.Material;
			}
			return a.SortDepth < b.SortDepth;
		});

		// [pickable][terrain surface], built when first needed.
		const bool hasEntityID = view.Target->GetSpecification().EntityID;
		VulkanPipeline* pipelines[2][2] = { { nullptr, nullptr }, { nullptr, nullptr } };
		bool pipelinesFetched[2][2] = { { false, false }, { false, false } };
		const auto pipelineFor = [&](const MeshDraw& draw)
		{
			const int pickable = draw.EntityID >= 0 && hasEntityID ? 1 : 0;
			const int terrain = draw.TerrainSurface != nullptr ? 1 : 0;
			if (pipelinesFetched[pickable][terrain] == false)
			{
				pipelines[pickable][terrain] = GetPipeline(view, *view.Target, pickable == 1, false, terrain == 1);
				pipelinesFetched[pickable][terrain] = true;
			}
			return pipelines[pickable][terrain];
		};

		VkCommandBuffer commandBuffer = context.CommandBuffer;
		VulkanPipeline* boundPipeline = nullptr;
		VkDescriptorSet boundSet = VK_NULL_HANDLE;
		const Mesh* boundMesh = nullptr;
		for (const MeshDraw& draw : view.MeshDraws)
		{
			VulkanPipeline* pipeline = pipelineFor(draw);
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
				boundSet = VK_NULL_HANDLE;
			}
			const VkDescriptorSet set = draw.TerrainSurface != nullptr ? draw.TerrainSurface->Set.Set : draw.Material->Set.Set;
			if (set != boundSet)
			{
				pipeline->BindDescriptorSet(commandBuffer, 1, set);
				boundSet = set;
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
			if (draw.TerrainSurface != nullptr)
			{
				s_Data3D->Stats.TerrainDraws++;
			}
		}
	}

	void Renderer3D::RecordShadowCasters(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		if (view.ShadowTarget == nullptr || std::none_of(view.MeshDraws.begin(), view.MeshDraws.end(), [](const MeshDraw& draw) { return draw.CastShadows; }))
		{
			return;
		}

		VulkanPipeline* pipeline = GetPipeline(view, *view.ShadowTarget, false, true);
		if (pipeline == nullptr)
		{
			return;
		}

		// In submission order (the shadow pass runs before RecordMeshes sorts them): the map has no colour, so
		// order only costs overdraw.
		VkCommandBuffer commandBuffer = context.CommandBuffer;
		pipeline->Bind(commandBuffer, view.ShadowTarget->GetExtent());
		// Set 0 for the shadow matrix (FrameLightingUniform, written in RecordView).
		pipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[0].DescriptorSet);
		const MeshMaterial* boundMaterial = nullptr;
		const Mesh* boundMesh = nullptr;
		for (const MeshDraw& draw : view.MeshDraws)
		{
			if (draw.CastShadows == false)
			{
				continue;
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
			constants.EntityID = -1;
			pipeline->PushConstants(commandBuffer, &constants, (uint32_t)sizeof(constants));

			const SubMesh& subMesh = draw.MeshRef->GetSubMeshes()[draw.SubMesh];
			vkCmdDrawIndexed(commandBuffer, subMesh.IndexCount, 1, subMesh.FirstIndex, 0, 0);
			s_Data3D->Stats.MeshDraws++;
			s_Data3D->Stats.ShadowDraws++;
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
		std::vector<Scope<TerrainSurfaceGPU>>& surfaces = s_Data3D->TerrainSurfaces;
		surfaces.erase(std::remove_if(surfaces.begin(), surfaces.end(), [&](const Scope<TerrainSurfaceGPU>& surface) { return unused(surface->LastUsedFrame); }), surfaces.end());
		std::vector<ShapeEntry>& shapes = s_Data3D->Shapes;
		shapes.erase(std::remove_if(shapes.begin(), shapes.end(), [&](const ShapeEntry& entry) { return unused(entry.LastUsedFrame); }), shapes.end());

		s_Data3D->Stats.CachedMeshes = (uint32_t)shapes.size();
		s_Data3D->Stats.CachedMaterials = (uint32_t)(materials.size() + surfaces.size());
		s_Data3D->LastStats = s_Data3D->Stats;
		s_Data3D->Stats = {};
	}

	void Renderer3D::OnShaderReloaded(const std::string& name)
	{
		if (s_Data3D != nullptr && (name == MeshShaderName || name == MeshShadowCasterShaderName || name == TerrainSurfaceShaderName))
		{
			s_Data3D->Pipelines.clear();
		}
	}

	Renderer3DStatistics Renderer3D::GetStats()
	{
		return s_Data3D != nullptr ? s_Data3D->LastStats : Renderer3DStatistics{};
	}
}
