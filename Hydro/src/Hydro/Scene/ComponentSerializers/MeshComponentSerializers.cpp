#include "hypch.h"
#include "ComponentSerializers.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Material.h"
#include "Hydro/Renderer/MaterialSerializer.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Terrain/Terrain.h"

#include <algorithm>

namespace Hydro
{
	static const char* MeshShapeToString(MeshShape shape)
	{
		switch (shape)
		{
		case MeshShape::Box: return "Box";
		case MeshShape::Plane: return "Plane";
		case MeshShape::Ramp: return "Ramp";
		case MeshShape::Stairs: return "Stairs";
		case MeshShape::Cylinder: return "Cylinder";
		}

		return "Box";
	}

	static MeshShape MeshShapeFromString(const std::string& name)
	{
		if (name == "Plane")
		{
			return MeshShape::Plane;
		}
		if (name == "Ramp")
		{
			return MeshShape::Ramp;
		}
		if (name == "Stairs")
		{
			return MeshShape::Stairs;
		}
		if (name == "Cylinder")
		{
			return MeshShape::Cylinder;
		}

		return MeshShape::Box;
	}

	void ComponentSerializers::SaveMesh(YAML::Emitter& out, const MeshComponent& mesh)
	{
		if ((uint64_t)mesh.Model != 0)
		{
			out << YAML::Key << "ModelHandle" << YAML::Value << (uint64_t)mesh.Model;
		}
		out << YAML::Key << "Shape" << YAML::Value << MeshShapeToString(mesh.Shape);
		out << YAML::Key << "Size" << YAML::Value << mesh.Size;
		if (mesh.Shape == MeshShape::Stairs)
		{
			out << YAML::Key << "Steps" << YAML::Value << mesh.Steps;
		}
		if (mesh.Shape == MeshShape::Cylinder)
		{
			out << YAML::Key << "Segments" << YAML::Value << mesh.Segments;
		}
		out << YAML::Key << "CastShadows" << YAML::Value << mesh.CastShadows;
		if (mesh.Opacity < 1.0f)
		{
			out << YAML::Key << "Opacity" << YAML::Value << mesh.Opacity;
		}
		out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
		// Each slot: its material asset's handle (when it has one) and its inline settings, the fallback.
		for (const MeshMaterialSlot& slot : mesh.Materials)
		{
			out << YAML::BeginMap;
			if ((uint64_t)slot.Material != 0)
			{
				out << YAML::Key << "MaterialHandle" << YAML::Value << (uint64_t)slot.Material;
			}
			out << YAML::Key << "Inline" << YAML::Value;
			MaterialSerializer::SerializeData(out, slot.Inline);
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	void ComponentSerializers::LoadMesh(const YAML::Node& meshNode, MeshComponent& mesh)
	{
		mesh.Model = AssetHandle(meshNode["ModelHandle"].as<uint64_t>(0));
		if ((uint64_t)mesh.Model != 0)
		{
			// Imported now (its materials and textures too), so the renderer, which never loads, finds it.
			AssetManager::GetAsset<Mesh>(mesh.Model);
		}
		mesh.Shape = MeshShapeFromString(meshNode["Shape"].as<std::string>("Box"));
		mesh.Size = meshNode["Size"].as<glm::vec3>(mesh.Size);
		mesh.Steps = meshNode["Steps"].as<uint32_t>(mesh.Steps);
		mesh.Segments = meshNode["Segments"].as<uint32_t>(mesh.Segments);
		mesh.CastShadows = meshNode["CastShadows"].as<bool>(mesh.CastShadows);
		mesh.Opacity = std::clamp(meshNode["Opacity"].as<float>(1.0f), 0.0f, 1.0f);
		if (auto materialsNode = meshNode["Materials"]; materialsNode && materialsNode.IsSequence())
		{
			mesh.Materials.clear();
			for (const YAML::Node& materialNode : materialsNode)
			{
				MeshMaterialSlot slot;
				slot.Material = AssetHandle(materialNode["MaterialHandle"].as<uint64_t>(0));
				// Older scenes keep the settings in the slot itself.
				const YAML::Node inlineNode = materialNode["Inline"];
				slot.Inline = MaterialSerializer::DeserializeData(inlineNode.IsDefined() ? inlineNode : materialNode);
				// Loaded now (with its textures), so the renderer, which never loads, finds it.
				if ((uint64_t)slot.Material != 0)
				{
					AssetManager::GetAsset<MaterialAsset>(slot.Material);
				}
				mesh.Materials.push_back(slot);
			}
		}
	}

	void ComponentSerializers::SaveTerrain(YAML::Emitter& out, const TerrainComponent& terrain)
	{
		out << YAML::Key << "TerrainHandle" << YAML::Value << (uint64_t)terrain.Terrain;
		out << YAML::Key << "CastShadows" << YAML::Value << terrain.CastShadows;
		out << YAML::Key << "Layer" << YAML::Value << PhysicsLayerToString(terrain.Layer);
	}

	void ComponentSerializers::LoadTerrain(const YAML::Node& terrainNode, TerrainComponent& terrain)
	{
		terrain.Terrain = AssetHandle(terrainNode["TerrainHandle"].as<uint64_t>(0));
		terrain.CastShadows = terrainNode["CastShadows"].as<bool>(terrain.CastShadows);
		terrain.Layer = PhysicsLayerFromString(terrainNode["Layer"].as<std::string>("World"), terrain.Layer);
		// Loaded now, with every material (and texture) and decor sprite it names, so the first frame draws it all.
		const Ref<Terrain> asset = (uint64_t)terrain.Terrain != 0 ? AssetManager::GetAsset<Terrain>(terrain.Terrain) : nullptr;
		if (asset != nullptr)
		{
			const TerrainSettings& settings = asset->GetSettings();
			auto loadMaterial = [](AssetHandle handle)
			{
				if ((uint64_t)handle != 0)
				{
					AssetManager::GetAsset<MaterialAsset>(handle);
				}
			};
			for (const TerrainLayer& layer : settings.Layers)
			{
				loadMaterial(layer.Material);
				loadMaterial(layer.Fringe);
			}
			for (const TerrainWallLayer& wall : settings.WallLayers)
			{
				loadMaterial(wall.Material);
			}
			loadMaterial(settings.CapStoneMaterial);
			for (const TerrainDecorRule& rule : settings.Decor)
			{
				for (AssetHandle sprite : rule.Sprites)
				{
					if ((uint64_t)sprite != 0)
					{
						AssetManager::GetAsset<Texture2D>(sprite);
					}
				}
			}
		}
	}
}
