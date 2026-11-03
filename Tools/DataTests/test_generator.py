#!/usr/bin/env python3
"""Checks that the reflection generator rejects what HY_DATA / HY_ENUM don't accept, with the file and line.

Run by scripts/test_data.sh. Each case is a header written to a temporary folder and run through
generate_reflection.py; it must fail with every expected fragment in its error.
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "Tools" / "ReflectionGenerator" / "generate_reflection.py"

ITEM = """
HY_DATA(Table)
struct ItemDefinition
{
	HY_PROPERTY()
	std::string Name;
};
"""

# (name, header, fragments the error must contain). Line numbers count from the header's first line.
FAILING = [
    ("unknown HY_DATA argument", "\nHY_DATA(Definition, Shape(Round))\nstruct A\n{\n};\n", ["Bad.h:2", "unsupported argument 'Shape(Round)'"]),
    ("lowercase argument", "\nHY_DATA(table)\nstruct A\n{\n};\n", ["Bad.h:2", "unsupported argument 'table'"]),
    ("Definition twice", "HY_DATA(Definition, Definition)\nstruct A\n{\n};\n", ["Bad.h:1", "appears twice"]),
    ("DisplayField without a field", "HY_DATA(Table, DisplayField())\nstruct A\n{\n};\n", ["Bad.h:1", "unsupported argument 'DisplayField()'"]),
    ("DisplayField not a string", "HY_DATA(Table, DisplayField(Count))\nstruct A\n{\n\tHY_PROPERTY()\n\tint Count = 0;\n};\n", ["Bad.h:1", "not a std::string"]),
    ("DisplayField on a definition", "HY_DATA(Definition, DisplayField(Name))\nstruct A\n{\n\tHY_PROPERTY()\n\tstd::string Name;\n};\n", ["DisplayField(...) is for table types"]),
    ("inheritance", "HY_DATA()\nstruct A : public B\n{\n};\n", ["Bad.h:1", "can't derive"]),
    ("not above a struct", "HY_DATA()\nint x = 0;\n", ["Bad.h:1", "must be right above a struct"]),
    ("unknown field type", "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY()\n\tdouble Weight = 0.0;\n};\n", ["Bad.h:4", "unsupported data field type 'double'"]),
    ("list of lists", "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY()\n\tstd::vector<std::vector<int>> Grid;\n};\n", ["lists of lists"]),
    ("struct not HY_DATA", "struct B {};\nHY_DATA()\nstruct A\n{\n\tHY_PROPERTY()\n\tB Inner;\n};\n", ["Bad.h:5", "unsupported data field type 'B'"]),
    ("row ref without DataRow", ITEM + "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY()\n\tDataRowRef Item;\n};\n", ["needs DataRow"]),
    ("DataRow of a non-table", "HY_DATA(Definition)\nstruct D\n{\n};\nHY_DATA()\nstruct A\n{\n\tHY_PROPERTY(DataRow(D))\n\tDataRowRef Item;\n};\n", ["DataRow(D): not an HY_DATA(Table) type"]),
    ("Range on a string", "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY(Range(0, 1))\n\tstd::string Name;\n};\n", ["Range / Min / Max need"]),
    ("private field", "HY_DATA()\nclass A\n{\n\tHY_PROPERTY()\n\tint Hidden = 0;\n};\n", ["Bad.h:4", "must be public"]),
    ("plain enum", "HY_ENUM()\nenum Kind { A, B };\n", ["Bad.h:1", "enum class"]),
    ("HY_ENUM arguments", "HY_ENUM(Flags)\nenum class Kind { A };\n", ["Bad.h:1", "takes no arguments"]),
    ("name clash", "HY_ENUM()\nenum class A { X };\nHY_DATA()\nstruct A\n{\n};\n", ["Duplicate reflected name 'A'"]),
    ("Asset(Definition) of a table type", ITEM + "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY(Asset(Definition, ItemDefinition))\n\tAssetHandle Def = 0;\n};\n", ["Asset(Definition, ItemDefinition): not an HY_DATA(Definition) type"]),
    ("data type on a texture", "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY(Asset(Texture2D, A))\n\tAssetHandle Icon = 0;\n};\n", ["only Definition and DataTable take a data type"]),
    ("script row ref without DataRow", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY()\n\tDataRowRef Item;\n};\n", ["Bad.h:5", "needs DataRow"]),
    ("script DataRow of an unknown type", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY(DataRow(Nothing))\n\tDataRowRef Item;\n};\n", ["DataRow(Nothing): not an HY_DATA(Table) type"]),
    # Former names (SC1): only FormerName in HY_SCRIPT; names, not clashing with current or other former names.
    ("HY_SCRIPT argument", "HY_SCRIPT(Hidden)\nclass S : public ScriptableEntity\n{\n};\n", ["Bad.h:1", "HY_SCRIPT accepts only FormerName"]),
    ("FormerName without a name", "HY_SCRIPT(FormerName())\nclass S : public ScriptableEntity\n{\n};\n", ["Bad.h:1", "FormerName needs at least one name"]),
    ("FormerName not a name", "HY_SCRIPT(FormerName(\"Old Script\"))\nclass S : public ScriptableEntity\n{\n};\n", ["not a C++ name"]),
    ("FormerName is its own name", "HY_SCRIPT(FormerName(\"S\"))\nclass S : public ScriptableEntity\n{\n};\n", ["is its own current name"]),
    ("script FormerName is another script", "HY_SCRIPT()\nclass A : public ScriptableEntity\n{\n};\n" + "HY_SCRIPT(FormerName(\"A\"))\nclass B : public ScriptableEntity\n{\n};\n", ["Bad.h:5", "a script 'A' exists"]),
    ("script FormerName shared", "HY_SCRIPT(FormerName(\"Old\"))\nclass A : public ScriptableEntity\n{\n};\nHY_SCRIPT(FormerName(\"Old\"))\nclass B : public ScriptableEntity\n{\n};\n", ["also a former name of 'A'"]),
    ("property FormerName is another property", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY()\n\tfloat Speed = 1.0f;\n\tHY_PROPERTY(FormerName(\"Speed\"))\n\tfloat Pace = 1.0f;\n};\n", ["Bad.h:7", "S's property 'Speed' exists"]),
    ("data type FormerName is another type", ITEM + "HY_DATA(Table, FormerName(\"ItemDefinition\"))\nstruct Loot\n{\n};\n", ["a data type 'ItemDefinition' exists"]),
    ("data field FormerName shared", "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY(FormerName(\"Old\"))\n\tint X = 0;\n\tHY_PROPERTY(FormerName(\"Old\"))\n\tint Y = 0;\n};\n", ["also a former name of 'X'"]),
    ("HY_DATA FormerName without a name", "HY_DATA(FormerName())\nstruct A\n{\n};\n", ["FormerName needs at least one name"]),
    ("script Asset(DataTable) of a definition", "HY_DATA(Definition)\nstruct D\n{\n};\nHY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY(Asset(DataTable, D))\n\tAssetHandle Table = 0;\n};\n", ["Asset(DataTable, ...)(D): not an HY_DATA(Table) type"]),
    ("Required on a float", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY(Required)\n\tfloat Speed = 1.0f;\n};\n", ["Bad.h:5", "Required needs an AssetHandle, DataRowRef, EntityRef or ComponentRef<T> field"]),
    ("Required with a value", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY(Required(true))\n\tEntityRef Door;\n};\n", ["Required(...) takes 0 argument(s)"]),
    ("EntityRef in data", "HY_DATA()\nstruct A\n{\n\tHY_PROPERTY()\n\tEntityRef Door;\n};\n", ["Bad.h:4", "unsupported data field type 'EntityRef'"]),
    ("other template", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY()\n\tstd::optional<int> Count;\n};\n", ["unsupported property type 'std::optional<int>'"]),
    # Lists, enums and structs on scripts (Nov 3, 2026)
    ("list of an unknown type", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY()\n\tstd::vector<Thing> Things;\n};\n", ["S.Things", "unsupported list element 'Thing'"]),
    ("list of ComponentRef", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY()\n\tstd::vector<ComponentRef<S>> Others;\n};\n", ["lists of ComponentRef<T> are not supported"]),
    ("Range on a list of text", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY(Range(0, 1))\n\tstd::vector<std::string> Names;\n};\n", ["Range / Min / Max need an int, float or vector field (or a list of them)"]),
    ("Required on a list", "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY(Required)\n\tstd::vector<EntityRef> Stops;\n};\n", ["Required needs an AssetHandle"]),
    ("list of rows without DataRow", ITEM + "HY_SCRIPT()\nclass S : public ScriptableEntity\n{\npublic:\n\tHY_PROPERTY()\n\tstd::vector<DataRowRef> Stock;\n};\n", ["a DataRowRef needs DataRow(<row type>)"]),
]

# (name, header, fragments the generated script / data registration must contain)
PASSING = [
    ("former names", """
