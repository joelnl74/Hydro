#include "State/SaveGame.h"

#include "State/RpgSession.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <system_error>

namespace Hydro
{
	// ---- Values -------------------------------------------------------------------------------------------

	static void WriteRow(YAML::Emitter& out, const DataRowRef& row)
	{
		out << YAML::Flow << YAML::BeginMap << YAML::Key << "Table" << YAML::Value << (uint64_t)row.Table
			<< YAML::Key << "Row" << YAML::Value << row.Row << YAML::EndMap;
	}

	static DataRowRef ReadRow(const YAML::Node& node)
	{
		DataRowRef row;
		if (node.IsMap())
		{
			row.Table = AssetHandle(node["Table"].as<uint64_t>(0));
			row.Row = node["Row"].as<std::string>(std::string());
		}
		return row;
	}

	template<typename Rows>
	static void WriteRows(YAML::Emitter& out, const char* key, const Rows& rows)
	{
		out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
		for (const DataRowRef& row : rows)
		{
			WriteRow(out, row);
		}
		out << YAML::EndSeq;
	}

	static std::string LocalTimeNow()
	{
		const std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local = {};
#if defined(_WIN32)
		localtime_s(&local, &seconds);
#else
		localtime_r(&seconds, &local);
#endif
		char stamp[32] = {};
		std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &local);
		return stamp;
	}

	// ---- The parts of the session -------------------------------------------------------------------------

	static void WriteParty(YAML::Emitter& out, const PartyState& party)
	{
		out << YAML::Key << "Party" << YAML::Value << YAML::BeginSeq;
		for (const PartyMember& member : party.Members)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Hero" << YAML::Value << member.Hero;
			out << YAML::Key << "HP" << YAML::Value << member.HP;
			out << YAML::Key << "MP" << YAML::Value << member.MP;
			out << YAML::Key << "Level" << YAML::Value << member.Level;
			out << YAML::Key << "Experience" << YAML::Value << member.Experience;
			WriteRows(out, "Abilities", member.Abilities);
			WriteRows(out, "AbilitySlots", member.AbilitySlots);
			WriteRows(out, "Equipment", member.Equipment);
			out << YAML::Key << "LastCommand" << YAML::Value << member.LastCommand;
			out << YAML::Key << "LastAbility" << YAML::Value << member.LastAbility;
			out << YAML::Key << "Reserve" << YAML::Value << member.Reserve;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	// Members whose hero is gone from Heroes.hytable are left out; the line-up stays first and is never empty.
	static void ReadParty(const YAML::Node& node, PartyState& party)
	{
		party = PartyState{};
		party.Started = true;
		for (const YAML::Node& entry : node)
		{
			PartyMember member;
			member.Hero = entry["Hero"].as<std::string>(std::string());
			if (BattleData::HeroData(member.Hero).Name.empty())
			{
				HY_WARN("Save: the hero '{}' is not in Heroes.hytable any more; left out", member.Hero);
				continue;
			}
			member.HP = entry["HP"].as<int>(0);
			member.MP = entry["MP"].as<int>(0);
			member.Level = std::max(1, entry["Level"].as<int>(1));
			member.Experience = entry["Experience"].as<int>(0);
			for (const YAML::Node& row : entry["Abilities"])
			{
				member.Abilities.push_back(ReadRow(row));
			}
			size_t slot = 0;
			for (const YAML::Node& row : entry["AbilitySlots"])
			{
				if (slot < member.AbilitySlots.size())
				{
					member.AbilitySlots[slot++] = ReadRow(row);
				}
			}
			slot = 0;
			for (const YAML::Node& row : entry["Equipment"])
			{
				if (slot < member.Equipment.size())
				{
					member.Equipment[slot++] = ReadRow(row);
				}
			}
			member.LastCommand = entry["LastCommand"].as<int>(0);
			member.LastAbility = entry["LastAbility"].as<int>(0);
			member.Reserve = entry["Reserve"].as<bool>(false);
			party.Members.push_back(member);
		}
		std::stable_partition(party.Members.begin(), party.Members.end(),
			[](const PartyMember& member) { return member.Reserve == false; });
		if (party.Members.empty() == false && party.GetLineupSize() == 0)
		{
			party.Members.front().Reserve = false;
		}
	}

	static void WriteInventory(YAML::Emitter& out, const Inventory& inventory)
	{
		out << YAML::Key << "Inventory" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Money" << YAML::Value << inventory.Money;
		out << YAML::Key << "Items" << YAML::Value << YAML::BeginSeq;
		for (const Inventory::Slot& slot : inventory.Slots)
		{
			out << YAML::Flow << YAML::BeginMap << YAML::Key << "Item" << YAML::Value;
			WriteRow(out, slot.Item);
			out << YAML::Key << "Count" << YAML::Value << slot.Count << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::Key << "Found" << YAML::Value << YAML::BeginSeq;
		for (const std::string& key : inventory.Found)
		{
			out << key;
		}
		out << YAML::EndSeq << YAML::EndMap;
	}

	static void ReadInventory(const YAML::Node& node, Inventory& inventory)
	{
		inventory = Inventory{};
		inventory.Started = true; // the starting inventory is not given again
		inventory.Money = node["Money"].as<int>(0);
		for (const YAML::Node& entry : node["Items"])
		{
			inventory.Slots.push_back(Inventory::Slot{ ReadRow(entry["Item"]), entry["Count"].as<int>(0) });
		}
		for (const YAML::Node& key : node["Found"])
		{
			inventory.Found.push_back(key.as<std::string>(std::string()));
		}
	}

	static void WriteQuests(YAML::Emitter& out, const QuestLog& log)
	{
		out << YAML::Key << "Quests" << YAML::Value << YAML::BeginSeq;
		for (const QuestLog::Quest& quest : log.Quests)
		{
			out << YAML::Flow << YAML::BeginMap << YAML::Key << "Quest" << YAML::Value;
			WriteRow(out, quest.Definition);
			out << YAML::Key << "Stage" << YAML::Value << quest.Stage << YAML::Key << "Progress" << YAML::Value << quest.Progress
				<< YAML::Key << "Goal" << YAML::Value << quest.Goal << YAML::Key << "Done" << YAML::Value << quest.Done << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	// The NPCs a quest speaks for and the banners are left out: the scene's scripts claim them again.
	static void ReadQuests(const YAML::Node& node, QuestLog& log)
	{
		log = QuestLog{};
		for (const YAML::Node& entry : node)
		{
			QuestLog::Quest quest;
			quest.Definition = ReadRow(entry["Quest"]);
			quest.Stage = entry["Stage"].as<int>(0);
			quest.Progress = entry["Progress"].as<int>(0);
			quest.Goal = entry["Goal"].as<int>(0);
			quest.Done = entry["Done"].as<bool>(false);
			log.Quests.push_back(quest);
		}
		log.Revision++;
	}

	static void WriteRevealed(YAML::Emitter& out, const BattleState& battle)
	{
		out << YAML::Key << "Revealed" << YAML::Value << YAML::BeginMap;
		for (const auto& [row, found] : battle.Revealed)
		{
			out << YAML::Key << row << YAML::Value << YAML::Flow << YAML::BeginSeq << found[0] << found[1] << found[2] << YAML::EndSeq;
		}
		out << YAML::EndMap;
	}

	static void ReadRevealed(const YAML::Node& node, BattleState& battle)
	{
		battle = BattleState{};
		for (const auto& entry : node)
		{
			std::array<bool, 3> found = {};
			for (size_t w = 0; w < found.size() && w < entry.second.size(); w++)
			{
				found[w] = entry.second[w].as<bool>(false);
			}
			battle.Revealed[entry.first.as<std::string>(std::string())] = found;
		}
	}

	// A scene file name as words: "FarmVillageInn" -> "Farm Village Inn".
	static std::string Spaced(const std::string& name)
	{
		std::string words;
		for (size_t i = 0; i < name.size(); i++)
		{
			if (i > 0 && std::isupper((unsigned char)name[i]) != 0 && std::islower((unsigned char)name[i - 1]) != 0)
			{
				words += ' ';
			}
			words += name[i];
		}
		return words;
	}

	static bool ReadFile(int slot, YAML::Node& root)
	{
		const std::filesystem::path path = SaveGame::GetPath(slot);
		std::error_code error;
		if (std::filesystem::exists(path, error) == false)
		{
			return false;
		}
		try
		{
			root = YAML::LoadFile(path.string())["SaveGame"];
		}
		catch (const YAML::Exception& exception)
		{
			HY_WARN("Save: {} can't be read ({})", path.string(), exception.what());
			return false;
		}
		if (root.IsMap() == false)
		{
			HY_WARN("Save: {} is not a save file", path.string());
			return false;
		}
		return true;
	}

	// ---- SaveGame ---------------------------------------------------------------------------------------------

	std::filesystem::path SaveGame::GetPath(int slot)
	{
		const Ref<Project> project = Project::GetActive();
		const std::filesystem::path folder = project != nullptr ? project->GetProjectDirectory() / "Saves" : std::filesystem::path("Saves");
		return folder / ("Slot" + std::to_string(slot) + ".hysave");
	}

	bool SaveGame::Exists(int slot)
	{
		std::error_code error;
		return std::filesystem::exists(GetPath(slot), error);
	}

	SaveGame::Summary SaveGame::ReadSummary(int slot)
	{
		Summary summary;
		YAML::Node root;
		if (ReadFile(slot, root) == false)
		{
			return summary;
		}
		summary.Exists = true;
		summary.Scene = Spaced(root["Scene"].as<std::string>(std::string()));
		summary.Saved = root["Saved"].as<std::string>(std::string());
		summary.Money = root["Inventory"]["Money"].as<int>(0);
		for (const YAML::Node& member : root["Party"])
		{
			if (member["Reserve"].as<bool>(false))
			{
				continue;
			}
			const std::string name = BattleData::HeroData(member["Hero"].as<std::string>(std::string())).Name;
			summary.Party += (summary.Party.empty() ? "" : ", ") + name + " Lv " + std::to_string(member["Level"].as<int>(1));
		}
		return summary;
	}

	bool SaveGame::Write(const SavePlace& place, int slot)
	{
		RpgSession& session = RpgSession::Get();
		YAML::Emitter out;
		out << YAML::BeginMap << YAML::Key << "SaveGame" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << Version;
		out << YAML::Key << "Saved" << YAML::Value << LocalTimeNow();
		out << YAML::Key << "Scene" << YAML::Value << place.Scene;
		out << YAML::Key << "Position" << YAML::Value << YAML::Flow << YAML::BeginSeq << place.Position.x << place.Position.y
			<< place.Position.z << YAML::EndSeq;
		out << YAML::Key << "LightingPreset" << YAML::Value << session.LastLightingPreset;
		WriteParty(out, session.Party);
		WriteInventory(out, session.Items);
		WriteQuests(out, session.Quests);
		WriteRevealed(out, session.Battle);
		out << YAML::EndMap << YAML::EndMap;

		const std::filesystem::path path = GetPath(slot);
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file << out.c_str() << "\n";
		if (file.good() == false)
		{
			HY_WARN("Save: could not write {}", path.string());
			return false;
		}
		HY_INFO("Save: slot {} written ({} in {})", slot, place.Scene, path.string());
		return true;
	}

	bool SaveGame::Load(int slot, SavePlace& place)
	{
		YAML::Node root;
		if (ReadFile(slot, root) == false)
		{
			return false;
		}
		if (root["Version"].as<int>(0) > Version)
		{
			HY_WARN("Save: slot {} was made by a newer build (version {})", slot, root["Version"].as<int>(0));
			return false;
		}
		place.Scene = root["Scene"].as<std::string>(std::string());
		const YAML::Node position = root["Position"];
		if (position.IsSequence() && position.size() == 3)
		{
			place.Position = { position[0].as<float>(0.0f), position[1].as<float>(0.0f), position[2].as<float>(0.0f) };
		}
		if (place.Scene.empty())
		{
			HY_WARN("Save: slot {} names no scene", slot);
			return false;
		}

		RpgSession& session = RpgSession::Get();
		ReadParty(root["Party"], session.Party);
		ReadInventory(root["Inventory"], session.Items);
		ReadQuests(root["Quests"], session.Quests);
		ReadRevealed(root["Revealed"], session.Battle);
		session.LastLightingPreset = root["LightingPreset"].as<int>(-1);
		session.Arrival = place;
		HY_INFO("Save: slot {} loaded ({})", slot, place.Scene);
		return true;
	}

	bool SaveGame::TakeArrival(const std::string& scene, glm::vec3& position)
	{
		SavePlace& arrival = RpgSession::Get().Arrival;
		if (arrival.Scene.empty() || arrival.Scene != scene)
		{
			return false;
		}
		position = arrival.Position;
		arrival = SavePlace{};
		return true;
	}

	bool SaveGame::Delete(int slot)
	{
		std::error_code error;
		const bool removed = std::filesystem::remove(GetPath(slot), error);
		if (removed)
		{
			HY_INFO("Save: slot {} deleted", slot);
		}
		return removed;
	}
}
