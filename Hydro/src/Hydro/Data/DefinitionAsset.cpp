#include "hypch.h"
#include "DefinitionAsset.h"

#include "Hydro/Project/Project.h"

#include <fstream>
#include <iterator>

namespace Hydro
{
	DefinitionAsset::DefinitionAsset(DefinitionData&& data, DataReport&& fileReport)
		: m_Data(std::move(data)), m_FileReport(std::move(fileReport))
	{
	}

	Ref<DefinitionAsset> DefinitionAsset::LoadUncached(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (in.is_open() == false)
		{
			HY_CORE_ERROR("DefinitionAsset: can't open '{}'", path.string());
			return nullptr;
		}
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		const std::string source = path.filename().string();

		DefinitionData data;
		DataReport fileReport;
		if (DataSerializer::ReadDefinition(text, data, fileReport, false) == false)
		{
			fileReport.Log(source);
			return nullptr;
		}

		// The values' own problems are logged too, but not kept: they change as the values are edited
		// (Data::Validate checks them again).
		DataReport values;
		DataSerializer::CheckRecord(data.Values, "", false, values);
		fileReport.Log(source);
		values.Log(source);

		return CreateRef<DefinitionAsset>(std::move(data), std::move(fileReport));
	}

	bool DefinitionAsset::Save(const std::filesystem::path& file, const DefinitionData& data)
	{
		if (Project::RefusesWrite("the definition", file))
		{
			return false;
		}

		std::string text;
		if (DataSerializer::WriteDefinition(data, text) == false)
		{
			HY_CORE_ERROR("DefinitionAsset: '{}' is read-only (newer than this build, or broken: see its messages); not saved",
				file.filename().string());
			return false;
		}
		std::ofstream out(file, std::ios::trunc);
		if (out.is_open() == false)
		{
			HY_CORE_ERROR("DefinitionAsset: can't write '{}'", file.string());
			return false;
		}
		out << text;
		// Data types are the game's: a generic project that saves one becomes that game's.
		Project::OnGameContentSaved();
		return out.good();
	}
}
