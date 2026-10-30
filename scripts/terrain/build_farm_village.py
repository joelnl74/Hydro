"""Puts the Farm Village on a terrain (Terrain plan TR4), with a reworked layout:

  - The ground boxes, terrace boxes, wall caps, banks, kit road tiles, path planes and the edge dressing they needed
    (about 300 entities) are replaced by one terrain: Assets/Terrains/FarmVillage.hyterrain.
  - Shelves no longer run the full width at one height (the V9 direction, Octopath Traveler II and Final Fantasy
    Resonance): the square's front edge bulges and recedes, its west end steps down through a low planter shelf, the
    north rise is two steps in the east and one in the west, the back woods sit on their own earth-banked shelf, the
    farm shelf has a ragged outline and a second step in the east, and the stream meanders except under the bridge.
  - Every placed thing keeps its height above the ground it stood on (units move with the new ground).

    python scripts/terrain/build_farm_village.py            # writes the terrain and the scene, and a preview map;
                                                            # once the scene has its terrain, only the terrain
    python scripts/terrain/build_farm_village.py --dry-run  # only the preview map (scripts/terrain/out/farm_village_map.png)
"""

import argparse
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import hyterrain  # noqa: E402
import scene_decor  # noqa: E402
import terrain_mesher as tm  # noqa: E402
from scene_terrain import (ASSETS, Grid, Paint, Scene, catmull_rom, distance_to_polyline, field_noise, handle_of, smoothstep, units,  # noqa: E402
                           wander)

SCENE = os.path.join(ASSETS, "Scenes", "FarmVillage.hyscene")
TERRAIN = os.path.join(ASSETS, "Terrains", "FarmVillage.hyterrain")
SEED = 4127

X0, Y0, WIDTH, HEIGHT = -40.0, -56.0, 80.0, 102.0

# Levels (0.25 m steps).
STREAM = -3
FIELD = 0
MEADOW = 1
LOW = 2       # the planter shelf west of the square
SQUARE = 4    # 1.0 m, as before
LEDGE = 6     # the east step below the north shelf
NORTH = 9     # 2.25 m (was 2.2)
WOODS = 13    # 3.25 m, the back woods
FARM = 4
FARM_HIGH = 6

# Layers and wall layers.
GRASS, SHADE, DIRT, FLAGS = 0, 1, 2, 3
STONE, EARTH, WOODS_BANK = 0, 1, 2


def old_ground(x, y):
    """The old scene's ground height at (x, y) (its boxes' tops)."""
    if 11.0 <= y:
        return 2.2
    if -1.0 <= y < 11.0:
        return 1.0
    if 4.05 <= x and -21.95 <= y <= -14.6:
        return 1.0
    return 0.0


OLD_LEVELS = (0.0, 1.0, 2.2)


