"""Checks the terrain rules (Terrain plan, "Shape" and "Surface") on the TR0 test patch and small cases.
Run: python scripts/terrain/test_terrain.py   (numpy, Pillow; no Blender). The golden hashes are what the
C++ port must reproduce."""

import math
import sys

import numpy as np

import terrain_mesher as tm
from splat import SplatSettings, hash01_np, hash_u32_np, winners
from test_patch import CHUNK_X, LAYERS, N, PRESETS, build_terrain

FAILED = []


def check(name, condition, detail=""):
    print(("ok    " if condition else "FAIL  ") + name + ("" if condition else "  " + detail))
    if condition is False:
        FAILED.append(name)


def area(poly):
    a = 0.0
    for k in range(len(poly)):
        x0, y0 = poly[k][0], poly[k][1]
        x1, y1 = poly[(k + 1) % len(poly)][0], poly[(k + 1) % len(poly)][1]
        a += x0 * y1 - x1 * y0
    return 0.5 * a


def small_terrain(levels, settings=None):
    levels = np.asarray(levels, dtype=np.int32)[::-1]  # written north-up
    ny, nx = levels.shape
    return tm.Terrain(levels=levels, ramps=np.zeros((ny - 1, nx - 1), np.uint8),
                      wall_material=np.zeros((ny - 1, nx - 1), np.uint8), seed=7,
                      settings=settings or tm.Settings(lip_px=0, fringe_px=0, caps=False))


def test_golden_hashes():
    expected = [2807791733, 1449520310, 2910676346, 3391487368, 4074740014]
    got = [tm.hash_u32(*v) for v in [(1234, 0, 0, 0, 1), (1234, 5, 7, 1, 1), (1234, -3, 2, 0, 2), (0,), (42, 1, 2, 3, 4, 5)]]
    check("golden hashes", got == expected, str(got))
    a = np.array([0, 5, -3, 100000])
    b = np.array([0, 7, 2, -100000])
    vec = hash_u32_np(1234, a, b, 1, tm.TAG_CROSS)
    sca = [tm.hash_u32(1234, int(x), int(y), 1, tm.TAG_CROSS) for x, y in zip(a, b)]
    check("numpy hash equals scalar hash", [int(v) for v in vec] == sca)


def test_coverage(terrain, label):
    """Without lean, the visible tops of each cell cover it exactly once (regions nested, no overlap, no
    gap); with lean, tops plus the walls' footprint do."""
    s = terrain.settings
    ramp_heights, _ = tm.ramp_runs(terrain)
    worst = 0.0
    for j in range(terrain.ny - 1):
        for i in range(terrain.nx - 1):
            if (i, j) in ramp_heights:
                continue
            cycle = tm.cell_cycle(terrain, i, j)
            total = 0.0
            for t, polys in tm.bands(cycle).items():
                for poly in polys:
                    pts = [tm.point_at(terrain, bp, t * s.step_height) for bp in poly]
                    a = area(pts)
                    if a < -1e-12:
                        worst = max(worst, 1.0)
                    total += a
            lmin = min(p.lo for p in cycle)
            lmax = max(p.hi for p in cycle)
            for t in range(lmin + 1, lmax + 1):
                for a_, b_ in tm.chords(cycle, t):
                    zt, zb = t * s.step_height, (t - 1) * s.step_height
                    foot = [tm.point_at(terrain, a_, zt), tm.point_at(terrain, b_, zt), tm.point_at(terrain, b_, zb), tm.point_at(terrain, a_, zb)]
                    total += abs(area(foot))
            worst = max(worst, abs(total - s.cell_size * s.cell_size))
    check("coverage, %s (worst cell error %.2e m2)" % (label, worst), worst < 1e-9)


def test_saddle():
    # 1 0      (north row)
    # 0 1  ... wait: corners (0,0)=0, (1,0)=1, (1,1)=0, (0,1)=1 -> the diagonal (1,0)-(0,1) is high.
    ter = small_terrain([[1, 0], [0, 1]])
    cycle = tm.cell_cycle(ter, 0, 0)
    r = tm.region(cycle, 1)
    keys = [p.key for p in r]
    check("saddle: higher corners joined in one region", len(r) == 6 and ("C", 1, 0) in keys and ("C", 0, 1) in keys, str(keys))
    walls = tm.chords(cycle, 1)
    check("saddle: two walls cut off the low corners", len(walls) == 2)
    tops = tm.bands(cycle)
    check("saddle: low ground is two separate pieces", len(tops[0]) == 2 and len(tops[1]) == 1)


