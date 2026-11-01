"""Writes the Japanese countryside (Nov 2, 2026): Oxygen/project/Assets/Terrains/JapaneseCountryside.hyterrain and
Oxygen/project/Assets/Scenes/JapaneseCountryside.hyscene, the largest field scene so far (140 x 152 m), laid out after
the overview the user drew (docs/TestBed.md, "Japanese countryside"):

  - A road from the south edge (towards the city gate: "Exit JapaneseCitySlice") north through terraced rice
    paddies, up four earth-banked terraces (1 m each, ramps where the road crosses) to the shrine stairs and the torii,
    and on to the Shrine Glade ("Exit ShrineGlade").
  - The paddies: sunk a step into each terrace, water over mud, rice seedlings (terrain decor), bunds to walk on.
  - West, a cedar forest on a raised, banked shelf: the ronin camp (Jokai and Sanzo, side quests), wisps between the
    trees, medicinal herbs to gather, and the ancient tree in its clearing where Orin waits (the quest builder,
    scripts/quests/build_countryside_quest.py, adds him, the leyline and the way down into it).
  - East, the farm hub: a ring of farmhouses, a kura storehouse, a well and a persimmon around a yard, a vegetable
    garden (Genji and Okiku, side quests), and a bamboo grove up the bank behind it with shoots to gather.
  - A fox shrine off the road (a short tunnel of torii, stone foxes, a red maple), a pond with a weeping cherry, slimes
    in the open meadows, camera zones for the camp, the tree, the hub and the stairs.

The city gate isn't modelled at the south edge: the field camera looks north, so anything tall south of the player
stands between her and the camera; the road leaves the bottom of the screen there instead.

    python scripts/japanese_city/make_countryside_art.py      # once: bamboo, the camp's tarp, the foxes, the rift
    python scripts/japanese_city/build_countryside.py         # the terrain and the scene, then the field UI and the quest
    python scripts/japanese_city/build_countryside.py --no-extras   # only the terrain and the scene (no UI, no quest)
    python scripts/japanese_city/build_countryside.py --map-only    # only the preview map (scripts/terrain/out/)

Close the scene in the editor first: the whole scene is written again (its handle is kept).
"""

import argparse
import math
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "scripts", "terrain"))
import build_kit_showcase as kit  # noqa: E402
import build_city_scene as city  # noqa: E402
import build_city_slice as slice_  # noqa: E402
import hyterrain  # noqa: E402
import scene_decor  # noqa: E402
import terrain_mesher as tm  # noqa: E402
from scene_terrain import Grid, Paint, catmull_rom, distance_to_polyline, field_noise, smoothstep, wander  # noqa: E402

kit.KEY = "JP_Country:"
ASSETS = kit.ASSETS
SCENE = os.path.join(ASSETS, "Scenes", "JapaneseCountryside.hyscene")
TERRAIN = os.path.join(ASSETS, "Terrains", "JapaneseCountryside.hyterrain")
SPRITES = os.path.join(ASSETS, "Sprites")
TREES, PLANTS, PROPS, CROPS = (os.path.join(SPRITES, k) for k in ("Trees", "Plants", "Props", "Crops"))
SEED = 9241

X0, Y0, WIDTH, HEIGHT = -70.0, -52.0, 140.0, 200.0

# Levels (0.25 m steps): four terraces rising north, the shrine's stone terrace on top, the forest a metre above the
# terrace it stands on, the paddies a step below theirs, the pond three.
BANDS = (0, 4, 8, 12, 16)
FOREST_RISE = 4
EDGES = (-6.0, 22.0, 48.0, 72.0)          # terrace edges (y); edge 22 sits at 23.5 east of the road (behind the farm)
# The mountain north of the fields (Nov 2): cliffs stepping up (y of the edge, level above it), wooded on every shelf;
# the shrine is high up in it, which is why few people make the climb.
MOUNTAIN = ((88.0, 28), (100.0, 42), (114.0, 60), (128.0, 76), (140.0, 92))
# The way up: a stone-lined cut through the cliffs with one long flight of stairs (x half-width, y from, y to, level of
# the cut's floor -> level at the top), a landing set into the mountain, and a narrower flight on up to the exit.
CUT = (3.5, 85.0, 116.0)                  # the first flight's cut: |x| <= 3.5, y 85..116, floor level 16
FLIGHT = (0.0, 86.0, 116.0, 16, 52, 6.0)  # x, y from, y to, level from, level to, width (9 m up over 30 m)
LANDING = (7.0, 116.0, 125.0, 52)         # |x| <= 7, y 116..125, level 52 (13 m)
CUT2 = (2.5, 125.0, 139.0)
FLIGHT2 = (0.0, 125.0, 139.0, 52, 76, 4.4)

# Layers and wall layers.
GRASS, SHADE, DIRT, MUD = 0, 1, 2, 3
STONE, EARTH, PADDY_BANK, FOREST_BANK = 0, 1, 2, 3

# The road (centre line) and where things cross the terrace edges: (edge index, x of the crossing's centre).
ROAD = [(0.5, -52.0), (0.5, -44.0), (-0.5, -30.0), (1.0, -16.0), (1.0, -9.0), (1.0, -3.0), (0.0, 8.0), (-2.0, 17.0),
        (-2.0, 19.0), (-2.0, 25.0), (0.0, 36.0), (2.0, 43.0), (2.0, 45.0), (2.0, 51.0), (0.5, 62.0), (-1.0, 69.0),
        (-1.0, 75.0), (0.0, 82.0), (0.0, 86.0)]
ROAD_RAMPS = ((0, 1.0), (1, -2.0), (2, 2.0), (3, -1.0))
FOREST_RAMPS = ((1, -52.0), (2, -50.0))   # inside the forest: the path from the camp up to the ancient tree
GROVE_RAMP = (1, 36.5)                    # behind the farm, up to the bamboo grove
RIM_RAMPS = (4.0, 56.0)                   # into the forest from the fields (y): the camp path, the tree path

CAMP = (-54.0, 4.0)
TREE = (-54.0, 62.0)                      # the ancient tree's foot (the quest builder places it)
HUB = (40.0, 6.5)
FOX = (-11.0, 36.0)
POND = (25.0, -30.0, 7.0, 4.5)
SHOOTS = ((44.6, 30.2), (50.5, 32.0), (55.8, 34.2), (52.6, 41.6), (47.0, 38.2))       # bamboo shoots in the grove
HERBS = ((-47.0, 9.0), (-57.5, 15.0), (-48.8, 28.0), (-55.0, 38.5), (-47.5, 44.2), (-58.0, 52.0))   # herbs in the forest
# Mysteries and collectibles (Nov 2): chests named after their loot table (Loot.hytable), riddle stones (Signs.hytable)
# that hint at the hidden ones, and the fox shrine's five lost charms (its side quest, SideFoxCharms).
CHESTS = (("PondCache", 30.0, -22.6), ("KuraCache", 52.0, 20.6), ("RoninOath", -64.0, 12.0), ("ShrineGift", -6.2, 120.4),
          ("LeyRoots", -57.6, 64.6), ("FoxOffering", -11.0, 41.4), ("RoadsideCache 1", -30.0, -49.0), ("RoadsideCache 2", 57.0, 67.6),
          ("ForestCache 1", -60.5, 46.0), ("ForestCache 2", 58.5, 44.0))
RIDDLES = (("RiddlePond", 3.4, -19.0), ("RiddleKura", 29.0, 1.6), ("RiddleCedar", -45.8, 8.2), ("RiddleShrine", 6.4, 82.2),
           ("RiddleRoots", -47.6, 61.6), ("FoxShrine", -13.8, 31.0))
CHARMS = ((-50.6, 7.6), (27.0, -24.6), (4.6, 31.6), (4.8, 121.8), (-20.0, 35.0))   # the camp, the cherry, the maple, the stairs' top, the paddies
WISPS = ((-58.0, 18.0, "Wisps"), (-47.5, 31.0, "Wisps"), (-56.5, 41.0, "WispAndSlimes"), (-61.0, 70.0, "Wisps"), (-50.0, 80.0, "Wisps"))

