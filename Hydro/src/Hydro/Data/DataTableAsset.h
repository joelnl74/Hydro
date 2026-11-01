// Hydro/Data/DataTableAsset.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Data/DataSerializer.h"

#include <filesystem>
#include <string>

namespace Hydro
{
	// A data table (.hytable) loaded by the AssetManager: rows of one HY_DATA type by row ID, such as the game's
	// items. Game code reads it through Data::GetTable<T>(handle) and Data::GetRow<T>(ref). Loaded again when its
	// file changes; holders of the old object keep it (DataView relies on that).
	class DataTableAsset : public Asset
	{
	public:
		static constexpr const char* FileExtension = ".hytable";

		DataTableAsset(DataTableData&& data, DataReport&& fileReport);

		// Reads the file; logs what is wrong with it (once per file version: the manager caches the result).
		// nullptr when it can't be used at all (not YAML, not a table, an unknown data type).
		static Ref<DataTableAsset> LoadUncached(const std::filesystem::path& path);

		// Writes `data` to `file` (canonical, with the platform's line endings, like Hydro's other files). false
		// (logged) for read-only data or a file that can't be written. The editor calls AssetManager::MarkSaved after.
		static bool Save(const std::filesystem::path& file, const DataTableData& data);
		bool Save(const std::filesystem::path& file) const { return Save(file, m_Data); }

		static AssetType GetStaticType() { return AssetType::DataTable; }
		AssetType GetType() const override { return GetStaticType(); }

		// The data type of its rows (never nullptr for a loaded asset).
		const DataTypeInfo* GetRowType() const { return m_Data.RowType; }
		// nullptr when there is no row with that ID.
		const DataRecord* FindRow(const std::string& id) const;

		const DataTableData& GetData() const { return m_Data; }
		DataTableData& GetData() { return m_Data; }

		// What reading the file found that editing the rows can't change (a newer version, duplicate row IDs).
		const DataReport& GetFileReport() const { return m_FileReport; }

	private:
		DataTableData m_Data;
		DataReport m_FileReport;
	};
}
