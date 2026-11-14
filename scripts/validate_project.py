#!/usr/bin/env python3
"""Checks a project's scenes, prefabs and data files without the engine or a GPU (Scripting plan SC5).

    python scripts/validate_project.py [--workspace <dir>] [project folder]   # default: the workspace's DefaultProject

What scripts and data types exist comes from the code itself: the reflection generator reads the HY_SCRIPT,
HY_PROPERTY and HY_DATA markers in Hydro/src and the workspace's game code (hydro-workspace.json), as the build does. Then every scene (.hyscene), prefab
(.hyprefab), definition (.hydef) and data table (.hytable) under the project's Assets/ is read.

Errors (exit 1):
  - a script that isn't registered under that name or a former name
  - a script bound twice to one entity (an entity holds one script of a class)
  - a property value of the wrong type (a string where a number goes, a vector with the wrong length...)
  - a Required asset, row or reference left empty
  - an EntityRef / ComponentRef<T> to an entity that isn't in the file, or a ComponentRef<T> whose entity has no T
  - an asset handle no file of the project has, or an asset of the wrong kind
  - a row reference to a missing table or row (in scenes and in data files)
  - a data file whose type isn't registered
Warnings (reported, exit 0): a saved property no property reads now (the engine keeps it, #143), and a former name
still in use (save the file in the editor to write the current one).

Needs Python 3.9+ and PyYAML (pip install pyyaml).
"""
from __future__ import annotations

import importlib.util
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    import yaml
except ImportError:
    print("validate_project.py needs PyYAML: pip install pyyaml", file=sys.stderr)
    sys.exit(2)

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import hydro_workspace  # noqa: E402

WORKSPACE_DIRECTORY, ARGUMENTS = hydro_workspace.take_workspace_argument(sys.argv[1:])
WORKSPACE = hydro_workspace.load(WORKSPACE_DIRECTORY)

# AssetType by file extension (Hydro/src/Hydro/Asset/Asset.cpp, GetAssetTypeFromExtension).
ASSET_TYPES = {
    ".png": "Texture2D", ".jpg": "Texture2D", ".jpeg": "Texture2D", ".tga": "Texture2D", ".bmp": "Texture2D",
    ".hyscene": "Scene", ".hyprefab": "Prefab", ".hyanim": "SpriteAnimation", ".hymat": "Material",
    ".gltf": "Mesh", ".glb": "Mesh", ".hyrender": "RenderProfile", ".fnt": "Font", ".hyterrain": "Terrain",
    ".hydef": "Definition", ".hytable": "DataTable", ".frag": "Shader",
}


