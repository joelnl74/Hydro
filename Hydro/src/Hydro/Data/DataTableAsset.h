// Hydro/Data/DataTableAsset.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Data/DataSerializer.h"

#include <filesystem>
#include <string>

namespace Hydro
{
	// A data table (.hytable): rows of one HY_DATA type by ID, read through Data::GetTable / GetRow. Reloaded when its
	// file changes; holders of the old object keep it.
	class DataTableAsset : public Asset
	{
	public:
		static constexpr const char* FileExtension = ".hytable";

		DataTableAsset(DataTableData&& data, DataReport&& fileReport);

		// Reads the file and logs its problems (once per version: the manager caches it). Null when unusable.
		static Ref<DataTableAsset> LoadUncached(const std::filesystem::path& path);

		// Writes `data` canonically. False (logged) for read-only data or an unwritable file. The editor then calls
		// AssetManager::MarkSaved.
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
