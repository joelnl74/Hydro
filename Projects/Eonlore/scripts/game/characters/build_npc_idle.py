#!/usr/bin/env python3
"""Builds an NPC's field sheet (Idle<Facing>, Walk<Facing> and Work<Facing> when given) from PixelLab exports.

    python scripts/game/characters/build_npc_idle.py <idle> [<idle.json>] <out dir> <Name> [--handles TEX ANIM]
        [--durations 0.5 0.4 0.5 0.4] [--idle-step N]
        [--walk <walk> [<walk.json>]] [--walk-frames east=0,1,2,3,1,2 ...] [--walk-mirror west=east ...]
        [--work <work> [<work.json>]] [--work-frames east=3,4,5,6,7,8] [--work-mirror west=east]
        [--work-directions east west] [--work-duration 0.12] [--no-glow]

An export is either PixelLab's "spritesheet" (a .png and its .json: row 0 the eight rotations, then one row per
direction of the animation, square cells with the pivot at the cell centre) or one state folder of an unpacked
character zip (<state>/rotations/<direction>.png and <state>/animations/<Animation>/<direction>/*.png, square
frames of any size, also centred). The same clean-up as build_miko_field.py (its one-frame flicker test limited to
isolated specks): every pixel snapped to the colours of the character's own rotations, binary alpha, each
animation's lowest foot on the baseline, every frame clipped to its cell. Fixes for PixelLab's usual faults:
  --idle-step 2           an 8-frame breathing idle kept as every other frame (a whole breath in 4 frames);
  --walk-frames d=list    a direction's walk built from these frames of its row, e.g. east=0,1,2,3,1,2 when the
                          last frames drop or swap the prop the character carries (the strides repeat instead);
  --walk-mirror d=src     a direction's walk is the mirror image of another's (its own frames are broken);
  --work-...              the same for an optional work loop (hoeing, hammering), kept only for the directions
                          listed in --work-directions (the others had baked-in effects or props);
  --no-glow               saturated red (a glowing hot spot that only some animations have) painted over with
                          the neighbouring colour, so the prop looks the same standing and walking.
Writes <Name>.png (96 x 96 cells: rows IdleDown, IdleRight, IdleUp, IdleLeft, then WalkDown, WalkRight, WalkUp,
WalkLeft, then the Work rows), <Name>.png.hymeta (cuts 96 x 90 ending at the feet, pivot [0.5, 0]: the cast's scale
2.5 x 2.34375, 38.4 pixels per metre) and <Name>.hyanim (Idle = IdleDown, the Idle<Facing>, Walk<Facing> (0.1 s a
frame) and Work<Facing> clips). Pass the old files' handles with --handles so scenes keep their references.
"""
from __future__ import annotations

