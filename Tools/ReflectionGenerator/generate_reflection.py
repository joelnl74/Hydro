#!/usr/bin/env python3
"""Hydro reflection generator.

Reads C++ headers for HY_SCRIPT() classes and their HY_PROPERTY(...) fields, HY_DATA(...) structs and HY_ENUM()
enums, and writes two .cpp files: RegisterGeneratedScriptReflection() (scripts, with ScriptRegistry) and
RegisterGeneratedDataReflection() (data types and enums, with DataTypeRegistry). Run by the Game project's
pre-build step; see Hydro/src/Hydro/Scene/Scripting/ReflectionMacros.h for what the macros accept.

    generate_reflection.py [repo_root] [--source DIR]... [--output FILE] [--data-output FILE] [--check]

Without --source it scans <repo_root>/Hydro/src and <repo_root>/Game/src; without --output / --data-output it
writes <repo_root>/Game/src/Generated/GeneratedScriptReflection.cpp and GeneratedDataReflection.cpp. Include
paths in the output are relative to the first of Hydro/src, Game/src or a --source directory that contains the
header. --check writes nothing and exits with 1 when an output is out of date (for CI).

It is a text scanner, not a C++ parser: one field per HY_PROPERTY declaration, the class declaration right
after HY_SCRIPT(). Anything it doesn't understand is an error with the file and the declaration.
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

# C++ type as written -> (type used in the generated code, ScriptPropertyType name)
SUPPORTED_TYPES = {
    "bool": "Bool",
    "int": "Int",
    "float": "Float",
    "glm::vec2": "Vec2",
    "glm::vec3": "Vec3",
    "glm::vec4": "Vec4",
    "std::string": "String",
    "AssetHandle": "Asset",
    "Hydro::AssetHandle": "Asset",
}

NUMERIC_KINDS = {"Int", "Float", "Vec2", "Vec3", "Vec4"}
COLOR_KINDS = {"Vec3", "Vec4"}

# AssetType enumerators (Hydro/src/Hydro/Asset/Asset.h) an Asset(...) argument may name.
ASSET_TYPES = {"Texture2D", "Scene", "Prefab", "SpriteAnimation", "Material", "Mesh", "RenderProfile", "Font", "Terrain"}

# Data fields (HY_PROPERTY inside an HY_DATA struct) have the script kinds plus these. Enum, Struct and List
# fields are resolved once every header has been read (the types can live in other headers).
DATA_VALUE_TYPES = dict(SUPPORTED_TYPES, **{"DataRowRef": "RowRef", "Hydro::DataRowRef": "RowRef"})

# The only arguments HY_DATA(...) accepts; anything else is an error (the generator stays a scanner).
HY_DATA_ARGUMENTS = "Definition, Table, DisplayField(field)"


@dataclass
class Property:
    type_name: str
    kind: str
    name: str
    display_name: str
    tooltip: str | None = None
    minimum: str | None = None
    maximum: str | None = None
    color: bool = False
    asset_type: str | None = None
    read_only: bool = False
    hidden: bool = False


@dataclass
class ReflectedScript:
    class_name: str
    include_path: str
    properties: list[Property] = field(default_factory=list)


@dataclass
class DataProperty:
    """An HY_PROPERTY of an HY_DATA struct, read in two steps: parse (type as written) then resolve (kinds)."""

    type_name: str
    name: str
    arguments: str
    where: str
    kind: str = ""
    element_type: str | None = None  # List: the element's C++ type
    element_kind: str | None = None
    display_name: str = ""
    tooltip: str | None = None
    minimum: str | None = None
    maximum: str | None = None
    color: bool = False
    asset_type: str | None = None
    data_row: str | None = None
    read_only: bool = False
    hidden: bool = False

    @property
    def value_kind(self) -> str:
        """The kind the arguments apply to: a list's elements, or the field itself."""
        return self.element_kind if self.kind == "List" else self.kind


@dataclass
class ReflectedData:
    struct_name: str
    include_path: str
    where: str
    definition: bool = False
    table: bool = False
    display_field: str | None = None
    display_field_explicit: bool = False
    properties: list[DataProperty] = field(default_factory=list)

    @property
    def display_name(self) -> str:
        # Create > Definition > Dialogue for DialogueDefinition.
        name = self.struct_name
        if name.endswith("Definition") and len(name) > len("Definition"):
            name = name[: -len("Definition")]
        return make_display_name(name)


@dataclass
class ReflectedEnum:
    enum_name: str
    include_path: str
    where: str
    values: list[str] = field(default_factory=list)


@dataclass
class ParsedHeader:
    scripts: list[ReflectedScript] = field(default_factory=list)
    data: list[ReflectedData] = field(default_factory=list)
    enums: list[ReflectedEnum] = field(default_factory=list)


