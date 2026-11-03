#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Data/DataReport.h>

#include <filesystem>
#include <string>

namespace Hydro
{
	// A selected .hydef or .hytable: its validation line and values, edited in the loaded asset and written once no field
	// is active.
	class DataAssetInspector
	{
	public:
		// A definition's validation line and values.
		void DrawDefinition(const std::filesystem::path& path);
		// A table's validation line, its rows and the selected row's values.
		void DrawTable(const std::filesystem::path& path);
		// Writes the definition or table edited last once no field is active (`force`: now).
		void Save(bool force);

	private:
		// Checked again after an edit and once a second, since it reads other files.
		const DataReport& GetValidation(AssetHandle handle);
		// Marks the definition or table to be saved once no field is active.
		void MarkEdited(AssetHandle handle);

	private:
		// Changed in memory and not written yet (0: none).
		AssetHandle m_UnsavedData = 0;
		// The selected asset's validation, kept between frames.
		AssetHandle m_ValidatedData = 0;
		DataReport m_Validation;
		double m_ValidationTime = -1.0;
		// The table shown, its selected row, the row search and the row popups' state.
		AssetHandle m_RowTable = 0;
		std::string m_SelectedRow;
		char m_RowSearch[64] = {};
		char m_RowIdBuffer[64] = {};
		std::string m_RowToDelete;
	};
}
