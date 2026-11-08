"""Shrine Glade light shafts (Nov 3, 2026): faked god rays through the forest and over the shrine.

The engine has no volumetric light yet, so the shafts are additive sprites (SpriteRendererComponent::AlphaMode =
Additive): tall pixel-art beams that add warm light onto whatever is behind them, standing upright and leaning in their
own plane (the Z rotation) as if the low sun came through the canopy from the upper left of the frame. They are unlit
and sort with the translucent sprites, so trees in front of a beam hide it and the shrine behind it brightens.

Two steps, both safe to run again:

  1. Art (Sprites/Effects/FX_LightShaft_*.png + .hymeta, stable handles): a narrow beam, a wide one and a dappled one
     (three beams of different strength with gaps, light through leaves). Pixel-art rules: binary alpha (the light's
     strength is in the colour, as the sprite adds colour times alpha), four bands of strength with ordered dithering
     between them, the brightest band a third of the way down where the beam leaves the canopy, fading out before it
     reaches the ground so its foot never shows as an edge.
  2. Placement in ShrineGlade.hyscene: a "Light Shafts" group (LightShaftScript: the beams breathe slowly, take the
     sun's colour and fade out with the sun, so night has none) with beams through the left and right woods, the
     sacred tree's crown and onto the shrine's courtyard. The script owns these entities (stable ids): a run puts them
     back as written here, so move them in this script, not in the editor. Nothing else in the scene is touched.

    python scripts/game/terrain/shrine_glade_light_shafts.py [--dry-run] [--art-only]

Close the scene in the editor first. A backup goes to bin-int/shrine_glade_backup/.
"""
import argparse
import math
import os
import shutil
import struct
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import shrine_glade_jp_pass as glade  # noqa: E402  (entities(), entity(), stable_id(), paths)

EFFECTS = glade.EFFECTS
SCENE = glade.SCENE
PIXELS_PER_METRE = 32

# The beams' colour at full strength (linear; the texture is sRGB): a warm white the sun's colour then tints.
WARM = (1.0, 0.9, 0.72)
# The four bands of strength (linear), the dither between them is a 4 x 4 Bayer matrix.
BANDS = (0.0, 0.16, 0.34, 0.6, 1.0)
BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))

# name -> (width px, height px, beams: [(centre across 0..1, half width 0..1, strength)])
ART = {
    "FX_LightShaft_Narrow": (24, 160, [(0.5, 0.36, 1.0)]),
    "FX_LightShaft_Wide": (48, 192, [(0.5, 0.42, 1.0)]),
    "FX_LightShaft_Dappled": (64, 192, [(0.2, 0.13, 0.85), (0.52, 0.17, 1.0), (0.83, 0.11, 0.7)]),
}


def srgb(linear):
    linear = max(0.0, min(1.0, linear))
    return 12.92 * linear if linear <= 0.0031308 else 1.055 * linear ** (1 / 2.4) - 0.055


def smoothstep(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def beam_strength(u, v, beams, seed):
    """Strength 0..1 at (u across, v down) of the image."""
    # along the beam: full where it leaves the canopy at the top (a short fade in, the leaves hide the end), then a
    # long fade to nothing before the ground
    along = smoothstep(0.0, 0.06, v) * (1.0 - smoothstep(0.18, 0.94, v))
    best = 0.0
    for k, (centre, half, strength) in enumerate(beams):
        # light spreads: the beam is 60 % as wide at the top as at the bottom
        width = half * (0.6 + 0.4 * v)
        across = 1.0 - smoothstep(width * 0.55, width, abs(u - centre))
        # streaks along the beam (light through gaps of different size), fixed per column
        column = int(u * 64)
        streak = 0.78 + 0.22 * ((column * 7 + seed * 5 + k * 3) % 5) / 4.0
        best = max(best, across * strength * streak)
    return along * best


def quantise(value, x, y):
    """The value as one of BANDS: flat bands, dithered only in a narrow seam between two (no noise over the beam)."""
    for i in range(len(BANDS) - 1):
        lo, hi = BANDS[i], BANDS[i + 1]
        if value <= hi:
            t = (value - lo) / (hi - lo)
            t = max(0.0, min(1.0, (t - 0.5) * 3.0 + 0.5))
            return hi if t * 16.0 > BAYER[y % 4][x % 4] + 0.5 else lo
    return BANDS[-1]


def write_png(path, width, height, rows):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(row) for row in rows)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def make_art():
    """Writes the three beams (and their metas, once). Returns name -> (handle, (width, height))."""
    out = {}
    for seed, (name, (w, h, beams)) in enumerate(sorted(ART.items())):
        rows = []
        for y in range(h):
            row = []
            for x in range(w):
                value = quantise(beam_strength((x + 0.5) / w, (y + 0.5) / h, beams, seed), x, y)
                if value <= 0.0:
                    row += [0, 0, 0, 0]
                else:
                    row += [int(round(srgb(value * c) * 255)) for c in WARM] + [255]
            rows.append(row)
        png = os.path.join(EFFECTS, name + ".png")
        write_png(png, w, h, rows)
        meta = png + ".hymeta"
        handle = glade.stable_id("sprite:" + name)
        if os.path.exists(meta) is False:
            with open(meta, "w", newline="\r\n") as f:
                f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
                        "  SpriteMode: Single\n  PixelsPerUnit: %d\n" % (handle, PIXELS_PER_METRE))
        else:
            with open(meta, encoding="utf-8") as f:
                handle = int(f.read().split("Handle: ")[1].split()[0])
        out[name] = (handle, (w, h))
    return out


