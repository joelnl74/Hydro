#!/usr/bin/env python3
"""Draws the shrine maiden's strike effects for the battle (Assets/Battle/FX_Strikes.png + .hymeta + .hyanim).

Pixel art made in code: hard edges, a few colours per effect, no anti-aliasing, 96 x 96 px cells like
Characters/Miko/Miko.png so both have the same pixel size in the battle. One row per clip:

    Punch  6 frames  white-gold starburst and shock ring (her punch lands on its last frame)
    Kick   7 frames  a crescent swoosh from the right (her leg) ending in a burst (lands on frame 4)
    Flame  7 frames  Flame Kick: the swoosh in fire, flame tongues and embers
    Holy   8 frames  Holy Palm: a pillar of light with a four-pointed star and motes

Each effect hits at the cell's centre and comes from the right (the maiden stands right of her target).
Frame timing is in the .hyanim and matches the impact times in BattleScript (PunchImpactTime, KickImpactTime).

    python scripts/battle/fx_strikes.py [--preview out.png]

Needs Pillow. Handles are hashed from the file path, so running it again keeps them.
"""
from __future__ import annotations

import argparse
import hashlib
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "Oxygen" / "project" / "Assets"
OUT_DIR = ASSETS / "Battle"
TEXTURE = "FX_Strikes.png"
CELL = 96
COLUMNS = 8

# (row name, frame count, frame rate): keep in step with BattleScript's clip names.
ROWS = [("Punch", 6, 14), ("Kick", 7, 14), ("Flame", 7, 14), ("Holy", 8, 12)]

WHITE = (255, 255, 255, 255)
CREAM = (255, 246, 214, 255)
PALE_GOLD = (255, 226, 140, 255)
GOLD = (255, 196, 72, 255)
ORANGE = (255, 138, 44, 255)
RED = (214, 64, 30, 255)
EMBER = (150, 40, 26, 255)
SMOKE = (88, 64, 58, 190)
AIR = (224, 240, 236, 230)
AIR_EDGE = (150, 196, 186, 200)


def handle_for(relative: str) -> int:
    digest = hashlib.sha1(("hydro:" + relative).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "little") & 0x7FFFFFFFFFFFFFFF


def star(draw: ImageDraw.ImageDraw, cx: float, cy: float, rays: int, inner: float, outer: float, colour, rotation: float = 0.0, width_deg: float = 9.0):
    """Sharp rays as thin triangles from a small core."""
    for i in range(rays):
        a = rotation + 2.0 * math.pi * i / rays
        length = outer * (1.0 if i % 2 == 0 else 0.62)
        w = math.radians(width_deg)
        tip = (cx + math.cos(a) * length, cy + math.sin(a) * length)
        left = (cx + math.cos(a - w) * inner, cy + math.sin(a - w) * inner)
        right = (cx + math.cos(a + w) * inner, cy + math.sin(a + w) * inner)
        draw.polygon([left, tip, right], fill=colour)


def ring(draw: ImageDraw.ImageDraw, cx: float, cy: float, radius: float, thickness: int, colour, squash: float = 0.8, gaps: int = 0, seed: int = 0):
    if radius <= 0:
        return
    box = [cx - radius, cy - radius * squash, cx + radius, cy + radius * squash]
    if gaps == 0:
        draw.ellipse(box, outline=colour, width=thickness)
        return
    rnd = random.Random(seed)
    step = 360 / (gaps * 2)
    start = rnd.uniform(0, step)
    for g in range(gaps):
        a0 = start + g * 2 * step
        draw.arc(box, a0, a0 + step * rnd.uniform(0.9, 1.5), fill=colour, width=thickness)


def sparks(draw: ImageDraw.ImageDraw, cx: float, cy: float, count: int, radius: float, colours, seed: int, size: int = 2, streak: float = 0.0):
    rnd = random.Random(seed)
    for i in range(count):
        a = rnd.uniform(0, 2 * math.pi)
        r = radius * rnd.uniform(0.7, 1.15)
        x, y = cx + math.cos(a) * r, cy + math.sin(a) * r * 0.85
        colour = colours[i % len(colours)]
        if streak > 0:
            x2, y2 = x - math.cos(a) * streak, y - math.sin(a) * streak * 0.85
            draw.line([(x2, y2), (x, y)], fill=colour, width=size)
        else:
            draw.rectangle([x, y, x + size - 1, y + size - 1], fill=colour)


def crescent(draw: ImageDraw.ImageDraw, cx: float, cy: float, radius: float, start_deg: float, end_deg: float, thickness: float, colour, steps: int = 28):
    """A swoosh: the band between two arcs, thickest in the middle, pointed at both ends."""
    outer, inner = [], []
    for s in range(steps + 1):
        t = s / steps
        a = math.radians(start_deg + (end_deg - start_deg) * t)
        w = thickness * math.sin(math.pi * t) ** 0.8
        outer.append((cx + math.cos(a) * (radius + w * 0.5), cy + math.sin(a) * (radius + w * 0.5) * 0.8))
        inner.append((cx + math.cos(a) * (radius - w * 0.5), cy + math.sin(a) * (radius - w * 0.5) * 0.8))
    draw.polygon(outer + inner[::-1], fill=colour)


