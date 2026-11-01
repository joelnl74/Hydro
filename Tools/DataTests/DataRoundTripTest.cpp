// Headless tests of the data serializer (D1, #168), the data API (D2, #169) and the editing operations (D3, #170). Built and run by scripts/test_data.sh with g++ on Linux: the
// engine's Data/ sources, the types in DataTestTypes.h and their generated registration. No window, GPU or project.

#include "hypch.h"

#include "DataTestTypes.h"
#include "GeneratedDataReflection.h"

#include "Hydro/Data/Data.h"
#include "Hydro/Data/DataEditing.h"
#include "Hydro/Data/DefinitionAsset.h"
#include "Hydro/Data/DataSerializer.h"
#include "Hydro/Data/DataTypeRegistry.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace Hydro
{
	// TestAssetManager.cpp: the stand-in AssetManager's registry.
	void TestRegisterAsset(AssetHandle handle, const std::filesystem::path& absolute, const std::string& relative);
	void TestMarkMissing(AssetHandle handle);
	int TestGetLoadCount(AssetHandle handle);

	static int s_Failures = 0;
	static int s_Checks = 0;

	static void Check(bool condition, const char* what, int line)
	{
		s_Checks++;
		if (condition == false)
		{
			s_Failures++;
			std::printf("FAIL  line %d: %s\n", line, what);
		}
	}

#define CHECK(condition) Check((condition), #condition, __LINE__)

	static bool HasMessage(const DataReport& report, DataMessageSeverity severity, const std::string& row, const std::string& path, const std::string& textPart)
	{
		for (const DataMessage& message : report.GetMessages())
		{
			if (message.Severity == severity && message.Row == row && message.Path == path && message.Text.find(textPart) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	static void PrintReport(const char* title, const DataReport& report)
	{
		std::printf("  %s: %s\n", title, report.GetSummary().c_str());
		for (const DataMessage& message : report.GetMessages())
		{
			std::printf("    %s %s\n", message.Severity == DataMessageSeverity::Error ? "error  " : "warning", message.ToString().c_str());
		}
	}

	// Write, read what was written, write again: the same bytes (canonical output is stable).
	static std::string RoundTripDefinition(const DefinitionData& data, const char* name)
	{
		std::string first;
		CHECK(DataSerializer::WriteDefinition(data, first));

		DefinitionData reread;
		DataReport report;
		CHECK(DataSerializer::ReadDefinition(first, reread, report));
		std::string second;
		CHECK(DataSerializer::WriteDefinition(reread, second));
		Check(first == second, (std::string(name) + ": write, read, write is byte-identical").c_str(), __LINE__);
		return first;
	}

	static std::string RoundTripTable(const DataTableData& data, const char* name)
	{
		std::string first;
		CHECK(DataSerializer::WriteTable(data, first));

		DataTableData reread;
		DataReport report;
		CHECK(DataSerializer::ReadTable(first, reread, report));
		std::string second;
		CHECK(DataSerializer::WriteTable(reread, second));
		Check(first == second, (std::string(name) + ": write, read, write is byte-identical").c_str(), __LINE__);
		return first;
	}

	static void TestRegistry()
	{
		std::printf("Registry\n");
		const DataTypeInfo* dialogue = DataTypeRegistry::Find("DialogueDefinition");
		CHECK(dialogue != nullptr);
		CHECK(dialogue != nullptr && dialogue->CanBeDefinition && dialogue->CanBeTable == false);
		CHECK(dialogue != nullptr && dialogue->DisplayName == "Dialogue");
		const DataTypeInfo* line = DataTypeRegistry::Find("DialogueLine");
		CHECK(line != nullptr && line->IsCreatable() == false);
		const DataTypeInfo* item = DataTypeRegistry::Find(typeid(ItemDefinition));
		CHECK(item != nullptr && item->CanBeTable && item->DisplayField == "Name");
		CHECK(item != nullptr && item->FindField("RestoreHP") != nullptr && item->FindField("RestoreHP")->Max == 999.0f);
		const DataEnumInfo* kind = DataTypeRegistry::FindEnum("ItemKind");
		CHECK(kind != nullptr && kind->Values.size() == 4);
		int64_t keyItem = 0;
		CHECK(kind != nullptr && kind->FindValue("KeyItem", keyItem) && keyItem == 5);
	}

	static void TestDialogueDefinition()
	{
		std::printf("Dialogue definition (three lines)\n");
		DefinitionData data;
		data.Handle = AssetHandle(7104338611223409153ull);
		data.Values = DataRecord(*DataTypeRegistry::Find("DialogueDefinition"));
		DialogueDefinition* dialogue = data.Values.As<DialogueDefinition>();
		CHECK(dialogue != nullptr);
		dialogue->Speaker = "Innkeeper";
		dialogue->Lines.push_back({ "", "Welcome to the Golden Sheaf. Warm yourself by the hearth, the stew's nearly ready." });
		dialogue->Lines.push_back({ "", "The bed by the window is made up. Rest whenever you like." });
		dialogue->Lines.push_back({ "Patron", "Best stew this side of the mountain!" });

		const std::string text = RoundTripDefinition(data, "Innkeeper.hydef");
		const std::string expected =
			"Handle: 7104338611223409153\n"
			"Definition:\n"
			"  Version: 1\n"
			"  Type: DialogueDefinition\n"
			"  Values:\n"
			"    Speaker: Innkeeper\n"
			"    Portrait: 0\n"
			"    Lines:\n"
			"      - Speaker: \"\"\n"
			"        Text: Welcome to the Golden Sheaf. Warm yourself by the hearth, the stew's nearly ready.\n"
			"      - Speaker: \"\"\n"
			"        Text: The bed by the window is made up. Rest whenever you like.\n"
			"      - Speaker: Patron\n"
			"        Text: Best stew this side of the mountain!\n";
		CHECK(text == expected);
		if (text != expected)
		{
			std::printf("--- got:\n%s--- expected:\n%s", text.c_str(), expected.c_str());
		}
		std::printf("%s", text.c_str());
	}

	static void TestItemTable()
	{
		std::printf("Item table (two rows)\n");
		const DataTypeInfo& type = *DataTypeRegistry::Find("ItemDefinition");
		DataTableData data;
		data.Handle = AssetHandle(5530912287741200231ull);
		data.RowType = &type;

		DataRecord potion(type);
		potion.As<ItemDefinition>()->Name = "Potion";
		potion.As<ItemDefinition>()->Description = "A bitter brew of the shrine's herbs. Restores 25 HP.";
		potion.As<ItemDefinition>()->RestoreHP = 25;
		potion.As<ItemDefinition>()->UsableInField = true;
		DataRecord sword(type);
		sword.As<ItemDefinition>()->Name = "Iron Sword";
		sword.As<ItemDefinition>()->Description = "A plain blade from the village smithy.";
		sword.As<ItemDefinition>()->Kind = ItemKind::Equipment;

		// Inserted Potion first; written sorted by ID.
		data.Rows.emplace("Potion", std::move(potion));
		data.Rows.emplace("IronSword", std::move(sword));

		const std::string text = RoundTripTable(data, "Items.hytable");
		const std::string expected =
			"Handle: 5530912287741200231\n"
			"DataTable:\n"
			"  Version: 1\n"
			"  Type: ItemDefinition\n"
			"  Rows:\n"
			"    IronSword:\n"
			"      Name: Iron Sword\n"
			"      Description: A plain blade from the village smithy.\n"
			"      Kind: Equipment\n"
			"      Icon: 0\n"
			"      RestoreHP: 0\n"
			"      RestoreMP: 0\n"
			"      UsableInField: false\n"
			"    Potion:\n"
			"      Name: Potion\n"
			"      Description: A bitter brew of the shrine's herbs. Restores 25 HP.\n"
			"      Kind: Consumable\n"
			"      Icon: 0\n"
			"      RestoreHP: 25\n"
			"      RestoreMP: 0\n"
			"      UsableInField: true\n";
		CHECK(text == expected);
		if (text != expected)
		{
			std::printf("--- got:\n%s--- expected:\n%s", text.c_str(), expected.c_str());
		}
		std::printf("%s", text.c_str());
	}

	static void TestEveryKind()
	{
		std::printf("Every field kind\n");
		DefinitionData data;
		data.Handle = AssetHandle(42);
		data.Values = DataRecord(*DataTypeRegistry::Find("TuningDefinition"));
		TuningDefinition* tuning = data.Values.As<TuningDefinition>();
		tuning->Tags = { "warm", "needs: quotes", "" };
		tuning->Allowed = { ItemKind::KeyItem, ItemKind::Material };
		tuning->Rewards = { DataRowRef{ AssetHandle(5530912287741200231ull), "Potion" }, DataRowRef{} };
		tuning->Greeting.Text = "Line one\nline two";

		const std::string text = RoundTripDefinition(data, "Tuning.hydef");
		std::printf("%s", text.c_str());
		CHECK(text.find("Speed: 0.1\n") != std::string::npos); // shortest float, not 0.100000001
		CHECK(text.find("Offset: [0.5, -1.25]") != std::string::npos);
		CHECK(text.find("Tint: [1, 0.7, 0.3]") != std::string::npos);
		CHECK(text.find("- {Table: 5530912287741200231, Row: Potion}") != std::string::npos);
		CHECK(text.find("- KeyItem") != std::string::npos);
		CHECK(text.find("Thresholds: [10, 20]") != std::string::npos);

		DefinitionData reread;
		DataReport report;
		CHECK(DataSerializer::ReadDefinition(text, reread, report));
		CHECK(report.IsEmpty());
		const TuningDefinition* back = reread.Values.As<TuningDefinition>();
		CHECK(back != nullptr && back->Speed == 0.1f && back->Tint == glm::vec3(1.0f, 0.7f, 0.3f));
		CHECK(back != nullptr && back->Tags.size() == 3 && back->Tags[1] == "needs: quotes" && back->Tags[2].empty());
		CHECK(back != nullptr && back->Allowed.size() == 2 && back->Allowed[0] == ItemKind::KeyItem);
		CHECK(back != nullptr && back->Rewards.size() == 2 && back->Rewards[0].Row == "Potion" && back->Rewards[1].IsSet() == false);
		CHECK(back != nullptr && back->Greeting.Text == "Line one\nline two");
		CHECK(back != nullptr && back->Thresholds == std::vector<int>({ 10, 20 }));

		// The same type as a table.
		DataTableData table;
		table.RowType = DataTypeRegistry::Find("TuningDefinition");
		table.Rows.emplace("Fast", DataRecord(*table.RowType));
		table.Rows.emplace("Empty", DataRecord(*table.RowType));
		table.Rows.at("Fast").As<TuningDefinition>()->Speed = 7.5f;
		RoundTripTable(table, "Tuning.hytable");
	}

	static void TestKeptAndReported()
	{
		std::printf("Hand-edited file: unknown keys, unreadable values, ranges\n");
		const std::string handEdited =
			"Handle: 99\n"
			"DataTable:\n"
			"  Version: 1\n"
			"  Type: ItemDefinition\n"
			"  Rows:\n"
			"    Potion:\n"
			"      Name: Potion\n"
			"      RestoreHP: 1200      # outside Range(0, 999): kept, warned\n"
			"      Price: 50            # no such field (renamed?): kept, warned\n"
			"      Kind: Weapon         # unknown enumerator: kept, error\n"
			"      RestoreMP: lots      # not a number: kept, error\n"
			"    Elixir:\n"
			"      Description: no name\n"
			"    2Bad:\n"
			"      Name: Bad ID\n";

		DataTableData data;
		DataReport report;
		CHECK(DataSerializer::ReadTable(handEdited, data, report));
		PrintReport("read", report);
		CHECK(data.ReadOnly == false);
		CHECK(data.Rows.size() == 3);
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "Potion", "RestoreHP", "outside 0..999"));
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "Potion", "Price", "unknown field"));
		CHECK(HasMessage(report, DataMessageSeverity::Error, "Potion", "Kind", "unknown ItemKind 'Weapon'"));
		CHECK(HasMessage(report, DataMessageSeverity::Error, "Potion", "RestoreMP", "expected a whole number"));
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "Elixir", "Name", "empty"));
		CHECK(HasMessage(report, DataMessageSeverity::Error, "2Bad", "", "not a valid row ID"));

		const ItemDefinition* potion = data.Rows.at("Potion").As<ItemDefinition>();
		CHECK(potion != nullptr && potion->RestoreHP == 1200);           // kept as written, not clamped
		CHECK(potion != nullptr && potion->Kind == ItemKind::Consumable); // default while unreadable
		CHECK(potion != nullptr && potion->RestoreMP == 0);

		std::string written;
		CHECK(DataSerializer::WriteTable(data, written));
		std::printf("%s", written.c_str());
		CHECK(written.find("Price: 50") != std::string::npos);
		CHECK(written.find("Kind: Weapon") != std::string::npos);
		CHECK(written.find("RestoreMP: lots") != std::string::npos);
		CHECK(written.find("RestoreHP: 1200") != std::string::npos);
		CHECK(written.find("#") == std::string::npos); // comments are not kept (v1)

		// Reading what was written finds the same problems and writes the same bytes.
		DataTableData again;
		DataReport againReport;
		CHECK(DataSerializer::ReadTable(written, again, againReport));
		CHECK(againReport.GetErrorCount() == report.GetErrorCount() && againReport.GetWarningCount() == report.GetWarningCount());
		std::string writtenAgain;
		CHECK(DataSerializer::WriteTable(again, writtenAgain));
		CHECK(written == writtenAgain);

		// CheckRecord (validation of data in memory) reports what the file had.
		DataReport check;
		DataSerializer::CheckRecord(again.Rows.at("Potion"), "Potion", true, check);
		CHECK(HasMessage(check, DataMessageSeverity::Warning, "Potion", "Price", "unknown field"));
		CHECK(HasMessage(check, DataMessageSeverity::Error, "Potion", "Kind", "unknown ItemKind 'Weapon'"));
		CHECK(HasMessage(check, DataMessageSeverity::Error, "Potion", "RestoreMP", "expected a whole number"));
		CHECK(check.GetErrorCount() == 2 && check.GetWarningCount() == 2); // the same as reading found for this row: nothing twice
		CHECK(HasMessage(check, DataMessageSeverity::Warning, "Potion", "RestoreHP", "outside"));

		// Setting the field in the editor forgets the kept value.
		DataRecord& record = again.Rows.at("Potion");
		record.As<ItemDefinition>()->Kind = ItemKind::Equipment;
		record.GetKept().Forget("Kind");
		std::string fixedText;
		CHECK(DataSerializer::WriteTable(again, fixedText));
		CHECK(fixedText.find("Kind: Equipment") != std::string::npos && fixedText.find("Weapon") == std::string::npos);
	}

	static void TestNested()
	{
		std::printf("Nested structs and lists keep their own unknown keys\n");
		const std::string text =
			"Definition:\n"
			"  Version: 1\n"
			"  Type: DialogueDefinition\n"
			"  Values:\n"
			"    Speaker: Elder\n"
			"    Lines:\n"
			"      - Text: First\n"
			"        Mood: Stern\n"
			"      - Text: Second\n"
			"      -\n"
			"      - [not, a, map]\n";
		DefinitionData data;
		DataReport report;
		CHECK(DataSerializer::ReadDefinition(text, data, report));
		PrintReport("read", report);
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "", "Lines[0].Mood", "unknown field"));
		CHECK(HasMessage(report, DataMessageSeverity::Error, "", "Lines[3]", "expected a map of fields"));
		// A list with an entry that can't be read keeps the whole list from the file; the field has its default.
		const DialogueDefinition* dialogue = data.Values.As<DialogueDefinition>();
		CHECK(dialogue != nullptr && dialogue->Lines.empty());

		std::string written;
		CHECK(DataSerializer::WriteDefinition(data, written));
		std::printf("%s", written.c_str());
		CHECK(written.find("Mood: Stern") != std::string::npos);
		CHECK(written.find("[not, a, map]") != std::string::npos);

		// A list that reads: the unknown key stays with its entry.
		const std::string good =
			"Definition:\n"
			"  Version: 1\n"
			"  Type: DialogueDefinition\n"
			"  Values:\n"
			"    Lines:\n"
			"      - Text: First\n"
			"        Mood: Stern\n"
			"      - Text: Second\n";
		DefinitionData goodData;
		DataReport goodReport;
		CHECK(DataSerializer::ReadDefinition(good, goodData, goodReport));
		CHECK(goodData.Values.As<DialogueDefinition>()->Lines.size() == 2);
		std::string goodWritten;
		CHECK(DataSerializer::WriteDefinition(goodData, goodWritten));
		std::printf("%s", goodWritten.c_str());
		CHECK(goodWritten.find("      - Speaker: \"\"\n        Text: First\n        Mood: Stern\n") != std::string::npos);
	}

	static void TestDuplicates()
	{
		std::printf("Duplicate row IDs and keys\n");
		const std::string text =
			"DataTable:\n"
			"  Version: 1\n"
			"  Type: ItemDefinition\n"
			"  Rows:\n"
			"    Potion:\n"
			"      Name: First Potion\n"
			"    Potion:\n"
			"      Name: Second Potion\n"
			"    Ether:\n"
			"      Name: Ether\n"
			"      Name: Ether Again\n";
		DataTableData data;
		DataReport report;
		CHECK(DataSerializer::ReadTable(text, data, report));
		PrintReport("read", report);
		CHECK(HasMessage(report, DataMessageSeverity::Error, "Potion", "", "duplicate row ID"));
		CHECK(HasMessage(report, DataMessageSeverity::Error, "Ether", "Name", "appears twice"));
		CHECK(data.ReadOnly);
		CHECK(data.Rows.size() == 2);
		CHECK(data.Rows.at("Potion").As<ItemDefinition>()->Name == "First Potion"); // not replaced by the second
		CHECK(data.Rows.at("Ether").As<ItemDefinition>()->Name == "Ether");
		std::string written;
		CHECK(DataSerializer::WriteTable(data, written) == false); // read-only: never written back
	}

	static void TestFileLevel()
	{
		std::printf("Versions, types and shapes\n");
		{
			DefinitionData data;
			DataReport report;
			CHECK(DataSerializer::ReadDefinition("Definition:\n  Version: 2\n  Type: DialogueDefinition\n  Values: {}\n", data, report));
			CHECK(data.ReadOnly);
			CHECK(HasMessage(report, DataMessageSeverity::Error, "", "Version", "newer Hydro"));
			std::string written;
			CHECK(DataSerializer::WriteDefinition(data, written) == false);
		}
		{
			DefinitionData data;
			DataReport report;
			CHECK(DataSerializer::ReadDefinition("Definition:\n  Version: 1\n  Type: GoneDefinition\n  Values: {}\n", data, report) == false);
			CHECK(HasMessage(report, DataMessageSeverity::Error, "", "Type", "unknown data type 'GoneDefinition'"));
		}
		{
			DefinitionData data;
			DataReport report;
			CHECK(DataSerializer::ReadDefinition("DataTable:\n  Version: 1\n  Type: ItemDefinition\n  Rows: {}\n", data, report) == false);
			CHECK(HasMessage(report, DataMessageSeverity::Error, "", "", "data table, not a definition"));
		}
		{
			DataTableData data;
			DataReport report;
			CHECK(DataSerializer::ReadTable("DataTable:\n  Version: 1\n  Type: DialogueDefinition\n  Rows: {}\n", data, report));
			CHECK(HasMessage(report, DataMessageSeverity::Error, "", "Type", "not a table type"));
		}
		{
			DataTableData data;
			DataReport report;
			CHECK(DataSerializer::ReadTable("DataTable: [broken\n", data, report) == false);
			CHECK(report.HasErrors() && data.ReadOnly);
		}
		{
			// Missing keys keep their defaults; no messages.
			DataTableData data;
			DataReport report;
			CHECK(DataSerializer::ReadTable("DataTable:\n  Version: 1\n  Type: ItemDefinition\n  Rows:\n    Potion:\n      Name: Potion\n", data, report));
			CHECK(report.IsEmpty());
			std::string written;
			CHECK(DataSerializer::WriteTable(data, written));
			CHECK(written.find("Handle") == std::string::npos); // no handle yet: the asset manager gives it one (D2)
		}
		{
			// An empty table and an empty list.
			DataTableData data;
			data.RowType = DataTypeRegistry::Find("ItemDefinition");
			std::string written = RoundTripTable(data, "Empty.hytable");
			CHECK(written.find("Rows: {}") != std::string::npos);
		}
	}

	static void TestRecordCopies()
	{
		std::printf("Records copy deeply\n");
		DataTableData data;
		DataReport report;
		CHECK(DataSerializer::ReadTable("DataTable:\n  Version: 1\n  Type: ItemDefinition\n  Rows:\n    Potion:\n      Name: Potion\n      Price: 5\n", data, report));
		DataRecord copy = data.Rows.at("Potion"); // the editor's Duplicate
		copy.As<ItemDefinition>()->Name = "Hi-Potion";
		copy.GetKept().Unknown["Price"] = 50;
		CHECK(data.Rows.at("Potion").As<ItemDefinition>()->Name == "Potion");
		CHECK(data.Rows.at("Potion").GetKept().Unknown["Price"].as<int>() == 5);
		CHECK(copy.As<DialogueDefinition>() == nullptr); // wrong type: nullptr, not a cast

		DataRecord moved = std::move(copy);
		CHECK(moved.IsValid() && copy.IsValid() == false);
		CHECK(DataSerializer::IsValidRowId("IronSword") && DataSerializer::IsValidRowId("_x2"));
		CHECK(DataSerializer::IsValidRowId("") == false && DataSerializer::IsValidRowId("Iron Sword") == false && DataSerializer::IsValidRowId("Items:Potion") == false);
	}

	static void WriteFile(const std::filesystem::path& path, const std::string& text)
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << text;
	}

	// The file looks changed to the manager even within the same second.
	static void Touch(const std::filesystem::path& path, int seconds)
	{
		std::error_code ec;
		std::filesystem::last_write_time(path, std::filesystem::last_write_time(path, ec) + std::chrono::seconds(seconds), ec);
	}

	static void TestDataApi()
	{
		std::printf("Data::Get, GetRow, GetTable, Validate (with a stand-in AssetManager)\n");
		const std::filesystem::path folder = std::filesystem::temp_directory_path() / ("hydro-data-test-" + std::to_string((long long)std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(folder);

		const AssetHandle items(1001), inn(1002), shop(1003), broken(1004), gone(1005);
		WriteFile(folder / "Items.hytable",
			"Handle: 1001\nDataTable:\n  Version: 1\n  Type: ItemDefinition\n  Rows:\n"
			"    Potion:\n      Name: Potion\n      RestoreHP: 25\n"
			"    Ether:\n      Name: Ether\n      RestoreMP: 10\n");
		WriteFile(folder / "Innkeeper.hydef",
			"Handle: 1002\nDefinition:\n  Version: 1\n  Type: DialogueDefinition\n  Values:\n    Speaker: Innkeeper\n    Lines:\n      - Text: Welcome!\n");
		WriteFile(folder / "Shop.hydef",
			"Handle: 1003\nDefinition:\n  Version: 1\n  Type: ShopDefinition\n  Values:\n    DisplayName: General Store\n    Greeting: 1001\n    Stock:\n"
			"      - Item: {Table: 1001, Row: Potion}\n      - Item: {Table: 1001, Row: MegaPotion}\n      - Item: {Table: 1002, Row: Potion}\n"
			"      - Item: {Table: 0, Row: Potion}\n      - Item: {Table: 0, Row: \"\"}\n");
		WriteFile(folder / "Broken.hydef", "Definition: [unclosed\n");
		WriteFile(folder / "Gone.hydef", "Handle: 1005\nDefinition:\n  Version: 1\n  Type: DialogueDefinition\n  Values: {}\n");
		TestRegisterAsset(items, folder / "Items.hytable", "Data/Items.hytable");
		TestRegisterAsset(inn, folder / "Innkeeper.hydef", "Data/Dialogue/Innkeeper.hydef");
		TestRegisterAsset(shop, folder / "Shop.hydef", "Data/Shop.hydef");
		TestRegisterAsset(broken, folder / "Broken.hydef", "Data/Broken.hydef");
		TestRegisterAsset(gone, folder / "Gone.hydef", "Data/Gone.hydef");
		TestMarkMissing(gone);

		// A definition.
		const DataView<DialogueDefinition> dialogue = Data::Get<DialogueDefinition>(inn);
		CHECK(dialogue.IsValid() && dialogue->Speaker == "Innkeeper" && dialogue->Lines.size() == 1);
		CHECK(Data::Get<DialogueDefinition>(AssetHandle(0)).IsValid() == false); // none: no warning
		CHECK(Data::Get<ShopDefinition>(inn).IsValid() == false);                 // a dialogue, not a shop (warned once)
		CHECK(Data::Get<ShopDefinition>(inn).IsValid() == false);                 // (not warned again)
		CHECK(Data::Get<DialogueDefinition>(items).IsValid() == false);           // a table, not a definition
		CHECK(Data::Get<DialogueDefinition>(AssetHandle(424242)).IsValid() == false); // not in the project
		CHECK(Data::Get<DialogueDefinition>(gone).IsValid() == false);            // file missing
		CHECK(Data::Get<DialogueDefinition>(broken).IsValid() == false);          // can't be read
		CHECK(Data::GetDataType(inn) != nullptr && Data::GetDataType(inn)->Name == "DialogueDefinition");
		CHECK(Data::GetDataType(items) != nullptr && Data::GetDataType(items)->Name == "ItemDefinition");

		// A table, its rows by ID (sorted), and single rows.
		const DataTableView<ItemDefinition> table = Data::GetTable<ItemDefinition>(items);
		CHECK(table.IsValid() && table.GetRowCount() == 2 && table.GetHandle() == items);
		std::string order;
		for (const auto& [id, row] : table.Rows())
		{
			order += id + "=" + row.Name + ";";
		}
		CHECK(order == "Ether=Ether;Potion=Potion;");
		CHECK(table.FindRow("Potion") != nullptr && table.FindRow("Potion")->RestoreHP == 25 && table.FindRow("Nope") == nullptr);
		CHECK(Data::GetTable<DialogueDefinition>(items).IsValid() == false);    // rows of another type
		CHECK(Data::GetTable<ItemDefinition>(inn).IsValid() == false);          // a definition, not a table
		const DataView<ItemDefinition> potion = Data::GetRow<ItemDefinition>(table.MakeRef("Potion"));
		CHECK(potion.IsValid() && potion->RestoreHP == 25);
		CHECK(Data::GetRow<ItemDefinition>(DataRowRef{ items, "MegaPotion" }).IsValid() == false); // no such row (warned once)
		CHECK(Data::GetRow<ItemDefinition>(DataRowRef{}).IsValid() == false);
		CHECK(Data::GetRow<DialogueDefinition>(DataRowRef{ items, "Potion" }).IsValid() == false);
		CHECK(DataTableView<ItemDefinition>().Rows().begin() == DataTableView<ItemDefinition>().Rows().end()); // an empty view iterates nothing

		// Edited outside the editor: the next Data::Get sees it; a view taken before still reads the old data safely.
		const int loadsBefore = TestGetLoadCount(inn);
		CHECK(Data::Get<DialogueDefinition>(inn)->Speaker == "Innkeeper" && TestGetLoadCount(inn) == loadsBefore); // cached
		WriteFile(folder / "Innkeeper.hydef",
			"Handle: 1002\nDefinition:\n  Version: 1\n  Type: DialogueDefinition\n  Values:\n    Speaker: Innkeeper Mara\n    Lines:\n      - Text: Welcome back!\n      - Text: Rooms are free tonight.\n");
		Touch(folder / "Innkeeper.hydef", 2);
		const DataView<DialogueDefinition> reloaded = Data::Get<DialogueDefinition>(inn);
		CHECK(TestGetLoadCount(inn) == loadsBefore + 1);
		CHECK(reloaded.IsValid() && reloaded->Speaker == "Innkeeper Mara" && reloaded->Lines.size() == 2);
		CHECK(dialogue.IsValid() && dialogue->Speaker == "Innkeeper" && dialogue->Lines[0].Text == "Welcome!"); // the old object lives on
		// A bad edit keeps the last good data.
		WriteFile(folder / "Innkeeper.hydef", "Definition: [half a save\n");
		Touch(folder / "Innkeeper.hydef", 4);
		CHECK(Data::Get<DialogueDefinition>(inn).IsValid() && Data::Get<DialogueDefinition>(inn)->Speaker == "Innkeeper Mara");

		// Validation: the file's problems, the values' and the references'.
		const DataReport shopReport = Data::Validate(shop);
		PrintReport("Shop.hydef", shopReport);
		CHECK(HasMessage(shopReport, DataMessageSeverity::Error, "", "Greeting", "is a DataTable, not a Definition"));
		CHECK(HasMessage(shopReport, DataMessageSeverity::Error, "", "Stock[1].Item", "Missing: MegaPotion"));
		CHECK(HasMessage(shopReport, DataMessageSeverity::Error, "", "Stock[2].Item", "not a data table"));
		CHECK(HasMessage(shopReport, DataMessageSeverity::Error, "", "Stock[3].Item", "without a table"));
		CHECK(shopReport.GetErrorCount() == 4); // Stock[0] is fine, Stock[4] is empty (none)
		const DataReport itemsReport = Data::Validate(items);
		CHECK(itemsReport.IsEmpty());
		const DataReport brokenReport = Data::Validate(broken);
		PrintReport("Broken.hydef", brokenReport);
		CHECK(HasMessage(brokenReport, DataMessageSeverity::Error, "", "", "not valid YAML"));
		CHECK(HasMessage(Data::Validate(gone), DataMessageSeverity::Error, "", "", "is missing"));
		CHECK(HasMessage(Data::Validate(AssetHandle(424242)), DataMessageSeverity::Error, "", "", "not in the project"));

		std::error_code ec;
		std::filesystem::remove_all(folder, ec);
	}

	static std::string ReadText(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}

	static void TestEditing()
	{
		std::printf("Editing (D3): list entries, rows, saving\n");
		const DataTypeInfo& dialogueType = *DataTypeRegistry::Find("DialogueDefinition");
		const DataField& lines = *dialogueType.FindField("Lines");

		// The plan's check: Elder.hydef with three lines, move one up, save, reopen: identical.
		const std::string text =
			"Handle: 77\nDefinition:\n  Version: 1\n  Type: DialogueDefinition\n  Values:\n    Speaker: Elder\n    Lines:\n"
			"      - Text: First\n      - Text: Second\n        Mood: Stern\n      - Text: Third\n";
		DefinitionData data;
		DataReport report;
		CHECK(DataSerializer::ReadDefinition(text, data, report));
		DialogueDefinition& elder = *data.Values.As<DialogueDefinition>();
		DataKeptValues& kept = data.Values.GetKept();
		DataEditing::SwapListEntries(lines.Type, &elder.Lines, kept, "Lines", 1, 0); // "Second" up
		CHECK(elder.Lines[0].Text == "Second" && elder.Lines[1].Text == "First");
		CHECK(kept.Nested.count("Lines[0]") == 1 && kept.Nested.count("Lines[1]") == 0); // its unknown key went with it

		const std::filesystem::path file = std::filesystem::temp_directory_path() / "hydro-data-test-Elder.hydef";
		CHECK(DefinitionAsset::Save(file, data));
		const std::string saved = ReadText(file);
		CHECK(saved.find("      - Speaker: \"\"\n        Text: Second\n        Mood: Stern\n      - Speaker: \"\"\n        Text: First\n") != std::string::npos);
		DefinitionData reopened;
		DataReport reopenReport;
		CHECK(DataSerializer::ReadDefinition(saved, reopened, reopenReport));
		std::string again;
		CHECK(DataSerializer::WriteDefinition(reopened, again));
		CHECK(again == saved); // saving what was opened changes nothing
		std::error_code ec;
		std::filesystem::remove(file, ec);

		// Insert before it, then remove it: the kept key follows, then goes.
		DataEditing::InsertListEntry(lines.Type, &elder.Lines, kept, "Lines", 0);
		CHECK(elder.Lines.size() == 4 && elder.Lines[0].Text.empty() && kept.Nested.count("Lines[1]") == 1);
		DataEditing::RemoveListEntry(lines.Type, &elder.Lines, kept, "Lines", 1);
		CHECK(elder.Lines.size() == 3 && elder.Lines[1].Text == "First" && kept.Nested.count("Lines[1]") == 0 && kept.Nested.count("Lines[0]") == 0);
		DataEditing::SwapListEntries(lines.Type, &elder.Lines, kept, "Lines", 0, 7); // out of range: nothing
		CHECK(elder.Lines.size() == 3);

		// A list that couldn't be read is kept from the file until it is changed.
		DefinitionData broken;
		DataReport brokenReport;
		CHECK(DataSerializer::ReadDefinition("Definition:\n  Version: 1\n  Type: DialogueDefinition\n  Values:\n    Lines: [not, maps]\n", broken, brokenReport));
		DialogueDefinition& empty = *broken.Values.As<DialogueDefinition>();
		CHECK(empty.Lines.empty() && broken.Values.GetKept().Unreadable["Lines"]);
		DataEditing::InsertListEntry(lines.Type, &empty.Lines, broken.Values.GetKept(), "Lines", 0);
		std::string brokenText;
		CHECK(DataSerializer::WriteDefinition(broken, brokenText));
		CHECK(brokenText.find("[not, maps]") == std::string::npos && brokenText.find("      - Speaker: \"\"\n        Text: \"\"\n") != std::string::npos);

		// Rows: add, duplicate, change ID, delete.
		DataTableData table;
		DataReport tableReport;
		CHECK(DataSerializer::ReadTable("DataTable:\n  Version: 1\n  Type: ItemDefinition\n  Rows:\n    Potion:\n      Name: Potion\n      Price: 5\n", table, tableReport));
		CHECK(DataEditing::AddRow(table) == "NewItem");
		CHECK(DataEditing::AddRow(table) == "NewItem2");
		const std::string copy = DataEditing::DuplicateRow(table, "Potion");
		CHECK(copy == "PotionCopy" && table.Rows.at(copy).As<ItemDefinition>()->Name == "Potion");
		CHECK(table.Rows.at(copy).GetKept().Unknown["Price"].as<int>() == 5);
		CHECK(DataEditing::DuplicateRow(table, "Potion") == "PotionCopy2");
		CHECK(DataEditing::DuplicateRow(table, "Nope").empty());
		std::string problem;
		CHECK(DataEditing::ChangeRowId(table, copy, "Iron Sword", problem) == false && problem.empty() == false);
		CHECK(DataEditing::ChangeRowId(table, copy, "Potion", problem) == false && problem.find("Another row") != std::string::npos);
		CHECK(DataEditing::CheckRowId(table, "Potion", "Potion").empty()); // keeping its own ID is fine
		CHECK(DataEditing::ChangeRowId(table, copy, "HiPotion", problem) && table.Rows.count("HiPotion") == 1 && table.Rows.count(copy) == 0);
		table.Rows.at("HiPotion").As<ItemDefinition>()->Name = "Hi-Potion";
		CHECK(DataEditing::GetRowLabel(table.Rows.at("HiPotion"), "HiPotion") == "Hi-Potion");
		CHECK(DataEditing::GetRowLabel(table.Rows.at("NewItem"), "NewItem") == "NewItem"); // an empty name: the ID
		CHECK(DataEditing::RemoveRow(table, "NewItem2") && DataEditing::RemoveRow(table, "NewItem2") == false);
		std::string tableText;
		CHECK(DataSerializer::WriteTable(table, tableText));
		CHECK(tableText.find("    HiPotion:\n      Name: Hi-Potion\n") != std::string::npos && tableText.find("NewItem2") == std::string::npos);
		CHECK(DataEditing::MakeFreeRowId(table, "Potion") == "Potion2" && DataEditing::MakeFreeRowId(table, "Ether") == "Ether");
	}
}

namespace Hydro
{
	// Former names (SC1): a file saved before a type or field was renamed loads, says so, and saves the new names.
	static void TestFormerNames()
	{
		std::printf("Former names: renamed types and fields\n");
		CHECK(DataTypeRegistry::Find("PrizeDefinition") == nullptr);
		CHECK(DataTypeRegistry::FindByFormerName("PrizeDefinition") == DataTypeRegistry::Find("RewardDefinition"));

		const std::string old =
			"Handle: 77\n"
			"Definition:\n"
			"  Version: 1\n"
			"  Type: PrizeDefinition\n"
			"  Values:\n"
			"    Title: Gold\n"
			"    Amount: 12\n"
			"    Line:\n"
			"      Speaker: Chest\n"
			"      Text: Ka-ching\n"
			"      Mood: happy\n";

		DefinitionData data;
		DataReport report;
		CHECK(DataSerializer::ReadDefinition(old, data, report));
		PrintReport("read", report);
		CHECK(report.GetErrorCount() == 0);
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "", "Type", "RewardDefinition's former name"));
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "", "Title", "saved under Name's former name"));
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "", "Amount", "saved under Count's former name"));
		CHECK(HasMessage(report, DataMessageSeverity::Warning, "", "Line", "saved under Says's former name"));
		const RewardDefinition* reward = data.Values.As<RewardDefinition>();
		CHECK(reward != nullptr && reward->Name == "Gold" && reward->Count == 12 && reward->Says.Speaker == "Chest" && reward->Says.Text == "Ka-ching");

		std::string written;
		CHECK(DataSerializer::WriteDefinition(data, written));
		std::printf("%s", written.c_str());
		CHECK(written.find("Type: RewardDefinition") != std::string::npos);
		CHECK(written.find("    Name: Gold\n    Count: 12\n    Says:\n      Speaker: Chest\n      Text: Ka-ching\n      Mood: happy\n") != std::string::npos);
		CHECK(written.find("Title") == std::string::npos && written.find("Amount") == std::string::npos && written.find("Line:") == std::string::npos);

		// Saved with the new names: nothing to report any more.
		DefinitionData saved;
		DataReport savedReport;
		CHECK(DataSerializer::ReadDefinition(written, saved, savedReport, true));
		CHECK(savedReport.GetErrorCount() == 0 && savedReport.GetWarningCount() == 1); // Says.Mood: an unknown key
		std::string writtenAgain;
		CHECK(DataSerializer::WriteDefinition(saved, writtenAgain) && writtenAgain == written);

		// Both a field's name and a former name (or two former names): the first one read wins, the other is kept.
		const std::string both =
			"Handle: 78\n"
			"Definition:\n"
			"  Version: 1\n"
			"  Type: RewardDefinition\n"
			"  Values:\n"
			"    Title: Old\n"
			"    Name: New\n"
			"    Amount: lots\n";
		DefinitionData twice;
		DataReport twiceReport;
		CHECK(DataSerializer::ReadDefinition(both, twice, twiceReport, true));
		PrintReport("read (name and former name)", twiceReport);
		CHECK(twice.Values.As<RewardDefinition>()->Name == "New" && twice.Values.As<RewardDefinition>()->Count == 0);
		CHECK(HasMessage(twiceReport, DataMessageSeverity::Warning, "", "Title", "a former name of Name, which has its own value"));
		CHECK(HasMessage(twiceReport, DataMessageSeverity::Error, "", "Count", "expected a whole number"));
		std::string twiceWritten;
		CHECK(DataSerializer::WriteDefinition(twice, twiceWritten));
		std::printf("%s", twiceWritten.c_str());
		// The unreadable value read under a former name is kept under the field's name, where it is written.
		CHECK(twiceWritten.find("Count: lots") != std::string::npos && twiceWritten.find("Amount") == std::string::npos);
		CHECK(twiceWritten.find("Title: Old") != std::string::npos && twiceWritten.find("Name: New") != std::string::npos);

		const std::string formers =
			"Handle: 79\n"
			"Definition:\n"
			"  Version: 1\n"
			"  Type: RewardDefinition\n"
			"  Values:\n"
			"    Heading: First\n"
			"    Title: Second\n";
		DefinitionData two;
		DataReport twoReport;
		CHECK(DataSerializer::ReadDefinition(formers, two, twoReport, true));
		CHECK(two.Values.As<RewardDefinition>()->Name == "First");
		CHECK(HasMessage(twoReport, DataMessageSeverity::Warning, "", "Title", "a former name of Name"));
	}

	int TestScriptSerializer(int& checks); // ScriptSerializerTest.cpp: returns its failures
}

int main()
{
	Hydro::Log::Init();
	Hydro::RegisterGeneratedDataReflection();

	Hydro::TestRegistry();
	Hydro::TestDialogueDefinition();
	Hydro::TestItemTable();
	Hydro::TestEveryKind();
	Hydro::TestKeptAndReported();
	Hydro::TestNested();
	Hydro::TestDuplicates();
	Hydro::TestFileLevel();
	Hydro::TestRecordCopies();
	Hydro::TestDataApi();
	Hydro::TestEditing();
	Hydro::TestFormerNames();
	Hydro::s_Failures += Hydro::TestScriptSerializer(Hydro::s_Checks);

	std::printf("\n%d checks, %d failed\n", Hydro::s_Checks, Hydro::s_Failures);
	return Hydro::s_Failures == 0 ? 0 : 1;
}
