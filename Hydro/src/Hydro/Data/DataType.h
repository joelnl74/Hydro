// Hydro/Data/DataType.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Data/DataRowRef.h"

#include <glm/glm.hpp>

#include <cfloat>
#include <cstdint>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace Hydro
{
	// The kinds of value a data field can hold (an HY_PROPERTY inside an HY_DATA struct). The first eight are the
	// kinds scripts have (ScriptPropertyType); data adds enums, nested structs, lists and row references.
	enum class DataFieldKind : uint8_t
	{
		Bool,
		Int,
		Float,
		Vec2,
		Vec3,
		Vec4,
		String,
		Asset,  // an AssetHandle (0 = none); DataField::AssetFilter says which kind of asset
		Enum,   // an HY_ENUM enum class, saved by the enumerator's name
		Struct, // an HY_DATA struct, saved as a nested map
		List,   // a std::vector of any other kind (not of lists), saved as a sequence
		RowRef  // a DataRowRef; DataField::DataTypeFilter names the table's row type
	};

	const char* DataFieldKindToString(DataFieldKind kind);

	struct DataListOps;

	// What a value is, without its name: its kind and, for the kinds that need it, how to reach the C++ type
	// behind it. Built at compile time by MakeDataValueType<T>(); nothing here is written by hand.
	struct DataValueType
	{
		DataFieldKind Kind = DataFieldKind::Int;

		// Enum: the enum; Struct: the struct; List: the std::vector. Enum and Struct are looked up in the
		// DataTypeRegistry with it (DataTypeRegistry::FindEnum / Find).
		std::type_index CppType = typeid(void);

		// Enum: the value as a number, whatever the enum's underlying type is.
		int64_t (*ReadEnum)(const void* value) = nullptr;
		void (*WriteEnum)(void* value, int64_t number) = nullptr;

		// List: the element type and how to change the vector.
		const DataListOps* List = nullptr;
	};

	// How to read and change a std::vector<T> field without knowing T.
	struct DataListOps
	{
		DataValueType Element;

		size_t (*Size)(const void* list) = nullptr;
		void* (*At)(void* list, size_t index) = nullptr;
		const void* (*AtConst)(const void* list, size_t index) = nullptr;
		void (*Resize)(void* list, size_t size) = nullptr;
		void (*Insert)(void* list, size_t index) = nullptr; // a default value before `index` (index == size appends)
		void (*Erase)(void* list, size_t index) = nullptr;
		void (*Swap)(void* list, size_t first, size_t second) = nullptr;
		void (*Assign)(void* destination, const void* source) = nullptr; // destination = source (the whole list)
	};

	// One HY_PROPERTY field of a data type: what the serializer reads and writes, and what the inspector shows.
	struct DataField
	{
		// The saved key: the field's name. Renaming it in C++ leaves the old key in files as an unknown (kept) key.
		std::string Name;
		// The inspector's label: the name split at capitals, or DisplayName("...").
		std::string DisplayName;

		DataValueType Type;

		// HY_PROPERTY(...) arguments. For a List they apply to its elements.
		std::string Tooltip;
		float Min = -FLT_MAX; // Int, Float and each vector component: validation warns outside it, the inspector clamps
		float Max = FLT_MAX;
		bool IsColor = false;                    // Vec3 / Vec4 edited as a colour
		AssetType AssetFilter = AssetType::None; // Asset: which assets the picker lists
		std::string DataTypeFilter;              // RowRef: the row type of the tables its picker lists ("ItemDefinition")
		bool ReadOnly = false;                   // shown, not editable
		bool HideInInspector = false;            // saved, not shown

		// The field inside an instance of its type (generated: &instance.*member).
		void* (*Access)(void* object) = nullptr;

		void* Get(void* object) const
		{
			return Access(object);
		}

		const void* Get(const void* object) const
		{
			return Access(const_cast<void*>(object));
		}

		bool HasRange() const
		{
			return Min != -FLT_MAX || Max != FLT_MAX;
		}
	};

	// A data type: an HY_DATA struct. Read-only output of the registration (like ScriptTypeInfo). The registry
	// makes, copies and frees instances through the type-erased functions, never by copying bytes: data types
	// hold strings, lists and nested structs.
	struct DataTypeInfo
	{
		// The saved type name (the C++ struct's name) and the label of the Create menus ("Dialogue" for
		// DialogueDefinition).
		std::string Name;
		std::string DisplayName;
		std::type_index CppType = typeid(void);

		// HY_DATA(Definition) / HY_DATA(Table): which assets it can be created as. Neither = a helper type,
		// only used inside other types.
		bool CanBeDefinition = false;
		bool CanBeTable = false;

		// HY_DATA(DisplayField(Name)): the string field row lists and pickers show. Empty = the row ID.
		// Editor-only: it changes no saving, identity or validity.
		std::string DisplayField;

		// In declaration order, which is also the order they are written in.
		std::vector<DataField> Fields;

		void* (*Construct)() = nullptr;                              // new T()
		void (*Destroy)(void* object) = nullptr;                     // delete (T*)object
		void (*Copy)(void* destination, const void* source) = nullptr; // destination = source
		void (*Move)(void* destination, void* source) = nullptr;       // destination = std::move(source)

		const DataField* FindField(const std::string& name) const;

		bool IsCreatable() const
		{
			return CanBeDefinition || CanBeTable;
		}
	};

	struct DataEnumValue
	{
		std::string Name;
		int64_t Value = 0;
	};

	// An HY_ENUM enum class: its enumerators by name, in declaration order. Saved by name, so reordering or
	// renumbering enumerators never changes data; renaming one does (validation reports the old name).
	struct DataEnumInfo
	{
		std::string Name;
		std::type_index CppType = typeid(void);
		std::vector<DataEnumValue> Values;

		// nullptr when no enumerator has this value.
		const char* FindName(int64_t value) const;
		// false when no enumerator has this name.
		bool FindValue(const std::string& name, int64_t& value) const;
	};

	// ---- Compile-time description of field types (used by DataTypeRegistration<T>) ----------------------

	template<typename>
	inline constexpr bool DataAlwaysFalse = false;

	template<typename T>
	struct DataIsList : std::false_type
	{
	};

	template<typename Element, typename Allocator>
	struct DataIsList<std::vector<Element, Allocator>> : std::true_type
	{
	};

	// A struct field type that isn't one of the value kinds: it must be an HY_DATA struct.
	template<typename T>
	inline constexpr bool DataIsStruct =
		std::is_class_v<T> && std::is_same_v<T, std::string> == false && std::is_same_v<T, glm::vec2> == false &&
		std::is_same_v<T, glm::vec3> == false && std::is_same_v<T, glm::vec4> == false &&
		std::is_same_v<T, AssetHandle> == false && std::is_same_v<T, DataRowRef> == false &&
		DataIsList<T>::value == false;

	template<typename T>
	DataValueType MakeDataValueType();

	template<typename List>
	const DataListOps& GetDataListOps()
	{
		using Element = typename List::value_type;
		static_assert(DataIsList<Element>::value == false, "Lists of lists are not supported in data types; wrap the inner list in an HY_DATA struct");

		static const DataListOps ops = []()
		{
			DataListOps result;
			result.Element = MakeDataValueType<Element>();
			result.Size = [](const void* list) -> size_t
			{
				return static_cast<const List*>(list)->size();
			};
			result.At = [](void* list, size_t index) -> void*
			{
				return &(*static_cast<List*>(list))[index];
			};
			result.AtConst = [](const void* list, size_t index) -> const void*
			{
				return &(*static_cast<const List*>(list))[index];
			};
			result.Resize = [](void* list, size_t size)
			{
				static_cast<List*>(list)->resize(size);
			};
			result.Insert = [](void* list, size_t index)
			{
				List& typed = *static_cast<List*>(list);
				typed.insert(typed.begin() + (std::ptrdiff_t)index, Element{});
			};
			result.Erase = [](void* list, size_t index)
			{
				List& typed = *static_cast<List*>(list);
				typed.erase(typed.begin() + (std::ptrdiff_t)index);
			};
			result.Swap = [](void* list, size_t first, size_t second)
			{
				List& typed = *static_cast<List*>(list);
				std::swap(typed[first], typed[second]);
			};
			result.Assign = [](void* destination, const void* source)
			{
				*static_cast<List*>(destination) = *static_cast<const List*>(source);
			};
			return result;
		}();
		return ops;
	}

	// The DataValueType of a C++ field type, checked at compile time.
	template<typename T>
	DataValueType MakeDataValueType()
	{
		DataValueType type;
		type.CppType = typeid(T);

		if constexpr (std::is_same_v<T, bool>)
		{
			type.Kind = DataFieldKind::Bool;
		}
		else if constexpr (std::is_same_v<T, int>)
		{
			type.Kind = DataFieldKind::Int;
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			type.Kind = DataFieldKind::Float;
		}
		else if constexpr (std::is_same_v<T, glm::vec2>)
		{
			type.Kind = DataFieldKind::Vec2;
		}
		else if constexpr (std::is_same_v<T, glm::vec3>)
		{
			type.Kind = DataFieldKind::Vec3;
		}
		else if constexpr (std::is_same_v<T, glm::vec4>)
		{
			type.Kind = DataFieldKind::Vec4;
		}
		else if constexpr (std::is_same_v<T, std::string>)
		{
			type.Kind = DataFieldKind::String;
		}
		else if constexpr (std::is_same_v<T, AssetHandle>)
		{
			type.Kind = DataFieldKind::Asset;
		}
		else if constexpr (std::is_same_v<T, DataRowRef>)
		{
			type.Kind = DataFieldKind::RowRef;
		}
		else if constexpr (std::is_enum_v<T>)
		{
			type.Kind = DataFieldKind::Enum;
			type.ReadEnum = [](const void* value) -> int64_t
			{
				return (int64_t)*static_cast<const T*>(value);
			};
			type.WriteEnum = [](void* value, int64_t number)
			{
				*static_cast<T*>(value) = (T)number;
			};
		}
		else if constexpr (DataIsList<T>::value)
		{
			type.Kind = DataFieldKind::List;
			type.List = &GetDataListOps<T>();
		}
		else if constexpr (DataIsStruct<T>)
		{
			static_assert(std::is_default_constructible_v<T> && std::is_copy_assignable_v<T>,
				"A nested data struct must be default-constructible and copyable");
			type.Kind = DataFieldKind::Struct;
		}
		else
		{
			static_assert(DataAlwaysFalse<T>,
				"Unsupported data field type: bool, int, float, glm::vec2/3/4, std::string, AssetHandle, DataRowRef, "
				"an HY_ENUM enum class, an HY_DATA struct or a std::vector of one of these");
		}

		return type;
	}
}
