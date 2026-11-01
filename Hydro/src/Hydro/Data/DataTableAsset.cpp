#include "hypch.h"
#include "DataTableAsset.h"

#include <fstream>
#include <iterator>

namespace Hydro
{
	DataTableAsset::DataTableAsset(DataTableData&& data, DataReport&& fileReport)
		: m_Data(std::move(data)), m_FileReport(std::move(fileReport))
	{
	}

	Ref<DataTableAsset> DataTableAsset::LoadUncached(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (in.is_open() == false)
		{
			HY_CORE_ERROR("DataTableAsset: can't open '{}'", path.string());
			return nullptr;
		}
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		const std::string source = path.filename().string();

		DataTableData data;
		DataReport fileReport;
		if (DataSerializer::ReadTable(text, data, fileReport, false) == false)
		{
			fileReport.Log(source);
			return nullptr;
		}

		// The rows' own problems are logged too, but not kept: they change as the rows are edited
		// (Data::Validate checks them again).
		DataReport rows;
		for (const auto& [id, record] : data.Rows)
		{
			DataSerializer::CheckRecord(record, id, true, rows);
		}
		fileReport.Log(source);
		rows.Log(source);

		return CreateRef<DataTableAsset>(std::move(data), std::move(fileReport));
	}

	const DataRecord* DataTableAsset::FindRow(const std::string& id) const
	{
		auto it = m_Data.Rows.find(id);
		return it == m_Data.Rows.end() ? nullptr : &it->second;
	}

	bool DataTableAsset::Save(const std::filesystem::path& file, const DataTableData& data)
	{
		std::string text;
		if (DataSerializer::WriteTable(data, text) == false)
		{
			HY_CORE_ERROR("DataTableAsset: '{}' is read-only (newer than this build, or broken: see its messages); not saved", file.filename().string());
			return false;
		}
		std::ofstream out(file, std::ios::trunc);
		if (out.is_open() == false)
		{
			HY_CORE_ERROR("DataTableAsset: can't write '{}'", file.string());
			return false;
		}
		out << text;
		return out.good();
	}
}
