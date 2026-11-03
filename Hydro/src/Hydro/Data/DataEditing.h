// Hydro/Data/DataEditing.h
#pragma once

#include "Hydro/Data/DataSerializer.h"
#include "Hydro/Data/DataType.h"

#include <cstddef>
#include <string>

namespace Hydro
{
	// The edits an editor makes to data in memory, here so they behave the same everywhere and can be tested. Nothing
	// here writes files; the caller saves.
	class DataEditing
	{
	public:
		// ---- Lists (a std::vector field) ----------------------------------------------------------------
		// Struct entries' kept values ("Lines[2]") move with their entry; the field's own unreadable value is dropped.
		static void InsertListEntry(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName,
			size_t index);
		static void RemoveListEntry(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName,
			size_t index);
		static void SwapListEntries(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName,
			size_t first, size_t second);

		// ---- Table rows ---------------------------------------------------------------------------------
		// A row with the type's defaults under a free ID ("NewItem", "NewItem2", ...). Returns its ID.
		static std::string AddRow(DataTableData& table);
		// A copy of row `id` (values and kept values) under a free ID ("PotionCopy", "PotionCopy2"). Returns its
		// ID; empty when there is no such row.
		static std::string DuplicateRow(DataTableData& table, const std::string& id);
		static bool RemoveRow(DataTableData& table, const std::string& id);
		// A new ID for row `id`; references to the old one then show as missing. False, with why in `problem`, when
		// `newId` is invalid or taken.
		static bool ChangeRowId(DataTableData& table, const std::string& id, const std::string& newId, std::string& problem);
		// Why `newId` can't be the ID of row `currentId` (empty: it can). `currentId` may be empty for a new row.
		static std::string CheckRowId(const DataTableData& table, const std::string& currentId, const std::string& newId);
		// `base` if it is free, else `base` followed by the first free number from 2.
		static std::string MakeFreeRowId(const DataTableData& table, const std::string& base);

		// The row's display field (an item's Name) when set, else its ID.
		static std::string GetRowLabel(const DataRecord& record, const std::string& id);
	};
}
