#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

SUPPORTED_TYPES = {"bool", "int", "float"}


@dataclass(frozen=True)
class Property:
    type_name: str
    name: str
    display_name: str


@dataclass(frozen=True)
class ReflectedScript:
    class_name: str
    include_path: str
    properties: tuple[Property, ...]


def make_display_name(name: str) -> str:
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
    return "".join(result)


def find_matching(text: str, start: int, opening: str, closing: str) -> int:
    assert text[start] == opening
    depth = 0
    i = start
    in_string: str | None = None
    escaped = False
    line_comment = False
    block_comment = False

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if line_comment:
            if ch == "\n":
                line_comment = False
            i += 1
            continue

        if block_comment:
            if ch == "*" and nxt == "/":
                block_comment = False
                i += 2
            else:
                i += 1
            continue

        if in_string is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == in_string:
                in_string = None
            i += 1
            continue

        if ch == "/" and nxt == "/":
            line_comment = True
            i += 2
            continue
        if ch == "/" and nxt == "*":
            block_comment = True
            i += 2
            continue
        if ch in ('"', "'"):
            in_string = ch
            i += 1
            continue

        if ch == opening:
            depth += 1
        elif ch == closing:
            depth -= 1
            if depth == 0:
                return i
        i += 1

    raise ValueError(f"Unmatched '{opening}' at offset {start}")


def access_at(body: str, offset: int, default_access: str) -> str:
    access = default_access
    token_re = re.compile(r"\b(public|private|protected)\s*:")
    for match in token_re.finditer(body, 0, offset):
        access = match.group(1)
    return access


def parse_properties(
    body: str,
    class_name: str,
    default_access: str,
    source: Path,
) -> tuple[Property, ...]:
    properties: list[Property] = []
    cursor = 0

    while True:
        marker = re.search(r"\bHY_PROPERTY\s*\(", body[cursor:])
        if marker is None:
            break

        marker_start = cursor + marker.start()
        open_paren = cursor + marker.end() - 1
        close_paren = find_matching(body, open_paren, "(", ")")

        current_access = access_at(body, marker_start, default_access)
        if current_access != "public":
            raise ValueError(
                f"{source}: {class_name}: HY_PROPERTY fields must be public "
                f"(found under {current_access}:)"
            )

        decl_start = close_paren + 1
        while decl_start < len(body) and body[decl_start].isspace():
            decl_start += 1

        semicolon = body.find(";", decl_start)
        if semicolon < 0:
            raise ValueError(
                f"{source}: {class_name}: HY_PROPERTY is not followed by a field declaration"
            )

        declaration = body[decl_start : semicolon + 1].strip()

        if "(" in declaration:
            raise ValueError(
                f"{source}: {class_name}: HY_PROPERTY can only annotate fields, got: {declaration}"
            )

        if declaration.startswith("static ") or " static " in f" {declaration} ":
            raise ValueError(
                f"{source}: {class_name}: static fields are not supported: {declaration}"
            )

        # V1 deliberately supports simple declarations only.
        # Examples:
        #   float MoveSpeed = 3.5f;
        #   int MaxHealth = 100;
        #   bool CanRun = true;
        field_match = re.fullmatch(
            r"(?P<type>[A-Za-z_][\w:]*)\s+"
            r"(?P<name>[A-Za-z_]\w*)\s*"
            r"(?:=[^;]*)?;",
            declaration,
            flags=re.DOTALL,
        )

        if field_match is None:
            raise ValueError(
                f"{source}: {class_name}: unsupported HY_PROPERTY declaration: {declaration}"
            )

        type_name = field_match.group("type")
        field_name = field_match.group("name")

        if type_name not in SUPPORTED_TYPES:
            supported = ", ".join(sorted(SUPPORTED_TYPES))
            raise ValueError(
                f"{source}: {class_name}.{field_name}: unsupported property type "
                f"'{type_name}'. V1 supports: {supported}"
            )

        properties.append(
            Property(
                type_name=type_name,
                name=field_name,
                display_name=make_display_name(field_name),
            )
        )

        cursor = semicolon + 1

    return tuple(properties)


