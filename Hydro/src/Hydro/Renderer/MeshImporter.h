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
		// glTF is Y-up; the engine is Z-up with the ground on XY. Rotates +90 degrees about X, so the
		// model's front (+Z in glTF) faces -Y, towards the diorama camera.
		bool ConvertYUpToZUp = true;
		// Flat normals for primitives that have none.
		bool GenerateNormals = true;
	};

	// What a model's first import made, kept in the .hymeta so a re-import (or a changed file) keeps the
	// same assets: glTF image index -> texture handle, glTF material index -> material (.hymat) handle.
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

	// Imports glTF 2.0 models (.gltf with its .bin and images, or .glb) through cgltf. Editor-only: the
	// runtime will read a cooked mesh format (RT8).
	//
	// One model file = one Mesh asset = one MeshComponent: the selected scene's nodes are flattened into one
	// Mesh, every (node, primitive) pair a sub-mesh with the node's world transform baked in and a material
	// slot per glTF material (plus one default slot for primitives without a material). Nodes whose name ends in
	// _COL or _WALK are collision proxies instead (MeshCollisionProxy, World collision plan WC3): kept for the
	// physics world, never drawn, not in the bounds. The ImportedModel
	// keeps the node tree, so "import as hierarchy" can be added later.
	class MeshImporter
	{
	public:
		// Reads the file. Logs what it ignores (skins, animations, morph targets, other scenes, BLEND,
		// metallic-roughness, unsupported image formats). False (logged) when it can't be read.
		static bool Import(const std::filesystem::path& path, ImportedModel& model);

		// One MeshData from the model: sub-meshes in node order, positions in metres, Z-up, counter-clockwise
		// (a node with a mirroring transform has its winding flipped), normals generated when missing.
		static MeshData Flatten(const ImportedModel& model, const MeshImportConfig& config);

		// Whether a glTF node is a collision proxy, and which kind (any case):
		//  - its name ends in "_COL" or "_WALK", optionally followed by Blender's ".001" numbering
		//    ("Torii_Pillar_L_COL", "Bridge_WALK.001");
		//  - or it starts with "COL_" (the medieval village kit's proxies): walk when "_WALK" is in its name
		//    ("COL_MV_Bridge_Stone_A_WALK_Deck"), else convex ("COL_MV_Bridge_Stone_A_COL_RailS").
		static bool GetCollisionKind(const std::string& nodeName, MeshCollisionKind& kind);

		// The engine's axis and unit conversion for `config` (applied once, after the node transforms).
		static glm::mat4 GetImportTransform(const MeshImportConfig& config);

		// The file extension an embedded image is written with, from its MIME type (".png", ".jpg"); empty
		// for formats the engine can't decode.
		static std::string GetImageExtension(const std::string& mimeType);

		// The AssetManager's loader: reads the .hymeta (settings and mapping), imports the file, writes its
		// embedded images to "<model>.textures/" and a .hymat per material next to the model the first time
		// (later imports reuse the mapped handles and never overwrite a material), records the mapping, and
		// uploads the Mesh with each slot's material. nullptr (logged) on failure.
		static Ref<Mesh> ImportAsset(const std::filesystem::path& path);
	};
}