def build_levels(g):
    X, Y = g.X, g.Y
    L = g.levels
    L[:] = FIELD
    ax = np.abs(X)

    # The stream: meanders except at the bridge (|x| < 4), 2.5 m wide.
    centre = -12.0 + wander(X[0], SEED + 1, 0.35, 7.0) * smoothstep(3.5, 7.0, np.abs(X[0]))
    half = 1.25 + 0.15 * np.sin(0.45 * X[0] + 0.7)
    L[np.abs(Y - centre[None, :]) < half[None, :]] = STREAM

    # The square (1 m). Its front edge is fixed at the stairs (y = -1) and staggers elsewhere, so the rise doesn't run
    # the whole width at one line: it bulges south east of the stairs (an overlook above the greens), sits in front
    # of the inn, and recedes far north at both ends, where lower steps take over (a 0.5 m planter shelf in the
    # west, a one-course meadow step in the east).
    x = X[0]
    front = -1.4 + wander(x, SEED + 2, 0.6, 5.0)
    front = front - 2.0 * np.sin(np.clip((x - 3.5) / 9.0, 0, 1) * math.pi) * ((x > 3.5) & (x < 12.5))
    front = front + (3.0 - front) * smoothstep(-13.0, -16.0, x)
    front = front + (2.6 - front) * smoothstep(14.0, 17.0, x)
    front = np.where(np.abs(x) <= 2.3, -1.45, front)  # over the stairs' top step (y -1.0), so the walk has no gap
    north_edge = 11.0 + wander(x, SEED + 3, 0.45, 5.0) * smoothstep(3.0, 5.0, np.abs(x))
    north_edge = north_edge + 1.8 * smoothstep(-13.0, -17.0, x) + 2.6 * smoothstep(17.0, 21.0, x)
    north_edge = np.where(np.abs(x) <= 2.0, 10.55, north_edge)  # over the north stairs' top step (y 11.0)
    L[(Y >= front[None, :]) & (Y < north_edge[None, :])] = SQUARE
    low_front = -2.6 + wander(x, SEED + 4, 0.35, 4.0)
    L[(X < -12.5) & (Y >= low_front[None, :]) & (Y < front[None, :])] = LOW
    meadow_front = -3.4 + wander(x, SEED + 10, 0.5, 4.0)
    L[(X > 13.5) & (Y >= meadow_front[None, :]) & (Y < front[None, :])] = 1

    # The north shelf (2.25 m). East of the north stairs' lamp a 1.5 m step lies in front of it, growing from 1 m to
    # 3 m deep, so the rise is two walls there; in the west the square first rises onto its own 1.5 m step.
    ledge_back = north_edge + 1.0 + 2.0 * smoothstep(15.5, 20.0, x)  # 1 m in front of the barn (its front is at y 13.4)
    ledge = (X > 3.4) & (Y >= north_edge[None, :]) & (Y < ledge_back[None, :])
    L[(Y >= north_edge[None, :]) & (ledge == False)] = NORTH  # noqa: E712
    L[ledge] = LEDGE
    west_step = (X < -13.0) & (Y >= north_edge[None, :] - 1.8) & (Y < north_edge[None, :])
    L[west_step & (L == SQUARE)] = LEDGE

    # The back woods (3.25 m, earth banks): behind the exit path, its front wandering 23..27 m.
    woods_edge = 25.0 + wander(X[0], SEED + 5, 2.2, 9.0)
    woods_edge = np.maximum(woods_edge, 23.4 + 1.2 * smoothstep(6.0, 1.0, np.abs(X[0])))
    L[Y >= woods_edge[None, :]] = WOODS

    # The farm shelf (1 m) around the cottage: a ragged outline, fixed where the farm stairs meet it (x 8..10,
    # y = -22), a second step (1.5 m) east of x = 20.
    farm_south = -22.0 + wander(X[0], SEED + 6, 0.5, 4.0) * smoothstep(1.5, 3.0, np.abs(X[0] - 9.0))
    farm_south = np.where(np.abs(X[0] - 9.0) <= 1.2, -22.45, farm_south)  # over the farm stairs' top step (y -22.0)
    farm_north = -14.2 + wander(X[0], SEED + 7, 0.25, 5.0)  # the cottage's back is at y -14.8
    farm_west = 4.2 + wander(Y[:, 0], SEED + 8, 0.5, 3.0)
    farm = (X >= farm_west[:, None]) & (Y >= farm_south[None, :]) & (Y <= farm_north[None, :])
    L[farm] = FARM
    high = farm & (X > 20.0 + wander(Y[:, 0], SEED + 9, 1.0, 4.0)[:, None]) & (Y > -20.5)
    L[high] = FARM_HIGH

    # Scenery beyond the walkable strip: a low earth mound in the south west, a rise in the south east.
    mound = ((X + 27.0) / 7.0) ** 2 + ((Y + 33.0) / 5.0) ** 2 < 1.0 + 0.12 * np.sin(2.0 * X)
    L[mound & (L == FIELD)] = 3
    rise = ((X - 29.0) / 8.0) ** 2 + ((Y + 42.0) / 6.0) ** 2 < 1.0 + 0.1 * np.sin(1.7 * Y)
    L[rise & (L == FIELD)] = 2

    # Walls: stone for the village's built shelves, earth banks for the stream, the woods and the scenery.
    mx = g.cell_max()
    mn = g.cell_min()
    stone = np.isin(mx, (MEADOW, LOW, SQUARE, LEDGE, NORTH)) | ((mx == FARM_HIGH) & (mn >= FIELD))
    g.walls[:] = np.where(stone & (mn >= FIELD), STONE, np.where(mn <= STREAM, EARTH, WOODS_BANK))


