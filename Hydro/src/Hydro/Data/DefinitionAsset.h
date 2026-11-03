// Hydro/Data/DefinitionAsset.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Data/DataSerializer.h"

#include <filesystem>

namespace Hydro
{
	// A definition (.hydef): one record of one HY_DATA type, read through Data::Get<T>. Reloaded when its file changes;
	// holders of the old object keep it.
	class DefinitionAsset : public Asset
	{
	public:
		static constexpr const char* FileExtension = ".hydef";

		DefinitionAsset(DefinitionData&& data, DataReport&& fileReport);

		// Reads the file and logs its problems (once per version: the manager caches it). Null when unusable.
		static Ref<DefinitionAsset> LoadUncached(const std::filesystem::path& path);

		// Writes `data` canonically. False (logged) for read-only data or an unwritable file. The editor then calls
		// AssetManager::MarkSaved.
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
