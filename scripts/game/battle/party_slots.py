#!/usr/bin/env python3
"""Writes the party's battle slots into Battle.hyscene: "Party Slot <i>" and "Party Slot <i> Shadow" in the Party folder.

    python scripts/game/battle/party_slots.py

BattleScript puts the party's fighters into these slots in party order (PartyState: the first members fight) and plays
each hero's battle animations (HeroDefinition::BattleAnimations) on its slot's sprite; empty slots are hidden. The
slots stand upright on the floor like the hero model they are copied from, at FEET (floor points in metres; the
enemies stand on the left). Move them in the editor freely: the script reads where each slot stands when the battle
starts. The status blocks ("Status <i>") are runtime UI: scripts/game/ui/build_game_ui.py battle.

The model is the first of "Party Slot 0" and the old "Hero Miko". Idempotent: the slots it wrote before and the old
per-hero entities ("Hero Miko", "Hero Orin" and their shadows) are replaced; nothing else in the file changes.
"""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[3]
SCENE = ROOT / "Oxygen" / "project" / "Assets" / "Scenes" / "Battle.hyscene"

# PartyState::MaxFighters slots: the floor point each one's feet stand on, and the sheet it shows in the editor
# (texture, animations, sprite name); the battle dresses it from the hero's data.
MIKO = (16527506524077974322, 9503405222623168017, "Miko_64", 64)
ORIN = (7924613408950187331, 3182937116204411939, "Orin_0", 0)
FEET = [((3.1759, -0.8897), MIKO), ((4.55, 1.05), ORIN), ((3.9, -2.4), MIKO), ((5.9, 2.4), ORIN)]
OLD = ["Hero Miko", "Hero Miko Shadow", "Hero Orin", "Hero Orin Shadow"]


def uid(tag: str) -> int:
    digest = hashlib.sha1(("Battle/party/%s" % tag).encode()).digest()
    return int.from_bytes(digest[:8], "little") & ((1 << 63) - 1)


def main() -> int:
    raw = SCENE.read_bytes().decode("utf-8")
    text = raw.replace("\r\n", "\n")
    ends_newline = text.endswith("\n")
    match = re.search(r"^( *)- Entity: ", text, re.M)
    indent = match.group(1)
    head, body = text[: match.start()], text[match.start():]
    blocks = re.split(r"\n(?=%s- Entity: )" % indent, body.rstrip("\n"))
    parsed = [yaml.safe_load(block[len(indent):].replace("\n" + indent, "\n"))[0] for block in blocks]
    by_tag = {entity["TagComponent"]["Tag"]: (block, entity) for block, entity in zip(blocks, parsed)}

    model_tag = "Party Slot 0" if "Party Slot 0" in by_tag else "Hero Miko"
    model_block, model = by_tag[model_tag]
    shadow_block, model_shadow = by_tag[model_tag + " Shadow"]
    # The sprite stands this far from its feet (BattleScript StandOffset at its scale).
    feet = model_shadow["TransformComponent"]["Translation"]
    stand = [model["TransformComponent"]["Translation"][i] - feet[i] for i in range(3)]

    slots = []
    for s, ((x, y), (texture, animations, sprite, index)) in enumerate(FEET):
        tag = "Party Slot %d" % s
        block = model_block
        block = block.replace("- Entity: %d" % model["Entity"], "- Entity: %d" % uid(tag))
        block = block.replace("Tag: %s" % model_tag, "Tag: %s" % tag)
        block = re.sub(r"Translation: \[[^\]]*\]", "Translation: [%s, %s, %s]" % (round(x + stand[0], 4), round(y + stand[1], 4),
                                                                                 round(stand[2], 4)), block, count=1)
        block = re.sub(r"TextureHandle: \d+", "TextureHandle: %d" % texture, block)
        block = re.sub(r"Sprite: \w+", "Sprite: %s" % sprite, block, count=1)
        block = re.sub(r"SpriteIndex: \d+", "SpriteIndex: %d" % index, block)
        block = re.sub(r"AnimationsHandle: \d+", "AnimationsHandle: %d" % animations, block)
        slots.append(block)
        shadow = shadow_block
        shadow = shadow.replace("- Entity: %d" % model_shadow["Entity"], "- Entity: %d" % uid(tag + " Shadow"))
        shadow = shadow.replace("Tag: %s Shadow" % model_tag, "Tag: %s Shadow" % tag)
        shadow = re.sub(r"Translation: \[[^\]]*\]", "Translation: [%s, %s, 0]" % (x, y), shadow, count=1)
        slots.append(shadow)

    ours = {uid("Party Slot %d" % s) for s in range(len(FEET))} | {uid("Party Slot %d Shadow" % s) for s in range(len(FEET))}
    replaced = {entity["Entity"] for entity in parsed if entity["Entity"] in ours or entity["TagComponent"]["Tag"] in OLD}
    for block, entity in zip(blocks, parsed):
        if entity["Entity"] in replaced:
            continue
        stale = replaced.intersection(int(number) for number in re.findall(r"\b\d+\b", block))
        if stale:
            raise SystemExit("Battle.hyscene: a kept entity still refers to replaced %d:\n%s" % (min(stale), block[:300]))

    # Where the model's shadow was, in the Party folder (the blocks keep the model's parent).
    out = []
    for block, entity in zip(blocks, parsed):
        if entity["Entity"] == model_shadow["Entity"]:
            out.extend(slots)
        if entity["Entity"] not in replaced:
            out.append(block)
    new = head + "\n".join(out) + ("\n" if ends_newline else "")
    yaml.safe_load(new)
    SCENE.write_bytes(new.replace("\n", "\r\n").encode("utf-8"))
    print("Battle.hyscene: %s" % ", ".join("Party Slot %d" % s for s in range(len(FEET))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