def flame(draw: ImageDraw.ImageDraw, x: float, y: float, height: float, width: float, lean: float, colours):
    """A flame tongue: a teardrop pointing up (and leaning), drawn in nested colours."""
    for i, colour in enumerate(colours):
        k = 1.0 - i * 0.28
        h, w = height * k, width * k
        tip = (x + lean * h, y - h)
        draw.polygon([(x - w, y), (x - w * 0.6, y - h * 0.45), tip, (x + w * 0.6, y - h * 0.45), (x + w, y), (x, y + w * 0.6)], fill=colour)


def punch_frame(i: int) -> Image.Image:
    im = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = CELL / 2
    if i == 0:
        star(d, c, c, 8, 2, 12, PALE_GOLD, 0.2)
        d.ellipse([c - 4, c - 4, c + 4, c + 4], fill=WHITE)
    elif i == 1:
        star(d, c, c, 10, 4, 34, GOLD, 0.15)
        star(d, c, c, 10, 3, 24, CREAM, 0.15)
        d.ellipse([c - 8, c - 7, c + 8, c + 7], fill=WHITE)
        ring(d, c, c, 16, 2, PALE_GOLD)
    elif i == 2:
        star(d, c, c, 10, 6, 44, ORANGE, 0.15)
        star(d, c, c, 10, 5, 34, GOLD, 0.15)
        star(d, c, c, 10, 3, 20, CREAM, 0.15)
        d.ellipse([c - 7, c - 6, c + 7, c + 6], fill=WHITE)
        ring(d, c, c, 26, 3, CREAM)
        sparks(d, c, c, 10, 36, [WHITE, PALE_GOLD], 3)
    elif i == 3:
        star(d, c, c, 6, 4, 22, GOLD, 0.35, 7)
        star(d, c, c, 6, 2, 13, CREAM, 0.35, 6)
        ring(d, c, c, 34, 2, PALE_GOLD, gaps=6, seed=4)
        sparks(d, c, c, 14, 40, [WHITE, PALE_GOLD, GOLD], 4, 2, 5)
    elif i == 4:
        ring(d, c, c, 40, 2, GOLD, gaps=5, seed=5)
        sparks(d, c, c, 12, 43, [PALE_GOLD, GOLD, ORANGE], 5, 2, 3)
        d.ellipse([c - 2, c - 2, c + 2, c + 2], fill=CREAM)
    else:
        ring(d, c, c, 44, 1, ORANGE, gaps=4, seed=6)
        sparks(d, c, c, 8, 45, [GOLD, ORANGE], 6, 2)
    return im


def kick_frame(i: int, fire: bool) -> Image.Image:
    im = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = CELL / 2
    body, edge, core = (ORANGE, RED, PALE_GOLD) if fire else (AIR, AIR_EDGE, WHITE)
    # The leg sweeps over the top from the right (angle 0 = right, 90 = down in image space).
    # (start, end) of the trail in degrees: it grows over the top, then its tail catches up with the burst.
    sweep = [(10, -25), (10, -80), (5, -135), (-20, -178), (-110, -195), None, None][i]
    if sweep is not None:
        thick = [7, 12, 15, 15, 9][i]
        crescent(d, c + 2, c + 8, 34, sweep[0], sweep[1], thick, edge)
        crescent(d, c + 2, c + 8, 34, sweep[0] - 5, sweep[1] + 5, thick * 0.62, body)
        crescent(d, c + 2, c + 8, 34, sweep[0] - 12, sweep[1] + 12, max(2, thick * 0.25), core)
    if i == 3:
        star(d, c - 18, c + 4, 8, 3, 16, core, 0.4)
    if i == 4:
        star(d, c - 20, c + 6, 10, 5, 38, edge if fire else PALE_GOLD, 0.2)
        star(d, c - 20, c + 6, 10, 4, 28, body if fire else CREAM, 0.2)
        d.ellipse([c - 27, c, c - 13, c + 12], fill=WHITE)
        sparks(d, c - 20, c + 6, 10, 32, [WHITE, core], 11)
    if i == 5:
        star(d, c - 20, c + 6, 10, 3, 24, body, 0.45, 5)
        ring(d, c - 20, c + 6, 30, 2, core, gaps=5, seed=12)
        sparks(d, c - 20, c + 6, 14, 36, [core, body], 13, 2, 5)
    if i == 6:
        ring(d, c - 20, c + 6, 36, 1, edge, gaps=4, seed=14)
        sparks(d, c - 20, c + 6, 10, 40, [body, edge], 15, 2)
    if fire and i >= 3:
        rnd = random.Random(20 + i)
        count = {3: 3, 4: 6, 5: 7, 6: 5}[i]
        for f in range(count):
            x = c - 20 + rnd.uniform(-22, 22)
            y = c + 14 + rnd.uniform(-6, 8)
            h = rnd.uniform(14, 30) * (1.0 if i < 6 else 0.6)
            flame(d, x, y, h, rnd.uniform(4, 7), rnd.uniform(-0.25, 0.25), [RED, ORANGE, GOLD, PALE_GOLD])
        if i >= 5:
            sparks(d, c - 20, c - 8, 10, 30 + 6 * (i - 5), [GOLD, ORANGE, EMBER], 30 + i, 2)
        if i == 6:
            for s in range(4):
                x = c - 30 + s * 14
                d.ellipse([x, c - 22 - s * 3, x + 9, c - 15 - s * 3], fill=SMOKE)
    return im


