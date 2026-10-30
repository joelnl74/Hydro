"""Puts the Shrine Glade on a terrain (Terrain plan TR6 style), with a reworked layout:

  - The ground box, valley plane, terrace and wing boxes, grass and path planes, wall caps, wall colliders and the
    edge dressing they needed are replaced by one terrain: Assets/Terrains/ShrineGlade.hyterrain.
  - The rises stagger instead of running the full width (V9 direction, Octopath Traveler II, Final Fantasy
    Resonance): the plaza's front edge bulges east of the stairs and recedes west behind a low step; the west wing
    rises in one wall, the east wing in two (a 2 m ledge first); both wings fall back at the sides; the shrine
    shelf's back drops to the valley along a wandering earth cliff deep enough for the trees that stood on it.
  - Things that stood at the foot or the top of a moved edge (fences, banners, rocks, canopies) move with it; every
    placed thing keeps its height above the ground it stood on.

    python scripts/terrain/build_shrine_glade.py [--dry-run]
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
from build_farm_village import preview_map  # noqa: E402
from scene_terrain import (ASSETS, Grid, Paint, Scene, field_noise, handle_of, smoothstep, units, wander)  # noqa: E402

SCENE = os.path.join(ASSETS, "Scenes", "ShrineGlade.hyscene")
TERRAIN = os.path.join(ASSETS, "Terrains", "ShrineGlade.hyterrain")
SEED = 2211

X0, Y0, WIDTH, HEIGHT = -40.0, -20.0, 80.0, 54.0

VALLEY = -12   # -3 m
GROUND = 0
STEP = 2       # 0.5 m
PLAZA = 4      # 1 m
LEDGE = 8      # 2 m
SHRINE = 12    # 3 m (was 3.1)

GRASS, SHADE, DIRT, FLAGS = 0, 1, 2, 3
STONE, EARTH, CLIFF = 0, 1, 2

OLD_LEVELS = (-3.0, 0.0, 1.0, 3.1)
STAIRS_HALF = 3.4      # the lower stairs and their cheeks
CORRIDOR_HALF = 1.9    # the upper stairs between the wings


def plaza_front(x):
    """The plaza's front edge (old: y = -3)."""
    x = np.asarray(x, dtype=np.float64)
    f = -2.7 + wander(x, SEED + 1, 0.4, 4.0)
    f = f - 1.1 * np.sin(np.clip((x - 3.4) / 6.8, 0, 1) * math.pi) * ((x > 3.4) & (x < 10.2))
    f = f + (-0.6 - f) * smoothstep(-10.0, -13.0, x)
    f = np.where(np.abs(x) <= STAIRS_HALF, -3.0, f)
    # Over the lower stairs' top step (y -3.0), so the walk up has no gap.
    return np.where(np.abs(x) <= 2.1, -3.45, f)


def wing_front(x):
    """Where the wings rise to the shrine level (old: y = 1)."""
    x = np.asarray(x, dtype=np.float64)
    f = 1.2 + wander(x, SEED + 2, 0.3, 4.0)
    f = np.where((x > 5.0) & (x < 12.5), 2.0 + wander(x, SEED + 3, 0.25, 3.0), f)
    f = f + (3.7 - f) * smoothstep(-11.0, -14.0, x) + (2.8 - f) * smoothstep(13.0, 16.0, x)
    return f


def ledge_front(x):
    """The east wing's 2 m ledge (x 5..12.5), in front of its shrine-level edge."""
    x = np.asarray(x, dtype=np.float64)
    return 0.75 + wander(x, SEED + 4, 0.25, 3.0)


def has_ledge(x):
    return (np.asarray(x) > 5.0) & (np.asarray(x) < 12.5)


def foot_edge(x):
    return np.where(has_ledge(x), ledge_front(x), wing_front(x))


def back_edge(x):
    x = np.asarray(x, dtype=np.float64)
    return np.clip(17.8 + wander(x, SEED + 5, 0.9, 6.0), 17.4, 19.0)


