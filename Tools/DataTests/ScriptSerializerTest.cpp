// Tools/DataTests/ScriptSerializerTest.cpp
// Script bindings in scene files (ScriptSerializer): former names (SC1) and saved values no property reads (#143).
// Built into DataRoundTripTest by scripts/test_data.sh; no scene, window or GPU.

#include "hypch.h"
#include "Hydro/Scene/Scripting/ScriptSerializer.h"

#include <cstdio>

namespace Hydro
{
	class TestDoorScript : public ScriptableEntity
	{
	public:
		float OpenSpeed = 1.0f;
		std::string Label = "Door";
	};

	static int s_ScriptFailures = 0;

	static void ScriptCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_ScriptFailures++;
			std::printf("FAIL  ScriptSerializerTest.cpp line %d: %s\n", line, what);
		}
	}

#define SCRIPT_CHECK(condition) ScriptCheck((condition), #condition, __LINE__, checks)

	static std::string WriteBindings(const NativeScriptComponent& scripts)
	{
		YAML::Emitter out;
		ScriptSerializer::Write(out, scripts, "Test");
		return std::string(out.c_str()) + "\n";
	}

	static float FloatOf(const NativeScriptComponent::Script& script, const std::string& name)
	{
		auto it = script.Properties.find(name);
		return it != script.Properties.end() && std::holds_alternative<float>(it->second) ? std::get<float>(it->second) : -1.0f;
	}

	static std::string StringOf(const NativeScriptComponent::Script& script, const std::string& name)
	{
		auto it = script.Properties.find(name);
		return it != script.Properties.end() && std::holds_alternative<std::string>(it->second) ? std::get<std::string>(it->second) : std::string();
	}

	// Returns the failures; adds to `checks`.
	int TestScriptSerializer(int& checks)
	{
		std::printf("Script bindings: former names and kept values\n");

		ScriptRegistry::Register<TestDoorScript>("TestDoor", { "OldTestDoor" })
			.Property("OpenSpeed", "Open Speed", &TestDoorScript::OpenSpeed).FormerName("Speed")
			.Property("Label", "Label", &TestDoorScript::Label);

		SCRIPT_CHECK(ScriptRegistry::FindType("OldTestDoor") != nullptr && ScriptRegistry::FindType("OldTestDoor")->Name == "TestDoor");
		SCRIPT_CHECK(ScriptRegistry::Find("OldTestDoor") != nullptr && ScriptRegistry::Find("Nothing") == nullptr);

		// Bind by a former name: the current name, the factory and the defaults.
		NativeScriptComponent bound;
		bound.Bind("OldTestDoor");
		SCRIPT_CHECK(bound.Scripts.back().Name == "TestDoor" && bound.Scripts.back().Instantiate != nullptr);
		SCRIPT_CHECK(FloatOf(bound.Scripts.back(), "OpenSpeed") == 1.0f && StringOf(bound.Scripts.back(), "Label") == "Door");

		const YAML::Node saved = YAML::Load(
			"- Name: OldTestDoor\n"
			"  Properties:\n"
			"    Speed: 2.5\n"
			"    Label: Gate\n"
			"    Squeak: true\n"
			"- Name: GoneScript\n"
			"  Properties:\n"
			"    Power: 9000\n"
			"    Where: [1, 2, 3]\n"
			"- PlainName\n");

		NativeScriptComponent scripts;
		ScriptSerializer::Read(saved, scripts, "Test");
		SCRIPT_CHECK(scripts.Scripts.size() == 3);
		if (scripts.Scripts.size() != 3)
		{
			return s_ScriptFailures;
		}

		const NativeScriptComponent::Script& door = scripts.Scripts[0];
		SCRIPT_CHECK(door.Name == "TestDoor" && door.Instantiate != nullptr);
		SCRIPT_CHECK(FloatOf(door, "OpenSpeed") == 2.5f && StringOf(door, "Label") == "Gate");
		SCRIPT_CHECK(door.KeptProperties.size() == 1 && door.KeptProperties.count("Squeak") == 1);

		const NativeScriptComponent::Script& gone = scripts.Scripts[1];
		SCRIPT_CHECK(gone.Name == "GoneScript" && gone.Instantiate == nullptr && gone.Properties.empty());
		SCRIPT_CHECK(gone.KeptProperties.size() == 2 && gone.KeptProperties.count("Power") == 1 && gone.KeptProperties.count("Where") == 1);
		SCRIPT_CHECK(scripts.Scripts[2].Name == "PlainName" && scripts.Scripts[2].KeptProperties.empty());

		// Written under the current names, the kept values among the rest (sorted), nothing lost.
		const std::string written = WriteBindings(scripts);
		std::printf("%s", written.c_str());
		const YAML::Node reread = YAML::Load(written);
		SCRIPT_CHECK(reread[0]["Name"].as<std::string>() == "TestDoor");
		SCRIPT_CHECK(reread[0]["Properties"]["OpenSpeed"].as<float>() == 2.5f && reread[0]["Properties"]["Speed"].IsDefined() == false);
		SCRIPT_CHECK(reread[0]["Properties"]["Squeak"].as<bool>() == true && reread[0]["Properties"]["Label"].as<std::string>() == "Gate");
		SCRIPT_CHECK(reread[1]["Name"].as<std::string>() == "GoneScript" && reread[1]["Properties"]["Power"].as<int>() == 9000);
		SCRIPT_CHECK(reread[1]["Properties"]["Where"].IsSequence() && reread[1]["Properties"]["Where"].size() == 3 && reread[1]["Properties"]["Where"][2].as<int>() == 3);
		SCRIPT_CHECK(reread[2]["Name"].as<std::string>() == "PlainName" && reread[2]["Properties"].IsDefined() == false);
		SCRIPT_CHECK(written.find("Label: Gate\n    OpenSpeed: 2.5\n    Squeak: true") != std::string::npos);

		// Reading what was written writes the same text.
		NativeScriptComponent again;
		ScriptSerializer::Read(reread, again, "Test");
		SCRIPT_CHECK(WriteBindings(again) == written);

		// The current key and a former key both saved: the current one is used, the former one is kept.
		NativeScriptComponent both;
		ScriptSerializer::Read(YAML::Load("- Name: TestDoor\n  Properties: { Speed: 1, OpenSpeed: 3 }\n"), both, "Test");
		SCRIPT_CHECK(FloatOf(both.Scripts[0], "OpenSpeed") == 3.0f && both.Scripts[0].KeptProperties.count("Speed") == 1);

		return s_ScriptFailures;
	}
}
