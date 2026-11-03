#!/usr/bin/env python3
"""Moves a script's saved bindings to a game component (HY_COMPONENT) in every scene and prefab (Nov 3, 2026).

When a data-only script becomes a component (WaypointScript -> WaypointComponent), its values live in a different
place in the files: under the entity's NativeScriptComponent as a binding, and then as a key of the entity itself.

    NativeScriptComponent:                 WaypointComponent:
      Scripts:                      ->       WaitMin: 4
        - Name: WaypointScript               Facing: Down
          Properties: { WaitMin: 4, Facing: Down }

This rewrites each binding of --script on an entity as a --component key with the same values (an entity left with no
scripts loses its NativeScriptComponent), and leaves every other line of the file as it is. Safe to run again: an
entity that already has the component keeps it, and the binding is dropped.

    python scripts/migrate_script_to_component.py --script WaypointScript --component WaypointComponent [--dry-run] [files...]

Without files: every .hyscene and .hyprefab under Oxygen/project/Assets. Close the scenes in the editor first.
"""
import argparse
import json
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "Oxygen" / "project" / "Assets"


def scalar(value) -> str:
    """A value as the engine's YAML writer would put it on one line."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return str(int(value)) if value.is_integer() else repr(value)
    if value is None:
        return "~"
    if isinstance(value, list):
        return "[" + ", ".join(scalar(v) for v in value) + "]"
    if isinstance(value, dict):
        return "{" + ", ".join(f"{k}: {scalar(v)}" for k, v in value.items()) + "}"
    text = str(value)
    if text == "" or re.fullmatch(r"[A-Za-z_][\w .-]*", text) is None or text.lower() in ("true", "false", "null", "yes", "no", "on", "off"):
        return json.dumps(text)
    return text


def migrate_block(block: str, script: str, component: str) -> tuple[str, bool]:
    """One entity block ("  - Entity: ..." to the next): the binding moved to the component. (block, changed)"""
    match = re.search(r"^    NativeScriptComponent:\n      Scripts:\n((?:        .*\n|      .*\n)*)", block, flags=re.M)
    if match is None:
        return block, False
    scripts_text = match.group(1)
    # Split the Scripts list into entries ("        - ..." and its deeper lines).
    entries = re.findall(r"(        - .*\n(?:          .*\n)*)", scripts_text)
    if "".join(entries) != scripts_text:
        return block, False  # a layout this tool doesn't know: left alone
    kept, moved = [], None
    for entry in entries:
        data = yaml.safe_load("\n".join(line[8:] for line in entry.splitlines()))
        item = data[0] if isinstance(data, list) and data else None
        name = item if isinstance(item, str) else (item or {}).get("Name") if isinstance(item, dict) else None
        if name == script and moved is None:
            moved = (item.get("Properties") or {}) if isinstance(item, dict) else {}
        elif name == script:
            continue  # bound twice: loading kept the first
        else:
            kept.append(entry)
    if moved is None:
        return block, False

    replacement = ""
    if kept:
        replacement = "    NativeScriptComponent:\n      Scripts:\n" + "".join(kept)
    block = block[: match.start()] + replacement + block[match.end():]
    if re.search(r"^    %s:" % re.escape(component), block, flags=re.M) is None:
        body = "".join(f"      {key}: {scalar(value)}\n" for key, value in moved.items()) if moved else ""
        text = f"    {component}:\n{body}" if body else f"    {component}: {{}}\n"
        if block.endswith("\n") is False:
            block += "\n"
        block += text
    return block, True


def migrate_file(path: Path, script: str, component: str, dry_run: bool) -> int:
    raw = path.read_bytes()
    newline = "\r\n" if b"\r\n" in raw else "\n"
    text = raw.decode("utf-8").replace("\r\n", "\n")
    parts = re.split(r"(?=^  - Entity: )", text, flags=re.M)
    count = 0
    for i in range(1, len(parts)):
        trailing = ""
        block = parts[i]
        # The last block may be followed by other top-level keys: keep them out of the entity.
        top = re.search(r"^\S", block, flags=re.M)
        if top is not None:
            block, trailing = block[: top.start()], block[top.start():]
        block, changed = migrate_block(block, script, component)
        if changed:
            count += 1
        parts[i] = block + trailing
    if count and dry_run is False:
        path.write_bytes("".join(parts).replace("\n", newline).encode("utf-8"))
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--script", required=True)
    parser.add_argument("--component", required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("files", nargs="*", type=Path)
    args = parser.parse_args()
    files = args.files or sorted(list(ASSETS.rglob("*.hyscene")) + list(ASSETS.rglob("*.hyprefab")))
    total = 0
    for path in files:
        count = migrate_file(path, args.script, args.component, args.dry_run)
        if count:
            print(f"{path}: {count} {args.script} -> {args.component}")
            total += count
    print(f"{total} binding(s) {'would be ' if args.dry_run else ''}moved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
