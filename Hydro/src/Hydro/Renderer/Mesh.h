#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// One vertex of a mesh: Z-up, metres. The colour multiplies the material (white for built-in shapes).
	struct MeshVertex
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		glm::vec2 UV{ 0.0f };
		glm::vec4 Color{ 1.0f };
	};

	// A range of a mesh's indices drawn with one material (MeshComponent::Materials[MaterialSlot]).
	struct SubMesh
	{
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t MaterialSlot = 0;
		// Where an imported sub-mesh came from (M5): the glTF node (index, name) and the primitive of its
		// mesh. -1 for built-in shapes.
		int32_t SourceNode = -1;
		uint32_t SourcePrimitive = 0;
		std::string SourceName;
	};

	// A mesh on the CPU: what MeshFactory and the glTF importer (MeshImporter) make, and what Mesh uploads.
	struct MeshData
	{
		std::vector<MeshVertex> Vertices;
		std::vector<uint32_t> Indices; // triangles, counter-clockwise seen from outside
		std::vector<SubMesh> SubMeshes;
	};

	// A material slot of an imported mesh: its name in the model and the material asset (.hymat) the import
	// made for it, which a MeshComponent uses unless it names another one.
	struct MeshMaterialInfo
	{
		std::string Name;
		AssetHandle DefaultMaterial = 0;
	};

	struct MeshBuffers; // Platform/Vulkan/VulkanMeshData.h

	// A static mesh on the GPU: vertex and index buffers in GPU-only memory (uploaded once, at creation),
	// its sub-meshes and local bounds. Shared through Ref<Mesh> (several entities may draw it). Frees its
	// buffers through the deletion queue when the last Ref goes.
	//
	// Built-in shapes (MeshFactory) are made in code and have no handle. A model file (.gltf, .glb) is an
	// asset (AssetType::Mesh): the AssetManager imports it once (LoadUncached) and shares it.
	class Mesh : public Asset
	{
	public:
		Mesh(const MeshData& data, const std::string& debugName);
		~Mesh() override;

		// Imports a .gltf / .glb with the import settings in its .hymeta (MeshImporter::ImportAsset): makes its
		// textures and materials assets on the first import. nullptr (logged) when it can't be read. The
		// AssetManager's loader calls this.
		static Ref<Mesh> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Mesh; }
		AssetType GetType() const override { return GetStaticType(); }

		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;

		const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
		const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
		const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }
		uint32_t GetVertexCount() const { return m_VertexCount; }
		uint32_t GetIndexCount() const { return m_IndexCount; }

		const MeshBuffers& GetBuffers() const { return *m_Buffers; }

		// One per material slot (the largest SubMesh::MaterialSlot + 1); empty for built-in shapes.
		const std::vector<MeshMaterialInfo>& GetMaterials() const { return m_Materials; }
		void SetMaterials(const std::vector<MeshMaterialInfo>& materials) { m_Materials = materials; }
		uint32_t GetMaterialSlotCount() const;

	private:
		std::vector<MeshMaterialInfo> m_Materials;
		Scope<MeshBuffers> m_Buffers;
		std::vector<SubMesh> m_SubMeshes;
		glm::vec3 m_BoundsMin{ 0.0f };
		glm::vec3 m_BoundsMax{ 0.0f };
		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;
	};
}