# Paths painted as dirt (and kept free of paddies): the lane to the farm, the camp and tree paths, the fox shrine's.
LANE = [(1.0, 6.5), (10.0, 7.0), (20.0, 6.0), (28.0, 6.5), (36.0, 6.5)]
CAMP_PATH = [(0.5, 4.0), (-12.0, 4.2), (-26.0, 3.8), (-37.0, 4.0), (-43.0, 4.0), (-49.0, 4.0)]
TREE_PATH = [(1.0, 56.0), (-14.0, 56.2), (-28.0, 55.8), (-37.0, 56.0), (-43.0, 56.0), (-48.0, 57.5)]
FOREST_PATH = [(-54.0, 9.0), (-52.5, 15.0), (-52.0, 19.0), (-52.0, 25.0), (-51.0, 35.0), (-50.0, 45.0), (-50.0, 51.0), (-51.5, 54.0)]
GROVE_PATH = [(36.5, 13.0), (36.5, 19.0), (36.5, 27.0), (42.0, 31.0), (49.0, 30.5), (54.0, 35.0), (51.0, 40.0)]
FOX_PATH = [(-1.0, 30.5), (-6.0, 30.5), (-11.0, 31.0), (-11.0, 35.5)]
POND_PATH = [(1.0, -20.0), (8.0, -21.5), (15.0, -23.5), (19.0, -25.0)]


# ------------------------------------------------------------------------------------------------------------
# The shape

def edge_line(x, k):
    """Terrace edge k's y along x: the edge wanders, but runs straight across every ramp that crosses it."""
    e = EDGES[k]
    base = np.full(np.shape(x), e)
    if k == 1:
        base = e + 1.5 * smoothstep(14.0, 20.0, x)                 # behind the farm's row of houses
    y = base + wander(x, SEED + 10 + k, 1.4, 7.0)
    crossings = [xc for (kk, xc) in ROAD_RAMPS + FOREST_RAMPS + (GROVE_RAMP,) if kk == k]
    for xc in crossings:
        ec = e + (1.5 if (k == 1 and xc > 14.0) else 0.0)
        w = smoothstep(5.0, 3.0, np.abs(x - xc))
        y = y * (1.0 - w) + ec * w
    return y


def forest_rim(y):
    """The west forest's edge (x) along y: straight where the camp and tree paths climb into it."""
    x = -40.0 + wander(y, SEED + 20, 2.5, 8.0)
    for yc in RIM_RAMPS:
        w = smoothstep(5.0, 3.0, np.abs(y - yc))
        x = x * (1.0 - w) + (-40.0) * w
    return x


def mountain_edge(x, k):
    """Mountain cliff k's foot (y) along x: rough and wandering, lower towards the map's sides."""
    e = MOUNTAIN[k][0]
    y = e + wander(x, SEED + 40 + k, 3.0, 8.0) - 6.0 * smoothstep(30.0, 66.0, np.abs(x))
    if k == 0:
        y = np.where(np.abs(x) < 10.0, np.maximum(y, 87.0), y)   # clear of the torii and the lanterns at the foot
    return y


def east_rim(y):
    return 62.0 + wander(y, SEED + 21, 2.0, 7.0)


def paddies():
    """The paddies: rectangles (x0, x1, y0, y1) cut from blocks between the paths, bunds 1.2 m wide between them."""
    blocks = [
        # west of the road, terrace by terrace (the camp path at y 4 and the tree path at y 56 run between them)
        (-35.5, -5.0, -46.0, -9.0, 4), (-35.5, -5.0, -3.0, 1.6, 2), (-35.5, -5.0, 6.4, 19.0, 3),
        (-35.5, -21.0, 25.0, 45.0, 4), (-35.5, -5.0, 51.0, 53.8, 1), (-35.5, -5.0, 58.2, 69.0, 3),
        # east of the road (meadows left open for the slimes; the farm and the grove beyond)
        (5.5, 17.0, -24.0, -9.0, 3), (5.5, 31.0, 25.6, 45.0, 4), (5.5, 23.0, 51.0, 69.0, 4), (8.0, 30.0, 75.5, 84.0, 2),
    ]
    out = []
    for n, (x0, x1, y0, y1, rows) in enumerate(blocks):
        cols = max(1, int(round((x1 - x0) / 8.0)))
        bund = 1.2
        # column and row cuts, a little uneven (hand-dug), at whole cells
        xs = [x0] + [x0 + (x1 - x0) * (k + 0.15 * math.sin(3.1 * k + n)) / cols for k in range(1, cols)] + [x1]
        ys = [y0] + [y0 + (y1 - y0) * (k + 0.12 * math.cos(2.3 * k + n)) / rows for k in range(1, rows)] + [y1]
        snap = lambda v: round(v * 2.0) / 2.0  # noqa: E731
        for i in range(cols):
            for j in range(rows):
                a, b = snap(xs[i] + (bund / 2 if i > 0 else 0.0)), snap(xs[i + 1] - (bund / 2 if i < cols - 1 else 0.0))
                c, d = snap(ys[j] + (bund / 2 if j > 0 else 0.0)), snap(ys[j + 1] - (bund / 2 if j < rows - 1 else 0.0))
                if b - a >= 2.0 and d - c >= 2.0:
                    out.append((a, b, c, d))
    return out


PADDIES = paddies()


def ground_z(x, y):
    """The ground's height at (x, y) without building the terrain (for scripts/quests/build_countryside_quest.py):
    the terrace, forest, paddy and pond levels, and the slope on the north-going ramps. Close to the mesher's answer
    away from the jittered edges, which is where things are put."""
    ax = np.array([x])
    band = sum(1 for k in range(len(EDGES)) if y >= float(edge_line(ax, k)[0]))
    level = BANDS[band]
    for k, (e, lv) in enumerate(MOUNTAIN):
        if y >= float(mountain_edge(ax, k)[0]):
            level = lv
    if in_forest(x, y, 0.0):
        level += FOREST_RISE
    if any(a <= x <= b and c <= y <= d for (a, b, c, d) in PADDIES):
        level -= 1
    z = level * 0.25
    for (fx, fy0, fy1, lo, hi, fw) in (FLIGHT, FLIGHT2):
        if abs(x - fx) <= fw / 2 and fy0 <= y <= fy1:
            return (lo + (y - fy0) / (fy1 - fy0) * (hi - lo)) * 0.25
    if abs(x) <= LANDING[0] and LANDING[1] <= y <= LANDING[2]:
        return LANDING[3] * 0.25
    runs = [(xc - 2.0, xc + 2.0, EDGES[k] + (1.5 if (k == 1 and xc > 14.0) else 0.0), 4) for (k, xc) in ROAD_RAMPS + FOREST_RAMPS + (GROVE_RAMP,)]
    for (x0, x1, e, rise) in runs:
        if x0 <= x <= x1 and e - 2.0 <= y <= e + 2.0:
            low = float(ground_z(x, e - 2.1))
            return low + (y - (e - 2.0)) / 4.0 * rise * 0.25
    for yc in RIM_RAMPS:
        if -42.0 <= x <= -38.0 and yc - 2.0 <= y <= yc + 2.0:
            return float(ground_z(-37.9, y)) + (-38.0 - x) / 4.0 * FOREST_RISE * 0.25
    return z


def ramp_cells(g, x0, x1, y0, y1, direction):
    i0, i1 = int(round((x0 - g.x0) / g.cell)), int(round((x1 - g.x0) / g.cell))
    j0, j1 = int(round((y0 - g.y0) / g.cell)), int(round((y1 - g.y0) / g.cell))
    g.ramps[j0:j1, i0:i1] = direction


def build_levels(g):
    X, Y = g.X, g.Y
    x, y = X[0], Y[:, 0]
    band = np.zeros(X.shape, dtype=np.int32)
    for k in range(len(EDGES)):
        band += (Y >= edge_line(x, k)[None, :]).astype(np.int32)
    L = np.array(BANDS, dtype=np.int32)[band]
    for k, (e, lv) in enumerate(MOUNTAIN):
        L[Y >= mountain_edge(x, k)[None, :]] = lv
    forest = (X < forest_rim(y)[:, None]) | (X > east_rim(y)[:, None])
    L[forest] += FOREST_RISE
    # The cut and the landing (stone walls round them), sunk into the cliffs and the forest alike.
    cut = (np.abs(X) <= CUT[0]) & (Y >= CUT[1]) & (Y <= CUT[2])
    landing = (np.abs(X) <= LANDING[0]) & (Y >= LANDING[1]) & (Y <= LANDING[2])
    cut2 = (np.abs(X) <= CUT2[0]) & (Y > LANDING[2]) & (Y <= CUT2[2])
    L[cut] = FLIGHT[3]
    L[landing] = LANDING[3]
    L[cut2] = FLIGHT2[3]
    shrine = cut | landing | cut2
    paddy = np.zeros(X.shape, dtype=bool)
    for (a, b, c, d) in PADDIES:
        paddy |= (X >= a) & (X <= b) & (Y >= c) & (Y <= d)
    L[paddy] -= 1
    cx, cy, rx, ry = POND
    pond = ((X - cx) / rx) ** 2 + ((Y - cy) / ry) ** 2 + 0.08 * np.sin(1.7 * X + 0.9 * Y) < 1.0
    L[pond] -= 3
    g.levels[:] = L

    # Ramps: the road over each terrace edge, the forest path, the grove, into the forest, the shrine stairs.
    for (k, xc) in ROAD_RAMPS + FOREST_RAMPS + (GROVE_RAMP,):
        e = EDGES[k] + (1.5 if (k == 1 and xc > 14.0) else 0.0)
        ramp_cells(g, xc - 2.0, xc + 2.0, e - 2.0, e + 2.0, tm.RAMP_NORTH)
    for yc in RIM_RAMPS:
        ramp_cells(g, -42.0, -38.0, yc - 2.0, yc + 2.0, tm.RAMP_WEST)

    # Walls: cut stone lining the way up to the shrine, earth banks elsewhere (lower and gentler round the paddies);
    # the mountain's cliffs are earth banks too, sloping back, so it reads as a hillside rather than masonry.
    def corners(mask):
        m = mask.astype(np.int32)
        return m[:-1, :-1] + m[:-1, 1:] + m[1:, :-1] + m[1:, 1:]
    walls = np.full(g.walls.shape, EARTH, dtype=np.uint8)
    walls[(corners(paddy) > 0) | (corners(pond) > 0)] = PADDY_BANK
    f = corners(forest)
    walls[(f > 0) & (f < 4)] = FOREST_BANK
    s = corners(shrine)
    walls[(s > 0) & (s < 4)] = STONE
    g.walls[:] = walls
    return dict(forest=forest, paddy=paddy, pond=pond, shrine=shrine)


