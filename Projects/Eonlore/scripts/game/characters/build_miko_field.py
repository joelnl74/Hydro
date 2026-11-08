#!/usr/bin/env python3
"""Builds Miko's field sheet (Characters/Player/Miko/Miko_Field.png + .hymeta + .hyanim) from PixelLab exports.

    python scripts/game/characters/build_miko_field.py --idle <Idle export dir> --walk <walking export dir>
        [--old <current Miko_Field.png, for the Kick/Punch/Death rows>] [--out <dir>] [--preview <png>]

The export dirs are PixelLab character zips unpacked (metadata.json, <name>/animations/<Anim>/<direction>/frame_*.png).
Clean-up, in order:
  1. every pixel snapped to Miko's palette (her v2 rotation colours plus three golds), alpha made binary;
  2. one-frame flicker removed: a pixel that differs from the frames before and after it, where those two agree
     on its whole 3 x 3 neighbourhood, takes their colour (PixelLab re-rolls a few pixels in still areas);
  3. the gold put back where PixelLab dropped it: the hair bell at the top of the paper strip (all frames that
     lack gold), the medallion on the sash in the front views;
  4. each direction placed in 96 x 96 cells with its lowest foot on the sheet's baseline (6 px above the cell's
     bottom), so the feet stay put between frames and directions.
Rows: 0 IdleDown, 1 WalkDown, 2 WalkRight, 3 WalkUp, 4 WalkLeft, 5 Kick, 6 Punch, 7 Death (5-7 copied from the old
sheet until they are redrawn), 8 IdleRight, 9 IdleUp, 10 IdleLeft. Sprites are cut 96 x 90 with the pivot at the
feet, so the Player's scale is [3, 2.8125] (32 pixels per metre).
"""
from __future__ import annotations

import argparse
import glob
from pathlib import Path

from PIL import Image

CELL, BASE, CUT = 96, 6, 90  # cell size, feet above the cell bottom, sprite height (from the feet up)
HI, MID, SH = (242, 207, 107, 255), (200, 149, 47, 255), (140, 90, 34, 255)
PALETTE = [(0x25, 0x15, 0x14), (0x26, 0x1a, 0x19), (0x28, 0x1b, 0x19), (0x2c, 0x1e, 0x1a), (0x31, 0x20, 0x1d),
           (0x3a, 0x1a, 0x1e), (0x33, 0x22, 0x20), (0x35, 0x25, 0x22), (0x3c, 0x28, 0x24), (0x43, 0x31, 0x2b),
           (0x4d, 0x38, 0x31), (0x56, 0x3f, 0x35), (0x5d, 0x43, 0x39), (0x73, 0x25, 0x22), (0x7a, 0x4e, 0x43),
           (0xa9, 0x7c, 0x65), (0xac, 0x48, 0x3e), (0xc6, 0x9e, 0x83), (0xdb, 0xba, 0xa3), (0xe2, 0xaf, 0x8c),
           (0xe9, 0xd3, 0xc5), (0xf1, 0xde, 0xd3), HI[:3], MID[:3], SH[:3]]
DIRECTIONS = ["south", "east", "north", "west"]
ROW = {("idle", "south"): 0, ("walk", "south"): 1, ("walk", "east"): 2, ("walk", "north"): 3, ("walk", "west"): 4,
       ("idle", "east"): 8, ("idle", "north"): 9, ("idle", "west"): 10}
CLIPS = [("Idle", 0, 4, 5), ("IdleDown", 0, 4, 5), ("WalkDown", 1, 6, 9), ("WalkRight", 2, 6, 9), ("WalkUp", 3, 6, 9),
         ("WalkLeft", 4, 6, 9), ("IdleRight", 8, 4, 5), ("IdleUp", 9, 4, 5), ("IdleLeft", 10, 4, 5)]
OLD_ROWS = {5: (7, 83), 6: (6, 82), 7: (7, 74)}  # old row: frames, feet row in the old cell (from its .hymeta cuts)


def snap(img: Image.Image, palette: list = None) -> Image.Image:
    palette = palette or PALETTE
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    src, dst = img.load(), out.load()
    cache = {}
    for y in range(img.height):
        for x in range(img.width):
            p = src[x, y]
            if p[3] < 128:
                continue
            key = p[:3]
            if key not in cache:
                cache[key] = min(palette, key=lambda c: sum((c[i] - key[i]) ** 2 for i in range(3)))
            dst[x, y] = cache[key] + (255,)
    return out