HY_SCRIPT(FormerName("OldDoor", "OlderDoor"))
class Door : public ScriptableEntity
{
public:
	HY_PROPERTY(Range(0, 5), FormerName("Speed"))
	float OpenSpeed = 1.0f;
};

HY_DATA(Table, FormerName("LootDefinition"))
struct ItemDefinition
{
	HY_PROPERTY(FormerName("Title"), Tooltip("Shown in lists"))
	std::string Name;
};
""", [
        'ScriptRegistry::Register<Door>("Door", { "OldDoor", "OlderDoor" })',
        '.Range(0.0f, 5.0f).FormerName("Speed")',
        'DataTypeRegistry::Register<ItemDefinition>("ItemDefinition", "Item", { "LootDefinition" })',
        '.Tooltip("Shown in lists").FormerName("Title")',
    ]),
    ("references", """
HY_SCRIPT()
class Lever : public ScriptableEntity
{
public:
	HY_PROPERTY(Required, Tooltip("What it opens"))
	EntityRef Target;
	HY_PROPERTY()
	ComponentRef<DialogueScript> Dialogue;
	HY_PROPERTY(Required)
	Hydro::ComponentRef< Hydro::TransformComponent > Anchor;
	HY_PROPERTY(Required, Asset(Texture2D))
	AssetHandle Icon = 0;
};
""", [
        '.Property("Target", "Target", &Lever::Target).Tooltip("What it opens").Required()',
        '.Property("Dialogue", "Dialogue", &Lever::Dialogue).TargetType("DialogueScript")',
        '.Property("Anchor", "Anchor", &Lever::Anchor).Required().TargetType("TransformComponent")',
        '.AssetFilter(AssetType::Texture2D).Required()',
    ]),
    ("script lists, enums and structs", ITEM + """