def holy_frame(i: int) -> Image.Image:
    im = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = CELL / 2
    widths = [2, 6, 12, 16, 14, 10, 6, 2]
    w = widths[i]
    if i <= 6:
        # The pillar from above, brightest in the middle.
        d.rectangle([c - w, 0, c + w, c + 10], fill=(255, 226, 140, 150))
        d.rectangle([c - w * 0.55, 0, c + w * 0.55, c + 10], fill=(255, 246, 214, 210))
        d.rectangle([c - max(1, w * 0.2), 0, c + max(1, w * 0.2), c + 10], fill=WHITE)
    size = [6, 16, 30, 40, 34, 24, 14, 6][i]
    # A four-pointed star (long vertical, short horizontal) and a thin diamond around it.
    d.polygon([(c, c - size), (c + size * 0.18, c - size * 0.18), (c + size * 0.7, c), (c + size * 0.18, c + size * 0.18),
               (c, c + size * 0.8), (c - size * 0.18, c + size * 0.18), (c - size * 0.7, c), (c - size * 0.18, c - size * 0.18)], fill=PALE_GOLD)
    d.polygon([(c, c - size * 0.6), (c + size * 0.1, c - size * 0.1), (c + size * 0.4, c), (c + size * 0.1, c + size * 0.1),
               (c, c + size * 0.45), (c - size * 0.1, c + size * 0.1), (c - size * 0.4, c), (c - size * 0.1, c - size * 0.1)], fill=WHITE)
    if 2 <= i <= 6:
        ring(d, c, c + 8, 14 + 6 * i, 2 if i < 5 else 1, PALE_GOLD, squash=0.35, gaps=0 if i < 4 else 4, seed=40 + i)
    if i >= 3:
        rnd = random.Random(50 + i)
        for m in range(10):
            x = c + rnd.uniform(-34, 34)
            y = c + 14 - (i - 3) * 9 - rnd.uniform(0, 22)
            colour = WHITE if m % 3 == 0 else PALE_GOLD
            d.rectangle([x, y, x + 1, y + 2], fill=colour)
    return im


def build_sheet() -> Image.Image:
    sheet = Image.new("RGBA", (CELL * COLUMNS, CELL * len(ROWS)), (0, 0, 0, 0))
    for row, (name, count, _) in enumerate(ROWS):
        for i in range(count):
            if name == "Punch":
                frame = punch_frame(i)
            elif name == "Kick":
                frame = kick_frame(i, fire=False)
            elif name == "Flame":
                frame = kick_frame(i, fire=True)
            else:
                frame = holy_frame(i)
            sheet.alpha_composite(frame, (i * CELL, row * CELL))
    return sheet


def write_crlf(path: Path, text: str) -> None:
    path.write_bytes(text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preview", type=Path, help="also write an enlarged contact sheet on a dark background")
    args = parser.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    sheet = build_sheet()
    sheet.save(OUT_DIR / TEXTURE)

    texture_handle = handle_for("Battle/" + TEXTURE)
    anim_handle = handle_for("Battle/FX_Strikes.hyanim")
    row_names = ", ".join(name for name, _, _ in ROWS)
    write_crlf(OUT_DIR / (TEXTURE + ".hymeta"), f"""Handle: {texture_handle}
Type: Texture2D
TextureImport:
  Format: SRGB
  Filter: Nearest
  Wrap: Clamp
  SpriteMode: Multiple
  PixelsPerUnit: 32
  Slicing:
    Mode: Grid
    CellSize: [{CELL}, {CELL}]
    Offset: [0, 0]
    Spacing: [0, 0]
    Columns: {COLUMNS}
    Rows: {len(ROWS)}
    RowNames: [{row_names}]
    DefaultPivot: [0.5, 0.5]
""")

    clips = []
    for name, count, rate in ROWS:
        frames = ", ".join(f"{name}_{i}" for i in range(count))
        clips.append(f"""    - Name: {name}
      FrameRate: {rate}
      Loop: Once
      Frames: [{frames}]""")
    write_crlf(OUT_DIR / "FX_Strikes.hyanim", f"""# The shrine maiden's strike effects (generated by scripts/battle/fx_strikes.py).
Handle: {anim_handle}
SpriteAnimationSet:
  TextureHandle: {texture_handle}
  Texture: {TEXTURE}
  DefaultClip: Punch
  Clips:
""" + "\n".join(clips) + "\n")

    if args.preview:
        scale = 2
        preview = Image.new("RGBA", (sheet.width * scale, sheet.height * scale), (34, 40, 36, 255))
        preview.alpha_composite(sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST))
        preview.save(args.preview)

    print(f"FX_Strikes: texture {texture_handle}, animations {anim_handle}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