def build_weights(g):
    p = Paint(g, resolution=8, layers=4)
    PX, PY = p.PX, p.PY
    rim = forest_rim(PY[:, 0])[:, None]
    east = east_rim(PY[:, 0])[:, None]

    def woods(px, py):
        inside = np.maximum(smoothstep(1.5, -1.0, px - rim), smoothstep(-1.5, 1.0, px - east))
        n = field_noise(px, py, SEED + 40, 5.0)
        return np.clip(inside + 0.6 * smoothstep(0.66, 0.78, n) * (1.0 - inside), 0.0, 1.0)
    p.where(woods, SHADE)

    road = dict(soft=0.9)
    p.road(ROAD, 3.2, DIRT, seed=31, **road)
    for k, path in enumerate((LANE, CAMP_PATH, TREE_PATH, FOREST_PATH, GROVE_PATH, FOX_PATH, POND_PATH)):
        p.road(path, 1.9 if k else 2.4, DIRT, seed=32 + k, **road)
    p.blob(HUB, (15.0, 6.5), DIRT, seed=40, soft=1.1, corner=4.0)                   # the farm yard
    p.blob(CAMP, (6.0, 5.0), DIRT, seed=41, soft=1.0, corner=3.0)                   # the camp
    p.blob((TREE[0] + 1.0, TREE[1] - 3.0), (6.5, 4.5), DIRT, seed=42, soft=1.2, corner=3.0)
    p.blob((FOX[0], FOX[1] + 1.0), (3.6, 3.0), DIRT, seed=43, soft=0.8, corner=1.5)
    p.blob((0.0, 84.0), (4.0, 2.5), DIRT, seed=44, soft=0.8)                        # before the torii
    p.blob((0.0, 120.5), (6.6, 4.2), DIRT, seed=45, soft=0.8, corner=2.0)           # the landing

    # Trodden grass in the yard and along the road's middle (partial: thin grass, not islands).
    def wear(px, py):
        n = field_noise(px, py, SEED + 50, 2.2)
        yard = 1.0 - smoothstep(0.5, 0.9, np.hypot((px - HUB[0]) / 15.0, (py - HUB[1]) / 6.5))
        line = catmull_rom(ROAD)
        middle = (1.0 - smoothstep(0.15, 0.45, distance_to_polyline(px, py, line))) * smoothstep(-10.0, -14.0, py)
        return np.maximum(yard, middle) * smoothstep(0.55, 0.7, n)
    p.where(wear, GRASS, 0.6)

    # The paddies' mud (under the water; the seedlings grow from it).
    for (a, b, c, d) in PADDIES:
        p.blob(((a + b) / 2, (c + d) / 2), ((b - a) / 2 + 0.1, (d - c) / 2 + 0.1), MUD, wander_m=0.0, soft=0.2, seed=1, corner=0.3)
    return p.to_bytes()


def handle(relative):
    return kit.handle_of(os.path.join(ASSETS, relative + ("" if relative.endswith(".hymat") else ".hymeta")))


def make_settings():
    s = hyterrain.TerrainSettings()
    s.seed = SEED
    s.add_layer(handle("Materials/Ground/FarmGrass.hymat"), 0.9, handle("Materials/Ground/FarmGrassFringe.hymat"), 7.0, blend=0.5, height_blend=1.0)
    s.add_layer(handle("Materials/Ground/FarmGrassShade.hymat"), 0.9, handle("Materials/Ground/FarmGrassShadeFringe.hymat"), 6.0,
                blend=0.8, height_blend=1.0)
    s.add_layer(handle("Materials/Ground/VillageDirt.hymat"), 0.9, blend=0.5, height_blend=1.0)
    s.add_layer(handle("Materials/Ground/TilledSoil.hymat"), 0.4, blend=0.2, height_blend=0.5)
    s.add_wall_layer(handle("Materials/Ground/StoneWall.hymat"), 0.65, True)
    s.add_wall_layer(handle("Materials/Ground/EarthBank.hymat"), 0.57, False, slope=0.5)
    s.add_wall_layer(handle("Materials/Ground/EarthBank.hymat"), 0.5, False, slope=0.3)    # the paddies' low banks
    s.add_wall_layer(handle("Materials/Ground/EarthBank.hymat"), 0.57, False, slope=0.45)  # the forest's banks
    s.cap_material = handle("Materials/Ground/Boulder.hymat")
    fv = {k: [kit.handle_of(os.path.join(ASSETS, p + ".hymeta")) for p in v] for k, v in scene_decor.FARMVILLAGE.items()}
    size = (0.84, 1.23)
    seedlings = [handle("Sprites/Crops/Wheat_1%s.png" % c) for c in "ABCD"]
    s.add_decor("LayerBorder", GRASS, "Upright", 1.4, size, fv["Decor Path Edge"])
    s.add_decor("Layer", SHADE, "Upright", 0.35, size, fv["Decor Forest Floor"] + fv["Decor Woods Bush"])
    s.add_decor("Layer", MUD, "Upright", 4.5, (0.3, 0.42), seedlings)                    # rice seedlings in the water
    s.add_decor("Layer", GRASS, "Upright", 0.06, size, fv["Decor Field Edge"] + fv["Decor Farm Meadow"])
    s.add_decor("WallFoot", hyterrain.ANY_WALL_LAYER, "Upright", 0.9, size, fv["Decor Wall Foot"])
    s.add_decor("WallTop", EARTH, "Upright", 1.1, size, fv["Decor Stream Bank"])
    s.add_decor("WallTop", FOREST_BANK, "Upright", 0.8, size, fv["Decor Forest Edge"])
    s.add_decor("WallFoot", STONE, "Stone", 0.9, (0.06, 0.1), [handle("Textures/Ground/Boulder.png")])
    return s


# ------------------------------------------------------------------------------------------------------------
# Placing things on the ground

class Ground:
    def __init__(self, g, terrain):
        self.g, self.terrain = g, terrain

    def z(self, x, y):
        return self.g.height_at(self.terrain, x, y)

    def clear(self, x, y, half=1.0, behind=1.8, front=0.4):
        """Whether nothing higher stands within `half` m either side, `behind` m north or `front` m south of (x, y):
        banks lean back over the ground at their foot, so a tree or a prop there would sink into the slope."""
        g = self.g
        i0, i1 = max(0, int((x - half - g.x0) / g.cell)), min(g.nx - 1, int((x + half - g.x0) / g.cell) + 1)
        j0, j1 = max(0, int((y - front - g.y0) / g.cell)), min(g.ny - 1, int((y + behind - g.y0) / g.cell) + 1)
        window = g.levels[j0:j1 + 1, i0:i1 + 1]
        return int(window.max()) == int(window.min())


def png_size(path):
    return kit.png_size(path)


def sprite_body(texture, sort=0, color=(1, 1, 1, 1), flip=False, emission=None, sprite=None):
    b = ("    SpriteRendererComponent:\n      Color: [%g, %g, %g, %g]\n      TilingFactor: 1\n      SortOrder: %d\n      TextureHandle: %d\n"
         % (tuple(color) + (sort, texture)))
    if sprite:
        b += "      Sprite: %s\n" % sprite
    b += "      SpriteIndex: 0\n      FlipX: %s\n      FlipY: false\n      Facing: Upright\n" % ("true" if flip else "false")
    if emission:
        b += "      EmissionColor: [%g, %g, %g]\n      EmissionIntensity: %g\n" % emission
    return b


