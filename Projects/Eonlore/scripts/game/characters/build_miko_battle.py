#!/usr/bin/env python3
"""Builds Miko's battle sheet (Characters/Player/Miko/Miko.png + Miko.hyanim) from PixelLab exports.

    python scripts/game/characters/build_miko_battle.py --field-idle DIR --field-walk DIR --battle DIR --out DIR [--preview PNG]

--field-idle / --field-walk: the field Idle and walking character exports, unzipped (as build_miko_field.py takes).
--battle: a folder holding the battle "spritesheet" exports unzipped one per sub-folder, named battle_idle, punch,
combo (the two-punch combo), spin_kick and death (each a <name>.png + <name>.json, west-facing rows).

The layout stays the one BattleScript and scripts/game/battle/build_arena.py expect: 96 x 96 cells, pivot at the cell
centre, the feet 15 px above the cell's bottom (HeroFoot = 15 / 96), drawn at 2.4 m (HERO_SIZE), sprite names
Miko_<row * 8 + column>, the battle stance first frame Miko_64. Rows: 0 Idle (field, facing the camera), 1 WalkDown,
2 WalkRight, 3 WalkUp, 4 WalkLeft (field walks; the battle uses Left and Right), 5 Kick, 6 Punch, 7 Death,
8 BattleIdle, 9 DoublePunch (Holy Palm). Punch and DoublePunch are picked frame by frame from PixelLab's loops (PICKS):
the guard, the extension held over the impact time, the recovery. Each animation gets build_miko_field.py's clean-up (palette, flicker, the hair bell put back) and its
lowest foot on the feet row; the death frames each rest on the floor. Punch and Kick are timed so their contact
frame is on screen at BattleScript's PunchImpactTime (0.5 s) and KickImpactTime (0.4 s); DoublePunch's second
strike holds over 0.5 s too (the first strike is for show, the hit lands on the second).
"""
from __future__ import annotations

import argparse
import glob
import json
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_miko_field import add_hair_bell, bbox, deflicker, has_gold, load_frames, snap  # noqa: E402

CELL, FEET = 96, 96 - 15  # the feet row (bottom of the lowest opaque pixel, exclusive)
# Clip: (export, [(frame, seconds), ...]). The exports' loops start mid-punch, so the clips are cut from them.
PICKS = {
    "Punch": ("punch", [(5, 0.2), (7, 0.2), (0, 0.2), (1, 0.1), (2, 0.15)]),  # contact 0.4-0.6 s
    "DoublePunch": ("combo", [(6, 0.12), (0, 0.1), (1, 0.1), (2, 0.12), (3, 0.2), (4, 0.1), (5, 0.12)]),  # 2nd 0.44-0.64 s
}


def battle_frames(folder: Path, direction: str = "west") -> list:
    meta = json.loads(next(folder.glob("*.json")).read_text(encoding="utf-8"))["spritesheet"]
    size = meta["cell_size"]["width"]
    sheet = Image.open(next(folder.glob("*.png"))).convert("RGBA")
    for row in meta["rows"]:
        if row["type"] == "animation" and row.get("direction") == direction:
            r = row["row"]  # a row holds all its frames, however many (the sheet is as wide as it needs)
            return [sheet.crop((i * size, r * size, i * size + size, r * size + size)) for i in range(row["frame_count"])]
    raise SystemExit(f"{folder}: no {direction} animation row")


def deflicker_once(frames: list) -> int:
    """deflicker() for a one-shot clip: the first and last frames are left alone (no wrap-around)."""
    return sum(deflicker_triplet(frames[i - 1:i + 2]) for i in range(1, len(frames) - 1))


