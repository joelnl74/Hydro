#include "hypch.h"
#include "DataEditing.h"

#include <cctype>
#include <map>

namespace Hydro
{
	// "Lines[3]" -> 3 for `fieldName` "Lines"; false for keys of other fields.
	static bool ParseEntryKey(const std::string& key, const std::string& fieldName, size_t& index)
	{
		if (key.size() < fieldName.size() + 3 || key.compare(0, fieldName.size(), fieldName) != 0 || key[fieldName.size()] != '['
			|| key.back() != ']')
		{
			return false;
		}
		const std::string digits = key.substr(fieldName.size() + 1, key.size() - fieldName.size() - 2);
		if (digits.empty())
		{
			return false;
		}
		for (char c : digits)
		{
			if (std::isdigit((unsigned char)c) == 0)
			{
				return false;
			}
		}
		index = (size_t)std::stoull(digits);
		return true;
	}

	static std::string EntryKey(const std::string& fieldName, size_t index)
	{
		return fieldName + "[" + std::to_string(index) + "]";
	}

	// Renumbers the kept values of `fieldName`'s entries: `map(index)` gives each entry's new index, or false to drop it.
	template<typename MapIndex>
	static void RenumberEntries(DataKeptValues& kept, const std::string& fieldName, MapIndex map)
	{
		std::map<std::string, DataKeptValues> renumbered;
		for (auto it = kept.Nested.begin(); it != kept.Nested.end();)
		{
			size_t index = 0;
			if (ParseEntryKey(it->first, fieldName, index) == false)
			{
				++it;
				continue;
			}
			size_t moved = 0;
			if (map(index, moved))
			{
				renumbered.emplace(EntryKey(fieldName, moved), std::move(it->second));
			}
			it = kept.Nested.erase(it);
		}
		for (auto& [key, value] : renumbered)
		{
			kept.Nested.emplace(key, std::move(value));
		}
	}

	void DataEditing::InsertListEntry(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName,
		size_t index)
	{
		HY_CORE_ASSERT(listType.Kind == DataFieldKind::List, "InsertListEntry: not a list");
		index = std::min(index, listType.List->Size(list));
		listType.List->Insert(list, index);
		kept.ForgetUnreadable(fieldName);
		RenumberEntries(kept, fieldName, [index](size_t old, size_t& moved)
		{
			moved = old >= index ? old + 1 : old;
			return true;
		});
	}

	void DataEditing::RemoveListEntry(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName,
		size_t index)
	{
		HY_CORE_ASSERT(listType.Kind == DataFieldKind::List, "RemoveListEntry: not a list");
		if (index >= listType.List->Size(list))
		{
			return;
		}
		listType.List->Erase(list, index);
		kept.ForgetUnreadable(fieldName);
		RenumberEntries(kept, fieldName, [index](size_t old, size_t& moved)
		{
			moved = old > index ? old - 1 : old;
			return old != index;
		});
	}

	void DataEditing::SwapListEntries(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName,
		size_t first, size_t second)
	{
		HY_CORE_ASSERT(listType.Kind == DataFieldKind::List, "SwapListEntries: not a list");
		const size_t size = listType.List->Size(list);
		if (first >= size || second >= size || first == second)
		{
			return;
		}
		listType.List->Swap(list, first, second);
		kept.ForgetUnreadable(fieldName);
		RenumberEntries(kept, fieldName, [first, second](size_t old, size_t& moved)
		{
			moved = old == first ? second : (old == second ? first : old);
			return true;
		});
	}

	std::string DataEditing::MakeFreeRowId(const DataTableData& table, const std::string& base)
	{
		if (table.Rows.find(base) == table.Rows.end())
		{
			return base;
		}
		for (int number = 2;; number++)
		{
			const std::string id = base + std::to_string(number);
			if (table.Rows.find(id) == table.Rows.end())
			{
				return id;
			}
		}
	}

	std::string DataEditing::AddRow(DataTableData& table)
	{
		HY_CORE_ASSERT(table.RowType != nullptr, "AddRow: the table has no row type");

		// "New" + the type's display name without what an ID can't hold: "NewItem" for ItemDefinition.
		std::string base = "New";
		for (char c : table.RowType->DisplayName)
		{
			if (std::isalnum((unsigned char)c) != 0 || c == '_')
			{
				base += c;
			}
		}
		const std::string id = MakeFreeRowId(table, base);
		table.Rows.emplace(id, DataRecord(*table.RowType));
		return id;
	}

	std::string DataEditing::DuplicateRow(DataTableData& table, const std::string& id)
	{
		auto found = table.Rows.find(id);
		if (found == table.Rows.end())
		{
			return {};
		}
		const std::string copyId = MakeFreeRowId(table, id + "Copy");
		DataRecord copy = found->second;
		table.Rows.emplace(copyId, std::move(copy));
		return copyId;
	}

	bool DataEditing::RemoveRow(DataTableData& table, const std::string& id)
	{
		return table.Rows.erase(id) > 0;
	}

	std::string DataEditing::CheckRowId(const DataTableData& table, const std::string& currentId, const std::string& newId)
	{
		if (newId.empty())
		{
			return "An ID can't be empty.";
		}
		if (DataSerializer::IsValidRowId(newId) == false)
		{
			return "Letters, digits and '_' only, not starting with a digit (PascalCase: IronSword).";
		}
		if (newId != currentId && table.Rows.find(newId) != table.Rows.end())
		{
			return "Another row has this ID.";
		}
		return {};
	}

	bool DataEditing::ChangeRowId(DataTableData& table, const std::string& id, const std::string& newId, std::string& problem)
	{
		auto found = table.Rows.find(id);
		if (found == table.Rows.end())
		{
			problem = "There is no row '" + id + "'.";
			return false;
		}
		problem = CheckRowId(table, id, newId);
		if (problem.empty() == false)
		{
			return false;
		}
		if (newId == id)
		{
			return true;
		}
		DataRecord record = std::move(found->second);
		table.Rows.erase(found);
		table.Rows.emplace(newId, std::move(record));
		return true;
	}

	std::string DataEditing::GetRowLabel(const DataRecord& record, const std::string& id)
	{
		const DataTypeInfo* type = record.GetType();
		if (type == nullptr || type->DisplayField.empty())
		{
			return id;
		}
		const DataField* field = type->FindField(type->DisplayField);
		if (field == nullptr || field->Type.Kind != DataFieldKind::String)
		{
			return id;
		}
		const std::string& name = *static_cast<const std::string*>(field->Get(record.Get()));
		return name.empty() ? id : name;
	}
}