def trunk(sx, sy, w=0.7, d=0.6, h=2.0):
    """A solid trunk or base for a sprite of size (sx, sy): its box collider, in the sprite's scaled space."""
    return ("    BoxColliderComponent:\n      Offset: [0, 0, %g]\n      Size: [%g, %g, %g]\n      Layer: World\n      IsTrigger: false\n"
            % (h / 2, w / sx, d / sy, h))


class Builder(slice_.Slice):
    def __init__(self, summary, ground):
        super().__init__(summary)
        self.ground = ground

    def card(self, parent, folder, name, x, y, scale=1.0, tag=None, solid=None, z=None, **kw):
        """An upright sprite standing on the ground at (x, y), `scale` times its size at 32 px per metre."""
        png = os.path.join(folder, name + ".png")
        if os.path.exists(png + ".hymeta") is False:
            self.d.missing.append(name)
            return None
        pw, ph = png_size(png)
        sx, sy = pw / 32.0 * scale, ph / 32.0 * scale
        z = self.ground.z(x, y) if z is None else z
        tag = tag or self._n(name)
        body = sprite_body(kit.handle_of(png + ".hymeta"), **kw)
        if solid:
            body += trunk(sx, sy, *solid)
        self.w.entity("card:" + tag, tag, parent=parent, t=(x, y, z), s=(sx, sy, 1), body=body)
        return tag

    def place(self, parent, path, x, y, rot=0.0, collider=True, tag=None):
        self.model(parent, path, x, y, self.ground.z(x, y), rot, collider=collider, tag=tag)

    def asset_model(self, parent, relative, x, y, scale=1.0, rot=0.0, collider=False, tag=None):
        """A model of the project's own (Models/<relative>.glb, e.g. the Shrine Glade's Architecture/ToriiGate) on the
        ground at (x, y), scaled evenly; collider: a mesh collider (scaled with it)."""
        handle = kit.handle_of(os.path.join(ASSETS, "Models", relative + ".glb.hymeta"))
        tag = tag or self._n(os.path.basename(relative))
        self.w.entity("asset-model:" + tag, tag, parent=parent, t=(x, y, self.ground.z(x, y)), r=(0, 0, math.radians(rot)),
                      s=(scale, scale, scale), body=kit.mesh_body(handle, collider))
        return tag

    def torii(self, parent, tag, x, y, scale=1.0):
        """The Shrine Glade's torii (Architecture/ToriiGate: 5.4 m wide, 5.2 m tall at scale 1), walked through between
        its two posts, which are solid (boxes, as in the Shrine Glade: the beams overhead don't collide)."""
        self.asset_model(parent, "Architecture/ToriiGate", x, y, scale, tag=tag)
        z = self.ground.z(x, y)
        for k, side in enumerate((-1, 1)):
            self.w.entity("torii-post:%s:%d" % (tag, k), "%s Post %d" % (tag, k + 1), parent=parent, t=(x + side * 1.6 * scale, y, z), body=(
                "    BoxColliderComponent:\n      Offset: [0, 0, %g]\n      Size: [%g, %g, %g]\n      Layer: World\n      IsTrigger: false\n"
                % (1.6 * scale, 0.7 * scale, 0.7 * scale, 3.2 * scale)))

    def house(self, parent, name, x, y, rot=0.0, windows=True):
        z = self.ground.z(x, y)
        info = self.building(parent, name, x, y, z, rot)
        if windows and rot == 0.0 and info:
            w = info.get("width", 6.0)
            for k in range(max(1, int(w / 3))):
                self.d.window(parent, x - w / 2 + (k + 0.5) * w / max(1, int(w / 3)), y - 0.6, z + 1.5, 2.6)
        return info

    def light(self, parent, x, y, height, intensity, radius, kind="Lantern"):
        self.d.lantern(parent, x, y, self.ground.z(x, y) + height, intensity, radius, kind)

    def person(self, parent, sheet, name, x, y, clip="IdleDown"):
        """An NPC the player can talk to: tag "Npc <name>" (Npcs.hytable's row, a quest's Giver)."""
        base = os.path.join(ASSETS, "Characters", "NPC", sheet, sheet)
        tex, anim = kit.handle_of(base + ".png.hymeta"), kit.handle_of(base + ".hyanim")
        self.w.entity("npc:" + name, "Npc " + name, parent=parent, t=(x, y, self.ground.z(x, y)), s=(2.5, 2.34375, 1),
                      body=kit.character_body(tex, anim, clip, clip + "_0"))

    def enemy(self, parent, kind, tag, x, y, encounter, home=6.0):
        """A slime or a wisp (EnemyChaseScript; Enemies.hytable by the tag's first word): it keeps its height, so it is
        placed on flat ground well inside its terrace and leashed short."""
        base = os.path.join(ASSETS, "Characters", "Enemy", kind, kind)
        tex, anim = kit.handle_of(base + ".png.hymeta"), kit.handle_of(base + ".hyanim")
        z = self.ground.z(x, y)
        wisp = kind == "Wisp"
        body = sprite_body(tex, emission=(1, 1, 1, 1.5) if wisp else None, sprite=kind + "_0")
        body += ("    SpriteAnimatorComponent:\n      AnimationsHandle: %d\n      StartClip: %s\n      PlayOnStart: true\n      Speed: 1\n"
                 % (anim, "Float" if wisp else "Bounce"))
        if wisp:
            body += ("    Light2DComponent:\n      Type: Point\n      Color: [0.35, 0.85, 1]\n      Intensity: 1.2\n      Radius: 3.2\n"
                     "      Height: 0\n")
        body += ("    Shadow2DComponent:\n      Offset: [0, %g]\n      Size: [%g, %g]\n      Strength: %g\n"
                 "    Rigidbody2DComponent:\n      Type: Dynamic\n      FixedRotation: true\n      IsBullet: false\n      GravityScale: 0\n"
                 "      LinearDamping: 10\n      AngularDamping: 0.05\n"
                 "    BoxCollider2DComponent:\n      Offset: [0, 0.25]\n      Size: [0.55, 0.3]\n      Density: 1\n      Friction: 0.4\n"
                 "      Restitution: 0\n      IsSensor: false\n"
                 "    BoxColliderComponent:\n      Offset: [0, 0.25, 0.8]\n      Size: [0.55, 0.3, 1.6]\n      Layer: World\n      IsTrigger: false\n"
                 "    NativeScriptComponent:\n      Scripts:\n        - Name: EnemyChaseScript\n          Properties:\n"
                 "            AggroRange: %g\n            ChaseSpeed: %g\n            Dormant: false\n"
                 "            Encounter: {Table: %d, Row: %s}\n            Enemy: {Table: 0, Row: \"\"}\n"
                 "            LeashRange: %g\n            MaxFromHome: %g\n            ReturnSpeed: 1.8\n            TouchRange: 0.7\n"
                 "            WanderRadius: %g\n            WanderSpeed: %g\n") % (
            0.05 if wisp else -0.02, 0.8 if wisp else 0.85, 0.45 if wisp else 0.5, 0.32 if wisp else 0.5,
            5.0 if wisp else 4.5, 2.2 if wisp else 2.0, ENCOUNTERS, encounter, home + 2.0, home, 2.5 if wisp else 2.0, 0.8 if wisp else 0.7)
        self.w.entity("enemy:" + tag, tag, parent=parent, t=(x, y, z), s=(1.5, 1.5, 1), body=body)

    def gather(self, parent, item, n, folder, sprite, x, y, scale=1.0):
        """Something to pick up: "Gather <ItemRow> <n>" (FieldDirectorScript gives the item and removes it)."""
        self.card(parent, folder, sprite, x, y, scale, tag="Gather %s %d" % (item, n), sort=1)

    def trigger(self, parent, tag, x, y, size, z=None):
        z = self.ground.z(x, y) if z is None else z
        self.w.entity("trigger:" + tag, tag, parent=parent, t=(x, y, z), body=(
            "    BoxColliderComponent:\n      Offset: [0, 0, 1]\n      Size: [%g, %g, 2]\n      Layer: Trigger\n      IsTrigger: true\n" % size))


ENCOUNTERS = 5637391413083366177     # Assets/Data/Encounters.hytable


# ------------------------------------------------------------------------------------------------------------
# What stands on it

def scatter(spacing, seed, accept):
    """Points about `spacing` apart over the map (a jittered grid), kept where accept(x, y)."""
    out = []
    n = 0
    y = Y0 + spacing / 2
    while y < Y0 + HEIGHT:
        x = X0 + spacing / 2
        while x < X0 + WIDTH:
            n += 1
            px = x + (tm.hash01(seed, n, 1, 3) - 0.5) * spacing * 0.8
            py = y + (tm.hash01(seed, n, 2, 3) - 0.5) * spacing * 0.8
            if accept(px, py):
                out.append((px, py, tm.hash01(seed, n, 3, 3), tm.hash01(seed, n, 4, 3)))
            x += spacing
        y += spacing
    return out


