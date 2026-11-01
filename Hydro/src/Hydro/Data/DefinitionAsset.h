// Hydro/Data/DefinitionAsset.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Data/DataSerializer.h"

#include <filesystem>

namespace Hydro
{
	// A definition (.hydef) loaded by the AssetManager: one record of one HY_DATA type, such as a dialogue or a
	// shop. Game code reads it through Data::Get<T>(handle), never directly. Loaded again when its file changes;
	// holders of the old object keep it (DataView relies on that).
	class DefinitionAsset : public Asset
	{
	public:
		static constexpr const char* FileExtension = ".hydef";

		DefinitionAsset(DefinitionData&& data, DataReport&& fileReport);

		// Reads the file; logs what is wrong with it (once per file version: the manager caches the result).
		// nullptr when it can't be used at all (not YAML, not a definition, an unknown data type).
		static Ref<DefinitionAsset> LoadUncached(const std::filesystem::path& path);

		// Writes `data` to `file` (canonical, with the platform's line endings, like Hydro's other files). false
		// (logged) for read-only data or a file that can't be written. The editor calls AssetManager::MarkSaved after.
		static bool Save(const std::filesystem::path& file, const DefinitionData& data);
		bool Save(const std::filesystem::path& file) const { return Save(file, m_Data); }

		static AssetType GetStaticType() { return AssetType::Definition; }
		AssetType GetType() const override { return GetStaticType(); }

		// The data type of its record (never nullptr for a loaded asset).
		const DataTypeInfo* GetDataType() const { return m_Data.Values.GetType(); }
		const DefinitionData& GetData() const { return m_Data; }
		DefinitionData& GetData() { return m_Data; }

		// What reading the file found that editing the values can't change (a newer version, duplicate keys).
		// Data::Validate adds the checks of the values and their references.
		const DataReport& GetFileReport() const { return m_FileReport; }

	private:
		DefinitionData m_Data;
		DataReport m_FileReport;
	};
}
