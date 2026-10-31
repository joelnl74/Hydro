#!/usr/bin/env python3
"""Draws the Slime and the Spirit Wisp: a field sheet and a battle sheet each, to the HD-2D sprite rules.

    python scripts/enemies/make_enemy_art.py [--out <dir>] [--preview <png>]

Field sheets (Characters/Enemy/<Name>/<Name>.png): 48 x 48 cells at 32 pixels per metre, like the props and
Miko. Battle sheets (<Name>_Battle.png): 96 x 96 cells at about 53 pixels per metre, the battle Miko's density,
so the cell has room for the bigger body and its motion. The battle sprites face right (towards the party).

Both are drawn from the same shapes and lighting, at each sheet's own resolution (never upscaled):
  - one key light from the top-left, 5-7 hue-shifted tones per material (cool shadows, warm highlights);
  - a selective outline in a dark tone of the material (lighter on the lit edge), no black, no pure white;
  - binary alpha and no baked ground shadow (the engine draws the contact shadow);
  - the feet (the slime's base, the wisp's float line) on the row the .hymeta pivot puts on the ground.

Clips (frame names unchanged, so the scenes and BattleScript keep working):
  Slime: Bounce, 4 frames (rest, squash, stretch in the air, settle). Wisp: Float, 6 frames (flame flicker).
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ENEMIES = ROOT / "Oxygen" / "project" / "Assets" / "Characters" / "Enemy"


def hexc(value: str) -> tuple:
    value = value.lstrip("#")
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16), 255)


# Hue-shifted ramps, darkest first. The darkest is the outline in shadow, the second the outline on the lit edge.
SLIME = [hexc(c) for c in ("1c3a35", "24544a", "2c6e55", "3a8c5c", "56a862", "82c66c", "b4de86", "e2f3b4")]
SLIME_EYE, SLIME_EYE_LIGHT = hexc("142a2a"), hexc("e8f6dc")
LEAF = [hexc(c) for c in ("27401c", "3d6424", "5d8a2c", "8fb43c", "c4d867")]
STEM = hexc("4b5a26")
WISP = [hexc(c) for c in ("172c4c", "1f4f80", "2a7fb0", "43b2d4", "7fdbe8", "c2f4f3", "f0fff6")]
WISP_EYE = hexc("1a3a5e")
MOTE = hexc("aaf0f2")

LIGHT = (-0.55, 0.62, 0.56)  # x right, y up, z towards the viewer


def normalize(v):
    length = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / length for c in v)


L = normalize(LIGHT)
H = normalize((L[0], L[1], L[2] + 1.0))  # half vector for the specular glint


class Canvas:
    """A cell being drawn: y grows upwards from the ground row; px() turns metres into pixels."""

    def __init__(self, size: int, ppm: float, ground_row: int):
        self.size, self.ppm, self.ground = size, ppm, ground_row
        self.img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        self.mask = [[0] * size for _ in range(size)]  # 1 where a body pixel is, for the outline pass

    def put(self, x: int, y_up: int, color, body: bool = True):
        y = self.size - 1 - self.ground - y_up
        if 0 <= x < self.size and 0 <= y < self.size:
            self.img.putpixel((x, y), color)
            if body:
                self.mask[y][x] = 1

    def get_alpha(self, x: int, y: int) -> int:
        if 0 <= x < self.size and 0 <= y < self.size:
            return self.img.getpixel((x, y))[3]
        return 0


def quantize(value: float, ramp: list, low: int = 2) -> tuple:
    """value 0..1 to a ramp tone, never the two outline tones."""
    value = max(0.0, min(1.0, value))
    span = len(ramp) - 1 - low
    return ramp[low + min(span, int(value * (span + 1)))]


def outline(canvas: Canvas, ramp: list, lit_test):
    """A one pixel selective outline around the body: the darkest tone, the next one where lit_test(x, y) says
    the edge faces the light."""
    size = canvas.size
    edge = []
    for y in range(size):
        for x in range(size):
            if canvas.mask[y][x] == 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < size and 0 <= ny < size) or canvas.get_alpha(nx, ny) == 0:
                    edge.append((x, y))
                    break
    for x, y in edge:
        canvas.img.putpixel((x, y), ramp[1] if lit_test(x, y) else ramp[0])


# Hand-drawn details, as character stamps (top row first): l/L/h lit leaf, r/R dark leaf, s stem, e eye, w eye light.
STAMP_COLORS = {"L": LEAF[3], "l": LEAF[2], "h": LEAF[4], "R": LEAF[2], "r": LEAF[1], "s": STEM}
SPROUT_FIELD = [
    "hL.R.",
    "Ll.rR",
    "..s..",
    "..s..",
]
SPROUT_BATTLE = [
    ".hLL...RRR.",
    "LLhLl.RRRRr",
    ".LlLlsRRRr.",
    "...lls.rr..",
    ".....s.....",
    ".....s.....",
]


def stamp_sprite(canvas: Canvas, stamp: list, left: int, top_up: int, sway: int = 0, colors: dict = None):
    """Draws a stamp with its top row at height top_up; the upper half shifts by sway (leaves in the wind)."""
    colors = colors or STAMP_COLORS
    rows = len(stamp)
    for i, row in enumerate(stamp):
        shift = sway if i < rows // 2 else 0
        for j, ch in enumerate(row):
            if ch in colors:
                canvas.put(left + j + shift, top_up - i, colors[ch], body=False)


def outline_color_set(canvas: Canvas, tones: list, dark, lit):
    """Outlines the pixels whose colour is in tones (empty neighbours get dark, or lit above/left)."""
    rgb = {t[:3] for t in tones}
    for y in range(canvas.size):
        for x in range(canvas.size):
            p = canvas.img.getpixel((x, y))
            if p[3] and p[:3] in rgb:
                for dx, dy in ((1, 0), (-1, 0), (0, -1), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < canvas.size and 0 <= ny < canvas.size and canvas.img.getpixel((nx, ny))[3] == 0:
                        canvas.img.putpixel((nx, ny), dark if dy >= 0 or dx > 0 else lit)


# ----------------------------------------------------------------------------------------------- the slime

SLIME_FRAMES = [  # width, height (metres), lift (metres)
    (1.06, 0.78, 0.00),
    (1.20, 0.63, 0.00),
    (0.92, 0.94, 0.07),
    (1.10, 0.73, 0.00),
]


def draw_slime(canvas: Canvas, frame: int, facing: float, detail: int):
    width, height, lift = SLIME_FRAMES[frame]
    ppm = canvas.ppm
    a, b = width * ppm / 2.0, height * ppm / 2.0
    cx = canvas.size / 2.0 - 0.5
    base = lift * ppm
    cells = {}
    for y_up in range(int(base), int(base + 2 * b) + 2):
        for x in range(canvas.size):
            u = (x - cx) / a
            v = (y_up + 0.5 - base - b) / b
            q = 2.0 if v > 0 else 3.4  # a flatter base where it rests on the ground
            if abs(u) ** 2.15 + abs(v) ** q > 1.0:
                continue
            z = math.sqrt(max(0.0, 1.0 - u * u - (v * v if v > 0 else (abs(v) ** q))))
            n = normalize((u * 0.9, max(v, -0.4) * 0.9, z + 0.15))
            lambert = max(0.0, sum(n[i] * L[i] for i in range(3)))
            value = 0.06 + 0.88 * lambert
            # Translucency: a lighter band just inside the base edge, where light passes through the jelly.
            edge = abs(u) ** 2.15 + abs(v) ** q
            if v < -0.1 and edge > 0.72 and u > -0.75:
                value = max(value, 0.42)
            spec = max(0.0, sum(n[i] * H[i] for i in range(3))) ** 40
            # A second, smaller glint lower on the lit side.
            if abs(u + 0.55) < 0.05 and abs(v - 0.02) < 0.05:
                spec = 1.0
            cells[(x, y_up)] = (value, spec, u, v)
    for (x, y_up), (value, spec, u, v) in cells.items():
        color = SLIME[-1] if spec > 0.55 else quantize(value, SLIME[:-1])
        canvas.put(x, y_up, color)

    # Eyes: dark, with one light pixel; looking the way the sprite faces.
    eye_w, eye_h = (2, 3) if detail == 1 else (3, 5)
    gap = 0.30 * a
    ey = base + b + 0.12 * b
    ex0 = cx + facing * 0.12 * a
    for side in (-1, 1):
        ex = int(round(ex0 + side * gap - eye_w / 2.0 + 0.5))
        for dy in range(eye_h):
            for dx in range(eye_w):
                canvas.put(ex + dx, int(ey) - dy, SLIME_EYE)
        canvas.put(ex + (0 if facing <= 0 else eye_w - 1), int(ey), SLIME_EYE_LIGHT)
    if detail >= 2:  # a small mouth
        my = int(ey) - eye_h - 2
        for dx in range(-1, 2):
            canvas.put(int(round(ex0)) + dx, my, SLIME[1])

    # The leaf sprout on top, a hand-drawn stamp (rows from the top), swaying with the bounce.
    top = int(round(base + 2 * b)) - 1
    stamp = SPROUT_FIELD if detail == 1 else SPROUT_BATTLE
    sway = (0, 1, -1, 0)[frame]
    stamp_sprite(canvas, stamp, int(round(cx)) - len(stamp[0]) // 2 + (1 if detail == 2 else 0), top + len(stamp), sway)

    def lit(x, y):
        y_up = canvas.size - 1 - canvas.ground - y
        u = (x - cx) / a
        v = (y_up - base - b) / b
        return u < 0.15 and v > -0.1
    outline(canvas, SLIME, lit)
    # The sprout gets the leaf outline instead of the jelly's.
    outline_color_set(canvas, [LEAF[1], LEAF[2], LEAF[3], LEAF[4], STEM], LEAF[0], LEAF[1])


# ------------------------------------------------------------------------------------------------- the wisp


TONGUES = [  # offset from the centre (orb radii), length (of the tail), width (orb radii), flicker speed
    (0.0, 1.0, 0.98, 1.0),
    (-0.5, 0.52, 0.48, 2.0),
    (0.48, 0.42, 0.42, 3.0),
]


def draw_wisp(canvas: Canvas, frame: int, facing: float, detail: int, frames: int = 6):
    ppm = canvas.ppm
    phase = frame / frames * 2.0 * math.pi
    r = 0.23 * ppm
    # The orb's bottom above the float line. In battle BattleScript adds its own bob (0.35 m), so the sheet floats lower.
    hover = (0.34 if detail == 1 else 0.10) * ppm
    cx = canvas.size / 2.0 - 0.5 - facing * 0.04 * ppm
    cy = hover + r
    tail_len = 0.66 * ppm
    lean = -facing * 0.12 * ppm  # trails behind when it faces a way

    def heat_at(x, y_up):
        best = None
        # The orb: a glowing sphere, its hot spot up and to the left, cooler at the lower right.
        du, dv = (x - cx) / r, (y_up - cy) / r
        d = math.hypot(du, dv)
        if d <= 1.0:
            z = math.sqrt(max(0.0, 1.0 - d * d))
            lit = max(0.0, (-0.5 * du + 0.6 * dv + 0.62 * z))
            hot = math.hypot(du + 0.28, dv - 0.25)
            best = 0.30 + 0.45 * lit + (0.35 if hot < 0.32 else 0.0)
        # The flame: three licking tongues rising from the orb's upper half, wavy-edged, swaying and flickering.
        for i, (offset, length, width, speed) in enumerate(TONGUES):
            grow = 1.0 + 0.16 * math.sin(phase * speed + i * 2.1)
            t = (y_up - (cy + 0.1 * r)) / (tail_len * length * grow)
            if not 0.0 <= t <= 1.0:
                continue
            base_x = cx + offset * r
            sway = math.sin(phase * speed + t * 3.0 + i) * 0.07 * ppm * t + lean * t * t + offset * r * 0.6 * t
            centre = base_x + sway
            wave = 1.0 + 0.16 * math.sin(t * 13.0 + phase * 2.0 + i * 1.7)
            w = r * width * (1.0 - t) ** 1.15 * wave
            dist = abs(x - centre)
            if dist <= w:
                inner = 1.0 - dist / max(w, 0.01)
                value = 0.20 + 0.52 * inner * (1.0 - t) ** 0.7 + (0.2 if i == 0 and inner > 0.5 and t < 0.5 else 0.0)
                best = value if best is None else max(best, value)
        return best

    top = int(cy + r + tail_len) + 2
    for y_up in range(0, top):
        for x in range(canvas.size):
            value = heat_at(x + 0.5, y_up + 0.5)
            if value is not None:
                canvas.put(x, y_up, quantize(value, WISP))

    def lit(x, y):
        y_up = canvas.size - 1 - canvas.ground - y
        return (x - cx) < 0.2 * r and y_up > cy - 0.3 * r
    outline(canvas, WISP, lit)

    # Eyes low on the orb, looking the way it faces; drawn after the outline so they stay crisp.
    eye_w, eye_h = (1, 2) if detail == 1 else (2, 3)
    ey = int(round(cy - 0.05 * r))
    for side in (-1, 1):
        ex = int(round(cx + facing * 0.12 * r + side * 0.32 * r - (eye_w - 1) / 2.0))
        for dy in range(eye_h):
            for dx in range(eye_w):
                canvas.put(ex + dx, ey - dy, WISP_EYE, body=False)

    # Motes drifting up around it (not outlined).
    spots = [(0.95, 0.2), (-1.15, 0.7), (1.25, 1.5)]
    for i, (mx, my) in enumerate(spots[: (2 if detail == 1 else 3)]):
        rise = ((frame + i * 2) % frames) / frames
        x = int(round(cx + mx * r + math.sin(phase + i) * 1.5))
        y_up = int(round(cy + my * r + rise * 0.3 * ppm))
        canvas.put(x, y_up, MOTE, body=False)
        if detail >= 2 and (frame + i) % 3 == 0:
            canvas.put(x, y_up + 1, MOTE, body=False)


# ------------------------------------------------------------------------------------------------- sheets

SHEETS = {
    # name: (draw, frames, field ground row, battle ground row)
    "Slime": (draw_slime, 4),
    "Wisp": (draw_wisp, 6),
}
FIELD = dict(size=48, ppm=32.0, detail=1, facing=0.0)
BATTLE = dict(size=96, ppm=96.0 / 1.8, detail=2, facing=1.0)
PIVOT = {"Slime": 0.125, "Wisp": 0.12}


def build(name: str, spec: dict) -> Image.Image:
    draw, frames = SHEETS[name]
    size = spec["size"]
    ground = int(round(PIVOT[name] * size))
    sheet = Image.new("RGBA", (size * frames, size), (0, 0, 0, 0))
    for f in range(frames):
        canvas = Canvas(size, spec["ppm"], ground)
        draw(canvas, f, spec["facing"], spec["detail"])
        sheet.alpha_composite(canvas.img, (f * size, 0))
    return sheet


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=None, help="write here instead of Characters/Enemy/<Name>/")
    parser.add_argument("--preview", type=Path, default=None, help="also write a 4x preview of all four sheets")
    args = parser.parse_args()
    sheets = []
    for name in SHEETS:
        for spec, suffix in ((FIELD, ""), (BATTLE, "_Battle")):
            img = build(name, spec)
            folder = args.out if args.out else ENEMIES / name
            folder.mkdir(parents=True, exist_ok=True)
            path = folder / f"{name}{suffix}.png"
            img.save(path)
            sheets.append(img)
            print(f"wrote {path} ({img.width} x {img.height})")
    if args.preview:
        scale = 4
        width = max(s.width for s in sheets) * scale
        height = sum(s.height for s in sheets) * scale
        preview = Image.new("RGBA", (width, height), (70, 78, 62, 255))
        y = 0
        for s in sheets:
            preview.alpha_composite(s.resize((s.width * scale, s.height * scale), Image.NEAREST), (0, y))
            y += s.height * scale
        preview.convert("RGB").save(args.preview)


if __name__ == "__main__":
    main()