def deflicker_triplet(trio: list) -> int:
    a, b, c = (f.load() for f in trio)
    w, h = trio[1].size
    changes = []
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            if b[x, y] != a[x, y] and a[x, y] == c[x, y]:
                if all(a[x + dx, y + dy] == c[x + dx, y + dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                    changes.append((x, y, a[x, y]))
    for x, y, p in changes:
        b[x, y] = p
    return len(changes)


def place(sheet: Image.Image, frames: list, row: int, per_frame_floor: bool = False):
    lowest = max(bbox(f)[3] for f in frames)
    for i, f in enumerate(frames[:8]):
        floor = bbox(f)[3] if per_frame_floor else lowest
        x = i * CELL + (CELL - f.width) // 2
        y = row * CELL + FEET - floor
        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.paste(f, (x - i * CELL, y - row * CELL), f)  # clipped to the cell
        sheet.alpha_composite(cell, (i * CELL, row * CELL))


def clean(frames: list, loop: bool) -> list:
    frames = [snap(f) for f in frames]
    if loop:
        deflicker(frames)
    else:
        deflicker_once(frames)
    for f in frames:
        if has_gold(f) is False:
            add_hair_bell(f)
    return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--field-idle", type=Path, required=True)
    parser.add_argument("--field-walk", type=Path, required=True)
    parser.add_argument("--battle", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--preview", type=Path, default=None)
    args = parser.parse_args()

    sheet = Image.new("RGBA", (CELL * 8, CELL * 10), (0, 0, 0, 0))
    counts = {}
    place(sheet, clean(load_frames(args.field_idle, "idle", "south"), True), 0)
    for row, direction in ((1, "south"), (2, "east"), (3, "north"), (4, "west")):
        place(sheet, clean(load_frames(args.field_walk, "walk", direction), True), row)
    for row, name, loop in ((5, "spin_kick", False), (7, "death", False), (8, "battle_idle", True)):
        frames = clean(battle_frames(args.battle / name), loop)
        counts[name] = len(frames)
        place(sheet, frames, row, per_frame_floor=(name == "death"))
    durations = {}
    for row, clip in ((6, "Punch"), (9, "DoublePunch")):
        export, picks = PICKS[clip]
        source = clean(battle_frames(args.battle / export), True)  # PixelLab's punches are loops
        place(sheet, [source[i] for i, _ in picks], row)
        durations[clip] = [d for _, d in picks]
    args.out.mkdir(parents=True, exist_ok=True)
    sheet.save(args.out / "Miko.png")

    def names(row, n):
        return [f"Miko_{row * 8 + i}" for i in range(n)]

    def rate_clip(name, row, n, fps, loop):
        return (f"    - Name: {name}\n      Loop: {'Loop' if loop else 'Once'}\n      FrameRate: {fps}\n"
                f"      Frames: [{', '.join(names(row, n))}]\n")

    def timed_clip(name, row, durations, loop):
        frames = "".join(f"        - Sprite: {s}\n          Duration: {d}\n" for s, d in zip(names(row, len(durations)), durations))
        return f"    - Name: {name}\n      Loop: {'Loop' if loop else 'Once'}\n      Frames:\n{frames}"

    clips = (rate_clip("Idle", 0, 4, 5, True) + rate_clip("WalkDown", 1, 6, 9, True) + rate_clip("WalkRight", 2, 6, 9, True)
             + rate_clip("WalkUp", 3, 6, 9, True) + rate_clip("WalkLeft", 4, 6, 9, True)
             + rate_clip("Kick", 5, counts["spin_kick"], 10, False)  # frame 5 (contact) from 0.4 s
             + timed_clip("Punch", 6, durations["Punch"], False)
             + rate_clip("Death", 7, counts["death"], 10, False)
             + rate_clip("BattleIdle", 8, counts["battle_idle"], 8, True)
             + timed_clip("DoublePunch", 9, durations["DoublePunch"], False))
    hyanim = ("Handle: 9503405222623168017\nSpriteAnimationSet:\n  TextureHandle: 16527506524077974322\n"
              "  Texture: Miko.png\n  DefaultClip: Idle\n  Clips:\n" + clips)
    (args.out / "Miko.hyanim").write_bytes(hyanim.replace("\n", "\r\n").encode())
    print("frames:", counts)
    if args.preview:
        bg = Image.new("RGBA", sheet.size, (70, 78, 62, 255))
        bg.alpha_composite(sheet)
        bg.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).convert("RGB").save(args.preview)


if __name__ == "__main__":
    main()
