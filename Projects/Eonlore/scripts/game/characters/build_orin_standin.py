#!/usr/bin/env python3
"""Builds the stand-in sheets for Orin Kenshi, the party's second hero, until his real art exists.

    python scripts/game/characters/build_orin_standin.py

Orin has no art yet (the brief for PixelLab is docs/OrinArtBrief.md). This makes a stand-in from the Gate Guard's
idle sheet (Characters/NPC/GateGuard), its dark leather recoloured to Orin's navy so he doesn't read as a city
guard, and writes:

  Characters/Player/Orin/Orin_Field.png (+ .hymeta, .hyanim)  the field sheet: the Gate Guard's layout (rows
      IdleDown, IdleRight, IdleUp, IdleLeft; 96 x 90 cuts, pivot at the feet) and its Idle<Facing> clips, for Orin
      standing in the countryside before he joins.
  Characters/Player/Orin/Orin.png (+ .hymeta, .hyanim)  the battle sheet in Miko's battle layout (96 x 96 grid,
      centred pivot, feet 15 px above the cell's bottom: BattleScript HeroFoot), facing left: Idle and BattleIdle
      (his left idle), WalkLeft / WalkRight (the idle with a step bob), Punch, Kick and Slash (a lunge: the idle
      pushed forward and leaning), Death (tipping over).

The handles are fixed below, so rerunning keeps every reference; the real sheets (build_miko_battle.py /
build_npc_idle.py from PixelLab exports) should be written with the same handles (--handles) to replace these
without touching any scene or data file.
"""
from __future__ import annotations

import colorsys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
ASSETS = ROOT / "Project" / "Assets"
SOURCE = ASSETS / "Characters" / "NPC" / "GateGuard" / "GateGuard.png"
OUT = ASSETS / "Characters" / "Player" / "Orin"

FIELD_TEXTURE = 6290817403346251521
FIELD_ANIMATIONS = 4415580237915726709
BATTLE_TEXTURE = 7924613408950187331
BATTLE_ANIMATIONS = 3182937116204411939

CELL = 96
SOURCE_FEET = 90   # the Gate Guard's feet: the bottom of its 96 x 90 cuts
BATTLE_FEET = 81   # Miko's battle sheet: 15 px above the cell's bottom
FACINGS = ["Down", "Right", "Up", "Left"]


def recolour(image: Image.Image) -> Image.Image:
    """Dark leather and lacquer (warm hues, saturated, dark) to deep navy; skin, metal and trim stay."""
    image = image.convert("RGBA")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if 10 <= h * 360 <= 50 and s > 0.25 and v < 0.55:
                r2, g2, b2 = colorsys.hsv_to_rgb(222 / 360, min(1.0, s * 0.9 + 0.15), v * 0.95)
                pixels[x, y] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), a)
    return image


def cell(sheet: Image.Image, row: int, column: int) -> Image.Image:
    return sheet.crop((column * CELL, row * CELL, column * CELL + CELL, row * CELL + CELL))


def placed(frame: Image.Image, dx: int = 0, dy: int = 0, shear: float = 0.0, angle: float = 0.0) -> Image.Image:
    """A field frame moved onto the battle sheet's baseline, then pushed, leaned (shear about the feet) or turned."""
    out = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    work = frame
    if shear != 0.0:
        # The head leans left (forward, towards the enemies) by shear * (feet - y); the feet stay.
        work = work.transform(work.size, Image.AFFINE, (1, -shear, shear * SOURCE_FEET, 0, 1, 0), resample=Image.NEAREST)
    if angle != 0.0:
        work = work.rotate(angle, resample=Image.NEAREST, center=(CELL / 2, SOURCE_FEET))
    out.alpha_composite(work, (dx, BATTLE_FEET - SOURCE_FEET + dy))
    return out


def write(path: Path, text: str) -> None:
    path.write_bytes(text.replace("\n", "\r\n").encode("utf-8"))


def texture_meta(handle: int, sprites: list[tuple[str, tuple[int, int, int, int], tuple[float, float]]], mode: str, pivot) -> str:
    lines = [f"Handle: {handle}", "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Nearest", "  Wrap: Clamp",
             "  SpriteMode: Multiple", "  PixelsPerUnit: 32", "  Slicing:", f"    Mode: {mode}", f"    CellSize: [{CELL}, {CELL}]",
             "    Offset: [0, 0]", "    Spacing: [0, 0]", "    Columns: 0", "    Rows: 0", "    RowNames: []",
             f"    DefaultPivot: [{pivot[0]}, {pivot[1]}]", "  Sprites:"]
    for name, rect, sprite_pivot in sprites:
        lines.append(f"    - {{Name: {name}, Rect: [{rect[0]}, {rect[1]}, {rect[2]}, {rect[3]}], Pivot: [{sprite_pivot[0]}, {sprite_pivot[1]}]}}")
    return "\n".join(lines) + "\n"