def build_levels(g):
    X, Y = g.X, g.Y
    x = X[0]
    L = g.levels
    L[:] = GROUND

    pf = plaza_front(x)
    wf = wing_front(x)
    lf = ledge_front(x)
    be = back_edge(x)
    corridor = np.abs(x) <= CORRIDOR_HALF
    shrine_start = np.where(corridor, 3.5, wf)  # over the upper stairs' top step (y 3.94)

    L[Y >= pf[None, :]] = PLAZA
    step_front = -3.4 + wander(x, SEED + 6, 0.35, 3.5)
    L[(X < -10.5) & (Y >= step_front[None, :]) & (Y < pf[None, :])] = STEP
    L[(has_ledge(X)) & (Y >= lf[None, :]) & (Y < shrine_start[None, :])] = LEDGE
    L[Y >= shrine_start[None, :]] = SHRINE
    L[Y >= be[None, :]] = VALLEY

    # Scenery beyond the walkable path: low rises at the sides of the glade.
    mound = ((X + 17.0) / 5.5) ** 2 + ((Y + 12.0) / 4.0) ** 2 < 1.0 + 0.1 * np.sin(2.3 * X)
    L[mound & (L == GROUND)] = STEP
    rise = ((X - 16.0) / 6.0) ** 2 + ((Y + 14.0) / 3.5) ** 2 < 1.0 + 0.1 * np.sin(1.9 * Y)
    L[rise & (L == GROUND)] = 1

    mx = g.cell_max()
    mn = g.cell_min()
    g.walls[:] = np.where((mn >= GROUND) & (mx <= SHRINE) & (mx >= PLAZA), STONE, EARTH)
    # The shrine shelf's tall drop to the valley (2 m and more) slopes less than the low banks, so its foot doesn't
    # reach far into the valley.
    g.walls[(g.walls == EARTH) & (mx - mn >= 8)] = CLIFF


def build_weights(g):
    p = Paint(g, resolution=8, layers=4)

    def shade(px, py):
        n = field_noise(px, py, SEED + 20, 5.0)
        return np.clip(smoothstep(0.6, 0.7, n) + smoothstep(10.0, 18.0, np.abs(px)) + smoothstep(16.5, 19.0, py), 0.0, 1.0)
    p.where(shade, SHADE)

    # Worn dirt where people walk and around the plaza's foot.
    # Painted soft (Oct 31) so the blending has room: the grass thins out into the dirt.
    p.road([(0.0, -20.0), (0.3, -15.0), (-0.2, -11.0), (0.0, -6.0)], 3.2, DIRT, wander_m=0.35, seed=31, soft=0.9)
    p.blob((0.0, -4.4), (4.6, 1.0), DIRT, wander_m=0.3, seed=32, soft=0.8)
    p.road([(3.0, -6.0), (5.5, -7.0), (8.0, -6.6)], 1.2, DIRT, seed=33, soft=0.7)
    # Flagstones: the front path, its foot, the plaza, the court in front of the shrine.
    p.road([(0.0, -12.2), (0.0, -8.0), (0.0, -4.7)], 3.6, FLAGS, wander_m=0.12, seed=40)
    p.blob((0.0, -4.95), (3.2, 0.45), FLAGS, wander_m=0.1, seed=41)
    p.blob((0.0, -1.0), (5.6, 1.9), FLAGS, wander_m=0.25, seed=42, corner=1.0)
    p.blob((0.0, 2.4), (1.8, 1.6), FLAGS, wander_m=0.05, seed=43)
    p.blob((0.0, 5.5), (4.4, 1.6), FLAGS, wander_m=0.2, seed=44, corner=0.8)
    return p.to_bytes()