class ReflectionError(ValueError):
    pass


def make_display_name(name: str) -> str:
    # m_MoveSpeed -> MoveSpeed: the member prefix isn't part of the label.
    if name.startswith("m_") and len(name) > 2:
        name = name[2:]
    if not name:
        return name

    result: list[str] = []
    for i, ch in enumerate(name):
        if i > 0:
            prev = name[i - 1]
            next_ch = name[i + 1] if i + 1 < len(name) else ""
            # Split camel/Pascal case, but keep acronym blocks together:
            # MoveSpeed -> Move Speed, MaxHP -> Max HP, HTTPServer -> HTTP Server.
            if ch.isupper() and (
                prev.islower()
                or prev.isdigit()
                or (prev.isupper() and next_ch.islower())
            ):
                result.append(" ")
        result.append(ch)
    return "".join(result).replace("_", " ").strip()


def strip_comments(text: str) -> str:
    """Comments become spaces (newlines kept), so offsets and line numbers stay the same; strings stay."""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if ch == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if ch == "/" and nxt == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
            continue
        if ch in ('"', "'"):
            quote = ch
            out.append(ch)
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i])
                    i += 1
                out.append(text[i])
                i += 1
            if i < n:
                out.append(text[i])
                i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def find_matching(text: str, start: int, opening: str, closing: str) -> int:
    """Index of the bracket closing the one at `start`, skipping string and character literals."""
    assert text[start] == opening
    depth = 0
    i = start
    while i < len(text):
        ch = text[i]
        if ch in ('"', "'"):
            i = skip_literal(text, i)
            continue
        if ch == opening:
            depth += 1
        elif ch == closing:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ReflectionError(f"Unmatched '{opening}' at offset {start}")


def skip_literal(text: str, start: int) -> int:
    quote = text[start]
    i = start + 1
    while i < len(text) and text[i] != quote:
        i += 2 if text[i] == "\\" else 1
    return i + 1


def split_top_level(text: str, separator: str = ",") -> list[str]:
    """Splits at `separator` outside brackets and literals."""
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    i = 0
    while i < len(text):
        ch = text[i]
        if ch in ('"', "'"):
            end = skip_literal(text, i)
            current.append(text[i:end])
            i = end
            continue
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        if ch == separator and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
        i += 1
    tail = "".join(current).strip()
    if tail or parts:
        parts.append(tail)
    return parts


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def find_statement_end(text: str, start: int) -> int:
    """Index of the ';' ending the declaration at `start`, outside brackets and literals."""
    depth = 0
    i = start
    while i < len(text):
        ch = text[i]
        if ch in ('"', "'"):
            i = skip_literal(text, i)
            continue
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == ";" and depth == 0:
            return i
        i += 1
    return -1


def cpp_float(number: str) -> str:
    """A float literal for a number read by parse_number: 0 -> 0.0f, 2.5 -> 2.5f, 1e3 -> 1e3f."""
    if re.search(r"[.eE]", number) is None:
        number += ".0"
    elif number.endswith("."):
        number += "0"
    return number + "f"


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def parse_string_argument(argument: str, where: str) -> str:
    match = re.fullmatch(r'"((?:[^"\\]|\\.)*)"', argument.strip())
    if match is None:
        raise ReflectionError(f"{where}: expected a string literal, got {argument}")
    return match.group(1).replace('\\"', '"').replace("\\\\", "\\")


def parse_number(argument: str, where: str) -> str:
    text = argument.strip()
    if re.fullmatch(r"[+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?f?", text) is None:
        raise ReflectionError(f"{where}: expected a number, got {argument}")
    return text.rstrip("f")