def patches(px, py, seed, scale):
    """Organic 0..1 noise for wear patches: two octaves of value noise on rotated, warped coordinates, so the
    patches don't line up with the noise lattice (one octave on the plain grid gives squares and diamonds)."""
    wx = px + 0.35 * scale * (field_noise(px, py, seed + 7, scale * 0.8) - 0.5)
    wy = py + 0.35 * scale * (field_noise(px, py, seed + 8, scale * 0.8) - 0.5)
    a = math.radians(31.0)
    b = math.radians(-58.0)
    n1 = field_noise(wx * math.cos(a) - wy * math.sin(a), wx * math.sin(a) + wy * math.cos(a), seed, scale)
    n2 = field_noise(wx * math.cos(b) - wy * math.sin(b), wx * math.sin(b) + wy * math.cos(b), seed + 1, scale * 0.45)
    return 0.65 * n1 + 0.35 * n2


def build_weights(g):
    p = Paint(g, resolution=8, layers=4)

    # Darker grass: soft patches everywhere, more towards the sides and under the woods.
    def shade(px, py):
        n = patches(px, py, SEED + 20, 6.0)
        sides = smoothstep(16.0, 26.0, np.abs(px))
        woods = smoothstep(22.0, 27.0, py)
        return np.clip(smoothstep(0.62, 0.72, n) + sides + woods, 0.0, 1.0)
    p.where(shade, SHADE)

    # Roads and paths (the Path brush's curve and wandering edges). Painted soft (about a metre from grass to dirt)
    # so the terrain's blending has room: grass thins out into the dirt instead of ending at a cut.
    road = dict(soft=0.9)
    yard = dict(soft=1.1)
    p.road([(0.0, -9.6), (0.2, -6.5), (-0.2, -4.0), (0.0, -1.2)], 2.2, DIRT, seed=31, **road)                       # the lane up to the square
    p.road([(0.0, -14.4), (0.4, -18.0), (-0.3, -22.0), (0.5, -26.0), (0.0, -31.0), (-0.8, -36.0), (0.0, -42.0), (0.6, -50.0), (0.0, -56.0)],
           2.4, DIRT, seed=32, **road)                                                                               # the road south
    p.road([(0.4, -24.6), (4.0, -24.9), (7.0, -24.1), (9.0, -23.6)], 1.8, DIRT, seed=33, **road)                      # to the farm stairs
    p.road([(9.5, -24.6), (13.0, -25.2), (17.0, -24.8), (24.0, -25.8), (32.0, -25.0), (40.0, -26.0)], 2.0, DIRT, seed=34, **road)
    p.road([(0.0, 11.2), (0.4, 14.5), (-0.2, 18.0), (0.0, 21.0), (0.3, 23.5)], 2.2, DIRT, seed=35, **road)          # up to the exit
    p.blob((0.0, 13.4), (7.2, 1.4), DIRT, seed=36, **yard)                                                           # the north yard
    p.blob((9.0, 12.8), (3.6, 1.5), DIRT, seed=37, **yard)                                                           # the barn yard
    p.blob((9.5, -19.6), (4.5, 2.2), DIRT, seed=38, **yard)                                                          # the cottage yard
    p.road([(-14.0, -12.0), (-6.0, -11.8), (6.0, -12.1), (14.0, -11.9)], 2.9, DIRT, wander_m=0.3, seed=39)   # the stream bed

    # Wear (Octopath's farm lanes): grass holding on in the middle of the road south and in patches of the yards,
    # trodden dirt spilling into the grass beside the lanes. Partial weights, so the blending shows them as thin grass
    # or bare patches rather than hard islands.
    centre_line = catmull_rom([(0.0, -14.4), (0.4, -18.0), (-0.3, -22.0), (0.5, -26.0), (0.0, -31.0), (-0.8, -36.0), (0.0, -42.0),
                               (0.6, -50.0), (0.0, -56.0)])

    def middle_strip(px, py):
        d = distance_to_polyline(px, py, centre_line)
        return (1.0 - smoothstep(0.15, 0.45, d)) * smoothstep(-15.0, -17.0, py) * smoothstep(0.42, 0.6, patches(px, py, SEED + 50, 3.0))
    p.where(middle_strip, GRASS, 0.62)

    def yard_patches(px, py):
        # Soft elliptical yards (not rectangles), fading out well before their edges.
        near = np.zeros_like(px)
        for (cx, cy), (hx, hy) in (((0.0, 13.4), (7.2, 1.4)), ((9.0, 12.8), (3.6, 1.5)), ((9.5, -19.6), (4.5, 2.2))):
            r = np.hypot((px - cx) / hx, (py - cy) / hy)
            near = np.maximum(near, 1.0 - smoothstep(0.45, 0.85, r))
        return near * smoothstep(0.52, 0.68, patches(px, py, SEED + 51, 2.2))
    p.where(yard_patches, GRASS, 0.58)

    def trodden(px, py):
        lane_y = np.maximum(smoothstep(-10.5, -9.0, py) * smoothstep(-0.5, -2.0, py), smoothstep(10.5, 12.0, py) * smoothstep(24.5, 23.0, py))
        lanes = (1.0 - smoothstep(2.5, 3.8, np.abs(px))) * lane_y
        return lanes * smoothstep(0.56, 0.7, patches(px, py, SEED + 52, 1.8))
    p.where(trodden, DIRT, 0.55)

    # The square's flagstones and the inn's path (cleaner edges: flagstones have half the roughness).
    p.blob((0.0, 4.4), (8.2, 4.9), FLAGS, wander_m=0.35, seed=40, corner=1.2)
    p.blob((-6.8, 2.4), (2.6, 1.0), FLAGS, wander_m=0.2, seed=41)
    p.road([(0.0, -1.2), (0.0, 0.2)], 2.4, FLAGS, seed=42)
    return p.to_bytes()