def load_generator():
    path = ROOT / "Tools" / "ReflectionGenerator" / "generate_reflection.py"
    spec = importlib.util.spec_from_file_location("generate_reflection", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules["generate_reflection"] = module
    spec.loader.exec_module(module)
    return module


@dataclass
class Reflection:
    scripts: dict = field(default_factory=dict)  # current name -> ReflectedScript
    former_scripts: dict = field(default_factory=dict)  # former name -> current name
    data_types: dict = field(default_factory=dict)  # name -> ReflectedData (HY_COMPONENT structs too: .component)
    former_data: dict = field(default_factory=dict)
    bases: dict = field(default_factory=dict)  # class -> its base class


def read_reflection() -> Reflection:
    gen = load_generator()
    roots = [ROOT / "Hydro" / "src", WORKSPACE.game_source]
    scripts, data_types, enums = [], [], []
    bases: dict[str, str] = {}
    for root in roots:
        for header in gen.iter_headers(root):
            parsed = gen.parse_header(header, gen.include_path_for(header, roots))
            scripts.extend(parsed.scripts)
            data_types.extend(parsed.data)
            enums.extend(parsed.enums)
            text = gen.strip_comments(header.read_text(encoding="utf-8", errors="replace"))
            for match in re.finditer(r"\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?:\s*(?:public\s+)?([\w:]+)", text):
                bases[match.group(1)] = match.group(2).split("::")[-1]
    gen.resolve_data(data_types, enums, scripts)

    reflection = Reflection(bases=bases)
    for script in scripts:
        reflection.scripts[script.class_name] = script
        for former in script.former_names:
            reflection.former_scripts[former] = script.class_name
    for data in data_types:
        reflection.data_types[data.struct_name] = data
        for former in data.former_names:
            reflection.former_data[former] = data.struct_name
    return reflection


class Report:
    def __init__(self) -> None:
        self.errors = 0
        self.warnings = 0

    def error(self, where: str, message: str) -> None:
        self.errors += 1
        print(f"error    {where}: {message}")

    def warning(self, where: str, message: str) -> None:
        self.warnings += 1
        print(f"warning  {where}: {message}")


def read_yaml(path: Path):
    with open(path, encoding="utf-8") as file:
        return yaml.safe_load(file)


def index_assets(assets: Path, report: Report):
    """Every asset handle of the project -> (path, AssetType), and each data table's rows."""
    handles: dict[int, tuple[Path, str]] = {}
    tables: dict[int, set[str]] = {}
    for path in sorted(assets.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix == ".hymeta":
            source = path.with_suffix("")
            match = re.search(r"^Handle:\s*(\d+)", path.read_text(encoding="utf-8", errors="replace"), flags=re.M)
            if match is not None:
                handles[int(match.group(1))] = (source, ASSET_TYPES.get(source.suffix.lower(), "Unknown"))
            continue
        kind = ASSET_TYPES.get(path.suffix.lower())
        if kind is None or kind in ("Texture2D", "Mesh", "Font", "Shader"):
            continue  # imported files keep their handle in a .hymeta
        with open(path, encoding="utf-8", errors="replace") as file:
            head = "".join(file.readline() for _ in range(8))  # after any comment lines at the top
        match = re.search(r"^Handle:\s*(\d+)", head, flags=re.M)
        if match is None:
            continue
        handle = int(match.group(1))
        if handle in handles:
            report.error(str(path.relative_to(assets)), f"handle {handle} is also {handles[handle][0].relative_to(assets)}'s")
        handles[handle] = (path, kind)
        if kind == "DataTable":
            try:
                table = (read_yaml(path) or {}).get("DataTable") or {}
                tables[handle] = set((table.get("Rows") or {}).keys())
            except yaml.YAMLError as exc:
                report.error(str(path.relative_to(assets)), f"not valid YAML: {exc}")
    return handles, tables


class Checker:
    def __init__(self, reflection: Reflection, handles, tables, report: Report, assets: Path):
        self.reflection = reflection
        self.handles = handles
        self.tables = tables
        self.report = report
        self.assets = assets

    # ---- Values ----------------------------------------------------------------------------------------------
    def is_number(self, value) -> bool:
        return isinstance(value, (int, float)) and not isinstance(value, bool)

    def check_asset(self, where: str, value, asset_type: str | None, required: bool) -> None:
        if not isinstance(value, int) or isinstance(value, bool):
            self.report.error(where, f"an asset handle must be a number, got {value!r}")
            return
        if value == 0:
            if required:
                self.report.error(where, "Required: no asset set")
            return
        found = self.handles.get(value)
        if found is None:
            self.report.error(where, f"asset {value} isn't in the project (deleted, or its .hymeta was lost)")
        elif asset_type is not None and found[1] != asset_type:
            self.report.error(where, f"asset {value} is a {found[1]} ({found[0].relative_to(self.assets)}), not a {asset_type}")

    def check_row(self, where: str, value, required: bool) -> None:
        if not isinstance(value, dict):
            self.report.error(where, f"a row reference is {{ Table: <handle>, Row: <id> }}, got {value!r}")
            return
        table, row = value.get("Table", 0), value.get("Row", "")
        if table in (0, None) and row in ("", None):
            if required:
                self.report.error(where, "Required: no row set")
            return
        if table not in self.tables:
            self.report.error(where, f"row '{row}' of table {table}, which isn't a data table of the project")
        elif str(row) not in self.tables[table]:
            self.report.error(where, f"table {self.handles[table][0].relative_to(self.assets)} has no row '{row}'")

    def check_value(self, where: str, kind: str, value, prop) -> bool:
        """Checks a scalar kind; False when the kind isn't one of these."""
        vectors = {"Vec2": 2, "Vec3": 3, "Vec4": 4}
        if kind == "Bool":
            if not isinstance(value, bool):
                self.report.error(where, f"expected true / false, got {value!r}")
        elif kind == "Int":
            if not isinstance(value, int) or isinstance(value, bool):
                self.report.error(where, f"expected a whole number, got {value!r}")
        elif kind == "Float":
            if not self.is_number(value):
                self.report.error(where, f"expected a number, got {value!r}")
        elif kind in vectors:
            if not isinstance(value, list) or len(value) != vectors[kind] or not all(self.is_number(v) for v in value):
                self.report.error(where, f"expected {vectors[kind]} numbers, got {value!r}")
        elif kind == "String":
            if isinstance(value, (dict, list)):
                self.report.error(where, f"expected text, got {value!r}")
        elif kind == "Asset":
            self.check_asset(where, value, prop.asset_type, getattr(prop, "required", False))
        elif kind == "RowRef":
            self.check_row(where, value, getattr(prop, "required", False))
        else:
            return False
        return True

    # ---- Scenes and prefabs ----------------------------------------------------------------------------------
    def entity_has(self, entity: dict, target: str) -> bool:
        """The entity has a component of that name, or binds a script of that class or one deriving from it."""
        if target in entity:
            return True
        for binding in ((entity.get("NativeScriptComponent") or {}).get("Scripts") or []):
            name = binding if isinstance(binding, str) else (binding or {}).get("Name", "")
            name = self.reflection.former_scripts.get(name, name)
            seen = set()
            while name and name not in seen:
                if name == target:
                    return True
                seen.add(name)
                name = self.reflection.bases.get(name)
        return False

    def check_entities(self, file_label: str, entities: list) -> None:
        by_id = {}
        for entity in entities:
            if isinstance(entity, dict) and "Entity" in entity:
                by_id[entity["Entity"]] = entity
        self.by_id = by_id  # for EntityRef fields of the game's components
        components = [d for d in self.reflection.data_types.values() if getattr(d, "component", False)]

        for entity in entities:
            if not isinstance(entity, dict):
                continue
            tag = ((entity.get("TagComponent") or {}).get("Tag")) or "?"
            label = f"{file_label}: '{tag}'"
            # The game's components (HY_COMPONENT): their fields, as a data record's
            for component in components:
                key = next((k for k in [component.struct_name] + component.former_names if k in entity), None)
                if key is None:
                    continue
                if key != component.struct_name:
                    self.report.warning(f"{label} {component.struct_name}", f"saved under its former name '{key}'")
                self.check_record(f"{label} {component.struct_name}", component, entity[key])
            scripts = (entity.get("NativeScriptComponent") or {}).get("Scripts") or []
            bound: set[str] = set()
            for binding in scripts:
                if isinstance(binding, str):
                    saved, values = binding, {}
                elif isinstance(binding, dict):
                    saved, values = binding.get("Name", ""), binding.get("Properties") or {}
                else:
                    continue
                name = self.reflection.former_scripts.get(saved, saved)
                where = f"{label} {name}"
                script = self.reflection.scripts.get(name)
                if script is None:
                    self.report.error(where, "no script is registered under this name (renamed without FormerName, or deleted)")
                    continue
                if saved != name:
                    self.report.warning(where, f"saved under its former name '{saved}'")
                if name in bound:
                    self.report.error(where, "bound twice to the entity (loading keeps the first)")
                bound.add(name)
                self.check_script_values(where, script, values, by_id)

    def check_script_values(self, where: str, script, values: dict, by_id: dict) -> None:
        read: set[str] = set()
        for prop in script.properties:
            key = prop.name if prop.name in values else next((f for f in prop.former_names if f in values), None)
            if key is None:
                if prop.required:
                    self.report.error(f"{where}.{prop.name}", "Required: not set (the scene has no value)")
                continue
            read.add(key)
            if key != prop.name:
                self.report.warning(f"{where}.{prop.name}", f"saved under its former name '{key}'")
            value = values[key]
            here = f"{where}.{prop.name}"
            if prop.kind == "EntityRef":
                self.check_reference(here, prop, value, by_id)
            elif prop.kind == "Data":
                self.check_script_data(here, prop, value, by_id)
            else:
                self.check_value(here, prop.kind, value, prop)
        for key in values:
            if key not in read:
                self.report.warning(f"{where}.{key}", "no property reads this saved value now (kept in the file)")

    def check_script_data(self, where: str, prop, value, by_id: dict) -> None:
        """A list, enum or struct property (Nov 3, 2026): saved as a data file saves it; a list of entities as UUIDs."""
        if value is None:
            return
        if prop.data_kind == "List":
            if not isinstance(value, list):
                self.report.error(where, f"expected a list, got {value!r}")
                return
            for index, element in enumerate(value):
                here = f"{where}[{index}]"
                if prop.element_kind == "EntityRef":
                    self.check_reference(here, prop, element, by_id)
                else:
                    self.check_element(here, prop.element_kind, prop.element_type, element, prop)
            return
        self.check_element(where, prop.data_kind, prop.type_name, value, prop)

    def check_reference(self, where: str, prop, value, by_id: dict) -> None:
        if not isinstance(value, int) or isinstance(value, bool):
            self.report.error(where, f"a reference is an entity's UUID, got {value!r}")
            return
        if value == 0:
            if getattr(prop, "required", False):
                self.report.error(where, "Required: no entity set")
            return
        target = by_id.get(value)
        if target is None:
            self.report.error(where, f"entity {value} isn't in this file (deleted, or outside the prefab)")
            return
        if getattr(prop, "target_type", None) is not None:
            wanted = prop.target_type.split("::")[-1]
            if not self.entity_has(target, wanted):
                tag = ((target.get("TagComponent") or {}).get("Tag")) or value
                self.report.error(where, f"'{tag}' has no {wanted}")

    # ---- Data files ------------------------------------------------------------------------------------------
    def check_data_value(self, where: str, prop, value) -> None:
        kind = prop.kind
        if kind == "List":
            if not isinstance(value, list):
                self.report.error(where, f"expected a list, got {value!r}")
                return
            for index, element in enumerate(value):
                self.check_element(f"{where}[{index}]", prop.element_kind, prop.element_type, element, prop)
            return
        self.check_element(where, kind, prop.type_name, value, prop)

    def check_element(self, where: str, kind: str, type_name: str, value, prop) -> None:
        if kind == "Struct":
            data = self.reflection.data_types.get(type_name.split("::")[-1])
            if data is not None:
                self.check_record(where, data, value)
            return
        if kind == "Enum":
            if not isinstance(value, str):
                self.report.error(where, f"an enum value is saved by name, got {value!r}")
            return
        if kind == "EntityRef":  # a game component's field
            self.check_reference(where, prop, value, getattr(self, "by_id", {}))
            return
        self.check_value(where, kind, value, prop)

    def check_record(self, where: str, data, record) -> None:
        if record is None:
            return
        if not isinstance(record, dict):
            self.report.error(where, f"expected the fields of a {data.struct_name}, got {record!r}")
            return
        for prop in data.properties:
            key = prop.name if prop.name in record else next((f for f in prop.former_names if f in record), None)
            if key is not None:
                self.check_data_value(f"{where}.{prop.name}", prop, record[key])

    def check_data_file(self, path: Path) -> None:
        label = str(path.relative_to(self.assets))
        try:
            document = read_yaml(path) or {}
        except yaml.YAMLError as exc:
            self.report.error(label, f"not valid YAML: {exc}")
            return
        section = document.get("Definition") if path.suffix == ".hydef" else document.get("DataTable")
        if not isinstance(section, dict):
            self.report.error(label, f"no {'Definition' if path.suffix == '.hydef' else 'DataTable'} section")
            return
        saved = section.get("Type", "")
        name = self.reflection.former_data.get(saved, saved)
        data = self.reflection.data_types.get(name)
        if data is None:
            self.report.error(label, f"data type '{saved}' isn't registered (HY_DATA)")
            return
        if saved != name:
            self.report.warning(label, f"saved under the type's former name '{saved}'")
        if path.suffix == ".hydef":
            self.check_record(f"{label} ({name})", data, section.get("Values"))
        else:
            for row, record in (section.get("Rows") or {}).items():
                self.check_record(f"{label} row {row}", data, record)


def main() -> int:
    project = Path(ARGUMENTS[0]) if ARGUMENTS else WORKSPACE.project_folder
    if project is None:
        print("validate_project.py: no project folder given and the workspace has no DefaultProject", file=sys.stderr)
        return 2
    assets = project / "Assets"
    if not assets.is_dir():
        print(f"validate_project.py: no Assets folder in {project}", file=sys.stderr)
        return 2

    try:
        reflection = read_reflection()
    except Exception as exc:  # the generator's own error, with its file and line
        print(f"validate_project.py: reading the code's reflection failed: {exc}", file=sys.stderr)
        return 2

    report = Report()
    handles, tables = index_assets(assets, report)
    checker = Checker(reflection, handles, tables, report, assets)

    files = 0
    for path in sorted(assets.rglob("*")):
        if path.suffix not in (".hyscene", ".hyprefab", ".hydef", ".hytable"):
            continue
        files += 1
        if path.suffix in (".hydef", ".hytable"):
            checker.check_data_file(path)
            continue
        label = str(path.relative_to(assets))
        try:
            document = read_yaml(path) or {}
        except yaml.YAMLError as exc:
            report.error(label, f"not valid YAML: {exc}")
            continue
        entities = document.get("Entities")
        if entities is None and isinstance(document.get("Entity"), dict):
            entities = [document["Entity"]]  # an older prefab: one entity
        checker.check_entities(label, entities or [])

    print(f"Project validation: {files} file(s), {len(reflection.scripts)} script(s), {len(reflection.data_types)} data type(s); "
          f"{report.errors} error(s), {report.warnings} warning(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