def apply_arguments(prop: Property, arguments: str, where: str) -> None:
    for argument in split_top_level(arguments):
        if not argument:
            continue
        match = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:\((.*)\))?", argument, flags=re.DOTALL)
        if match is None:
            raise ReflectionError(f"{where}: can't read HY_PROPERTY argument '{argument}'")
        name, inner = match.group(1), match.group(2)
        values = split_top_level(inner) if inner is not None else []

        def expect(count: int) -> None:
            if len(values) != count:
                raise ReflectionError(f"{where}: {name}(...) takes {count} argument(s), got '{argument}'")

        if name == "Range":
            expect(2)
            prop.minimum = parse_number(values[0], where)
            prop.maximum = parse_number(values[1], where)
        elif name == "Min":
            expect(1)
            prop.minimum = parse_number(values[0], where)
        elif name == "Max":
            expect(1)
            prop.maximum = parse_number(values[0], where)
        elif name == "Tooltip":
            expect(1)
            prop.tooltip = parse_string_argument(values[0], where)
        elif name == "DisplayName":
            expect(1)
            prop.display_name = parse_string_argument(values[0], where)
        elif name == "Color":
            expect(0)
            prop.color = True
        elif name == "Asset":
            expect(1)
            asset_type = values[0].strip().removeprefix("AssetType::")
            if asset_type not in ASSET_TYPES:
                raise ReflectionError(f"{where}: Asset({values[0]}): use one of {', '.join(sorted(ASSET_TYPES))}")
            prop.asset_type = asset_type
        elif name == "ReadOnly":
            expect(0)
            prop.read_only = True
        elif name == "Hidden":
            expect(0)
            prop.hidden = True
        else:
            raise ReflectionError(
                f"{where}: unknown HY_PROPERTY argument '{name}' "
                "(Range, Min, Max, Tooltip, DisplayName, Color, Asset, ReadOnly, Hidden)"
            )

    if (prop.minimum is not None or prop.maximum is not None) and prop.kind not in NUMERIC_KINDS:
        raise ReflectionError(f"{where}: Range / Min / Max need an int, float or vector field")
    if prop.color and prop.kind not in COLOR_KINDS:
        raise ReflectionError(f"{where}: Color needs a glm::vec3 or glm::vec4 field")
    if prop.asset_type is not None and prop.kind != "Asset":
        raise ReflectionError(f"{where}: Asset(...) needs an AssetHandle field")
    if prop.kind == "Asset" and prop.asset_type is None:
        raise ReflectionError(f"{where}: an AssetHandle needs Asset(<AssetType>), e.g. HY_PROPERTY(Asset(Texture2D))")


def body_depths(body: str) -> list[int]:
    """Brace depth at every offset of a class body (literals skipped), to tell its own members from nested types'."""
    depths = [0] * (len(body) + 1)
    depth = 0
    i = 0
    while i < len(body):
        ch = body[i]
        if ch in ('"', "'"):
            end = skip_literal(body, i)
            for j in range(i, min(end, len(body))):
                depths[j] = depth
            i = end
            continue
        if ch == "{":
            depth += 1
        depths[i] = depth
        if ch == "}":
            depth -= 1
            depths[i] = depth
        i += 1
    depths[len(body)] = depth
    return depths


def access_at(body: str, depths: list[int], offset: int, default_access: str) -> str:
    access = default_access
    for match in re.finditer(r"\b(public|private|protected)\s*:", body[:offset]):
        if depths[match.start()] == 0:
            access = match.group(1)
    return access


def iter_property_declarations(
    body: str, body_offset: int, text: str, owner: str, default_access: str, source: Path, nested_hint: str
) -> Iterable[tuple[str, str, str]]:
    """(arguments, declaration, where) for every HY_PROPERTY of a class body, with the checks scripts and data share."""
    depths = body_depths(body)

    for marker in re.finditer(r"\bHY_PROPERTY\s*\(", body):
        where = f"{source}:{line_of(text, body_offset + marker.start())}: {owner}"
        if depths[marker.start()] != 0:
            raise ReflectionError(f"{where}: HY_PROPERTY inside a nested type; {nested_hint}")

        current_access = access_at(body, depths, marker.start(), default_access)
        if current_access != "public":
            raise ReflectionError(f"{where}: HY_PROPERTY fields must be public (found under {current_access}:)")

        open_paren = marker.end() - 1
        close_paren = find_matching(body, open_paren, "(", ")")
        arguments = body[open_paren + 1 : close_paren]

        decl_start = close_paren + 1
        semicolon = find_statement_end(body, decl_start)
        if semicolon < 0:
            raise ReflectionError(f"{where}: HY_PROPERTY is not followed by a field declaration")
        declaration = " ".join(body[decl_start : semicolon + 1].split())

        if re.search(r"\bstatic\b", declaration):
            raise ReflectionError(f"{where}: static fields are not supported: {declaration}")
        if len(split_top_level(declaration[:-1])) > 1:
            raise ReflectionError(f"{where}: one field per HY_PROPERTY declaration: {declaration}")
        if declaration.startswith("const "):
            raise ReflectionError(f"{where}: const fields can't be set from the inspector: {declaration}")

        yield arguments, declaration, where


