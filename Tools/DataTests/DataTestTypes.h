// Tools/DataTests/DataTestTypes.h
#pragma once

// Data types for the headless data tests (scripts/test_data.sh), shaped like the game's planned ones (D4/D5).
// They live here, not in Game/src: the generator reads this folder only when the test runs.

#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	HY_DATA()
	struct DialogueLine
	{
		HY_PROPERTY(Tooltip("Who says it; empty = the dialogue's speaker"))
		std::string Speaker;
		HY_PROPERTY(Tooltip("The text; wraps in the dialogue box"))
		std::string Text;
	};

	HY_DATA(Definition)
	struct DialogueDefinition
	{
		HY_PROPERTY(Tooltip("The name shown on the box"))
		std::string Speaker;
		HY_PROPERTY(Asset(Texture2D), Tooltip("Optional portrait"))
		AssetHandle Portrait = 0;
		HY_PROPERTY()
		std::vector<DialogueLine> Lines;
	};

	HY_ENUM()
	enum class ItemKind : uint8_t
	{
		Consumable,
		Equipment,
		KeyItem = 5, // values don't matter: enums are saved by name
		Material
	};

	HY_DATA(Table, DisplayField(Name))
	struct ItemDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY()
		std::string Description;
		HY_PROPERTY()
		ItemKind Kind = ItemKind::Consumable;
		HY_PROPERTY(Asset(Texture2D))
		AssetHandle Icon = 0;
		HY_PROPERTY(Range(0, 999))
		int RestoreHP = 0;
		HY_PROPERTY(Range(0, 999))
		int RestoreMP = 0;
		HY_PROPERTY(Tooltip("Can be used from the field inventory"))
		bool UsableInField = false;
	};

	HY_DATA()
	struct ShopEntry
	{
		HY_PROPERTY(DataRow(ItemDefinition))
		DataRowRef Item;
		HY_PROPERTY(Tooltip("-1 = the item's own price"))
		int PriceOverride = -1;
	};

	HY_DATA(Definition)
	struct ShopDefinition
	{
		HY_PROPERTY()
		std::string DisplayName;
		HY_PROPERTY()
		std::vector<ShopEntry> Stock;
	};

	// Every other kind, both shapes (a definition and a table).
	HY_DATA(Definition, Table)
	struct TuningDefinition
	{
		HY_PROPERTY(Range(0, 10))
		float Speed = 0.1f;
		HY_PROPERTY()
		glm::vec2 Offset{ 0.5f, -1.25f };
		HY_PROPERTY(Color)
		glm::vec3 Tint{ 1.0f, 0.7f, 0.3f };
		HY_PROPERTY(Color)
		glm::vec4 Glow{ 0.0f };
		HY_PROPERTY(Range(0, 100))
		std::vector<int> Thresholds{ 10, 20 };
		HY_PROPERTY()
		std::vector<std::string> Tags;
		HY_PROPERTY()
		std::vector<ItemKind> Allowed;
		HY_PROPERTY(DataRow(ItemDefinition))
		std::vector<DataRowRef> Rewards;
		HY_PROPERTY()
		DialogueLine Greeting;
	};
}