# The beams: key, art, foot (x, y, ground z), lean (radians, top to the left), scale on the art's size, strength
# (the sprite's alpha), sort order. The sun comes from the upper left of the frame, so every beam leans the same way.
SHAFTS = [
    # the shrine's courtyard: two beams either side of the hall's door, onto the paving in front of the lanterns
    ("shrine-left", "FX_LightShaft_Wide", (-2.9, 4.1, 3.0), 0.34, 1.0, 0.24, 3),
    ("shrine-right", "FX_LightShaft_Narrow", (2.4, 3.7, 3.0), 0.34, 1.1, 0.2, 3),
    # through the sacred tree's crown onto its roots
    ("sacred-crown", "FX_LightShaft_Dappled", (-5.2, 8.4, 3.0), 0.3, 1.0, 0.26, 3),
    ("sacred-side", "FX_LightShaft_Narrow", (-8.6, 9.3, 3.0), 0.3, 0.9, 0.18, 3),
    # the left woods
    ("woods-left", "FX_LightShaft_Dappled", (-15.6, 10.6, 3.0), 0.36, 1.1, 0.22, 3),
    ("woods-left-far", "FX_LightShaft_Narrow", (-19.4, 12.4, 3.0), 0.36, 1.0, 0.16, 3),
    ("woods-left-edge", "FX_LightShaft_Narrow", (-13.4, 6.2, 3.0), 0.36, 0.85, 0.18, 3),
    # the right woods
    ("woods-right", "FX_LightShaft_Dappled", (15.2, 9.9, 3.0), 0.36, 1.1, 0.22, 3),
    ("woods-right-far", "FX_LightShaft_Wide", (19.2, 12.6, 3.0), 0.36, 1.0, 0.16, 3),
    ("woods-right-edge", "FX_LightShaft_Narrow", (13.2, 5.6, 3.0), 0.36, 0.85, 0.18, 3),
]
SCRIPT = ("    NativeScriptComponent:\n      Scripts:\n        - Name: LightShaftScript\n          Properties:\n"
          "            Breathe: 0.3\n            Period: 7\n            HiddenBelow: 0.25\n            FullFrom: 0.7\n"
          "            FollowSunColor: true\n")


def shaft_body(handle, strength, sort):
    return ("    SpriteRendererComponent:\n      Color: [1, 1, 1, %g]\n      TilingFactor: 1\n      SortOrder: %d\n"
            "      TextureHandle: %d\n      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n"
            "      AlphaMode: Additive\n      CastShadows: false\n") % (strength, sort, handle)


def place(blocks, art):
    index = {int(b.split("Entity: ")[1].split()[0]): i for i, b in enumerate(blocks)}
    added, updated = [], []

    def put(eid, text, tag):
        if eid not in index:
            added.append(text)
        elif blocks[index[eid]].rstrip("\n") != text.rstrip("\n"):
            blocks[index[eid]] = text + ("\n" if blocks[index[eid]].endswith("\n") else "")
            updated.append(tag)

    group = glade.stable_id("light-shafts")
    put(group, glade.entity(group, "Light Shafts", None, (0, 0, 0), SCRIPT), "Light Shafts")
    for key, name, (x, y, z), lean, scale, strength, sort in SHAFTS:
        handle, (w, h) = art[name]
        sx, sy = w / PIXELS_PER_METRE * scale, h / PIXELS_PER_METRE * scale
        # The translation is the quad's centre; its foot (the middle of the bottom edge, where it stands up) is half its
        # height below that along the leaned card: place the centre so the foot lands where asked.
        cx, cy = x - math.sin(lean) * sy * 0.5, y + math.cos(lean) * sy * 0.5
        eid = glade.stable_id("light-shaft:" + key)
        tag = "Light Shaft " + key.replace("-", " ").title()
        put(eid, glade.entity(eid, tag, group, (cx, cy, z), shaft_body(handle, strength, sort), (sx, sy, 1), lean), tag)
    return blocks + added, len(added), updated


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true", help="make the art, report what the scene would get")
    ap.add_argument("--art-only", action="store_true")
    args = ap.parse_args()
    art = make_art()
    print("art: %s" % ", ".join("%s (%d x %d)" % (n, s[0], s[1]) for n, (_, s) in sorted(art.items())))
    if args.art_only:
        return
    raw = open(SCENE, "rb").read()
    nl = "\r\n" if b"\r\n" in raw else "\n"
    header, blocks = glade.entities(raw.decode("utf-8").replace("\r\n", "\n"))
    blocks, added, updated = place(blocks, art)
    print("scene: %d added, %d updated%s" % (added, len(updated), (": " + ", ".join(updated)) if updated else ""))
    if args.dry_run or (added == 0 and len(updated) == 0):
        return
    backup = os.path.join(glade.REPO, "bin-int", "shrine_glade_backup")
    os.makedirs(backup, exist_ok=True)
    shutil.copy2(SCENE, os.path.join(backup, "ShrineGlade.%s.hyscene" % time.strftime("%Y%m%d-%H%M%S")))
    text = header + "".join(blocks)
    with open(SCENE, "wb") as f:
        f.write(text.replace("\n", nl).encode("utf-8"))


if __name__ == "__main__":
    main()