def parse_properties(body: str, body_offset: int, text: str, class_name: str, default_access: str, source: Path) -> list[Property]:
    properties: list[Property] = []

    for arguments, declaration, where in iter_property_declarations(
        body, body_offset, text, class_name, default_access, source, "put the field on the script itself"
    ):
        # float MoveSpeed = 3.5f;   glm::vec3 Offset{ 0.0f };   std::string Name = "x";   AssetHandle Icon = 0;
        field_match = re.fullmatch(
            r"(?P<type>[A-Za-z_][\w:]*)\s+"
            r"(?P<name>[A-Za-z_]\w*)\s*"
            r"(?:=.*|\{.*\})?;",
            declaration,
            flags=re.DOTALL,
        )
        if field_match is None:
            raise ReflectionError(f"{where}: HY_PROPERTY can only mark a plain field, got: {declaration}")

        type_name = field_match.group("type")
        field_name = field_match.group("name")
        kind = SUPPORTED_TYPES.get(type_name)
        if kind is None:
            supported = ", ".join(sorted({t for t in SUPPORTED_TYPES if not t.startswith("Hydro::")}))
            raise ReflectionError(f"{where}.{field_name}: unsupported property type '{type_name}'. Supported: {supported}")

        prop = Property(type_name=type_name, kind=kind, name=field_name, display_name=make_display_name(field_name))
        apply_arguments(prop, arguments, f"{where}.{field_name}")
        properties.append(prop)

    return properties


def parse_scripts(text: str, header: Path, include_path: str) -> list[ReflectedScript]:
    scripts: list[ReflectedScript] = []

    for marker in re.finditer(r"\bHY_SCRIPT\s*\(", text):
        where = f"{header}:{line_of(text, marker.start())}"
        close_paren = find_matching(text, marker.end() - 1, "(", ")")

        # The class must come right after the marker (no other declaration in between).
        declaration = re.compile(
            r"\s*(?P<kind>class|struct)\s+"
            r"(?:alignas\s*\([^)]*\)\s*)?"
            r"(?P<name>[A-Za-z_]\w*)\s*"
            r"(?:final\s*)?"
            r":\s*public\s+(?P<base>[A-Za-z_][\w:]*)"
        ).match(text, close_paren + 1)
        if declaration is None:
            raise ReflectionError(f"{where}: HY_SCRIPT() must be right above a class/struct that derives publicly from ScriptableEntity")

        class_kind = declaration.group("kind")
        class_name = declaration.group("name")
        brace = text.find("{", declaration.end())
        if brace < 0:
            raise ReflectionError(f"{where}: {class_name}: class body not found")
        body_end = find_matching(text, brace, "{", "}")
        body = text[brace + 1 : body_end]

        scripts.append(
            ReflectedScript(
                class_name=class_name,
                include_path=include_path,
                properties=parse_properties(
                    body=body,
                    body_offset=brace + 1,
                    text=text,
                    class_name=class_name,
                    default_access="private" if class_kind == "class" else "public",
                    source=header,
                ),
            )
        )

    return scripts


# ---- Data types (HY_DATA) and enums (HY_ENUM) --------------------------------------------------------


def parse_data_arguments(arguments: str, where: str) -> tuple[bool, bool, str | None]:
    """HY_DATA(...) -> (definition, table, display field). Only Definition, Table and DisplayField(field)."""
    definition = False
    table = False
    display_field: str | None = None
    for argument in split_top_level(arguments):
        if not argument:
            continue
        match = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:\((.*)\))?", argument, flags=re.DOTALL)
        name = match.group(1) if match else None
        inner = match.group(2) if match else None
        if name in ("Definition", "Table") and inner is None:
            if (definition if name == "Definition" else table):
                raise ReflectionError(f"{where}: HY_DATA({arguments.strip()}): '{name}' appears twice")
            if name == "Definition":
                definition = True
            else:
                table = True
        elif name == "DisplayField" and inner is not None and re.fullmatch(r"\s*[A-Za-z_]\w*\s*", inner):
            if display_field is not None:
                raise ReflectionError(f"{where}: HY_DATA({arguments.strip()}): DisplayField appears twice")
            display_field = inner.strip()
        else:
            raise ReflectionError(
                f"{where}: HY_DATA({arguments.strip()}): unsupported argument '{argument}' (HY_DATA accepts only {HY_DATA_ARGUMENTS})"
            )
    return definition, table, display_field


