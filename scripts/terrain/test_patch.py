"""The TR0 test patch: 20 x 20 m with one of everything risky (Terrain plan, TR0).

South (y = 0) is nearest the camera, which looks north like the field scenes:
  - a stream cut (level -2, 0.5 m deep) across the south, with earth banks
  - flat grass (level 0) and a curved dirt path from the stream up to a ramp
  - a ramp run (6 cells rising 3 levels, 14 degrees) up to the upper shelf
  - the upper shelf (level 3, 0.75 m) across the north, held by a stone retaining wall, with a flagstone yard
  - a one-step shelf (level 1) in the east: a tilled field with a low stone edge
  - a two-level drop (level 2 straight from 0) in the west: one wall face, no ledge
  - a saddle: two one-step mounds touching diagonally, which must join
  - a chunk boundary at x = 10 m (meshed as two chunks; it must not show)
"""

import math
import os

import numpy as np

from terrain_mesher import RAMP_NORTH, Settings, Terrain
from splat import Layer, SplatSettings

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GROUND = os.path.join(REPO, "Oxygen", "project", "Assets", "Textures", "Ground")

SIZE_M = 20.0
CELL = 0.5
N = int(SIZE_M / CELL) + 1  # corners per side
CHUNK_X = 10.0               # chunk boundary for the seam test

# Layers: the Farm Village's own materials (their .hymat tiling and tint).
LAYERS = [
    Layer("Grass", os.path.join(GROUND, "Grass.png"), (0.1, 0.1), (1.0, 1.0, 1.0), 0.3),
    Layer("Dirt", os.path.join(GROUND, "FarmDirt.png"), (0.1063, 0.1063), (0.84, 0.78, 0.74), 0.3),
    Layer("Flagstones", os.path.join(GROUND, "Flagstones.png"), (0.21, 0.198758), (1.0, 1.0, 1.0), 0.15),
    Layer("TilledSoil", os.path.join(GROUND, "TilledSoil.png"), (0.5, 0.5), (1.0, 1.0, 1.0), 0.2),
]
GRASS, DIRT, FLAGSTONES, TILLED = 0, 1, 2, 3

# Wall materials: 0 stone (StoneWall), 1 earth bank (FarmDirt, darker).
WALLS = {
    0: {"name": "StoneWall", "texture": os.path.join(GROUND, "StoneWall.png"), "tiling": (0.150235, 0.150235), "tint": (1.0, 1.0, 1.0)},
    1: {"name": "EarthBank", "texture": os.path.join(GROUND, "FarmDirt.png"), "tiling": (0.1063, 0.1063), "tint": (0.62, 0.55, 0.50)},
}
CAPS = {"name": "Boulder", "texture": os.path.join(GROUND, "Boulder.png"), "tint": (0.88, 0.84, 0.74)}

# Settings presets: start subtle, raise one at a time (plan, TR0).
PRESETS = {
    "bare": Settings(jitter=0.3, lean_px=0.0, wall_noise_px=0.0, lip_px=0.0, lip_face_px=0.0, fringe_px=0.0, caps=False),
    "conservative": Settings(jitter=0.3, lean_px=0.0, wall_noise_px=0.0, lip_px=2.0, lip_face_px=2.0, fringe_px=4.0, caps=True),
    "stronger": Settings(jitter=0.38, lean_px=2.0, wall_noise_px=1.5, lip_px=3.0, lip_face_px=2.0, fringe_px=7.0, caps=True),
    # The approved TR1 defaults (October 30): stronger outlines and path breakup, walls calmer (stone 65 %,
    # earth banks 57 % of stronger), lean between conservative and stronger, the stronger lip, coping rows.
    "tr1": Settings(jitter=0.38, lean_px=1.0, wall_noise_px=1.5, lip_px=3.0, lip_face_px=2.0, fringe_px=7.0, caps=True,
                    wall_variation={0: 0.65, 1: 0.57}),
}

# Edge noise per preset: (roughness of grass, dirt and tilled soil; flagstones get half), cluster size in art
# pixels, share of per-pixel noise.
PRESET_SPLAT = {
    "bare": (0.6, 3.0, 0.3),
    "conservative": (0.6, 3.0, 0.3),
    "stronger": (0.9, 2.5, 0.35),
    "tr1": (0.9, 2.5, 0.35),
}


def preset_layers_and_splat(preset):
    import dataclasses
    from splat import SplatSettings
    rough, cell, pixel = PRESET_SPLAT[preset]
    layers = [dataclasses.replace(l, roughness=rough * (0.5 if l.name == "Flagstones" else 1.0)) for l in LAYERS]
    return layers, SplatSettings(noise_cell_px=cell, pixel_noise=pixel)


PATH = [(12.6, 5.0), (12.0, 7.0), (10.4, 8.6), (8.4, 9.6), (7.0, 10.9), (7.0, 12.0), (7.0, 14.0), (7.3, 15.4), (8.8, 16.6), (11.0, 17.2), (12.8, 17.3)]
PATH_HALF_WIDTH = 0.8


def stream_centre(x):
    return 3.9 + 0.5 * math.sin(0.45 * x)


def shelf_edge(x):
    return 14.0 + 1.0 * math.sin(0.35 * x + 1.0)


def inside_ellipse(x, y, cx, cy, rx, ry):
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