def make_settings():
    s = hyterrain.TerrainSettings()
    s.seed = SEED
    fringe = handle_of("Materials/Ground/FarmGrassFringe.hymat")
    # Blending (Oct 31): grass and dirt mix over the soft painted borders, the grass texture's blades reaching into
    # the dirt; the two grass shades fade into each other; flagstones keep a firmer edge. The paths use the shrine's
    # dirt texture warmed up (VillageDirt): FarmDirt's large orange blocks read as a stamp at the game camera.
    s.add_layer(handle_of("Materials/Ground/FarmGrass.hymat"), 0.9, fringe, 7.0, blend=0.5, height_blend=1.0)
    s.add_layer(handle_of("Materials/Ground/FarmGrassShade.hymat"), 0.9, handle_of("Materials/Ground/FarmGrassShadeFringe.hymat"), 6.0,
                blend=0.8, height_blend=1.0)
    s.add_layer(handle_of("Materials/Ground/VillageDirt.hymat"), 0.9, blend=0.5, height_blend=1.0)
    s.add_layer(handle_of("Materials/Ground/Flagstones.hymat"), 0.45, blend=0.3, height_blend=0.8)
    # Stone walls stand upright; the earth banks slope back (Octopath's grassy banks), steeper than walkable.
    s.add_wall_layer(handle_of("Materials/Ground/StoneWall.hymat"), 0.65, True)
    s.add_wall_layer(handle_of("Materials/Ground/EarthBank.hymat"), 0.57, False, slope=0.5)
    s.add_wall_layer(handle_of("Materials/Ground/EarthBank.hymat"), 0.57, False, slope=0.45)   # the woods and scenery banks
    s.cap_material = handle_of("Materials/Ground/Boulder.hymat")
    # Decor: exactly the sprites of the hand-placed dressing these rules replace (scene_decor), at its sizes
    # (0.84..1.23 of the sprites' own size); nothing the scene didn't already use.
    d = {k: [handle_of(p) for p in v] for k, v in scene_decor.FARMVILLAGE.items()}
    size = (0.84, 1.23)
    s.add_decor("LayerBorder", GRASS, "Upright", 1.6, size, d["Decor Path Edge"])                       # grass meeting paths
    s.add_decor("Layer", SHADE, "Upright", 0.12, size, d["Decor North Edge"] + d["Decor Field Edge"] + d["Decor Square Edge"])
    s.add_decor("WallFoot", hyterrain.ANY_WALL_LAYER, "Upright", 1.2, size, d["Decor Wall Foot"] + d["Decor Wall Foot North"])
    s.add_decor("WallTop", STONE, "Upright", 0.18, size, d["Decor Wall Bush"])                          # the odd bush on walls
    s.add_decor("WallTop", EARTH, "Upright", 1.2, size, d["Decor Stream Bank"])                         # the stream's banks
    s.add_decor("WallTop", WOODS_BANK, "Upright", 0.7, size, d["Decor Forest Edge"])                    # the woods' edge
    s.add_decor("WallFoot", STONE, "Stone", 0.9, (0.06, 0.1), [handle_of("Textures/Ground/Boulder.png")])  # loose cap stones
    return s