def parse_data_types(text: str, header: Path, include_path: str) -> list[ReflectedData]:
    data_types: list[ReflectedData] = []

    for marker in re.finditer(r"\bHY_DATA\s*\(", text):
        where = f"{header}:{line_of(text, marker.start())}"
        close_paren = find_matching(text, marker.end() - 1, "(", ")")
        definition, table, display_field = parse_data_arguments(text[marker.end() : close_paren], where)

        declaration = re.compile(
            r"\s*(?P<kind>struct|class)\s+"
            r"(?:alignas\s*\([^)]*\)\s*)?"
            r"(?P<name>[A-Za-z_]\w*)\s*"
            r"(?:final\s*)?"
            r"(?P<next>[:{])"
        ).match(text, close_paren + 1)
        if declaration is None:
            raise ReflectionError(f"{where}: HY_DATA(...) must be right above a struct")

        struct_name = declaration.group("name")
        if declaration.group("next") == ":":
            raise ReflectionError(f"{where}: {struct_name}: data types can't derive from another type (no inheritance between data types)")

        brace = declaration.end() - 1
        body_end = find_matching(text, brace, "{", "}")
        body = text[brace + 1 : body_end]

        reflected = ReflectedData(
            struct_name=struct_name,
            include_path=include_path,
            where=f"{where}: {struct_name}",
            definition=definition,
            table=table,
            display_field=display_field,
            display_field_explicit=display_field is not None,
        )

        for arguments, field_declaration, field_where in iter_property_declarations(
            body,
            brace + 1,
            text,
            struct_name,
            "private" if declaration.group("kind") == "class" else "public",
            header,
            "nest data types as separate HY_DATA structs",
        ):
            # std::string Name;   std::vector<DialogueLine> Lines;   ItemKind Kind = ItemKind::Consumable;
            field_match = re.fullmatch(
                r"(?P<type>[A-Za-z_][\w:]*(?:\s*<[^;=]*>)?)\s+"
                r"(?P<name>[A-Za-z_]\w*)\s*"
                r"(?:=.*|\{.*\})?;",
                field_declaration,
                flags=re.DOTALL,
            )
            if field_match is None:
                raise ReflectionError(f"{field_where}: HY_PROPERTY can only mark a plain field, got: {field_declaration}")

            field_name = field_match.group("name")
            reflected.properties.append(
                DataProperty(
                    type_name=re.sub(r"\s+", "", field_match.group("type")),
                    name=field_name,
                    arguments=arguments,
                    where=f"{field_where}.{field_name}",
                    display_name=make_display_name(field_name),
                )
            )

        data_types.append(reflected)

    return data_types


def parse_enums(text: str, header: Path, include_path: str) -> list[ReflectedEnum]:
    enums: list[ReflectedEnum] = []

    for marker in re.finditer(r"\bHY_ENUM\s*\(", text):
        where = f"{header}:{line_of(text, marker.start())}"
        close_paren = find_matching(text, marker.end() - 1, "(", ")")
        if text[marker.end() : close_paren].strip():
            raise ReflectionError(f"{where}: HY_ENUM() takes no arguments")

        declaration = re.compile(
            r"\s*enum\s+(?P<scoped>class\s+|struct\s+)?(?P<name>[A-Za-z_]\w*)\s*(?::[^{;]*)?\{"
        ).match(text, close_paren + 1)
        if declaration is None or declaration.group("scoped") is None:
            raise ReflectionError(f"{where}: HY_ENUM() must be right above an enum class")

        enum_name = declaration.group("name")
        brace = declaration.end() - 1
        body = text[brace + 1 : find_matching(text, brace, "{", "}")]

        reflected = ReflectedEnum(enum_name=enum_name, include_path=include_path, where=f"{where}: {enum_name}")
        for item in split_top_level(body):
            if not item:
                continue
            match = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:=.*)?", item, flags=re.DOTALL)
            if match is None:
                raise ReflectionError(f"{where}: {enum_name}: can't read enumerator '{item}'")
            reflected.values.append(match.group(1))
        if not reflected.values:
            raise ReflectionError(f"{where}: {enum_name}: an HY_ENUM needs at least one enumerator")
        enums.append(reflected)

    return enums


def parse_header(header: Path, include_path: str) -> ParsedHeader:
    text = strip_comments(header.read_text(encoding="utf-8"))
    return ParsedHeader(
        scripts=parse_scripts(text, header, include_path),
        data=parse_data_types(text, header, include_path),
        enums=parse_enums(text, header, include_path),
    )


def data_value_kind(type_name: str, where: str, data_names: set[str], enum_names: set[str]) -> str:
    plain = type_name.removeprefix("Hydro::")
    kind = DATA_VALUE_TYPES.get(type_name) or DATA_VALUE_TYPES.get(plain)
    if kind is not None:
        return kind
    if plain in enum_names:
        return "Enum"
    if plain in data_names:
        return "Struct"
    if plain.startswith("std::vector<"):
        raise ReflectionError(f"{where}: lists of lists are not supported; wrap the inner list in an HY_DATA struct")
    supported = ", ".join(sorted({t for t in DATA_VALUE_TYPES if not t.startswith("Hydro::")}))
    raise ReflectionError(
        f"{where}: unsupported data field type '{type_name}'. Supported: {supported}, an HY_ENUM enum class, "
        "an HY_DATA struct, or a std::vector of one of these"
    )