PATH_POINTS = None


def near_path(x, y, width):
    global PATH_POINTS
    if PATH_POINTS is None:
        PATH_POINTS = np.concatenate([catmull_rom(p, spacing=0.5) for p in (ROAD, LANE, CAMP_PATH, TREE_PATH, FOREST_PATH, GROVE_PATH,
                                                                             FOX_PATH, POND_PATH)])
    return float(np.min(np.hypot(PATH_POINTS[:, 0] - x, PATH_POINTS[:, 1] - y))) < width


SECRETS = [(x, y) for (_, x, y) in CHESTS + RIDDLES] + list(CHARMS)


def clear_of(x, y, spots, radius):
    return all(math.hypot(x - p[0], y - p[1]) >= radius for p in spots)


def in_forest(x, y, margin=1.5):
    return x < float(forest_rim(np.array([y]))[0]) - margin or x > float(east_rim(np.array([y]))[0]) + margin


def woods(c):
    """The cedar forest west, the woods east and along the north: tall firs and pines with solid trunks, clearings
    for the camp and the ancient tree, the paths left open."""
    T = c.group("Woods")
    tall = ["FirTallA", "FirTallB", "FirTallC", "FirTallD", "PineTall"]
    small = ["FirSmallA", "FirSmallB", "FirSmallC", "FirSmallD", "FirSmallE", "FirSmallF", "FirSmallG"]

    def accept(x, y):
        mountain = y >= float(mountain_edge(np.array([x]), 0)[0]) + 1.5
        if in_forest(x, y) is False and mountain is False:
            return False
        if abs(x) < 7.5 and y > 82.0:
            return False                                      # the cut, the stairs and the landing stay clear
        if math.hypot(x - CAMP[0], (y - CAMP[1]) * 1.2) < 9.5 or math.hypot(x - TREE[0], y - TREE[1] + 2.0) < 10.5:
            return False
        return (near_path(x, y, 2.6) is False and clear_of(x, y, HERBS, 2.2) and clear_of(x, y, WISPS, 3.0) and clear_of(x, y, SECRETS, 2.4)
                and c.ground.clear(x, y, 2.0, 2.2, 0.6))
    count = 0
    for (x, y, r, s) in scatter(3.3, SEED + 60, accept):
        # On the mountain (out of reach above its cliffs) a third fewer, and no trunks to collide with.
        mountain = y >= float(mountain_edge(np.array([x]), 0)[0]) + 1.5
        if mountain and (r * 7.0) % 1.0 > 0.66:
            continue
        if r < 0.72:
            name = tall[int(s * len(tall)) % len(tall)]
            scale = 1.25 + 0.45 * s if name != "PineTall" else 1.35 + 0.3 * s
        else:
            name = small[int(s * len(small)) % len(small)]
            scale = 1.2 + 0.4 * s
        c.card(T, TREES, name, x, y, scale, solid=None if mountain else (0.7, 0.6, 2.0), flip=r > 0.86)
        count += 1
    # A few broadleaf trees among the fields and along the banks, so the open land isn't bare.
    for (name, x, y, scale) in (("OakSunA", -38.0, -40.0, 1.0), ("OakSunB", 21.0, -44.0, 0.9), ("OakYoungB", -2.5, 12.0, 1.0),
                                ("OakSunC", 22.0, 20.0, 0.9), ("OakGnarledSmall", 4.4, 44.6, 1.1), ("LindenAutumnC", 46.0, 48.0, 1.2),
                                ("OakSunA", 33.0, 68.0, 1.0), ("PineTall", -4.5, 66.4, 1.4), ("PineTall", 5.0, 80.0, 1.3),
                                ("OakSunB", -24.0, 78.0, 1.0), ("LindenAutumnE", 40.0, 78.0, 1.1), ("OakYoungB", 24.0, 50.0, 1.0)):
        c.card(T, TREES, name, x, y, scale, solid=(0.8, 0.7, 2.0))
    return count


def road_side(c):
    """The shrine stairs and their torii, the fox shrine, the pond, the crossroads sign, the south exit."""
    R = c.group("Roadside")
    # The way up to the shrine: one great torii at the foot, the long flight up the cut (StepStone, solid: it walks as a
    # ramp), lanterns at the foot and on the landing, the second flight on up into the mountain (the exit).
    c.torii(R, "Torii Stairs", 0.0, 82.8, 1.3)
    for s in (-1, 1):
        c.place(R, "Props/JP_Prop_StoneLantern_A", s * 4.6, 84.2)
        c.light(R, s * 4.6, 84.2, 1.15, 0.9, 3.0)
        c.card(R, PROPS, "shrine_banner", s * 6.2, 83.6, 0.1, sort=1)
    step = handle("Materials/Ground/StepStone.hymat")
    for (fx, fy0, fy1, lo, hi, fw) in (FLIGHT, FLIGHT2):
        c.stairs(R, step, fx, fy0, fy1, lo * 0.25, hi * 0.25, width=fw, step_rise=0.2)
    lz = LANDING[3] * 0.25
    for s in (-1, 1):
        for y in (117.0, 122.2):
            c.place(R, "Props/JP_Prop_StoneLantern_A", s * 5.6, y)
            c.light(R, s * 5.6, y, 1.15, 1.0, 3.5)
    c.card(R, PROPS, "Jizo", -3.6, 122.6, 1.0, solid=(0.6, 0.5, 1.4))
    c.card(R, PROPS, "Jizo", -4.6, 122.2, 0.9, solid=(0.6, 0.5, 1.4), flip=True)
    c.card(R, PROPS, "shrine_banner", 3.8, 122.6, 0.1, sort=1)
    # The second flight climbs on into the mountain, but Miko stops at its foot: a blocker across it, and the exit's
    # trigger in front of it, where the prompt to go up to the shrine shows ("E  Leave").
    c.blocker(R, -CUT2[0] - 0.2, CUT2[0] + 0.2, LANDING[2] - 0.1, LANDING[2] + 0.6, lz, 4.0)

    # The fox shrine: a short tunnel of small torii up to a little shrine (the offering box), foxes either side.
    F = c.group("Fox Shrine", R)
    fx, fy = FOX
    for k, y in enumerate((31.6, 33.0, 34.4)):
        c.torii(F, "Torii Fox %d" % (k + 1), fx, y, 0.56 - 0.03 * k)
    # the little shrine: the Shrine Glade's hall at a third of its size (2.6 m wide), solid; an offering bowl and a rack
    # of prayer plaques before it
    # Its voice gives the charms' side quest: an NPC to the field director ("Npc Inari", Npcs.hytable's Inari).
    c.asset_model(F, "Architecture/ShrineHall", fx, fy + 3.0, 0.35, collider=True, tag="Npc Inari")
    c.asset_model(F, "Props/OfferingBowl", fx, fy + 1.7, 1.6, tag="Fox Shrine Offering")
    c.asset_model(F, "Props/PrayerPlaqueRack", fx + 1.0, fy + 0.6, 1.0, collider=True, tag="Fox Shrine Plaques")
    c.card(F, PROPS, "KitsuneStatue", fx - 1.8, fy + 1.4, 1.0, solid=(0.7, 0.4, 1.2))
    c.card(F, PROPS, "KitsuneStatue", fx + 1.8, fy + 1.4, 1.0, solid=(0.7, 0.4, 1.2), flip=True)
    for s in (-1, 1):
        c.card(F, PROPS, "shrine_banner", fx + s * 3.0, fy + 1.8, 0.075, sort=1)
        c.place(F, "Props/JP_Prop_StoneLantern_A", fx + s * 2.6, fy - 1.6)
        c.light(F, fx + s * 2.6, fy - 1.6, 1.15, 0.8, 3.0)
    # the red maple behind it, and two more on the verges between the road and the paddies (Octopath's autumn accents)
    for (x, y, scale) in ((fx - 5.5, fy + 5.0, 1.25), (4.3, 33.0, 1.0), (-3.9, 64.0, 1.05)):
        c.card(F, TREES, "LindenAutumnA", x, y, scale, color=(1.0, 0.52, 0.44, 1), solid=(0.8, 0.7, 2.0))

    # The pond and its weeping cherry, a bench to sit by it.
    P = c.group("Pond", R)
    cx, cy, rx, ry = POND
    z_water = -0.75 + 0.6
    c.w.entity("pond-water", "Pond Water", parent=P, t=(cx, cy, z_water), body=(
        "    MeshComponent:\n      Shape: Plane\n      Size: [%g, %g, 0]\n      CastShadows: false\n      Materials:\n"
        "        - MaterialHandle: %d\n" % (2 * rx + 1.0, 2 * ry + 1.0, WATER)))
    c.blocker(P, cx - rx + 0.6, cx + rx - 0.6, cy - ry + 0.6, cy + ry - 0.6, -0.75, 1.6)
    c.card(P, TREES, "SakuraWeeping", cx + 4.5, cy + 5.6, 1.0, solid=(0.9, 0.8, 2.0))
    c.place(P, "Props/JP_Prop_TeaBench_A", cx - 3.0, cy + 5.2)
    for (name, x, y) in (("MossRockA", cx - 8.4, cy + 1.5), ("MossRockB", cx + 6.8, cy - 0.5), ("RootRockA", cx + 1.0, cy + 4.9)):
        c.card(P, PROPS, name, x, y, 1.0, solid=(0.8, 0.5, 0.8))

    # The crossroads: a sign where the lane leaves for the farm and the camp path for the forest.
    c.card(R, PROPS, "NoticeBoard", 3.6, 3.6, 0.75, tag="Sign Crossroads", solid=(1.8, 0.4, 1.6))
    c.card(R, PROPS, "Jizo", -3.2, 3.2, 1.0, solid=(0.6, 0.5, 1.4))
    # Along the road: a handcart resting, a waystone by the south exit, lanterns at the ramps.
    c.place(R, "Props/JP_Prop_Handcart_A", 3.6, -30.0, 15.0)
    c.card(R, PROPS, "mossy_stone_block", 3.2, -45.0, 0.1, solid=(0.9, 0.5, 1.0))
    for (k, xc) in ROAD_RAMPS:
        e = EDGES[k]
        c.place(R, "Props/JP_Prop_StoneLantern_A", xc + 3.0, e + 2.6)
        c.light(R, xc + 3.0, e + 2.6, 1.15, 0.8, 3.0)


