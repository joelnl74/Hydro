#!/usr/bin/env python3
"""Adds the party's other heroes to Battle.hyscene next to Miko: "Hero <Id>" and "Hero <Id> Shadow" in the Party folder.

    python scripts/game/battle/party_slots.py

BattleScript finds a hero by "Hero <Id>" (its sprite, standing upright on the floor like "Hero Miko") and
"Hero <Id> Shadow" (its contact shadow), and hides heroes not in the party. Miko's entities are the model: the same
scale and sprite settings, its texture and animations swapped for the hero's, standing at FEET (the floor point,
in metres; Miko stands at about (3.2, -0.9), the enemies on the left). The status block ("Status <Id>") is runtime
UI: scripts/game/ui/build_game_ui.py battle.

Idempotent: the entities it wrote before are replaced (their IDs come from their tags); nothing else in the file
changes. The rest of the arena was built by build_arena.py, which is out of date (its asset paths moved).
"""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[3]
SCENE = ROOT / "Oxygen" / "project" / "Assets" / "Scenes" / "Battle.hyscene"

# Id, battle texture, battle animations, the floor point its feet stand on.
HEROES = [
    ("Orin", 7924613408950187331, 3182937116204411939, (4.55, 1.05)),
]


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

    ours = {uid("Hero %s" % hero) for hero, *_ in HEROES} | {uid("Hero %s Shadow" % hero) for hero, *_ in HEROES}
    kept = [(block, entity) for block, entity in zip(blocks, parsed) if entity["Entity"] not in ours]
    by_tag = {entity["TagComponent"]["Tag"]: (block, entity) for block, entity in kept}
    miko_block, miko = by_tag["Hero Miko"]
    shadow_block, miko_shadow = by_tag["Hero Miko Shadow"]
    # Miko's sprite stands this far from her feet (BattleScript StandOffset at her scale).
    feet = miko_shadow["TransformComponent"]["Translation"]
    stand = [miko["TransformComponent"]["Translation"][i] - feet[i] for i in range(3)]

    added = []
    for hero, texture, animations, (x, y) in HEROES:
        block = miko_block
        block = block.replace("- Entity: %d" % miko["Entity"], "- Entity: %d" % uid("Hero %s" % hero))
        block = block.replace("Tag: Hero Miko", "Tag: Hero %s" % hero)
        block = re.sub(r"Translation: \[[^\]]*\]", "Translation: [%s, %s, %s]" % (round(x + stand[0], 4), round(y + stand[1], 4), round(stand[2], 4)), block, count=1)
        block = re.sub(r"TextureHandle: \d+", "TextureHandle: %d" % texture, block)
        block = re.sub(r"Sprite: \w+", "Sprite: Orin_0" if hero == "Orin" else "Sprite: %s_0" % hero, block, count=1)
        block = re.sub(r"SpriteIndex: \d+", "SpriteIndex: 0", block)
        block = re.sub(r"AnimationsHandle: \d+", "AnimationsHandle: %d" % animations, block)
        added.append(block)
        shadow = shadow_block
        shadow = shadow.replace("- Entity: %d" % miko_shadow["Entity"], "- Entity: %d" % uid("Hero %s Shadow" % hero))
        shadow = shadow.replace("Tag: Hero Miko Shadow", "Tag: Hero %s Shadow" % hero)
        shadow = re.sub(r"Translation: \[[^\]]*\]", "Translation: [%s, %s, 0]" % (x, y), shadow, count=1)
        added.append(shadow)

    # After Miko's shadow, in the Party folder (the blocks keep Miko's parent).
    out = []
    for block, entity in kept:
        out.append(block)
        if entity["Entity"] == miko_shadow["Entity"]:
            out.extend(added)
    new = head + "\n".join(out) + ("\n" if ends_newline else "")
    yaml.safe_load(new)
    SCENE.write_bytes(new.replace("\n", "\r\n").encode("utf-8"))
    print("Battle.hyscene: %s" % ", ".join("Hero %s" % hero for hero, *_ in HEROES))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