import argparse
import glob
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
    colour. (build_miko_field's version also flattens a real two-frame motion, like a breathing loop's rise.) The
    frames must be the same size."""
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


class Export:
    """One PixelLab export: its rotations and, per direction, the frames of its animation."""

    def __init__(self, paths: list):
        path = Path(paths[0])
        self.frames = {}
        if path.is_dir():
            self.rotations = [Image.open(f).convert("RGBA") for f in sorted(glob.glob(str(path / "rotations" / "*.png")))]
            for d in ORDER:
                files = sorted(glob.glob(str(path / "animations" / "*" / d / "*.png")))
                if files:
                    self.frames[d] = [Image.open(f).convert("RGBA") for f in files]
        else:
            meta = Path(paths[1]) if len(paths) > 1 else path.with_suffix(".json")
            data = json.loads(meta.read_text(encoding="utf-8"))["spritesheet"]
            src, size = Image.open(path).convert("RGBA"), data["cell_size"]["width"]
            self.rotations = [src.crop((i * size, 0, i * size + size, size)) for i in range(8)]
            for row in data["rows"]:
                if row["type"] == "animation" and row["direction"] in ORDER:
                    r = row["row"]
                    self.frames[row["direction"]] = [src.crop((i * size, r * size, i * size + size, r * size + size))
                                                     for i in range(row["frame_count"])]


def uniform(frames: list) -> list:
    """The frames on one canvas (the largest), centred, so a direction's frames can be compared pixel by pixel."""
    size = max(f.width for f in frames)
    out = []
    for f in frames:
        canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        canvas.paste(f, ((size - f.width) // 2, (size - f.height) // 2))
        out.append(canvas)
    return out


def place(sheet, frames, out_row, label):
    frames = uniform(frames)
    fixed = deflicker(frames)
    size = frames[0].width
    lowest = max(bbox(f)[3] for f in frames)
    for i, f in enumerate(frames):
        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.paste(f, ((CELL - size) // 2, (CELL - BASE) - lowest), f)
        sheet.paste(cell, (i * CELL, out_row * CELL), cell)
    print(f"{label}: {len(frames)} frames, {fixed} flicker pixels fixed")


def parse_pairs(items):
    return dict(item.split("=", 1) for item in items or [])


def build(export: Export, picks: dict, mirrors: dict, directions, palette, no_glow) -> dict:
    raw = {}
    for d in directions:
        if d not in export.frames and d not in mirrors:
            sys.exit(f"the export has no {d} frames")
        frames = export.frames.get(d, [])
        raw[d] = [frames[int(i)] for i in picks[d].split(",")] if d in picks else frames
    for d, other in mirrors.items():
        raw[d] = [ImageOps.mirror(f) for f in raw[other]]
    out = {}
    for d in directions:
        frames = [f.copy() for f in raw[d]]
        if no_glow:
            for f in frames:
                cool_glow(f)
        out[d] = [snap(f, palette) for f in frames]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inputs", nargs="+", help="<idle> [<idle.json>] <out dir> <Name>")
    parser.add_argument("--handles", type=int, nargs=2, default=None)
    parser.add_argument("--durations", type=float, nargs="+", default=[0.5, 0.4, 0.5, 0.4])
    parser.add_argument("--idle-step", type=int, default=1)
    parser.add_argument("--walk", nargs="+", default=None)
    parser.add_argument("--walk-frames", nargs="*", default=None)
    parser.add_argument("--walk-mirror", nargs="*", default=None)
    parser.add_argument("--walk-duration", type=float, default=0.1)
    parser.add_argument("--work", nargs="+", default=None)
    parser.add_argument("--work-frames", nargs="*", default=None)
    parser.add_argument("--work-mirror", nargs="*", default=None)
    parser.add_argument("--work-directions", nargs="*", default=None)
    parser.add_argument("--work-duration", type=float, default=0.12)
    parser.add_argument("--no-glow", action="store_true")
    args = parser.parse_args()
    if len(args.inputs) not in (3, 4):
        parser.error("expected <idle> [<idle.json>] <out dir> <Name>")
    *idle_paths, out_dir, name = args.inputs
    out_dir = Path(out_dir)

    exports = {"idle": Export(idle_paths)}
    if args.walk:
        exports["walk"] = Export(args.walk)
    if args.work:
        exports["work"] = Export(args.work)
    colours = set()
    for export in exports.values():
        for r in export.rotations:
            colours |= {p[:3] for p in r.get_flattened_data() if p[3] >= 128 and (args.no_glow == False or is_glow(p) == False)}
    palette = sorted(colours)

    idle = build(exports["idle"], {}, {}, ORDER, palette, args.no_glow)
    idle = {d: f[::args.idle_step][:4] for d, f in idle.items()}
    walks = build(exports["walk"], parse_pairs(args.walk_frames), parse_pairs(args.walk_mirror), ORDER, palette,
                  args.no_glow) if args.walk else {}
    work_dirs = [d for d in ORDER if d in (args.work_directions or ORDER)]
    works = build(exports["work"], parse_pairs(args.work_frames), parse_pairs(args.work_mirror), work_dirs, palette,
                  args.no_glow) if args.work else {}

    blocks = [("Idle", idle, ORDER), ("Walk", walks, ORDER), ("Work", works, work_dirs)]
    counts = {kind: min((len(f) for f in clips.values()), default=0) for kind, clips, _ in blocks}
    rows, row = [], 0
    for kind, clips, dirs in blocks:
        for d in dirs if clips else []:
            rows.append((kind, d, row, clips[d][:counts[kind]]))
            row += 1
    sheet = Image.new("RGBA", (CELL * max(counts.values()), CELL * len(rows)), (0, 0, 0, 0))
    for kind, d, r, frames in rows:
        place(sheet, frames, r, f"{kind} {d}")

    out_dir.mkdir(parents=True, exist_ok=True)
    sheet.save(out_dir / f"{name}.png")
    texture, animations = args.handles if args.handles else (random.getrandbits(63), random.getrandbits(63))
    top = CELL - BASE - CUT
    sprites = "\n".join(f"    - {{Name: {kind}{FACINGS[d]}_{i}, Rect: [{i * CELL}, {r * CELL + top}, {CELL}, {CUT}], Pivot: [0.5, 0]}}"
                        for kind, d, r, frames in rows for i in range(len(frames)))
    hymeta = (f"Handle: {texture}\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
              f"  SpriteMode: Multiple\n  PixelsPerUnit: 32\n  Slicing:\n    Mode: Manual\n    CellSize: [{CELL}, {CELL}]\n"
              f"    Offset: [0, 0]\n    Spacing: [0, 0]\n    Columns: 0\n    Rows: 0\n    RowNames: []\n"
              f"    DefaultPivot: [0.5, 0]\n  Sprites:\n{sprites}\n")
    (out_dir / f"{name}.png.hymeta").write_bytes(hymeta.replace("\n", "\r\n").encode())

    def clip(clip_name, sprite, count, durations):
        frames = "".join(f"        - Sprite: {sprite}_{i}\n          Duration: {durations[i % len(durations)]}\n"
                         for i in range(count))
        return f"    - Name: {clip_name}\n      Loop: Loop\n      Frames:\n{frames}"
    durations = {"Idle": args.durations, "Walk": [args.walk_duration], "Work": [args.work_duration]}
    what = ", ".join(k.lower() for k, c, _ in blocks if c)
    text = (f"# {name}: {what} in its directions (scripts/game/characters/build_npc_idle.py).\n"
            f"Handle: {animations}\nSpriteAnimationSet:\n  TextureHandle: {texture}\n  Texture: {name}.png\n"
            f"  DefaultClip: IdleDown\n  Clips:\n" + clip("Idle", "IdleDown", counts["Idle"], args.durations)
            + "".join(clip(f"{kind}{FACINGS[d]}", f"{kind}{FACINGS[d]}", len(frames), durations[kind])
                      for kind, d, r, frames in rows))
    (out_dir / f"{name}.hyanim").write_bytes(text.replace("\n", "\r\n").encode())


if __name__ == "__main__":
    main()