def farm(c):
    """The farm hub: houses round a yard, the kura, the well and the persimmon, the garden, the bamboo grove."""
    H = c.group("Farm")
    hx, hy = HUB
    c.house(H, "JP_Poor_RowHouse_A", 28.0, 14.5)
    c.house(H, "JP_Poor_House_A", 42.0, 15.0)
    c.house(H, "JP_Rich_Storehouse_A", 52.0, 14.5, windows=False)
    c.house(H, "JP_Poor_Hut_A", 57.5, 7.0, rot=-90.0, windows=False)
    c.house(H, "JP_Poor_House_B", 57.0, -1.5, rot=-90.0, windows=False)
    c.house(H, "JP_Poor_Hut_B", 22.5, 15.5)
    for (name, x, y, rot) in (("Well_A", 44.5, 5.5, 0), ("Handcart_A", 33.0, 9.6, 30), ("Kamado_A", 46.6, 13.4, 0), ("Barrel_A", 38.3, 13.6, 0),
                              ("WaterJar_A", 40.0, 13.7, 0), ("Buckets_A", 45.4, 4.2, 0), ("LaundryPole_A", 35.0, 12.8, 0),
                              ("RainBarrel_A", 49.0, 13.8, 0), ("Crate_A", 54.5, 12.9, 10), ("Barrel_A", 55.4, 12.6, 0),
                              ("FenceBamboo_2m_A", 47.0, -4.6, 0), ("FenceBamboo_2m_A", 49.0, -4.6, 0), ("FenceBamboo_2m_A", 51.0, -4.6, 0),
                              ("FenceBamboo_2m_A", 53.0, -4.6, 0), ("FenceBamboo_2m_A", 45.9, -3.6, 90), ("FenceBamboo_2m_A", 45.9, -1.6, 90)):
        c.place(H, "Props/JP_Prop_" + name, x, y, rot)
    c.light(H, 46.6, 13.0, 0.6, 1.0, 3.0, "Fire")
    # the persimmon in the yard's corner (an autumn linden, warmer), and a younger one by the lane
    c.card(H, TREES, "LindenAutumnB", 33.0, 2.5, 1.15, color=(1.0, 0.78, 0.55, 1), solid=(0.8, 0.7, 2.0))
    c.card(H, TREES, "LindenAutumnD", 24.0, 9.5, 0.8, color=(1.0, 0.8, 0.6, 1), solid=(0.7, 0.6, 2.0))
    # the vegetable garden south east of the yard: rows, a scarecrow, tools
    rows = ["CabbageRow", "GreensRow", "OnionRow", "TurnipRow", "LettuceRow", "RedCabbageRow"]
    for k in range(6):
        for n in range(2):
            c.card(H, CROPS, rows[(k + n) % len(rows)], 48.6 + n * 3.2, -3.4 + k * 0.95, 1.0)
    for (name, x, y, folder) in (("Scarecrow", 55.4, -1.0, PROPS), ("Wheelbarrow", 53.8, 2.6, PROPS), ("FarmTools", 46.6, 2.2, PROPS),
                                 ("HayBaleRound", 30.0, 10.9, PROPS), ("HayBale", 31.4, 11.4, PROPS), ("Sacks", 50.4, 13.3, PROPS),
                                 ("Produce", 47.8, 12.8, PROPS), ("Firewood", 25.6, 13.6, PROPS), ("WaterTrough", 37.0, 3.0, PROPS)):
        c.card(H, folder, name, x, y, 1.0, solid=(0.6, 0.4, 1.0))
    # windows lit at dusk, and a lantern at the yard's gate
    c.place(H, "Props/JP_Prop_StoneLantern_A", 30.0, 4.0)
    c.light(H, 30.0, 4.0, 1.15, 0.9, 3.0)

    # The bamboo grove up the bank behind the farm: clumps with solid feet, a path through, shoots to gather.
    G = c.group("Bamboo Grove", H)
    bamboo = ["BambooA", "BambooB", "BambooC"]

    def grove(x, y):
        return (41.0 < x < 61.0 and 26.0 < y < 46.0 and near_path(x, y, 1.8) is False and in_forest(x, y, 0.0) is False
                and clear_of(x, y, SHOOTS, 2.0) and clear_of(x, y, SECRETS, 2.2) and c.ground.clear(x, y, 0.8, 2.0, 0.4))
    for (x, y, r, s) in scatter(2.6, SEED + 70, grove):
        c.card(G, TREES, bamboo[int(r * 3) % 3], x, y, 0.95 + 0.2 * s, solid=(0.9, 0.5, 2.0), flip=s > 0.5)
    for n, (x, y) in enumerate(SHOOTS, start=1):
        c.gather(G, "BambooShoot", n, PROPS, "BambooShoot", x, y)


def camp(c):
    """The ronin camp in its clearing: two tarps, the fire, a spear rack and a banner, Jokai and Sanzo, a sentry."""
    C = c.group("Ronin Camp")
    x0, y0 = CAMP
    c.card(C, PROPS, "CampTarp", x0 - 3.6, y0 + 4.2, 1.0, solid=(3.2, 1.6, 2.0))
    c.card(C, PROPS, "CampTarp", x0 + 3.8, y0 + 4.8, 0.9, solid=(2.8, 1.4, 2.0), flip=True)
    c.place(C, "Props/JP_Prop_Brazier_A", x0, y0 + 0.4)
    c.light(C, x0, y0 + 0.4, 1.2, 1.7, 5.5, "Fire")
    for (name, x, y, rot) in (("SpearRack_A", x0 - 6.2, y0 + 0.6, 90), ("WarDrum_A", x0 + 5.6, y0 + 1.6, 0), ("Crate_A", x0 - 5.6, y0 - 2.4, 20),
                              ("Barrel_A", x0 + 6.0, y0 - 1.6, 0), ("TeaBench_A", x0 + 1.6, y0 - 2.6, 0), ("BannerPole_A", x0 - 1.6, y0 + 6.8, 0),
                              ("Kamado_A", x0 + 2.2, y0 + 2.2, 0)):
        c.place(C, "Props/JP_Prop_" + name, x, y, rot)
    z = c.ground.z(x0 - 1.6, y0 + 6.8)
    c.d.sprite(C, "JP_Banner_Nobori_Cloth_A", x0 - 1.6 + 0.45, y0 + 6.72, z + 3.3, hanging=True, sort=2)
    for (name, x, y) in (("Firewood", x0 - 0.6, y0 + 2.8), ("Sacks", x0 - 4.0, y0 + 1.8), ("Bucket", x0 - 2.6, y0 - 1.6)):
        c.card(C, PROPS, name, x, y, 1.0, solid=(0.6, 0.4, 1.0))
    c.card(C, PROPS, "Jizo", -44.6, 6.0, 1.0, solid=(0.6, 0.5, 1.4))               # at the camp path's mouth
    c.person(C, "Monk", "Jokai", x0 - 2.4, y0 + 0.2, "IdleRight")
    c.person(C, "Pilgrim", "Sanzo", x0 + 3.4, y0 + 0.4, "IdleLeft")
    c.person(C, "GateGuard", "Ronin", -45.8, 3.0, "IdleRight")