def make_settings():
    s = hyterrain.TerrainSettings()
    s.seed = SEED
    # Blending (Oct 31), as in the Farm Village.
    s.add_layer(handle_of("Materials/Ground/Grass.hymat"), 0.9, handle_of("Materials/Ground/GrassFringe.hymat"), 7.0, blend=0.5, height_blend=1.0)
    s.add_layer(handle_of("Materials/Ground/GrassShade.hymat"), 0.9, handle_of("Materials/Ground/GrassShadeFringe.hymat"), 6.0,
                blend=0.8, height_blend=1.0)
    s.add_layer(handle_of("Materials/Ground/Dirt.hymat"), 0.9, blend=0.5, height_blend=1.0)
    s.add_layer(handle_of("Materials/Ground/Flagstones.hymat"), 0.45, blend=0.3, height_blend=0.8)
    s.add_wall_layer(handle_of("Materials/Ground/StoneWall.hymat"), 0.65, True)
    s.add_wall_layer(handle_of("Materials/Ground/ShrineEarthBank.hymat"), 0.57, False, slope=0.5)
    s.add_wall_layer(handle_of("Materials/Ground/ShrineEarthBank.hymat"), 0.57, False, slope=0.2)   # the drop to the valley
    s.cap_material = handle_of("Materials/Ground/Boulder.hymat")
    # Decor: exactly the sprites of the hand-placed dressing these rules replace (scene_decor), at its sizes
    # (0.84..1.23 of the sprites' own size); nothing the scene didn't already use.
    d = {k: [handle_of(p) for p in v] for k, v in scene_decor.SHRINEGLADE.items()}
    size = (0.84, 1.23)
    s.add_decor("LayerBorder", GRASS, "Upright", 1.2, size, d["Decor Plaza Edge"] + d["Decor Front"])
    s.add_decor("Layer", SHADE, "Upright", 0.12, size, d["Decor Shrine"])
    s.add_decor("WallFoot", hyterrain.ANY_WALL_LAYER, "Upright", 1.2, size, d["Decor Wall Foot"] + d["Decor Plaza Wall"])
    s.add_decor("WallFoot", STONE, "Upright", 0.15, size, d["Decor Wall Bush"])
    s.add_decor("WallTop", STONE, "Upright", 1.0, size, d["Decor Shrine Edge"])
    s.add_decor("WallTop", EARTH, "Upright", 0.7, (0.86, 1.23), d["Decor Hill Edge"])
    s.add_decor("WallTop", CLIFF, "Upright", 0.7, (0.86, 1.23), d["Decor Hill Edge"])
    s.add_decor("WallFoot", STONE, "Stone", 0.9, (0.06, 0.1), [handle_of("Textures/Ground/Boulder.png")])
    return s