def animation_set(comment: str, handle: int, texture: int, texture_file: str, default: str, clips: list[tuple[str, str, list[tuple[str, float]]]]) -> str:
    lines = [f"# {comment}", f"Handle: {handle}", "SpriteAnimationSet:", f"  TextureHandle: {texture}", f"  Texture: {texture_file}",
             f"  DefaultClip: {default}", "  Clips:"]
    for name, loop, frames in clips:
        lines += [f"    - Name: {name}", f"      Loop: {loop}", "      Frames:"]
        for sprite, duration in frames:
            lines += [f"        - Sprite: {sprite}", f"          Duration: {duration}"]
    return "\n".join(lines) + "\n"


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    field = recolour(Image.open(SOURCE))

    # The field sheet: the source's layout, recoloured.
    field.save(OUT / "Orin_Field.png")
    sprites = []
    for row, facing in enumerate(FACINGS):
        for column in range(4):
            sprites.append((f"Idle{facing}_{column}", (column * CELL, row * CELL, CELL, SOURCE_FEET), (0.5, 0)))
    write(OUT / "Orin_Field.png.hymeta", texture_meta(FIELD_TEXTURE, sprites, "Manual", (0.5, 0)))
    durations = [0.5, 0.4, 0.5, 0.4]
    clips = [("Idle", "Loop", [(f"IdleDown_{i}", durations[i]) for i in range(4)])]
    clips += [(f"Idle{facing}", "Loop", [(f"Idle{facing}_{i}", durations[i]) for i in range(4)]) for facing in FACINGS]
    write(OUT / "Orin_Field.hyanim", animation_set("Orin (stand-in, build_orin_standin.py): idle in its directions.",
                                                   FIELD_ANIMATIONS, FIELD_TEXTURE, "Orin_Field.png", "IdleDown", clips))

    # The battle sheet: 8 columns, one row per clip, facing left (the enemies stand on the left).
    left = [cell(field, 3, column) for column in range(4)]
    right = [cell(field, 1, column) for column in range(4)]
    rows = [
        ("BattleIdle", [placed(frame) for frame in left]),
        ("WalkLeft", [placed(left[i % 4], dy=-1 if i % 2 else 0) for i in range(6)]),
        ("WalkRight", [placed(right[i % 4], dy=-1 if i % 2 else 0) for i in range(6)]),
        ("Lunge", [placed(left[0]), placed(left[0], dx=-2, shear=0.04), placed(left[0], dx=-6, shear=0.1),
                   placed(left[0], dx=-9, shear=0.14), placed(left[0], dx=-7, shear=0.08), placed(left[0], dx=-3, shear=0.03)]),
        ("Death", [placed(left[0], angle=a, dy=d) for a, d in ((0, 0), (-12, 0), (-30, 2), (-55, 4), (-80, 6), (-90, 7))]),
    ]
    sheet = Image.new("RGBA", (CELL * 8, CELL * len(rows)), (0, 0, 0, 0))
    names = {}
    sprites = []
    for row, (clip, frames) in enumerate(rows):
        names[clip] = []
        for column, frame in enumerate(frames):
            index = row * 8 + column
            sheet.alpha_composite(frame, (column * CELL, row * CELL))
            names[clip].append(f"Orin_{index}")
        for column in range(8):
            index = row * 8 + column
            sprites.append((f"Orin_{index}", (column * CELL, row * CELL, CELL, CELL), (0.5, 0.5)))
    sheet.save(OUT / "Orin.png")
    write(OUT / "Orin.png.hymeta", texture_meta(BATTLE_TEXTURE, sprites, "Grid", (0.5, 0.5)))

    idle = [(name, 0.2) for name in names["BattleIdle"]]
    lunge = [(name, d) for name, d in zip(names["Lunge"], (0.12, 0.1, 0.1, 0.18, 0.12, 0.12))]
    clips = [
        ("Idle", "Loop", idle),
        ("BattleIdle", "Loop", idle),
        ("WalkLeft", "Loop", [(name, 0.11) for name in names["WalkLeft"]]),
        ("WalkRight", "Loop", [(name, 0.11) for name in names["WalkRight"]]),
        ("Punch", "Once", lunge),
        ("Kick", "Once", lunge),
        ("Slash", "Once", lunge),
        ("Death", "Once", [(name, 0.1) for name in names["Death"]]),
    ]
    write(OUT / "Orin.hyanim", animation_set("Orin (stand-in, build_orin_standin.py): battle clips facing left.",
                                             BATTLE_ANIMATIONS, BATTLE_TEXTURE, "Orin.png", "BattleIdle", clips))
    print(f"Orin stand-in: {OUT.relative_to(ROOT)} (Orin.png {sheet.size[0]} x {sheet.size[1]}, Orin_Field.png)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