def people(c):
    """The farm's people (two give side quests), a pilgrim at the fox shrine, a field hand in the paddies."""
    P = c.group("People")
    c.person(P, "Farmer", "Genji", 44.0, 8.6, "IdleDown")
    c.person(P, "FarmWife", "Okiku", 51.6, 0.6, "IdleDown")          # her sheet has only IdleDown
    c.person(P, "OldWoman", "Hatsu", 36.4, 10.8, "IdleDown")
    c.person(P, "TownChild", "Kenta", 34.6, 4.0, "IdleLeft")
    c.person(P, "Pilgrim", "Pilgrim", FOX[0] + 3.2, FOX[1] - 4.8, "IdleUp")
    c.person(P, "Porter", "FieldHand", -4.0, -12.0, "IdleLeft")
    # Passers-by (NpcWanderScript, build_city_slice.Slice.walker): it moves them in x and y only, so each walks a
    # stretch that stays on one terrace: a merchant up and down the road below the first ramp, a townswoman along
    # the lane to the farm.
    c.walker(P, "Merchant", 0.3, -30.0, c.ground.z(0.3, -30.0), "Y", 22.0, 1.1, "IdleUp", pause=(3.0, 6.0), stop=0.6)
    c.walker(P, "Townswoman", 16.0, 6.6, c.ground.z(16.0, 6.6), "X", 16.0, 1.0, "IdleRight", pause=(2.0, 5.0), delay=5.0)


def gathering(c):
    """Medicinal herbs in the forest's undergrowth, by the paths (Items.hytable MedicinalHerb)."""
    G = c.group("Herbs")
    for n, (x, y) in enumerate(HERBS, start=1):
        c.gather(G, "MedicinalHerb", n, CROPS, "HerbBush", x, y, 0.8)


def mysteries(c):
    """Chests to find (the hidden ones hinted at by riddle stones), the stones themselves, and the fox charms."""
    M = c.group("Mysteries")
    sheet = os.path.join(PROPS, "TreasureChest.png.hymeta")         # Chest_0 closed, Chest_1 open (FieldDirectorScript)
    chest = kit.handle_of(sheet)
    for (row, x, y) in CHESTS:
        c.w.entity("chest:" + row, "Chest " + row, parent=M, t=(x, y, c.ground.z(x, y)), s=(1.14286, 1.14286, 1),
                   body=sprite_body(chest, sprite="Chest_0") + trunk(1.14286, 1.14286, 0.9, 0.6, 0.8))
    for (row, x, y) in RIDDLES:
        c.asset_model(M, "Props/MemorialMarkerMediumA", x, y, 1.3, collider=True, tag="Sign " + row)
    for n, (x, y) in enumerate(CHARMS, start=1):
        c.gather(M, "FoxCharm", n, PROPS, "FoxCharm", x, y)


def enemies(c):
    """Slimes in the open meadows east of the road and below the farm, wisps between the cedars."""
    E = c.group("Enemies")
    for n, (x, y, encounter) in enumerate(((12.0, -40.0, "SlimePair"), (14.0, -31.0, "Slime"), (12.0, 0.0, "SlimePair"),
                                           (15.0, 14.5, "Slime"), (38.5, 36.0, "SlimePair"), (34.0, 60.0, "Slime"),
                                           (-22.0, 75.0, "SlimePair")), start=1):
        c.enemy(E, "Slime", "Slime Meadow %d" % n, x, y, encounter, home=5.0)
    for n, (x, y, encounter) in enumerate(WISPS, start=1):
        c.enemy(E, "Wisp", "Wisp Cedars %d" % n, x, y, encounter, home=4.5)


def paddy_water(c):
    """A water plane over each paddy (just below its bunds; the ground hides its edges) and a blocker, so the player
    walks the bunds; the seedlings are terrain decor."""
    W = c.group("Paddies")
    for k, (a, b, cc, d) in enumerate(PADDIES):
        x, y = (a + b) / 2, (cc + d) / 2
        floor = c.ground.z(x, y)
        c.w.entity("paddy:%d" % k, "Paddy %d" % (k + 1), parent=W, t=(x, y, floor + 0.14), body=(
            "    MeshComponent:\n      Shape: Plane\n      Size: [%g, %g, 0]\n      CastShadows: false\n      Materials:\n"
            "        - MaterialHandle: %d\n" % (b - a + 0.6, d - cc + 0.6, WATER)) + (
            "    BoxColliderComponent:\n      Offset: [0, 0, 0.5]\n      Size: [%g, %g, 1.2]\n      Layer: World\n      IsTrigger: false\n"
            % (b - a - 0.3, d - cc - 0.3)))


def edges_and_exits(c):
    X = c.group("Exits")
    c.trigger(X, "Exit JapaneseCitySlice", 0.5, -49.0, (4.0, 2.4))
    c.trigger(X, "Exit ShrineGlade", 0.0, LANDING[2] - 1.0, (4.6, 2.0))          # at the foot of the closed second flight
    c.w.entity("arrive:city", "Arrive JapaneseCitySlice", parent=X, t=(0.5, -45.0, c.ground.z(0.5, -45.0)))
    c.blocker(X, X0, X0 + WIDTH, Y0 - 1.0, Y0 + 0.6, -1.0, 10.0)
    c.blocker(X, X0, X0 + WIDTH, Y0 + HEIGHT - 0.6, Y0 + HEIGHT + 1.0, -1.0, 10.0)
    c.blocker(X, X0 - 1.0, X0 + 0.6, Y0, Y0 + HEIGHT, -1.0, 10.0)
    c.blocker(X, X0 + WIDTH - 0.6, X0 + WIDTH + 1.0, Y0, Y0 + HEIGHT, -1.0, 10.0)


def camera_zones(c):
    """The scene's framing (the director) everywhere, closer and lower at the camp and the tree, the hub framed
    whole, the stairs pulled back."""
    Z = c.group("Camera Zones")

    def zone(name, cx, cy, size, pan_min, pan_max, **kw):
        # CameraZoneScript's PanMin / PanMax are offsets from the zone's centre; here they are given in world metres.
        c.zone(Z, name, cx, cy, size, pan_min=(pan_min[0] - cx, pan_min[1] - cy), pan_max=(pan_max[0] - cx, pan_max[1] - cy), **kw)
    zone("Camp", CAMP[0] + 1.0, CAMP[1] + 1.0, (24.0, 20.0), (CAMP[0] - 4.0, CAMP[1] - 3.0), (CAMP[0] + 6.0, CAMP[1] + 6.0),
         priority=1, tilt=66, fov=30, scale=0.85, lead=1.5, rise=2.6, blend=1.2)
    zone("Ancient Tree", TREE[0] + 2.0, TREE[1] - 3.0, (24.0, 22.0), (TREE[0] - 2.0, TREE[1] - 8.0), (TREE[0] + 8.0, TREE[1] - 1.0),
         priority=1, tilt=64, fov=30, scale=0.95, lead=1.5, rise=3.4, blend=1.4)
    zone("Farm", HUB[0], HUB[1] + 1.0, (40.0, 30.0), (28.0, 0.0), (52.0, 14.0),
         priority=1, tilt=68, fov=30, scale=1.0, lead=2.0, rise=3.0, blend=1.2)
    zone("Shrine Stairs", 0.0, 104.0, (16.0, 44.0), (-2.0, 84.0), (2.0, 124.0),
         priority=1, tilt=64, fov=30, scale=1.05, lead=3.0, rise=3.6, blend=1.0)

def backdrop(c):
    """The far hills and the sky behind the shrine (the city's painted layers; their tiling fits these sizes)."""
    B = c.group("Backdrop")
    for (name, width, hp, wp, y, z) in (("JP_Backdrop_Far", 220.0, 294, 1086, 250.0, 6.0), ("JP_Backdrop_Sky_Dusk", 380.0, 362, 1086, 360.0, -14.0)):
        mat = kit.handle_of(os.path.join(ASSETS, "Materials", "JapaneseCity", name + ".hymat"))
        c.w.entity("backdrop:" + name, "Backdrop " + name.replace("JP_Backdrop_", ""), parent=B, t=(0, y, z), body=(
            "    MeshComponent:\n      Shape: Box\n      Size: [%g, 0.1, %g]\n      CastShadows: false\n      Materials:\n"
            "        - MaterialHandle: %d\n" % (width, width * hp / wp, mat)))


WATER = 0


# ------------------------------------------------------------------------------------------------------------
# The preview map and the scene