def follow_edges(scene, eid, x, y, z):
    """New (x, y) for a thing that stood at the foot or on the top of an edge that moved."""
    if abs(x) > STAIRS_HALF:
        if z < 0.5 and -4.9 < y < -2.4:                 # at the plaza's foot (canopies, rocks, edge trees)
            return x, y + float(plaza_front(x)) + 3.0
        if abs(z - 1.0) < 0.15 and -2.3 < y < -1.4:     # the plaza's front fence
            return x, float(plaza_front(x)) + 1.13
    if abs(x) > CORRIDOR_HALF:
        if abs(z - 1.0) < 0.15 and 0.3 < y < 1.7:       # at the wings' foot (banners, rocks, wall flowers)
            return x, float(foot_edge(x)) - 0.3
        if abs(z - 3.1) < 0.15 and 1.4 < y < 2.7:       # on the wings' edge (fences)
            return x, y + float(wing_front(x)) - 1.0
    return x, y


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    g = Grid(X0, Y0, WIDTH, HEIGHT)
    build_levels(g)
    weights = build_weights(g)
    settings = make_settings()

    scene = Scene.load(SCENE)
    if scene.find("Glade Terrain") and args.dry_run is False:
        # Already converted: the layout is in the terrain, so only the terrain is written again (the scene's things
        # were moved once and must not move twice).
        handle = hyterrain.read(TERRAIN)[0]
        hyterrain.write(TERRAIN, handle, settings, g.levels, g.ramps, g.walls, weights)
        print("the scene is already on its terrain: wrote", TERRAIN, "only")
        return
    terrain_folder = scene.find("Terrain")[0]
    removed_roots = []
    for name in ("Floor", "Terraces", "Walls", "Paths"):
        removed_roots += scene.find(name, parent=terrain_folder)
    decor_folder = scene.find("Decor")
    for name in ("Wall Foot", "Wall Bush", "Plaza Wall", "Plaza Edge", "Shrine Edge", "Hill Edge"):
        for d in decor_folder:
            removed_roots += scene.find(name, parent=d)
    # The path's stepping stones were in Paths: keep them (move them out of the folder being removed).
    paths = scene.find("Paths", parent=terrain_folder)
    kept_stones = [e for p in paths for e in scene.children(p) if scene.tag(e).startswith("Glade Path Stone")]

    fixed = []
    for name in ("Stairs", "Edge Colliders"):
        for f in scene.find(name, parent=terrain_folder):
            fixed += scene.subtree(f)
    lantern_plaza = scene.find("Lantern Plaza") + scene.find("Lantern Plaza 2")
    for lp in lantern_plaza:
        fixed += scene.subtree(lp)
    removed_ids = set()
    for r in removed_roots:
        removed_ids.update(scene.subtree(r))
    removed_ids.difference_update(kept_stones)
    skip = set(scene.find("Sky") + scene.find("Directors") + scene.find("UI") + scene.find("Camera") + fixed) | removed_ids
    roots = [b[0] for b in scene.blocks if scene.parent(b[0]) == 0 and b[0] not in skip]
    placed = [u for u in units(scene, roots) if u not in skip] + kept_stones

    mesher_settings = tm.Settings(jitter=0.38, lean_px=1.0, wall_noise_px=1.5, lip_px=3.0, lip_face_px=2.0, fringe_px=7.0, caps=True,
                                  wall_variation={0: 0.65, 1: 0.57, 2: 0.57})
    terrain = g.mesher_terrain(mesher_settings, SEED)
    moves = []
    report = []
    for u in placed:
        x, y, z = scene.translation(u)
        nx, ny = follow_edges(scene, u, x, y, z)
        base = max([lv for lv in OLD_LEVELS if lv <= z + 0.15] or [z])
        new = g.height_at(terrain, nx, ny)
        nz = new if scene.tag(u) == "Player" else z + (new - base)
        report.append((scene.tag(u), round(x, 2), round(y, 2), round(z, 2), round(ny, 2), round(base, 2), round(new, 2)))
        if abs(nz - z) > 1e-4 or abs(ny - y) > 1e-4:
            moves.append((u, [nx, ny, nz]))

    os.makedirs(os.path.join(HERE, "out"), exist_ok=True)
    preview_map(g, weights, os.path.join(HERE, "out", "shrine_glade_map.png"),
                [(scene.tag(u), *([m[1] for m in moves if m[0] == u] or [scene.translation(u)])[0][:2]) for u in placed])
    with open(os.path.join(HERE, "out", "shrine_glade_moves.txt"), "w") as f:
        for r in sorted(report, key=lambda r: -abs(r[6] - r[5])):
            f.write("%-28s x %7.2f y %7.2f z %6.2f -> y %7.2f  ground %5.2f -> %5.2f\n" % r)
    print("units: %d, moved: %d, removed entities: %d" % (len(placed), len(moves), len(removed_ids)))
    if args.dry_run:
        return

    for u, t in moves:
        scene.set_translation(u, t)
    # The stepping stones go into the Terrain folder before Paths is removed.
    for stone in kept_stones:
        b = scene.block(stone)
        b[1] = b[1].replace("Parent: %d" % scene.parent(stone), "Parent: %d" % terrain_folder, 1)
    scene.remove(removed_ids)

    handle = hyterrain.read(TERRAIN)[0] if os.path.exists(TERRAIN) else hyterrain.new_handle()
    os.makedirs(os.path.dirname(TERRAIN), exist_ok=True)
    hyterrain.write(TERRAIN, handle, settings, g.levels, g.ramps, g.walls, weights)
    if not scene.find("Glade Terrain"):
        eid = hyterrain.new_handle()
        text = ("  - Entity: %d\n    TagComponent:\n      Tag: Glade Terrain\n    RelationshipComponent:\n      Parent: %d\n"
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
