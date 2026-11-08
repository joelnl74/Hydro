#!/usr/bin/env python3
"""Builds the shrine cat's sheet (Characters/NPC/Cat/Cat.png + .hymeta + .hyanim) from its PixelLab character zip.

    python scripts/game/characters/build_cat.py <unpacked zip> <out dir> [--handles TEX ANIM]

The zip's states: Idle, walk, sitting_down (lying down on its belly) and sleeping (curled up). The same clean-up as
build_npc_idle.py (every pixel snapped to the colours of the cat's rotations, binary alpha, one-frame specks removed,
each animation's lowest point on the baseline), then every 96 px cell halved to 48 (a 2 x 2 block becomes its darkest
pixel where it holds outline, else its mean, snapped back to the palette), so the cat is cat-sized, about 0.55 m
long, at the cast's 38.4 pixels per metre (scale 1.25 x 1.171875).
Rows (48 px cells, cuts 48 x 45 ending at the feet, pivot [0.5, 0]): IdleDown/Right/Up/Left (4 frames), WalkDown/
Right/Up/Left (6), LieDownRight/Left (10), SleepRight/Left/Down (9). Clips: Idle<Facing>, Walk<Facing> (0.1 s a frame),
LieDown<Facing> (once), GetUp<Facing> (the lying-down frames backwards, once), Sleep<Facing> (a slow loop). For
NpcWanderScript's WorkStartClip / WorkClip / WorkEndClip ("LieDown", "Sleep", "GetUp").
"""
from __future__ import annotations

import argparse
import random
import sys
from collections import Counter
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_miko_field import BASE, CELL, CUT  # noqa: E402
from build_npc_idle import FACINGS, ORDER, Export, build, place  # noqa: E402

HALF = CELL // 2


def halve(sheet: Image.Image) -> Image.Image:
    a = np.asarray(sheet.convert("RGBA")).astype(float)
    counts = Counter(tuple(int(v) for v in p[:3]) for p in a.reshape(-1, 4) if p[3] > 128)
    palette = np.array([k for k, _ in counts.most_common(64)], float)
    h, w = a.shape[0] // 2, a.shape[1] // 2
    blocks = a[:h * 2, :w * 2].reshape(h, 2, w, 2, 4).transpose(0, 2, 1, 3, 4).reshape(h, w, 4, 4)
    out = np.zeros((h, w, 4), np.uint8)
    for y in range(h):
        for x in range(w):
            px = blocks[y, x]
            solid = px[px[:, 3] > 128]
            if len(solid) < 2:
                continue
            light = solid[:, :3].sum(1)
            colour = solid[light.argmin(), :3] if (light < 170).sum() >= 2 else solid[:, :3].mean(0)
            out[y, x, :3] = palette[((palette - colour) ** 2).sum(1).argmin()]
            out[y, x, 3] = 255
    return Image.fromarray(out, "RGBA")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("zip_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--handles", type=int, nargs=2, default=None)
    args = parser.parse_args()
    root = args.zip_dir
    exports = {k: Export([str(root / k)]) for k in ("Idle", "walk", "sitting_down", "sleeping")}
    colours = set()
    for export in exports.values():
        for r in export.rotations:
            colours |= {p[:3] for p in r.get_flattened_data() if p[3] >= 128}
    palette = sorted(colours)
    idle = {d: f[::2][:4] for d, f in build(exports["Idle"], {}, {}, ORDER, palette, False).items()}
    walk = build(exports["walk"], {}, {}, ORDER, palette, False)
    side = ["east", "west"]
    lie = build(exports["sitting_down"], {}, {}, side, palette, False)
    sleep = build(exports["sleeping"], {}, {}, side + ["south"], palette, False)
    blocks = [("Idle", idle, ORDER), ("Walk", walk, ORDER), ("LieDown", lie, side), ("Sleep", sleep, side + ["south"])]
    rows, r = [], 0
    for kind, clips, dirs in blocks:
        count = min(len(clips[d]) for d in dirs)
        for d in dirs:
            rows.append((kind, d, r, clips[d][:count]))
            r += 1
    sheet = Image.new("RGBA", (CELL * max(len(f) for _, _, _, f in rows), CELL * len(rows)), (0, 0, 0, 0))
    for kind, d, r, frames in rows:
        place(sheet, frames, r, f"{kind} {d}")
    small = halve(sheet)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    small.save(args.out_dir / "Cat.png")

    texture, animations = args.handles if args.handles else (random.getrandbits(63), random.getrandbits(63))
    cut, top = CUT // 2, (CELL - BASE - CUT) // 2
    sprites = "\n".join(f"    - {{Name: {kind}{FACINGS[d]}_{i}, Rect: [{i * HALF}, {r * HALF + top}, {HALF}, {cut}], Pivot: [0.5, 0]}}"
                        for kind, d, r, frames in rows for i in range(len(frames)))
    hymeta = (f"Handle: {texture}\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
              f"  SpriteMode: Multiple\n  PixelsPerUnit: 32\n  Slicing:\n    Mode: Manual\n    CellSize: [{HALF}, {HALF}]\n"
              f"    Offset: [0, 0]\n    Spacing: [0, 0]\n    Columns: 0\n    Rows: 0\n    RowNames: []\n"
              f"    DefaultPivot: [0.5, 0]\n  Sprites:\n{sprites}\n")
    (args.out_dir / "Cat.png.hymeta").write_bytes(hymeta.replace("\n", "\r\n").encode())

    def clip(name, sprite, indices, durations, loop="Loop"):
        frames = "".join(f"        - Sprite: {sprite}_{i}\n          Duration: {durations[k % len(durations)]}\n"
                         for k, i in enumerate(indices))
        return f"    - Name: {name}\n      Loop: {loop}\n      Frames:\n{frames}"
    text = (f"# Cat: idle, walk, lying down, getting up and sleeping (scripts/game/characters/build_cat.py, Nov 3).\n"
            f"Handle: {animations}\nSpriteAnimationSet:\n  TextureHandle: {texture}\n  Texture: Cat.png\n"
            f"  DefaultClip: IdleDown\n  Clips:\n" + clip("Idle", "IdleDown", range(4), [0.5, 0.4, 0.5, 0.4]))
    for kind, d, r, frames in rows:
        n, f = len(frames), FACINGS[d]
        if kind == "Idle":
            text += clip(f"Idle{f}", f"Idle{f}", range(n), [0.5, 0.4, 0.5, 0.4])
        elif kind == "Walk":
            text += clip(f"Walk{f}", f"Walk{f}", range(n), [0.12])
        elif kind == "LieDown":
            text += clip(f"LieDown{f}", f"LieDown{f}", range(n), [0.11], "Once")
            text += clip(f"GetUp{f}", f"LieDown{f}", range(n - 1, -1, -1), [0.09], "Once")
        else:
            text += clip(f"Sleep{f}", f"Sleep{f}", range(n), [0.32])
    (args.out_dir / "Cat.hyanim").write_bytes(text.replace("\n", "\r\n").encode())


if __name__ == "__main__":
    main()
