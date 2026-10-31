#!/usr/bin/env python3
"""Builds an NPC's idle sheet (Idle<Facing>, four directions) from a PixelLab "spritesheet" export.

    python scripts/characters/build_npc_idle.py <export.png> <export.json> <out dir> <Name> [--handles TEX ANIM]
        [--durations 0.5 0.4 0.5 0.4]

The export is PixelLab's sheet: row 0 the eight rotations, then one row per direction of the "Breathing Idle"
animation (south, east, north, west), 128 px cells with the pivot at the cell centre. The same clean-up as
build_miko_field.py: every pixel snapped to the colours of the character's own rotations, binary alpha, one-frame
flicker removed, each direction's lowest foot on the baseline. Writes <Name>.png (96 x 96 cells: rows IdleDown,
IdleRight, IdleUp, IdleLeft), <Name>.png.hymeta (cuts 96 x 90 ending at the feet, pivot [0.5, 0]: draw the entity at
3 x 2.8125 for 32 pixels per metre, or keep the field cast's scale) and <Name>.hyanim (Idle = IdleDown, and the four
Idle<Facing> clips). Pass the old files' handles with --handles so scenes keep their references.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_miko_field import BASE, CELL, CUT, bbox, deflicker, snap  # noqa: E402

FACINGS = {"south": "Down", "east": "Right", "north": "Up", "west": "Left"}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sheet", type=Path)
    parser.add_argument("meta", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("name")
    parser.add_argument("--handles", type=int, nargs=2, default=None)
    parser.add_argument("--durations", type=float, nargs="+", default=[0.5, 0.4, 0.5, 0.4])
    args = parser.parse_args()

    meta = json.loads(args.meta.read_text(encoding="utf-8"))["spritesheet"]
    size = meta["cell_size"]["width"]
    src = Image.open(args.sheet).convert("RGBA")
    rotations = src.crop((0, 0, size * 8, size))
    palette = sorted({p[:3] for p in rotations.get_flattened_data() if p[3] >= 128})

    rows = [r for r in meta["rows"] if r["type"] == "animation"]
    order = ["south", "east", "north", "west"]
    sheet = Image.new("RGBA", (CELL * 4, CELL * 4), (0, 0, 0, 0))
    frames_per = 0
    for row in rows:
        direction = row["direction"]
        if direction not in order:
            continue
        r = row["row"]
        frames = [snap(src.crop((i * size, r * size, i * size + size, r * size + size)), palette)
                  for i in range(row["frame_count"])]
        fixed = deflicker(frames)
        lowest = max(bbox(f)[3] for f in frames)
        out_row = order.index(direction)
        for i, f in enumerate(frames[:4]):
            sheet.paste(f, (i * CELL + (CELL - size) // 2, out_row * CELL + (CELL - BASE) - lowest), f)
        frames_per = max(frames_per, min(4, len(frames)))
        print(f"{direction}: {len(frames)} frames, {fixed} flicker pixels fixed")

    args.out.mkdir(parents=True, exist_ok=True)
    sheet.save(args.out / f"{args.name}.png")
    texture, animations = args.handles if args.handles else (random.getrandbits(63), random.getrandbits(63))
    top = CELL - BASE - CUT
    sprites = "\n".join(f"    - {{Name: Idle{FACINGS[d]}_{i}, Rect: [{i * CELL}, {k * CELL + top}, {CELL}, {CUT}], Pivot: [0.5, 0]}}"
                        for k, d in enumerate(order) for i in range(frames_per))
    hymeta = (f"Handle: {texture}\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
              f"  SpriteMode: Multiple\n  PixelsPerUnit: 32\n  Slicing:\n    Mode: Manual\n    CellSize: [{CELL}, {CELL}]\n"
              f"    Offset: [0, 0]\n    Spacing: [0, 0]\n    Columns: 0\n    Rows: 0\n    RowNames: []\n"
              f"    DefaultPivot: [0.5, 0]\n  Sprites:\n{sprites}\n")
    (args.out / f"{args.name}.png.hymeta").write_bytes(hymeta.replace("\n", "\r\n").encode())

    def clip(name, facing):
        frames = "".join(f"        - Sprite: Idle{facing}_{i}\n          Duration: {args.durations[i % len(args.durations)]}\n"
                         for i in range(frames_per))
        return f"    - Name: {name}\n      Loop: Loop\n      Frames:\n{frames}"
    hyanim = (f"# {args.name}: a slow breathing idle in four directions (scripts/characters/build_npc_idle.py).\n"
              f"Handle: {animations}\nSpriteAnimationSet:\n  TextureHandle: {texture}\n  Texture: {args.name}.png\n"
              f"  DefaultClip: IdleDown\n  Clips:\n" + clip("Idle", "Down")
              + "".join(clip("Idle" + f, f) for f in ("Down", "Right", "Up", "Left")))
    (args.out / f"{args.name}.hyanim").write_bytes(hyanim.replace("\n", "\r\n").encode())


if __name__ == "__main__":
    main()