HY_ENUM()
enum class RouteMode : uint8_t { Loop, PingPong };

HY_DATA()
struct Stop
{
	HY_PROPERTY(Range(0, 60))
	float Wait = 0.0f;
};

HY_SCRIPT()
class Route : public ScriptableEntity
{
public:
	HY_PROPERTY(Tooltip("How it repeats"))
	RouteMode Mode = RouteMode::Loop;
	HY_PROPERTY()
	std::vector<EntityRef> Waypoints;
	HY_PROPERTY(DataRow(ItemDefinition))
	std::vector<DataRowRef> Stock;
	HY_PROPERTY(Range(0, 10))
	std::vector<float> Waits;
	HY_PROPERTY()
	Stop First;
	HY_PROPERTY()
	std::vector< Stop > Stops;
};
""", [
        '.Property("Mode", "Mode", &Route::Mode).Tooltip("How it repeats")',
        '.Property("Waypoints", "Waypoints", &Route::Waypoints)',
        '.Property("Stock", "Stock", &Route::Stock).DataTypeFilter("ItemDefinition")',
        '.Property("Waits", "Waits", &Route::Waits).Range(0.0f, 10.0f)',
        '.Property("First", "First", &Route::First)',
        '.Property("Stops", "Stops", &Route::Stops)',
    ]),
    ("gizmos", """
HY_SCRIPT()
class Zone : public ScriptableEntity
{
public:
	HY_PROPERTY()
	float Radius = 1.0f;
protected:
	void OnDrawGizmos(Gizmos& gizmos) override { }
};
""", ['ScriptRegistry::Register<Zone>("Zone").DrawsGizmos()']),
    ("all accepted forms", ITEM + """
HY_ENUM()
enum class Kind : uint8_t { A, B = 4, C, };

HY_DATA()
struct Entry
{
	HY_PROPERTY(DataRow(ItemDefinition))
	DataRowRef Item;
	HY_PROPERTY(Range(0, 9))
	std::vector<int> Counts;
	HY_PROPERTY()
	std::vector<Kind> Kinds;
};

HY_SCRIPT()
class Chest : public ScriptableEntity
{
public:
	HY_PROPERTY(DataRow(ItemDefinition))
	DataRowRef Item;
	HY_PROPERTY(Asset(DataTable, ItemDefinition))
	AssetHandle Items = 0;
	HY_PROPERTY(Asset(Definition, Both))
	AssetHandle Tuning = 0;
	HY_PROPERTY(Asset(Definition))
	AssetHandle AnyDefinition = 0;
};

HY_DATA( Definition , Table , DisplayField( Title ) )
struct Both
{
	HY_PROPERTY()
	std::string Title;
	HY_PROPERTY()
	std::vector< Entry > Entries;
	HY_PROPERTY()
	Entry Single;
	HY_PROPERTY()
	Kind Choice = Kind::C;
};
""", []),
]


def run(header: str) -> tuple[subprocess.CompletedProcess, str]:
    with tempfile.TemporaryDirectory() as folder:
        source = Path(folder) / "src"
        source.mkdir()
        (source / "Bad.h").write_text(header, encoding="utf-8")
        result = subprocess.run(
            [
                sys.executable,
                str(GENERATOR),
                str(ROOT),
                "--source",
                str(source),
                "--output",
                str(Path(folder) / "S.cpp"),
                "--data-output",
                str(Path(folder) / "D.cpp"),
            ],
            capture_output=True,
            text=True,
        )
        generated = "".join((Path(folder) / name).read_text(encoding="utf-8") for name in ("S.cpp", "D.cpp") if (Path(folder) / name).exists())
        return result, generated


def main() -> int:
    failures = 0
    for name, header, fragments in FAILING:
        result, _ = run(header)
        missing = [fragment for fragment in fragments if fragment not in result.stderr]
        if result.returncode == 0 or missing:
            failures += 1
            print(f"FAIL  {name}: exit {result.returncode}, missing {missing}\n      {result.stderr.strip()}")
        else:
            print(f"ok    {name}: {result.stderr.strip().split(': ', 1)[-1]}")

    for name, header, fragments in PASSING:
        result, generated = run(header)
        missing = [fragment for fragment in fragments if fragment not in generated]
        if result.returncode != 0 or missing:
            failures += 1
            print(f"FAIL  {name}: {result.stderr.strip()} missing {missing}")
        else:
            print(f"ok    {name}")

    print(f"{len(FAILING) + len(PASSING)} generator cases, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