RAMP_I0, RAMP_I1 = 12, 16      # ramp corners x 6..8 m (4 cells wide)
RAMP_J0, RAMP_J1 = 22, 28      # low edge y = 11 m, high edge y = 14 m: 6 cells rising 3 levels


def build_terrain(settings, seed=1234):
    levels = np.zeros((N, N), dtype=np.int32)
    for j in range(N):
        for i in range(N):
            x, y = i * CELL, j * CELL
            lv = 0
            if abs(y - stream_centre(x)) < 0.8:
                lv = -2
            if y > shelf_edge(x):
                lv = 3
            if inside_ellipse(x, y, 2.8, 10.5, 2.6, 2.2):
                lv = 2   # the two-level drop
            if inside_ellipse(x, y, 16.5, 8.0, 2.6, 1.6):
                lv = 1   # the tilled field shelf
            levels[j, i] = lv

    # The saddle: two 2 x 2 cell mounds touching at one cell's diagonal (cell (18, 16) at x 9, y 8).
    for j in range(14, 17):
        for i in range(16, 19):
            levels[j, i] = 1
    for j in range(17, 20):
        for i in range(19, 22):
            levels[j, i] = 1

    # The ramp: its low edge on the ground (level 0), its high edge on the shelf (level 3). Its side corners
    # keep the neighbouring ground's levels (the surface comes only from the run's ends).
    for i in range(RAMP_I0, RAMP_I1 + 1):
        levels[RAMP_J0, i] = 0
    for j in range(RAMP_J0 + 1, RAMP_J1):
        for i in range(RAMP_I0, RAMP_I1 + 1):
            if levels[j, i] == 3:
                levels[j, i] = 0   # the ground beside the ramp stays below the shelf's edge
    for j in range(RAMP_J1, min(N, RAMP_J1 + 3)):
        for i in range(RAMP_I0 - 1, RAMP_I1 + 2):
            levels[j, i] = 3
    ramps = np.zeros((N - 1, N - 1), dtype=np.uint8)
    ramps[RAMP_J0:RAMP_J1, RAMP_I0:RAMP_I1] = RAMP_NORTH

    # Wall material per cell: stone on the upper shelf and around the field, earth banks elsewhere.
    wall = np.ones((N - 1, N - 1), dtype=np.uint8)
    for j in range(N - 1):
        for i in range(N - 1):
            cell_levels = [levels[j, i], levels[j, i + 1], levels[j + 1, i], levels[j + 1, i + 1]]
            x, y = (i + 0.5) * CELL, (j + 0.5) * CELL
            if max(cell_levels) >= 3:
                wall[j, i] = 0
            elif x > 13.0 and 5.5 < y < 10.5 and max(cell_levels) == 1:
                wall[j, i] = 0
    return Terrain(levels=levels, ramps=ramps, wall_material=wall, seed=seed, settings=settings)


def distance_to_path(x, y):
    best = 1e9
    for (ax, ay), (bx, by) in zip(PATH[:-1], PATH[1:]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(x - (ax + dx * t), y - (ay + dy * t)))
    return best


def smooth_path_points():
    """Catmull-Rom through PATH, so the road bends instead of kinking."""
    pts = [PATH[0]] + PATH + [PATH[-1]]
    out = []
    for k in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[k - 1], pts[k], pts[k + 1], pts[k + 2]
        for n in range(8):
            t = n / 8.0
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[c]) + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t2 + (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t3) for c in range(2)))
    out.append(PATH[-1])
    return out


def build_weights(splat_settings, seed=1234):
    """The painted weight map (as a brush would leave it): soft macro shapes, normalised to 1."""
    res = splat_settings.resolution
    n = int(SIZE_M * res)
    smooth = smooth_path_points()
    w = np.zeros((n, n, 4))

    def ramp01(v, edge, width):
        return min(1.0, max(0.0, (edge - v) / width + 0.5))

    for ty in range(n):
        for tx in range(n):
            x, y = (tx + 0.5) / res, (ty + 0.5) / res
            d = 1e9
            for (ax, ay), (bx, by) in zip(smooth[:-1], smooth[1:]):
                dx, dy = bx - ax, by - ay
                ll = dx * dx + dy * dy
                t = 0.0 if ll == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / ll))
                d = min(d, math.hypot(x - (ax + dx * t), y - (ay + dy * t)))
            # The path widens a little and wanders (as the Path brush's wandering edges would).
            half = PATH_HALF_WIDTH + 0.12 * math.sin(1.7 * x + 0.9 * y)
            dirt = ramp01(d, half, 0.35)
            if abs(y - stream_centre(x)) < 0.75:
                dirt = 1.0   # the stream bed
            flag = 0.0
            if 12.2 < x < 17.8 and 15.9 < y < 19.4:
                edge = min(x - 12.2, 17.8 - x, y - 15.9, 19.4 - y)
                flag = min(1.0, edge / 0.3)
            till = 1.0 if inside_ellipse(x, y, 16.5, 8.0, 2.1, 1.15) else 0.0
            wf = flag
            wd = dirt * (1 - wf)
            wt = till * (1 - wf) * (1 - wd)
            wg = max(0.0, 1.0 - wf - wd - wt)
            w[ty, tx] = (wg, wd, wf, wt)
    return w
