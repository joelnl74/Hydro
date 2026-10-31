#!/usr/bin/env python3
"""Builds an NPC's field sheet (Idle<Facing>, and Walk<Facing> when a walk export is given) from PixelLab
"spritesheet" exports.

    python scripts/characters/build_npc_idle.py <idle.png> <idle.json> <out dir> <Name> [--handles TEX ANIM]
        [--durations 0.5 0.4 0.5 0.4] [--idle-step N]
        [--walk <walk.png> <walk.json>] [--walk-frames east=0,1,2,3,1,2 ...] [--walk-mirror west=east ...]
        [--no-glow]

An export is PixelLab's sheet: row 0 the eight rotations, then one row per direction of the animation (south,
east, north, west), square cells (116 or 128 px) with the pivot at the cell centre. The same clean-up as
build_miko_field.py (its one-frame flicker test limited to
isolated specks): every pixel snapped to the colours of the character's own rotations, binary alpha, one-frame
flicker removed, each animation's lowest foot on the baseline. Fixes for PixelLab's usual faults:
  --idle-step 2           an 8-frame breathing idle kept as every other frame (a whole breath in 4 frames);
  --walk-frames d=list    a direction's walk built from these frames of its row, e.g. east=0,1,2,3,1,2 when the
                          last frames drop or swap the prop the character carries (the strides repeat instead);
  --walk-mirror d=src     a direction's walk is the mirror image of another's (its own frames are broken);
  --no-glow               saturated red (a glowing hot spot that only some animations have) painted over with
                          the neighbouring colour, so the prop looks the same standing and walking.
Writes <Name>.png (96 x 96 cells: rows IdleDown, IdleRight, IdleUp, IdleLeft, then WalkDown, WalkRight, WalkUp,
WalkLeft), <Name>.png.hymeta (cuts 96 x 90 ending at the feet, pivot [0.5, 0]: the cast's scale 2.5 x 2.34375,
38.4 pixels per metre) and <Name>.hyanim (Idle = IdleDown, the Idle<Facing> clips, and the Walk<Facing> clips at 0.1 s
a frame). Pass the old files' handles with --handles so scenes keep their references.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
from collections import Counter
from pathlib import Path

from PIL import Image, ImageOps

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_miko_field import BASE, CELL, CUT, bbox, snap  # noqa: E402

FACINGS = {"south": "Down", "east": "Right", "north": "Up", "west": "Left"}
ORDER = ["south", "east", "north", "west"]


def is_glow(p) -> bool:
    return p[3] >= 128 and p[0] > 150 and p[1] < 100 and p[2] < 100 and p[0] - max(p[1], p[2]) > 70


def cool_glow(img: Image.Image) -> int:
    """Paints every glowing pixel with the commonest non-glowing colour around it (repeated inwards)."""
    px, painted = img.load(), 0
    for _ in range(6):
        todo = [(x, y) for y in range(img.height) for x in range(img.width) if is_glow(px[x, y])]
        if len(todo) == 0:
            break
        changes = []
        for x, y in todo:
            around = Counter(px[x + dx, y + dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                             if (dx or dy) and 0 <= x + dx < img.width and 0 <= y + dy < img.height
                             and px[x + dx, y + dy][3] >= 128 and is_glow(px[x + dx, y + dy]) == False)
            if around:
                changes.append((x, y, around.most_common(1)[0][0]))
        for x, y, p in changes:
            px[x, y] = p
        painted += len(changes)
    return painted


def deflicker(frames: list) -> int:
    """One-frame specks only: a pixel that differs from the frames before and after it, where those two agree on its
    whole 3 x 3 neighbourhood and at most one other pixel of that neighbourhood differs in this frame too, takes their
    colour. (build_miko_field's version also flattens a real two-frame motion, like a breathing loop's rise.)"""
    n, fixed = len(frames), 0
    if n < 3:
        return 0
    w, h = frames[0].size
    px = [f.load() for f in frames]
    near = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
    for i in range(n):
        a, b, c = px[(i - 1) % n], px[i], px[(i + 1) % n]
        changes = []
        for y in range(1, h - 1):
            for x in range(1, w - 1):
                if b[x, y] != a[x, y] and a[x, y] == c[x, y]:
                    if all(a[x + dx, y + dy] == c[x + dx, y + dy] for dx, dy in near):
                        if sum(1 for dx, dy in near if b[x + dx, y + dy] != a[x + dx, y + dy]) <= 2:
                            changes.append((x, y, a[x, y]))
        for x, y, p in changes:
            b[x, y] = p
        fixed += len(changes)
    return fixed


def load_export(sheet: Path, meta: Path):
    data = json.loads(meta.read_text(encoding="utf-8"))["spritesheet"]
    return Image.open(sheet).convert("RGBA"), data["cell_size"]["width"], data["rows"]


def frames_of(src, size, rows, direction, no_glow):
    row = next(r for r in rows if r["type"] == "animation" and r["direction"] == direction)
    frames = [src.crop((i * size, row["row"] * size, i * size + size, row["row"] * size + size))
              for i in range(row["frame_count"])]
    if no_glow:
        for f in frames:
            cool_glow(f)
    return frames


def place(sheet, frames, out_row, size, label):
    fixed = deflicker(frames)
    lowest = max(bbox(f)[3] for f in frames)
    for i, f in enumerate(frames):
        sheet.paste(f, (i * CELL + (CELL - size) // 2, out_row * CELL + (CELL - BASE) - lowest), f)
    print(f"{label}: {len(frames)} frames, {fixed} flicker pixels fixed")


def parse_pairs(items):
    return dict(item.split("=", 1) for item in items or [])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sheet", type=Path)
    parser.add_argument("meta", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("name")
    parser.add_argument("--handles", type=int, nargs=2, default=None)
    parser.add_argument("--durations", type=float, nargs="+", default=[0.5, 0.4, 0.5, 0.4])
    parser.add_argument("--idle-step", type=int, default=1)
    parser.add_argument("--walk", type=Path, nargs=2, default=None)
    parser.add_argument("--walk-frames", nargs="*", default=None)
    parser.add_argument("--walk-mirror", nargs="*", default=None)
    parser.add_argument("--walk-duration", type=float, default=0.1)
    parser.add_argument("--no-glow", action="store_true")
    args = parser.parse_args()

    idle_src, idle_size, idle_rows = load_export(args.sheet, args.meta)
    exports = [(idle_src, idle_size)]
    walk = load_export(*args.walk) if args.walk else None
    if walk:
        exports.append((walk[0], walk[1]))
    colours = set()
    for src, size in exports:
        colours |= {p[:3] for p in src.crop((0, 0, size * 8, size)).get_flattened_data()
                    if p[3] >= 128 and (args.no_glow == False or is_glow(p) == False)}
    palette = sorted(colours)

    idle = {}
    for d in ORDER:
        frames = frames_of(idle_src, idle_size, idle_rows, d, args.no_glow)[::args.idle_step][:4]
        idle[d] = [snap(f, palette) for f in frames]
    idle_count = min(len(f) for f in idle.values())

    walks = {}
    if walk:
        src, size, rows = walk
        picks = {d: [int(i) for i in v.split(",")] for d, v in parse_pairs(args.walk_frames).items()}
        raw = {}
        for d in ORDER:
            frames = frames_of(src, size, rows, d, args.no_glow)
            raw[d] = [frames[i] for i in picks[d]] if d in picks else frames
        for d, other in parse_pairs(args.walk_mirror).items():
            raw[d] = [ImageOps.mirror(f) for f in raw[other]]
        walks = {d: [snap(f, palette) for f in raw[d]] for d in ORDER}
    walk_count = min((len(f) for f in walks.values()), default=0)

    rows_out = 8 if walks else 4
    columns = max(idle_count, walk_count)
    sheet = Image.new("RGBA", (CELL * columns, CELL * rows_out), (0, 0, 0, 0))
    for k, d in enumerate(ORDER):
        place(sheet, idle[d][:idle_count], k, idle_size, f"Idle {d}")
    for k, d in enumerate(ORDER if walks else []):
        place(sheet, walks[d][:walk_count], 4 + k, walk[1], f"Walk {d}")

    args.out.mkdir(parents=True, exist_ok=True)
    sheet.save(args.out / f"{args.name}.png")
    texture, animations = args.handles if args.handles else (random.getrandbits(63), random.getrandbits(63))
    top = CELL - BASE - CUT
    cuts = [(f"Idle{FACINGS[d]}", k, idle_count) for k, d in enumerate(ORDER)]
    cuts += [(f"Walk{FACINGS[d]}", 4 + k, walk_count) for k, d in enumerate(ORDER) if walks]
    sprites = "\n".join(f"    - {{Name: {clip}_{i}, Rect: [{i * CELL}, {row * CELL + top}, {CELL}, {CUT}], Pivot: [0.5, 0]}}"
                        for clip, row, count in cuts for i in range(count))
    hymeta = (f"Handle: {texture}\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
              f"  SpriteMode: Multiple\n  PixelsPerUnit: 32\n  Slicing:\n    Mode: Manual\n    CellSize: [{CELL}, {CELL}]\n"
              f"    Offset: [0, 0]\n    Spacing: [0, 0]\n    Columns: 0\n    Rows: 0\n    RowNames: []\n"
              f"    DefaultPivot: [0.5, 0]\n  Sprites:\n{sprites}\n")
    (args.out / f"{args.name}.png.hymeta").write_bytes(hymeta.replace("\n", "\r\n").encode())

    def clip(name, sprite, count, durations):
        frames = "".join(f"        - Sprite: {sprite}_{i}\n          Duration: {durations[i % len(durations)]}\n"
                         for i in range(count))
        return f"    - Name: {name}\n      Loop: Loop\n      Frames:\n{frames}"
    what = "a slow breathing idle and a walk" if walks else "a slow breathing idle"
    hyanim = (f"# {args.name}: {what} in four directions (scripts/characters/build_npc_idle.py).\n"
              f"Handle: {animations}\nSpriteAnimationSet:\n  TextureHandle: {texture}\n  Texture: {args.name}.png\n"
              f"  DefaultClip: IdleDown\n  Clips:\n" + clip("Idle", "IdleDown", idle_count, args.durations)
              + "".join(clip("Idle" + f, "Idle" + f, idle_count, args.durations) for f in ("Down", "Right", "Up", "Left"))
              + ("".join(clip("Walk" + f, "Walk" + f, walk_count, [args.walk_duration]) for f in ("Down", "Right", "Up", "Left"))
                 if walks else ""))
    (args.out / f"{args.name}.hyanim").write_bytes(hyanim.replace("\n", "\r\n").encode())


if __name__ == "__main__":
    main()
