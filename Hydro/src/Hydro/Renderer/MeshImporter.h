#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Mesh.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Hydro
{
	// A model's import settings, kept in its .hymeta (MeshImport section) next to its handle.
	struct MeshImportConfig
	{
		// Multiplies every position after the axis conversion (glTF is in metres: 1 keeps the size).
		float Scale = 1.0f;
		// glTF is Y-up; the engine is Z-up. Rotates +90 degrees about X, so the model's front faces -Y (the camera).
		bool ConvertYUpToZUp = true;
		// Flat normals for primitives that have none.
		bool GenerateNormals = true;
	};

	// What a model's first import made, kept so a re-import keeps the same assets: glTF image index -> texture
	// handle, glTF material index -> material (.hymat) handle.
	struct MeshImportMapping
	{
		std::map<uint32_t, AssetHandle> Images;
		std::map<uint32_t, AssetHandle> Materials;
	};

	// --- The model as read from the file (no GPU, no assets) ---------------------------------------------

	struct ImportedNode
	{
		std::string Name;
		glm::mat4 LocalTransform{ 1.0f };
		int32_t Parent = -1;
		std::vector<uint32_t> Children;
		int32_t MeshIndex = -1; // index into ImportedModel::Meshes; -1 for none
	};

	struct ImportedPrimitive
	{
		std::vector<glm::vec3> Positions;
		std::vector<glm::vec3> Normals; // empty when the file has none
		std::vector<glm::vec2> UVs;     // empty when the file has none
		std::vector<glm::vec4> Colors;  // empty when the file has none
		std::vector<uint32_t> Indices;  // triangles
		int32_t Material = -1;          // index into ImportedModel::Materials; -1 = the default material
	};

	struct ImportedMesh
	{
		std::string Name;
		std::vector<ImportedPrimitive> Primitives;
	};

	// One glTF image: an external file, or bytes embedded in the model (GLB buffer view, data URI) in their
	// own format (MimeType).
	struct ImportedImage
	{
		std::string Name;
		std::filesystem::path File;       // absolute; empty for embedded images
		std::vector<uint8_t> EmbeddedData;
		std::string MimeType;             // "image/png", "image/jpeg", ...; may be empty for files
	};

	// The Standard material's view of a glTF material.
	struct ImportedMaterial
	{
		std::string Name;
		glm::vec4 BaseColor{ 1.0f };   // linear, like glTF
		int32_t BaseColorImage = -1;   // index into ImportedModel::Images; -1 = none
		glm::vec2 Tiling{ 1.0f };      // KHR_texture_transform scale of the base colour texture
		glm::vec3 Emissive{ 0.0f };
		float EmissiveStrength = 1.0f; // KHR_materials_emissive_strength
		int32_t EmissiveImage = -1;
		int32_t NormalImage = -1;      // the tangent-space normal map (glTF normalTexture); -1 = none
		float NormalScale = 1.0f;      // its scale (MaterialData::NormalStrength)
		float AlphaCutoff = 0.0f;      // 0 = opaque (glTF OPAQUE); MASK uses its cutoff
		bool LinearFilter = false;     // the base colour sampler's mag filter is LINEAR
	};

	struct ImportedModel
	{
		std::vector<ImportedNode> Nodes;
		std::vector<uint32_t> RootNodes; // of the selected scene
		std::vector<ImportedMesh> Meshes;
		std::vector<ImportedMaterial> Materials;
		std::vector<ImportedImage> Images;
	};

	// Imports glTF 2.0 models (.gltf or .glb) through cgltf; editor-only. A model is one Mesh: its nodes are flattened
	// into sub-meshes with a slot per material. Nodes ending in _COL or _WALK are collision proxies, never drawn.
	class MeshImporter
	{
	public:
		// Reads the file, logging what it ignores (skins, animations, morph targets, other scenes, unsupported images).
		// False (logged) when it can't be read.
		static bool Import(const std::filesystem::path& path, ImportedModel& model);

		// One MeshData: sub-meshes in node order, in metres, Z-up, counter-clockwise, normals generated when missing.
		static MeshData Flatten(const ImportedModel& model, const MeshImportConfig& config);

		// Whether a node is a collision proxy and which kind (any case): a name ending in "_COL" or "_WALK" (Blender's
		// ".001" allowed), or starting with "COL_" (walk when "_WALK" is in it). See docs/ResourceCreation.md.
		static bool GetCollisionKind(const std::string& nodeName, MeshCollisionKind& kind);

		// The engine's axis and unit conversion for `config` (applied once, after the node transforms).
		static glm::mat4 GetImportTransform(const MeshImportConfig& config);

		// The extension an embedded image is written with (".png", ".jpg"); empty for formats the engine can't decode.
		static std::string GetImageExtension(const std::string& mimeType);

		// The AssetManager's loader: imports the file, writes embedded images and a .hymat per material the first time
		// (never overwriting a material), records the mapping and uploads the Mesh. nullptr (logged) on failure.
		static Ref<Mesh> ImportAsset(const std::filesystem::path& path);
	};
}