def apply_data_arguments(prop: DataProperty, tables: set[str]) -> None:
    where = prop.where
    for argument in split_top_level(prop.arguments):
        if not argument:
            continue
        match = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:\((.*)\))?", argument, flags=re.DOTALL)
        if match is None:
            raise ReflectionError(f"{where}: can't read HY_PROPERTY argument '{argument}'")
        name, inner = match.group(1), match.group(2)
        values = split_top_level(inner) if inner is not None else []

        def expect(count: int) -> None:
            if len(values) != count:
                raise ReflectionError(f"{where}: {name}(...) takes {count} argument(s), got '{argument}'")

        if name == "Range":
            expect(2)
            prop.minimum = parse_number(values[0], where)
            prop.maximum = parse_number(values[1], where)
        elif name == "Min":
            expect(1)
            prop.minimum = parse_number(values[0], where)
        elif name == "Max":
            expect(1)
            prop.maximum = parse_number(values[0], where)
        elif name == "Tooltip":
            expect(1)
            prop.tooltip = parse_string_argument(values[0], where)
        elif name == "DisplayName":
            expect(1)
            prop.display_name = parse_string_argument(values[0], where)
        elif name == "Color":
            expect(0)
            prop.color = True
        elif name == "Asset":
            expect(1)
            asset_type = values[0].strip().removeprefix("AssetType::")
            if asset_type not in ASSET_TYPES:
                raise ReflectionError(f"{where}: Asset({values[0]}): use one of {', '.join(sorted(ASSET_TYPES))}")
            prop.asset_type = asset_type
        elif name == "DataRow":
            expect(1)
            row_type = values[0].strip()
            if row_type not in tables:
                raise ReflectionError(f"{where}: DataRow({row_type}): not an HY_DATA(Table) type")
            prop.data_row = row_type
        elif name == "ReadOnly":
            expect(0)
            prop.read_only = True
        elif name == "Hidden":
            expect(0)
            prop.hidden = True
        else:
            raise ReflectionError(
                f"{where}: unknown HY_PROPERTY argument '{name}' "
                "(Range, Min, Max, Tooltip, DisplayName, Color, Asset, DataRow, ReadOnly, Hidden)"
            )

    kind = prop.value_kind
    if (prop.minimum is not None or prop.maximum is not None) and kind not in NUMERIC_KINDS:
        raise ReflectionError(f"{where}: Range / Min / Max need an int, float or vector field (or a list of them)")
    if prop.color and kind not in COLOR_KINDS:
        raise ReflectionError(f"{where}: Color needs a glm::vec3 or glm::vec4 field")
    if prop.asset_type is not None and kind != "Asset":
        raise ReflectionError(f"{where}: Asset(...) needs an AssetHandle field")
    if kind == "Asset" and prop.asset_type is None:
        raise ReflectionError(f"{where}: an AssetHandle needs Asset(<AssetType>), e.g. HY_PROPERTY(Asset(Texture2D))")
    if prop.data_row is not None and kind != "RowRef":
        raise ReflectionError(f"{where}: DataRow(...) needs a DataRowRef field")
    if kind == "RowRef" and prop.data_row is None:
        raise ReflectionError(f"{where}: a DataRowRef needs DataRow(<row type>), e.g. HY_PROPERTY(DataRow(ItemDefinition))")


def resolve_data(data_types: list[ReflectedData], enums: list[ReflectedEnum], scripts: list[ReflectedScript]) -> None:
    """Field kinds, arguments and display fields, once every header is read; and names that clash."""
    seen: dict[str, str] = {}
    for kind, name, where in (
        [("script", s.class_name, s.include_path) for s in scripts]
        + [("data type", d.struct_name, d.where) for d in data_types]
        + [("enum", e.enum_name, e.where) for e in enums]
    ):
        previous = seen.get(name)
        if previous is not None:
            raise ReflectionError(f"Duplicate reflected name '{name}' ({kind}) in {previous} and {where}")
        seen[name] = where

    data_names = {d.struct_name for d in data_types}
    enum_names = {e.enum_name for e in enums}
    tables = {d.struct_name for d in data_types if d.table}

    for data in data_types:
        for prop in data.properties:
            list_match = re.fullmatch(r"(?:std::)?vector<(.+)>", prop.type_name)
            if list_match is not None:
                prop.kind = "List"
                prop.element_type = list_match.group(1)
                prop.element_kind = data_value_kind(prop.element_type, prop.where, data_names, enum_names)
            else:
                prop.kind = data_value_kind(prop.type_name, prop.where, data_names, enum_names)
            apply_data_arguments(prop, tables)

        string_fields = {p.name for p in data.properties if p.kind == "String"}
        if data.display_field_explicit:
            if not data.table:
                raise ReflectionError(f"{data.where}: DisplayField(...) is for table types: HY_DATA(Table, DisplayField(...))")
            if data.display_field not in string_fields:
                raise ReflectionError(f"{data.where}: DisplayField({data.display_field}): not a std::string HY_PROPERTY of {data.struct_name}")
        elif data.table and "Name" in string_fields:
            data.display_field = "Name"


