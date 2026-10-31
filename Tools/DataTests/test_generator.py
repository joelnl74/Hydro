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
]

PASSING = [
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
"""),
]


def run(header: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as folder:
        source = Path(folder) / "src"
        source.mkdir()
        (source / "Bad.h").write_text(header, encoding="utf-8")
        return subprocess.run(
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


def main() -> int:
    failures = 0
    for name, header, fragments in FAILING:
        result = run(header)
        missing = [fragment for fragment in fragments if fragment not in result.stderr]
        if result.returncode == 0 or missing:
            failures += 1
            print(f"FAIL  {name}: exit {result.returncode}, missing {missing}\n      {result.stderr.strip()}")
        else:
            print(f"ok    {name}: {result.stderr.strip().split(': ', 1)[-1]}")

    for name, header in PASSING:
        result = run(header)
        if result.returncode != 0:
            failures += 1
            print(f"FAIL  {name}: {result.stderr.strip()}")
        else:
            print(f"ok    {name}")

    print(f"{len(FAILING) + len(PASSING)} generator cases, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