def preview_map(g, weights, path, unit_points):
    from PIL import Image, ImageDraw
    scale = 8
    h, w = g.ny - 1, g.nx - 1
    colours = {STREAM: (70, 110, 160), FIELD: (120, 160, 80), MEADOW: (128, 165, 84), LOW: (135, 170, 90), 3: (130, 165, 85), SQUARE: (160, 185, 110),
               LEDGE: (175, 195, 120), NORTH: (190, 205, 135), WOODS: (95, 130, 70)}
    img = Image.new("RGB", (w * scale, h * scale))
    px = img.load()
    mx = g.cell_max()
    mn = g.cell_min()
    layer_tint = {GRASS: (1.0, 1.0, 1.0), SHADE: (0.8, 0.85, 0.8), DIRT: (1.25, 0.95, 0.7), FLAGS: (1.2, 1.2, 1.25)}
    for j in range(h):
        for i in range(w):
            c = colours.get(int(mx[j, i]), (150, 150, 150))
            if mx[j, i] != mn[j, i]:
                c = (60, 55, 50) if g.walls[j, i] == STONE else (95, 70, 45)
            tx = min(int((i + 0.5) * g.cell * 8), weights.shape[1] - 1)
            ty = min(int((j + 0.5) * g.cell * 8), weights.shape[0] - 1)
            k = int(np.argmax(weights[ty, tx]))
            t = layer_tint[k]
            c = tuple(min(255, int(c[n] * t[n])) for n in range(3))
            for dy in range(scale):
                for dx in range(scale):
                    px[i * scale + dx, (h - 1 - j) * scale + dy] = c
    draw = ImageDraw.Draw(img)
    for name, x, y in unit_points:
        u = (x - g.x0) / g.cell * scale
        v = (h - (y - g.y0) / g.cell) * scale
        draw.ellipse([u - 3, v - 3, u + 3, v + 3], outline=(255, 40, 40))
    # The walkable strip.
    for x in (-14.2, 14.2):
        u = (x - g.x0) / g.cell * scale
        draw.line([(u, 0), (u, h * scale)], fill=(255, 255, 0))
    img.save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    g = Grid(X0, Y0, WIDTH, HEIGHT)
    build_levels(g)
    weights = build_weights(g)
    settings = make_settings()

    scene = Scene.load(SCENE)
    if scene.find("Village Terrain") and args.dry_run is False:
        # Already converted: the layout is in the terrain, so only the terrain is written again (the scene's things
        # were moved once and must not move twice).
        handle = hyterrain.read(TERRAIN)[0]
        hyterrain.write(TERRAIN, handle, settings, g.levels, g.ramps, g.walls, weights)
        print("the scene is already on its terrain: wrote", TERRAIN, "only")
        return
    by_tag = lambda t: scene.find(t)  # noqa: E731
    terrain_folder = by_tag("Terrain")[0]
    removed_roots = []
    for name in ("Floor", "Terraces", "Walls", "Banks", "Roads"):
        removed_roots += scene.find(name, parent=terrain_folder)
    removed_roots += by_tag("Stream Bed")
    edge_dressing = ("Decor Path Edge", "Decor Wall Foot", "Decor Wall Foot North", "Decor Square Edge", "Decor North Edge",
                     "Decor Field Edge", "Decor Stream Bank", "Decor Wall Bush", "Decor Forest Edge")
    for b in list(scene.blocks):
        tag = scene.tag(b[0])
        base = tag.rstrip("0123456789").rstrip()
        if base in edge_dressing:
            removed_roots.append(b[0])

    # Units: the placed things, which keep their height above the ground they stood on.
    # The stream, stairs and edge colliders stay where they are: they span heights or belong to fixed edges.
    fixed = []
    for name in ("Stream", "Stairs", "Edge Colliders"):
        for f in scene.find(name, parent=terrain_folder):
            fixed += scene.subtree(f)
    skip = set(by_tag("Sky") + by_tag("Directors") + by_tag("UI") + by_tag("Camera") + removed_roots + fixed)
    roots = [b[0] for b in scene.blocks if scene.parent(b[0]) == 0 and b[0] not in skip]
    placed = [u for u in units(scene, roots) if u not in skip]
    removed_ids = set()
    for r in removed_roots:
        removed_ids.update(scene.subtree(r))
    placed = [u for u in placed if u not in removed_ids]

    mesher_settings = tm.Settings(jitter=0.38, lean_px=1.0, wall_noise_px=1.5, lip_px=3.0, lip_face_px=2.0, fringe_px=7.0, caps=True,
                                  wall_variation={0: 0.65, 1: 0.57, 2: 0.57})
    terrain = g.mesher_terrain(mesher_settings, SEED)
    moves = []
    report = []
    for u in placed:
        t = scene.translation(u)
        x, y, z = t
        # The old ground it stood on: the highest old level at or below it, within half a metre (fences and walls
        # stood on box edges).
        ground_old = max(old_ground(x + dx, y + dy) for dx in (-0.5, 0.0, 0.5) for dy in (-0.5, 0.0, 0.5))
        base = max([lv for lv in OLD_LEVELS if lv <= min(z, ground_old) + 0.05] or [min(z, 0.0)])
        new = g.height_at(terrain, x, y)
        report.append((scene.tag(u), round(x, 2), round(y, 2), round(z, 2), round(base, 2), round(new, 2)))
        if scene.tag(u) == "Player":
            moves.append((u, [x, y, new]))
        elif abs(new - base) > 1e-4:
            moves.append((u, [x, y, z + (new - base)]))

    os.makedirs(os.path.join(HERE, "out"), exist_ok=True)
    points = [(scene.tag(u), scene.translation(u)[0], scene.translation(u)[1]) for u in placed]
    preview_map(g, weights, os.path.join(HERE, "out", "farm_village_map.png"), points)
    print("units: %d, moved: %d, removed entities: %d" % (len(placed), len(moves), len(removed_ids)))
    with open(os.path.join(HERE, "out", "farm_village_moves.txt"), "w") as f:
        for r in sorted(report, key=lambda r: -abs(r[5] - r[4])):
            f.write("%-28s x %7.2f y %7.2f z %6.2f  ground %5.2f -> %5.2f\n" % r)
    if args.dry_run:
        return

    for u, t in moves:
        scene.set_translation(u, t)
    scene.remove(removed_ids)

    # The water plane: wide enough for the meander (the ground hides it outside the channel).
    for water in by_tag("Stream Water"):
        b = scene.block(water)
        b[1] = b[1].replace("Size: [80, 2.4000001, 0]", "Size: [80, 3.6, 0]")

    # The terrain entity, first in the Terrain folder.
    handle = None
    if os.path.exists(TERRAIN):
        handle = hyterrain.read(TERRAIN)[0]
    handle = handle or hyterrain.new_handle()
    os.makedirs(os.path.dirname(TERRAIN), exist_ok=True)
    hyterrain.write(TERRAIN, handle, settings, g.levels, g.ramps, g.walls, weights)
    existing = [b[0] for b in scene.blocks if scene.tag(b[0]) == "Village Terrain"]
    if not existing:
        eid = hyterrain.new_handle()
        text = ("  - Entity: %d\n    TagComponent:\n      Tag: Village Terrain\n    RelationshipComponent:\n      Parent: %d\n"
                "    TransformComponent:\n      Translation: [%g, %g, 0]\n      Rotation: [0, 0, 0]\n      Scale: [1, 1, 1]\n"
                "    TerrainComponent:\n      TerrainHandle: %d\n      CastShadows: true\n      Layer: World\n") % (eid, terrain_folder, X0, Y0, handle)
        for k, b in enumerate(scene.blocks):
            if b[0] == terrain_folder:
                scene.blocks.insert(k + 1, [eid, text])
                break
    scene.save(SCENE)
    print("wrote", TERRAIN, "and", SCENE)


if __name__ == "__main__":
    main()