def parse_header(header: Path, include_path: str) -> list[ReflectedScript]:
    text = header.read_text(encoding="utf-8")
    scripts: list[ReflectedScript] = []
    cursor = 0

    while True:
        marker = re.search(r"\bHY_SCRIPT\s*\(", text[cursor:])
        if marker is None:
            break

        open_paren = cursor + marker.end() - 1
        close_paren = find_matching(text, open_paren, "(", ")")

        declaration_match = re.search(
            r"\b(?P<kind>class|struct)\s+"
            r"(?P<name>[A-Za-z_]\w*)\s*"
			r":\s*public\s+(?P<base>[A-Za-z_][\w:]*)\b",
			text[close_paren + 1 :],
        )

        if declaration_match is None:
            raise ValueError(
				f"{header}: HY_SCRIPT must be followed by a class/struct "
				f"publicly deriving from ScriptableEntity"
            )

        decl_abs_start = close_paren + 1 + declaration_match.start()
        decl_abs_end = close_paren + 1 + declaration_match.end()
        class_kind = declaration_match.group("kind")
        class_name = declaration_match.group("name")

        brace = text.find("{", decl_abs_end)
        if brace < 0:
            raise ValueError(f"{header}: {class_name}: class body not found")

        body_end = find_matching(text, brace, "{", "}")
        body = text[brace + 1 : body_end]
        default_access = "private" if class_kind == "class" else "public"

        properties = parse_properties(
            body=body,
            class_name=class_name,
            default_access=default_access,
            source=header,
        )

        scripts.append(
            ReflectedScript(
                class_name=class_name,
                include_path=include_path,
                properties=properties,
            )
        )

        cursor = body_end + 1

    return scripts


def iter_headers(source_root: Path) -> Iterable[Path]:
    if not source_root.exists():
        return []

    headers: list[Path] = []

    for pattern in ("*.h", "*.hpp"):
        headers.extend(source_root.rglob(pattern))

    excluded_files = {
        "ReflectionMacros.h",
    }

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


def include_path_for(header: Path, repo_root: Path) -> str:
    hydro_src = (repo_root / "Hydro" / "src").resolve()
    oxygen_src = (repo_root / "Oxygen" / "src").resolve()
    resolved = header.resolve()

    try:
        return resolved.relative_to(hydro_src).as_posix()
    except ValueError:
        pass

    try:
        return resolved.relative_to(oxygen_src).as_posix()
    except ValueError:
        pass

    raise ValueError(f"Header is outside supported source roots: {header}")


def escape_cpp(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


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
            lines.append(f'#include "{escape_cpp(include_path)}"')

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
        if script.properties:
            lines.append(
                f'\t\tScriptRegistry::Register<{script.class_name}>("{escape_cpp(script.class_name)}")'
            )
            for prop in script.properties:
                lines.append(
                    f'\t\t\t.Property("{escape_cpp(prop.name)}", '
                    f'"{escape_cpp(prop.display_name)}", '
                    f'&{script.class_name}::{prop.name})'
                )
            lines[-1] += ";"
        else:
            lines.append(
                f'\t\tScriptRegistry::Register<{script.class_name}>("{escape_cpp(script.class_name)}");'
            )

    lines.extend(["\t}", "}", ""])
    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == content:
        return False

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate Hydro native-script reflection registration."
    )
    parser.add_argument(
        "repo_root",
        nargs="?",
        default=Path(__file__).resolve().parents[2],
        type=Path,
        help="Hydro repository root (defaults to two directories above this script)",
    )
    args = parser.parse_args()

    repo_root: Path = args.repo_root.resolve()
    source_roots = [
        repo_root / "Hydro" / "src",
        repo_root / "Oxygen" / "src",
    ]

    scripts: list[ReflectedScript] = []

    try:
        for source_root in source_roots:
            for header in iter_headers(source_root):
                include_path = include_path_for(header, repo_root)
                scripts.extend(parse_header(header, include_path))

        seen: dict[str, str] = {}
        for script in scripts:
            previous = seen.get(script.class_name)
            if previous is not None:
                raise ValueError(
                    f"Duplicate HY_SCRIPT class name '{script.class_name}' in "
                    f"{previous} and {script.include_path}"
                )
            seen[script.class_name] = script.include_path

        scripts.sort(key=lambda s: (s.include_path.lower(), s.class_name.lower()))

        output = (
            repo_root
            / "Oxygen"
            / "src"
            / "Generated"
            / "GeneratedScriptReflection.cpp"
        )
        changed = write_if_changed(output, generate_cpp(scripts))

        status = "updated" if changed else "unchanged"
        print(
            f"Hydro reflection: {len(scripts)} script(s), "
            f"{sum(len(s.properties) for s in scripts)} property/properties; "
            f"{output.relative_to(repo_root)} {status}."
        )
        return 0

    except (OSError, ValueError) as exc:
        print(f"Hydro reflection error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
