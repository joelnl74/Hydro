// Hydro/Data/Data.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Assert.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Data/DataRecord.h"
#include "Hydro/Data/DataReport.h"
#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Data/DataType.h"

#include <cstddef>
#include <map>
#include <string>
#include <typeindex>
#include <utility>

namespace Hydro
{
	class DefinitionAsset;
	class DataTableAsset;

	// One record of data: a definition's values (Data::Get) or one table row (Data::GetRow). Read-only: data is
	// content; the game's progress (counts, flags, "talked to") is game state and lives elsewhere.
	//
	// A DataView keeps its source asset alive, so it stays safe to read when the asset is reloaded.
	// A view kept across a reload still shows the old data: store the AssetHandle or DataRowRef for
	// anything long-lived, and look it up again when you need the current data.
	template<typename T>
	class DataView
	{
	public:
		DataView() = default;
		DataView(Ref<Asset> owner, const T* value)
			: m_Owner(std::move(owner)), m_Value(value)
		{
		}

		// false when there was nothing to find (handle 0, an empty row reference) or the asset, table or row is
		// missing or of another type (warned once).
		bool IsValid() const { return m_Value != nullptr; }
		const T* Get() const { return m_Value; }

		const T& operator*() const
		{
			HY_CORE_ASSERT(m_Value != nullptr, "DataView: check IsValid() before reading");
			return *m_Value;
		}

		const T* operator->() const
		{
			HY_CORE_ASSERT(m_Value != nullptr, "DataView: check IsValid() before reading");
			return m_Value;
		}

	private:
		Ref<Asset> m_Owner;
		const T* m_Value = nullptr;
	};

	// The rows of a data table, by row ID (Data::GetTable). Row order means nothing to gameplay: rows come sorted
	// by ID; where order matters (a shop's stock), store it as a list of DataRowRef.
	//
	// A DataTableView keeps its source asset alive, so it stays safe to read when the asset is reloaded.
	// A view kept across a reload still shows the old data: store the AssetHandle or DataRowRef for
	// anything long-lived, and look it up again when you need the current data.
	template<typename T>
	class DataTableView
	{
	public:
		using RowMap = std::map<std::string, DataRecord>;

		class Iterator
		{
		public:
			explicit Iterator(typename RowMap::const_iterator it)
				: m_It(it)
			{
			}

			// for (const auto& [id, row] : table.Rows())
			std::pair<const std::string&, const T&> operator*() const
			{
				return { m_It->first, *m_It->second.template As<T>() };
			}

			Iterator& operator++()
			{
				++m_It;
				return *this;
			}

			bool operator==(const Iterator& other) const { return m_It == other.m_It; }
			bool operator!=(const Iterator& other) const { return m_It != other.m_It; }

		private:
			typename RowMap::const_iterator m_It;
		};

		class RowRange
		{
		public:
			explicit RowRange(const RowMap* rows)
				: m_Rows(rows)
			{
			}

			Iterator begin() const { return Iterator(m_Rows != nullptr ? m_Rows->begin() : s_Empty.begin()); }
			Iterator end() const { return Iterator(m_Rows != nullptr ? m_Rows->end() : s_Empty.end()); }

		private:
			const RowMap* m_Rows = nullptr;
			static inline const RowMap s_Empty;
		};

		DataTableView() = default;
		DataTableView(Ref<Asset> owner, const RowMap* rows)
			: m_Owner(std::move(owner)), m_Rows(rows)
		{
		}

		// false when the handle is 0, or the table is missing or of another row type (warned once).
		bool IsValid() const { return m_Rows != nullptr; }
		AssetHandle GetHandle() const { return m_Owner != nullptr ? m_Owner->GetHandle() : AssetHandle(0); }
		size_t GetRowCount() const { return m_Rows != nullptr ? m_Rows->size() : 0; }

		// nullptr when there is no row with that ID.
		const T* FindRow(const std::string& id) const
		{
			if (m_Rows == nullptr)
			{
				return nullptr;
			}
			auto it = m_Rows->find(id);
			return it == m_Rows->end() ? nullptr : it->second.template As<T>();
		}

		// A reference to one of its rows, to store (inventories, saves).
		DataRowRef MakeRef(const std::string& id) const { return DataRowRef{ GetHandle(), id }; }

		RowRange Rows() const { return RowRange(m_Rows); }

	private:
		Ref<Asset> m_Owner;
		const RowMap* m_Rows = nullptr;
	};

	// Game data: definitions (.hydef) and data tables (.hytable) of HY_DATA types, by handle. Scripts keep the
	// AssetHandle or DataRowRef (an HY_PROPERTY with Asset(Definition, T), Asset(DataTable, T) or DataRow(T)) and
	// ask here when they need the data; never a view between frames. Synchronous: it goes through
	// AssetManager::GetAsset, which reads the file again when it changed, so an edit shows on the next call.
	//
	//     HY_PROPERTY(Asset(Definition, DialogueDefinition))
	//     AssetHandle Dialogue = 0;
	//
	//     const DataView<DialogueDefinition> dialogue = Data::Get<DialogueDefinition>(Dialogue);
	//     if (dialogue.IsValid())
	//     {
	//         Open(*dialogue);
	//     }
	class Data
	{
	public:
		template<typename T>
		static DataView<T> Get(AssetHandle definition)
		{
			Ref<Asset> owner;
			const void* value = FindDefinition(definition, typeid(T), owner);
			return DataView<T>(std::move(owner), static_cast<const T*>(value));
		}

		template<typename T>
		static DataView<T> GetRow(const DataRowRef& row)
		{
			Ref<Asset> owner;
			const void* value = FindRow(row, typeid(T), owner);
			return DataView<T>(std::move(owner), static_cast<const T*>(value));
		}

		template<typename T>
		static DataTableView<T> GetTable(AssetHandle table)
		{
			Ref<Asset> owner;
			const std::map<std::string, DataRecord>* rows = FindTable(table, typeid(T), owner);
			return DataTableView<T>(std::move(owner), rows);
		}

		// Untyped, for the editor and tools: nullptr when the handle isn't a definition / table that loads.
		static Ref<DefinitionAsset> GetDefinitionAsset(AssetHandle handle);
		static Ref<DataTableAsset> GetTableAsset(AssetHandle handle);
		// The data type a .hydef or .hytable holds; nullptr for other assets and files that can't be read.
		static const DataTypeInfo* GetDataType(AssetHandle handle);

		// Every problem of a definition or table: the file's own (a newer version, duplicates), its values' (kept
		// values, values outside their Range, empty display fields) and its references (assets and rows that are
		// missing or of the wrong type). Loads what it refers to.
		static DataReport Validate(AssetHandle handle);

	private:
		static const void* FindDefinition(AssetHandle handle, std::type_index cppType, Ref<Asset>& owner);
		static const void* FindRow(const DataRowRef& row, std::type_index cppType, Ref<Asset>& owner);
		static const std::map<std::string, DataRecord>* FindTable(AssetHandle handle, std::type_index cppType, Ref<Asset>& owner);
	};
}
