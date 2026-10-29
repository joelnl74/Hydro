#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// A .hyprefab file read into memory: the saved entities (as YAML), ready to be instantiated any number of
	// times without touching the disk. The AssetManager loads it once and shares it until the project closes
	// (AssetManager::GetAsset<Prefab>(handle)), or again when its file changes. Instantiate it with
	// PrefabSerializer::Instantiate(scene, prefab) or Vfx::Play.
	//
	// A resource: immutable after loading (saving a prefab writes a new file, which loads as a new Prefab).
	class Prefab : public Asset
	{
	public:
		Prefab(const Prefab&) = delete;
		Prefab& operator=(const Prefab&) = delete;

		// Reads a .hyprefab file, without sharing: every call makes a new Prefab. Returns nullptr (logged)
		// when the file can't be read or isn't a prefab. The AssetManager's loader calls this.
		static Ref<Prefab> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Prefab; }
		AssetType GetType() const override { return GetStaticType(); }

		// The name it was saved with (the root entity's tag).
		const std::string& GetName() const { return m_Name; }
		// The saved entities as EntitySerializer::Serialize wrote them: the root first, then its subtree in
		// depth-first order (each parent before its children). One entry for a prefab without children, and for
		// prefabs saved before hierarchies (an `Entity:` key instead of `Entities:`).
		const std::vector<YAML::Node>& GetEntityNodes() const { return m_Entities; }

	private:
		Prefab() = default;

		std::string m_Name;
		std::vector<YAML::Node> m_Entities;
	};
}
