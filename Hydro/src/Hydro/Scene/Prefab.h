#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// A .hyprefab file in memory, ready to instantiate without touching the disk; the AssetManager shares it and
	// reloads it when its file changes. Immutable after loading.
	class Prefab : public Asset
	{
	public:
		Prefab(const Prefab&) = delete;
		Prefab& operator=(const Prefab&) = delete;

		// Reads a .hyprefab into a new, unshared Prefab; null (logged) on failure. The asset loader uses it.
		static Ref<Prefab> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Prefab; }
		AssetType GetType() const override { return GetStaticType(); }

		// The name it was saved with (the root entity's tag).
		const std::string& GetName() const { return m_Name; }
		// The saved entities as EntitySerializer::Serialize wrote them, root first, depth-first. One entry for a prefab
		// without children.
		const std::vector<YAML::Node>& GetEntityNodes() const { return m_Entities; }

	private:
		Prefab() = default;

		std::string m_Name;
		std::vector<YAML::Node> m_Entities;
	};
}