def deflicker(frames: list) -> int:
    n, fixed = len(frames), 0
    if n < 3:
        return 0
    w, h = frames[0].size
    for _ in range(2):
        px = [f.load() for f in frames]
        for i in range(n):
            a, b, c = px[(i - 1) % n], px[i], px[(i + 1) % n]
            changes = []
            for y in range(1, h - 1):
                for x in range(1, w - 1):
                    if b[x, y] != a[x, y] and a[x, y] == c[x, y]:
                        if all(a[x + dx, y + dy] == c[x + dx, y + dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                            changes.append((x, y, a[x, y]))
            for x, y, p in changes:
                b[x, y] = p
            fixed += len(changes)
    return fixed


def has_gold(img: Image.Image) -> bool:
    return any(p[3] and p[:3] in (HI[:3], MID[:3]) for p in img.get_flattened_data())


def bbox(img: Image.Image):
    return img.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()


def add_hair_bell(img: Image.Image):
    """2 x 2 gold at the top of the white paper strip beside the head (the topmost white pixels in the head zone)."""
    left, top, right, bottom = bbox(img)
    px = img.load()
    zone = top + int((bottom - top) * 0.36)
    for y in range(top, zone):
        for x in range(left, right):
            p = px[x, y]
            if p[3] and sum(p[:3]) > 600:
                for (dx, dy, c) in ((0, 0, HI), (1, 0, MID), (0, 1, MID), (1, 1, SH)):
                    if px[x + dx, y + dy][3]:
                        px[x + dx, y + dy] = c
                return True
    return False


def add_medallion(img: Image.Image):
    """3 x 2 gold on the sash, at the body's centre column: the first dark row under the white top."""
    left, top, right, bottom = bbox(img)
    px = img.load()
    cx = (left + right) // 2
    seen_white = False
    for y in range(top + (bottom - top) // 3, top + (bottom - top) * 2 // 3):
        p = px[cx, y]
        if p[3] and sum(p[:3]) > 600:
            seen_white = True
        elif seen_white and p[3] and sum(p[:3]) < 160:
            for dx, dy, c in ((-1, 0, MID), (0, 0, HI), (1, 0, MID), (-1, 1, MID), (0, 1, MID), (1, 1, SH)):
                px[cx + dx, y + dy] = c
            return True
    return False


def load_frames(export: Path, kind: str, direction: str) -> list:
    files = sorted(glob.glob(str(export / "*" / "animations" / "*" / direction / "frame_*.png")))
    return [Image.open(f).convert("RGBA") for f in files]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--idle", type=Path, required=True)
    parser.add_argument("--walk", type=Path, required=True)
    parser.add_argument("--old", type=Path, default=None)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--preview", type=Path, default=None)
    args = parser.parse_args()

    rows = 11
    sheet = Image.new("RGBA", (CELL * 8, CELL * rows), (0, 0, 0, 0))
    report = []
    for kind, export in (("idle", args.idle), ("walk", args.walk)):
        for direction in DIRECTIONS:
            frames = [snap(f) for f in load_frames(export, kind, direction)]
            fixed = deflicker(frames)
            gold = 0
            for f in frames:
                if has_gold(f) is False:
                    gold += add_hair_bell(f)
                    if direction == "south":
                        add_medallion(f)
            # Place the direction's frames with its lowest foot on the baseline, centred on the canvas centre.
            lowest = max(bbox(f)[3] for f in frames)
            row = ROW[(kind, direction)]
            for i, f in enumerate(frames[:8]):
                cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
                ox = (CELL - f.width) // 2
                oy = (CELL - BASE) - lowest
                cell.paste(f, (ox, oy), f)
                sheet.alpha_composite(cell, (i * CELL, row * CELL))
            report.append(f"{kind} {direction}: {len(frames)} frames, {fixed} flicker pixels fixed, gold added to {gold}")
    if args.old:
        old = Image.open(args.old).convert("RGBA")
        for row, (count, feet) in OLD_ROWS.items():
            for i in range(count):
                cell = old.crop((i * CELL, row * CELL, i * CELL + CELL, row * CELL + CELL))
                shifted = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
                shifted.paste(cell, (0, (CELL - BASE) - feet))
                sheet.alpha_composite(shifted, (i * CELL, row * CELL))
    args.out.mkdir(parents=True, exist_ok=True)
    sheet.save(args.out / "Miko_Field.png")
    for line in report:
        print(line)
    if args.preview:
        bg = Image.new("RGBA", sheet.size, (70, 78, 62, 255))
        bg.alpha_composite(sheet)
        bg.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).convert("RGB").save(args.preview)


if __name__ == "__main__":
    main()