def generate_data_cpp(data_types: list[ReflectedData], enums: list[ReflectedEnum]) -> str:
    lines: list[str] = [
        "// AUTO-GENERATED by Tools/ReflectionGenerator/generate_reflection.py.",
        "// DO NOT EDIT. Annotate source with HY_DATA(), HY_ENUM() and HY_PROPERTY() instead.",
        "",
        '#include "GeneratedDataReflection.h"',
        '#include "Hydro/Data/DataTypeRegistry.h"',
    ]

    include_paths = sorted({d.include_path for d in data_types} | {e.include_path for e in enums})
    if include_paths:
        lines.append("")
        for include_path in include_paths:
            lines.append(f"#include {cpp_string(include_path)}")

    lines.extend(
        [
            "",
            "namespace Hydro",
            "{",
            "\tvoid RegisterGeneratedDataReflection()",
            "\t{",
            "\t\tstatic bool registered = false;",
            "\t\tif (registered)",
            "\t\t{",
            "\t\t\treturn;",
            "\t\t}",
            "\t\tregistered = true;",
        ]
    )

    for reflected in enums:
        lines.append("")
        lines.append(f"\t\tDataTypeRegistry::RegisterEnum<{reflected.enum_name}>({cpp_string(reflected.enum_name)})")
        for value in reflected.values:
            lines.append(f"\t\t\t.Value({cpp_string(value)}, {reflected.enum_name}::{value})")
        lines[-1] += ";"

    for data in data_types:
        lines.append("")
        lines.append(
            f"\t\tDataTypeRegistry::Register<{data.struct_name}>({cpp_string(data.struct_name)}, {cpp_string(data.display_name)})"
        )
        if data.definition:
            lines.append("\t\t\t.Definition()")
        if data.table:
            lines.append("\t\t\t.Table()")
        if data.display_field is not None:
            lines.append(f"\t\t\t.DisplayField({cpp_string(data.display_field)})")
        for prop in data.properties:
            call = f"\t\t\t.Field<&{data.struct_name}::{prop.name}>({cpp_string(prop.name)}, {cpp_string(prop.display_name)})"
            if prop.minimum is not None and prop.maximum is not None:
                call += f".Range({cpp_float(prop.minimum)}, {cpp_float(prop.maximum)})"
            elif prop.minimum is not None:
                call += f".Min({cpp_float(prop.minimum)})"
            elif prop.maximum is not None:
                call += f".Max({cpp_float(prop.maximum)})"
            if prop.tooltip is not None:
                call += f".Tooltip({cpp_string(prop.tooltip)})"
            if prop.color:
                call += ".Color()"
            if prop.asset_type is not None:
                call += f".AssetFilter(AssetType::{prop.asset_type})"
            if prop.data_row is not None:
                call += f".DataRow({cpp_string(prop.data_row)})"
            if prop.read_only:
                call += ".ReadOnly()"
            if prop.hidden:
                call += ".Hidden()"
            lines.append(call)
        lines[-1] += ";"

    lines.extend(["\t}", "}", ""])
    return "\r\n".join(lines)


def iter_headers(source_root: Path) -> Iterable[Path]:
    if not source_root.exists():
        return []

    headers: list[Path] = []
    for pattern in ("*.h", "*.hpp"):
        headers.extend(source_root.rglob(pattern))

    excluded_files = {"ReflectionMacros.h"}
    return sorted(
        (
            header
            for header in headers
            if "Generated" not in header.parts
            and "vendor" not in header.parts
            and header.name not in excluded_files
        ),
        key=lambda p: p.as_posix().lower(),
    )


def include_path_for(header: Path, include_roots: list[Path]) -> str:
    resolved = header.resolve()
    for root in include_roots:
        try:
            return resolved.relative_to(root.resolve()).as_posix()
        except ValueError:
            pass
    raise ReflectionError(f"Header is outside the include roots: {header}")