def preview_map(g, weights, masks, path, points):
    from PIL import Image, ImageDraw
    scale = 3
    h, w = g.ny - 1, g.nx - 1
    mx, mn = g.cell_max(), g.cell_min()
    img = Image.new("RGB", (w * scale, h * scale))
    px = img.load()
    tint = {GRASS: (1.0, 1.0, 1.0), SHADE: (0.7, 0.82, 0.7), DIRT: (1.3, 1.0, 0.72), MUD: (0.55, 0.75, 1.25)}
    for j in range(h):
        for i in range(w):
            v = int(mx[j, i])
            base = 70 + 7 * max(-3, min(v, 26))
            col = (int(base * 0.75), base, int(base * 0.55))
            if mx[j, i] != mn[j, i]:
                col = {STONE: (60, 58, 56), EARTH: (110, 80, 52), PADDY_BANK: (96, 86, 60), FOREST_BANK: (80, 60, 40)}[int(g.walls[j, i])]
            if g.ramps[j, i]:
                col = (200, 170, 110)
            tx = min(int((i + 0.5) * g.cell * 8), weights.shape[1] - 1)
            ty = min(int((j + 0.5) * g.cell * 8), weights.shape[0] - 1)
            t = tint[int(np.argmax(weights[ty, tx]))]
            col = tuple(min(255, int(col[k] * t[k])) for k in range(3))
            for dy in range(scale):
                for dx in range(scale):
                    px[i * scale + dx, (h - 1 - j) * scale + dy] = col
    d = ImageDraw.Draw(img)
    for (x, y, colour) in points:
        u = (x - g.x0) / g.cell * scale
        v = (h - (y - g.y0) / g.cell) * scale
        d.ellipse([u - 3, v - 3, u + 3, v + 3], outline=colour)
    img.save(path)


def header(scene_handle, profile):
    return (
        "Handle: %d\nScene: JapaneseCountryside\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
        "    SpriteSorting: ViewDepth\n    Lighting: Forward\n    DefaultSpriteAlpha: Cutout\n    Meshes: true\n    Shadows: Sun\n"
        "    SpriteShadows: AsDrawn\n    UprightSprites: Vertical\n    HDR: true\n    Bloom: true\n    DepthOfField: true\n"
        "Look:\n  ProfileHandle: %d\n  Environment: Profile\n  PostProcess: Profile\n"
        "Environment:\n  SkyAmbient: [0.42, 0.36, 0.3]\n  GroundAmbient: [0.16, 0.12, 0.08]\n  Fog: true\n  FogColor: [0.78, 0.6, 0.38]\n"
        "  FogStart: 45\n  FogEnd: 170\nEntities:\n") % (scene_handle, profile)


def main():
    global WATER
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"])
    ap.add_argument("--start", default="north", choices=["south", "north"], help="where the player stands when the scene is played on its own")
    ap.add_argument("--map-only", action="store_true")
    ap.add_argument("--no-extras", action="store_true", help="don't run the field UI and quest builders afterwards")
    args = ap.parse_args()
    WATER = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Water.hymat"))

    g = Grid(X0, Y0, WIDTH, HEIGHT)
    masks = build_levels(g)
    weights = build_weights(g)
    settings = make_settings()
    mesher_settings = tm.Settings(jitter=0.38, lean_px=1.0, wall_noise_px=1.5, lip_px=3.0, lip_face_px=2.0, fringe_px=7.0, caps=True,
                                  wall_variation={STONE: 0.65, EARTH: 0.57, PADDY_BANK: 0.5, FOREST_BANK: 0.57})
    terrain = g.mesher_terrain(mesher_settings, SEED)
    _, problems = tm.ramp_runs(terrain)
    for p in problems:
        print("RAMP PROBLEM:", p)

    out = os.path.join(REPO, "scripts", "terrain", "out")
    os.makedirs(out, exist_ok=True)
    marks = [(CAMP[0], CAMP[1], (255, 60, 60)), (TREE[0], TREE[1], (80, 220, 255)), (HUB[0], HUB[1], (255, 220, 60)),
             (FOX[0], FOX[1], (255, 120, 40)), (0.0, LANDING[2] - 1.0, (255, 255, 255)), (0.5, -49.0, (255, 255, 255))]
    preview_map(g, weights, masks, os.path.join(out, "countryside_map.png"), marks)
    print("map: scripts/terrain/out/countryside_map.png; %d paddies" % len(PADDIES))
    if args.map_only:
        return

    handle_t = hyterrain.read(TERRAIN)[0] if os.path.exists(TERRAIN) else kit.stable_id("terrain-asset")
    os.makedirs(os.path.dirname(TERRAIN), exist_ok=True)
    hyterrain.write(TERRAIN, handle_t, settings, g.levels, g.ramps, g.walls, weights)

    summary = kit.json.load(open(kit.SUMMARY))
    profiles = {k: kit.handle_of(os.path.join(ASSETS, "RenderProfiles", "City", k + ".hyrender")) for k in ("Day", "Dusk", "Night")}
    scene_handle = kit.handle_of(SCENE) if os.path.exists(SCENE) else kit.stable_id("scene")
    c = Builder(summary, Ground(g, terrain))
    w = c.w
    start = {"south": (0.5, -45.0), "north": (0.0, 119.5)}[args.start]
    sz = c.ground.z(*start)
    w.entity("Player", "Player", t=(start[0], start[1], sz), s=(2.5, 2.34375, 3),
             body=kit.character_body(kit.MIKO_TEXTURE, kit.MIKO_ANIM, "IdleDown" if args.start == "north" else "IdleUp",
                                     "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 23.0, sz + 11.0), r=(1.239, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 30\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 320\n      OrthographicSize: 13\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [70, 100]\n            LookAhead: 0.5\n            Smoothing: 6\n"))
    w.entity("Director", "Country Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: JapaneseCountryside\n            TiltDegrees: 71\n            FieldOfView: 30\n"
        "            FocusLead: 2.5\n            FocusRise: 3\n            BoundsMin: [-56, -44]\n            BoundsMax: [56, 124]\n"
        "            YawDegrees: 0\n            ViewScale: 1\n"
        "            DuskSunColor: [1, 0.74, 0.5]\n            DuskSunIntensity: 0.95\n            DuskLanterns: 1.3\n            DuskWindows: 1.2\n"
        "            NightLanterns: 1.6\n            NightWindows: 1.3\n"
        "            StartPreset: %d\n            DayProfile: %d\n            DuskProfile: %d\n            NightProfile: %d\n")
        % (("Day", "Dusk", "Night").index(args.profile), profiles["Day"], profiles["Dusk"], profiles["Night"]))
    w.entity("FieldDirector", "Field Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldDirectorScript\n          Properties:\n"
        "            InteractRange: 1.7\n            MaxReachHeight: 1\n            FacingDegrees: 80\n            CloseRange: 0.6\n"
        "            SwitchMargin: 0.3\n            EyeHeight: 1\n            ItemHeight: 0.6\n            MessageTime: 3\n"
        "            LeaveTime: 0.6\n            ArriveTime: 0.6\n            RestTime: 1.2\n"))
    w.entity("Lighting", "Lighting")
    w.entity("Sun", "Sun", parent="Lighting", t=(-8, -6, 10), r=(1.01229095, 0, -0.663224995), body=(
        "    DirectionalLightComponent:\n      Color: [1, 0.86, 0.7]\n      Intensity: 0.8\n      IsMainLight: true\n      CastShadows: true\n"
        "      ShadowStrength: 0.85\n      ShadowDistance: 55\n      ShadowBias: 1\n"))
    Gr = c.group("Ground")
    w.entity("terrain", "Countryside Terrain", parent=Gr, t=(X0, Y0, 0), body=(
        "    TerrainComponent:\n      TerrainHandle: %d\n      CastShadows: true\n      Layer: World\n" % handle_t))
    paddy_water(c)
    trees = woods(c)
    road_side(c)
    farm(c)
    camp(c)
    people(c)
    gathering(c)
    mysteries(c)
    enemies(c)
    edges_and_exits(c)
    camera_zones(c)
    backdrop(c)

    with open(SCENE, "w", newline="\r\n") as f:
        f.write(header(scene_handle, profiles[args.profile]) + "\n".join(w.lines) + "\n")
    count = sum(1 for line in w.lines if line.startswith("  - Entity:"))
    print("wrote %s (%d entities, %d trees) and %s" % (os.path.relpath(SCENE, REPO), count, trees, os.path.relpath(TERRAIN, REPO)))
    if c.d.missing:
        print("missing (skipped):", sorted(set(c.d.missing)))
    if args.no_extras:
        return
    for script, extra in (("scripts/ui/build_game_ui.py", ["field"]), ("scripts/quests/build_countryside_quest.py", [])):
        print("running", script)
        subprocess.check_call([sys.executable, os.path.join(REPO, script)] + extra, cwd=REPO)


if __name__ == "__main__":
    main()