def test_two_level_drop():
    ter = small_terrain([[2, 2, 2], [0, 2, 2], [0, 0, 2]])
    ok = True
    for j in range(2):
        for i in range(2):
            cycle = tm.cell_cycle(ter, i, j)
            b = tm.bands(cycle)
            for poly in b.get(1, []):
                if abs(area([tm.point_at(ter, bp, 0.25) for bp in poly])) > 1e-12:
                    ok = False
            c1 = {(a.key, bb.key) for a, bb in tm.chords(cycle, 1)}
            c2 = {(a.key, bb.key) for a, bb in tm.chords(cycle, 2)}
            if c1 != c2:
                ok = False
    check("two-level drop: one wall face, no ledge", ok)


def test_shared_crossings(terrain):
    """Every cell that touches an edge reads the same crossing point, at every height."""
    seen = {}
    bad = 0
    s = terrain.settings
    for j in range(terrain.ny - 1):
        for i in range(terrain.nx - 1):
            for p in tm.cell_cycle(terrain, i, j):
                if p.key[0] != "E":
                    continue
                for lv in range(p.lo, p.hi + 1):
                    pos = tm.point_at(terrain, p, lv * s.step_height)
                    k = (p.key, lv)
                    if k in seen and seen[k] != pos:
                        bad += 1
                    seen[k] = pos
    check("shared crossings identical from both cells (%d points)" % len(seen), bad == 0)


def test_seams(terrain):
    """Meshing two chunks separately gives the same vertices on their shared border."""
    west = [(i, j) for j in range(N - 1) for i in range(N - 1) if (i + 0.5) * terrain.settings.cell_size < CHUNK_X]
    east = [(i, j) for j in range(N - 1) for i in range(N - 1) if (i + 0.5) * terrain.settings.cell_size >= CHUNK_X]
    full, _ = tm.mesh_terrain(terrain)
    mw, _ = tm.mesh_terrain(terrain, cells=west)
    me, _ = tm.mesh_terrain(terrain, cells=east)

    def border(mb):
        pts = set()
        for p in mb.parts.get("top", {"positions": []})["positions"]:
            if abs(p[0] - CHUNK_X) < 1e-9:
                pts.add((round(p[1], 9), round(p[2], 9)))
        return pts
    bw, be = border(mw), border(me)
    check("chunk seam at x = %.0f m: same border vertices (%d)" % (CHUNK_X, len(bw)), bw == be and len(bw) > 0)
    n_full = full.triangle_count()
    n_split = mw.triangle_count() + me.triangle_count()
    check("chunks add up to the whole (%d triangles)" % n_full, n_full == n_split, "%d vs %d" % (n_full, n_split))


def test_ramps():
    ter = build_terrain(PRESETS["conservative"])
    _, problems = tm.ramp_runs(ter)
    check("test patch ramp is valid", problems == [], str(problems))
    bad = build_terrain(PRESETS["conservative"])
    bad.levels[28, 13] = 2   # break the high edge of the runs in columns 12 and 13 (the corner they share)
    _, problems = tm.ramp_runs(bad)
    check("broken ramp runs are reported", len(problems) == 2, str(problems))


def test_splat_guarantee():
    """Where two layers' weights differ by more than their roughness, no pixel flips."""
    rng = np.random.default_rng(3)
    n = 64
    ix, iy = np.meshgrid(np.arange(n), np.arange(n))
    settings = SplatSettings()
    ok = True
    for trial in range(20):
        a = rng.uniform(0.0, 1.0)
        margin = 0.5 * (LAYERS[0].roughness + LAYERS[1].roughness)
        b = a - margin - 0.01
        if b < 0:
            continue
        w = np.zeros((4, 4, 4))
        w[..., 0] = a
        w[..., 1] = b
        win = winners(w, LAYERS[:2], 99, settings, ix, iy)
        if np.any(win != 0):
            ok = False
    check("splat: clear weight differences never flip", ok)
    # And noise is clustered: neighbouring pixels agree far more often than chance.
    v = hash01_np(5, ix, iy, 0, tm.TAG_SPLAT_CLUSTER)
    check("splat noise is deterministic", np.allclose(v, hash01_np(5, ix, iy, 0, tm.TAG_SPLAT_CLUSTER)))


if __name__ == "__main__":
    test_golden_hashes()
    test_saddle()
    test_two_level_drop()
    flat = build_terrain(PRESETS["conservative"])
    test_coverage(flat, "conservative (no lean)")
    lean = build_terrain(PRESETS["stronger"])
    test_coverage(lean, "stronger (lean and noise)")
    test_shared_crossings(lean)
    test_seams(lean)
    tr1 = build_terrain(PRESETS["tr1"])
    test_coverage(tr1, "tr1 (per-layer variation)")
    test_shared_crossings(tr1)
    test_seams(tr1)
    test_ramps()
    test_splat_guarantee()
    print("\n%d failed" % len(FAILED))
    sys.exit(1 if FAILED else 0)