def generate_cpp(scripts: list[ReflectedScript]) -> str:
    lines: list[str] = [
        "// AUTO-GENERATED by Tools/ReflectionGenerator/generate_reflection.py.",
        "// DO NOT EDIT. Annotate source with HY_SCRIPT() and HY_PROPERTY() instead.",
        "",
        '#include "GeneratedScriptReflection.h"',
        '#include "Hydro/Scene/Scripting/ScriptRegistry.h"',
    ]

    include_paths = sorted({script.include_path for script in scripts})
    if include_paths:
        lines.append("")
        for include_path in include_paths:
            lines.append(f"#include {cpp_string(include_path)}")

    lines.extend(
        [
            "",
            "namespace Hydro",
            "{",
            "\tvoid RegisterGeneratedScriptReflection()",
            "\t{",
            "\t\tstatic bool registered = false;",
            "\t\tif (registered)",
            "\t\t{",
            "\t\t\treturn;",
            "\t\t}",
            "\t\tregistered = true;",
        ]
    )

    for script in scripts:
        lines.append("")
        lines.append(f"\t\tScriptRegistry::Register<{script.class_name}>({cpp_string(script.class_name)})")
        for prop in script.properties:
            call = f"\t\t\t.Property({cpp_string(prop.name)}, {cpp_string(prop.display_name)}, &{script.class_name}::{prop.name})"
            if prop.minimum is not None and prop.maximum is not None:
                call += f".Range({cpp_float(prop.minimum)}, {cpp_float(prop.maximum)})"
            elif prop.minimum is not None:
                call += f".Min({cpp_float(prop.minimum)})"
            elif prop.maximum is not None:
                call += f".Max({cpp_float(prop.maximum)})"
            if prop.tooltip is not None:
                call += f".Tooltip({cpp_string(prop.tooltip)})"
            if prop.color:
                call += ".Color()"
            if prop.asset_type is not None:
                call += f".AssetFilter(AssetType::{prop.asset_type})"
            if prop.read_only:
                call += ".ReadOnly()"
            if prop.hidden:
                call += ".Hidden()"
            lines.append(call)
        lines[-1] += ";"

    lines.extend(["\t}", "}", ""])
    # CRLF, like the rest of the repository (.editorconfig).
    return "\r\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Hydro script and data reflection registration.")
    parser.add_argument(
        "repo_root",
        nargs="?",
        default=Path(__file__).resolve().parents[2],
        type=Path,
        help="Hydro repository root (defaults to two directories above this script)",
    )
    parser.add_argument("--source", action="append", type=Path, default=[], help="header folder to scan (repeatable)")
    parser.add_argument("--output", type=Path, help="the scripts' .cpp to write")
    parser.add_argument("--data-output", type=Path, help="the data types' .cpp to write")
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 when the output is out of date")
    args = parser.parse_args()

    repo_root: Path = args.repo_root.resolve()
    default_roots = [repo_root / "Hydro" / "src", repo_root / "Game" / "src"]
    source_roots = [p.resolve() for p in args.source] or default_roots
    include_roots = default_roots + [p for p in source_roots if p not in default_roots]
    generated = repo_root / "Game" / "src" / "Generated"
    output: Path = (args.output or generated / "GeneratedScriptReflection.cpp").resolve()
    data_output: Path = (args.data_output or generated / "GeneratedDataReflection.cpp").resolve()

    try:
        scripts: list[ReflectedScript] = []
        data_types: list[ReflectedData] = []
        enums: list[ReflectedEnum] = []
        for source_root in source_roots:
            for header in iter_headers(source_root):
                parsed = parse_header(header, include_path_for(header, include_roots))
                scripts.extend(parsed.scripts)
                data_types.extend(parsed.data)
                enums.extend(parsed.enums)

        resolve_data(data_types, enums, scripts)

        scripts.sort(key=lambda s: (s.include_path.lower(), s.class_name.lower()))
        data_types.sort(key=lambda d: (d.include_path.lower(), d.struct_name.lower()))
        enums.sort(key=lambda e: (e.include_path.lower(), e.enum_name.lower()))

        outputs = [
            (output, generate_cpp(scripts), f"{len(scripts)} script(s), {sum(len(s.properties) for s in scripts)} property/properties"),
            (
                data_output,
                generate_data_cpp(data_types, enums),
                f"{len(data_types)} data type(s), {len(enums)} enum(s), {sum(len(d.properties) for d in data_types)} field(s)",
            ),
        ]

        out_of_date = False
        for path, content, summary in outputs:
            old = path.read_bytes().decode("utf-8") if path.exists() else None
            changed = old != content
            try:
                shown = path.relative_to(repo_root)
            except ValueError:
                shown = path

            if args.check:
                print(f"Hydro reflection: {summary}; {shown} {'OUT OF DATE' if changed else 'up to date'}.")
                out_of_date = out_of_date or changed
                continue

            if changed:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content.encode("utf-8"))
            print(f"Hydro reflection: {summary}; {shown} {'updated' if changed else 'unchanged'}.")

        return 1 if out_of_date else 0

    except (OSError, ReflectionError) as exc:
        print(f"Hydro reflection error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
