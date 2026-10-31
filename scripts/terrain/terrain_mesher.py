"""Reference mesher for the terraced terrain (Terrain plan, TR0).

This is the reference the C++ TerrainMesher (TR1 to TR3) follows: same data, same hashes, same rules.
Pure Python + numpy, no Blender. Coordinates: Z up, metres, X east, Y north, corner (i, j) at
(i * CellSize, j * CellSize). Arrays are indexed [j, i].

The rules (see the plan, "Shape"):
  - Corners hold whole-step levels, cells hold ramp directions and wall materials.
  - Every grid edge whose corners differ has ONE crossing point, keyed by the edge's canonical ID
    (i, j, H) for corner (i, j) -> (i + 1, j) or (i, j, V) for (i, j) -> (i, j + 1). It is a pure function
    of that ID, so every cell and chunk that touches the edge reads the same point. Every level's outline
    uses it, so a drop of several levels is one wall face.
  - Region(>= t) in a cell is the cell boundary walked counter-clockwise, keeping the corners at t or
    higher and the crossing points of edges with a corner at t or higher. Its vertices lie on the cell's
    boundary in order, so it is convex, saddles connect the inside corners, and regions are nested.
  - The visible top of level t is Region(>= t) - Region(>= t + 1); walls are the chords of each region.
  - Wall lean and noise move a crossing point ALONG ITS GRID EDGE towards the lower corner, as a function of
    the edge ID and the height only, so tops and walls of neighbouring cells stay watertight. Each course
    steps out by the lean plus a random 0..noise, so a wall never undercuts.
"""

import math
from dataclasses import dataclass, field

import numpy as np

ART_PPM = 32  # art pixels per metre

# Hash tags, so the different uses of the hash never correlate.
TAG_CROSS = 1
TAG_WALL_NOISE = 2
TAG_CAP = 3
TAG_SPLAT_CLUSTER = 4
TAG_SPLAT_PIXEL = 5
TAG_FRINGE = 6

EDGE_H = 0  # (i, j) -> (i + 1, j)
EDGE_V = 1  # (i, j) -> (i, j + 1)

RAMP_NONE, RAMP_NORTH, RAMP_EAST, RAMP_SOUTH, RAMP_WEST = 0, 1, 2, 3, 4

WALL_HIDDEN = 7  # WallMaterial value: collision only, not drawn

M32 = 0xFFFFFFFF


def hash_u32(*values):
    """32-bit hash of a sequence of integers (negative values wrap as uint32). C++: the same steps on
    uint32_t."""
    h = 0x811C9DC5
    for v in values:
        h ^= int(v) & M32
        h = (h * 0x01000193) & M32
        h ^= h >> 15
        h = (h * 0x2C1B3C6D) & M32
        h ^= h >> 12
        h = (h * 0x297A2D39) & M32
        h ^= h >> 15
    return h


def hash01(*values):
    return hash_u32(*values) / 4294967296.0


@dataclass
class Settings:
    cell_size: float = 0.5
    step_height: float = 0.25
    jitter: float = 0.3          # crossing point = edge midpoint +- jitter * cell (0 .. 0.45)
    jitter_smooth: float = 0.75  # share of the jitter from a smooth field (the rest per edge)
    jitter_scale: float = 2.0    # metres between the smooth field's lattice points
    lean_px: float = 0.0         # wall lean back into the upper shelf, art pixels per metre of wall height
    wall_noise_px: float = 0.0   # wall vertex in/out noise, art pixels
    lip_px: float = 2.0          # how far a shelf's top sticks out over its wall, art pixels
    lip_face_px: float = 2.0     # the lip's front face, art pixels high
    fringe_px: float = 4.0       # cut-out grass hanging from the lip, art pixels long (0 = none)
    wall_variation: dict = field(default_factory=dict)  # wall layer -> 0..1 scale of jitter and wall noise
    caps: bool = True
    cap_pitch: float = 0.28            # metres of wall top per coping stone
    cap_gap: float = 0.015             # joint between stones, metres
    cap_size_jitter: float = 0.04      # stones up to this much shorter, metres
    cap_depth: float = 0.2             # across the wall, metres
    cap_height: tuple = (0.06, 0.08)   # metres
    cap_jitter: float = 0.015          # position jitter along and across, metres
    cap_yaw_degrees: float = 3.0       # rotation jitter
    cap_missing: float = 0.08          # chance a slot is empty


@dataclass
class Terrain:
    levels: np.ndarray               # int, [ny, nx] corners
    ramps: np.ndarray                # uint8, [ny - 1, nx - 1] cells
    wall_material: np.ndarray        # uint8, [ny - 1, nx - 1] cells (0..3, WALL_HIDDEN)
    seed: int = 1234
    settings: Settings = field(default_factory=Settings)

    @property
    def nx(self):
        return self.levels.shape[1]

    @property
    def ny(self):
        return self.levels.shape[0]

    @property
    def width(self):
        return (self.nx - 1) * self.settings.cell_size

    @property
    def height(self):
        return (self.ny - 1) * self.settings.cell_size

    def level(self, i, j):
        return int(self.levels[j, i])


# --------------------------------------------------------------------------------------------------------
# Crossing points

def edge_corners(orient, i, j):
    if orient == EDGE_H:
        return (i, j), (i + 1, j)
    return (i, j), (i, j + 1)


def smooth_noise(seed, x, y, scale, tag):
    """Value noise 0..1 over world metres, lattice every `scale` metres, smoothstep between lattice points."""
    gx, gy = x / scale, y / scale
    x0, y0 = math.floor(gx), math.floor(gy)
    fx, fy = gx - x0, gy - y0
    fx = fx * fx * (3.0 - 2.0 * fx)
    fy = fy * fy * (3.0 - 2.0 * fy)
    v00 = hash01(seed, x0, y0, tag)
    v10 = hash01(seed, x0 + 1, y0, tag)
    v01 = hash01(seed, x0, y0 + 1, tag)
    v11 = hash01(seed, x0 + 1, y0 + 1, tag)
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy


def edge_variation(terrain, orient, i, j):
    """How much outline jitter and wall noise an edge gets (0..1): the calmer of the two cells it borders, by
    their wall layer's Variation. A function of the edge ID and the data only, so it stays seam-safe."""
    table = terrain.settings.wall_variation
    if len(table) == 0:
        return 1.0
    cells = [(i, j), (i, j - 1)] if orient == EDGE_H else [(i, j), (i - 1, j)]
    ny, nx = terrain.wall_material.shape
    values = []
    for ci, cj in cells:
        if 0 <= ci < nx and 0 <= cj < ny:
            values.append(table.get(int(terrain.wall_material[cj, ci]), 1.0))
    return min(values) if len(values) > 0 else 1.0


def crossing_param(terrain, orient, i, j):
    """Where the outline crosses edge (i, j, orient), 0 = its first corner, 1 = its second. Keyed by the
    canonical edge ID only: a smooth noise field sampled at the edge's midpoint (so outlines wander instead of
    zigzagging from edge to edge) plus a little per-edge noise."""
    s = terrain.settings
    jitter = min(max(s.jitter * edge_variation(terrain, orient, i, j), 0.0), 0.45)
    mx = (i + (0.5 if orient == EDGE_H else 0.0)) * s.cell_size
    my = (j + (0.5 if orient == EDGE_V else 0.0)) * s.cell_size
    smooth = smooth_noise(terrain.seed, mx, my, s.jitter_scale, TAG_CROSS * 16 + orient)
    single = hash01(terrain.seed, i, j, orient, TAG_CROSS)
    v = s.jitter_smooth * smooth + (1.0 - s.jitter_smooth) * single
    return 0.5 + jitter * (2.0 * v - 1.0)


def edge_point(terrain, orient, i, j, z):
    """The crossing point of an edge at height z, with wall lean and noise: moved along the edge towards
    its lower corner by lean * (top - z) + noise. At the edge's top height it is the plain crossing."""
    s = terrain.settings
    (ia, ja), (ib, jb) = edge_corners(orient, i, j)
    la, lb = terrain.level(ia, ja), terrain.level(ib, jb)
    hi = max(la, lb)
    top_z = hi * s.step_height
    t = crossing_param(terrain, orient, i, j)
    if top_z - z > 1e-6:
        # Each course below the top steps out by the lean plus a random 0..noise, so the wall batters
        # unevenly but never undercuts (the offset only grows going down).
        k_top = hi
        k = int(round(z / s.step_height))
        noise_px = s.wall_noise_px * edge_variation(terrain, orient, i, j)
        offset = 0.0
        for course in range(k, k_top):
            offset += s.lean_px / ART_PPM * s.step_height
            if noise_px > 0.0:
                offset += noise_px / ART_PPM * hash01(terrain.seed, i, j, orient, course, TAG_WALL_NOISE)
        dt = offset / s.cell_size
        t = t - dt if la < lb else t + dt   # towards the lower corner
        t = min(max(t, 0.05), 0.95)
    x0, y0 = ia * s.cell_size, ja * s.cell_size
    x1, y1 = ib * s.cell_size, jb * s.cell_size
    return (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z)


def edge_down_dir(terrain, orient, i, j):
    """Unit vector along the edge from its higher corner to its lower one (the 'outward' of a wall there)."""
    (ia, ja), (ib, jb) = edge_corners(orient, i, j)
    d = (ib - ia, jb - ja)
    if terrain.level(ia, ja) < terrain.level(ib, jb):
        d = (-d[0], -d[1])
    return d


# --------------------------------------------------------------------------------------------------------
# Cells: boundary walk, regions, bands, chords

class BoundaryPoint:
    """A corner or a crossing point on a cell's boundary. key is global and canonical:
    ('C', i, j) or ('E', orient, i, j)."""
    __slots__ = ("key", "pos", "lo", "hi")

    def __init__(self, key, pos, lo, hi):
        self.key = key
        self.pos = pos      # position in the cell's 8-slot cycle (even = corners)
        self.lo = lo
        self.hi = hi

    def inside(self, t):
        return self.hi >= t


def cell_cycle(terrain, i, j):
    """The cell's boundary points counter-clockwise from its south-west corner. A crossing point is only
    there when its edge's corners differ."""
    items = [
        ("C", i, j), ("E", EDGE_H, i, j),
        ("C", i + 1, j), ("E", EDGE_V, i + 1, j),
        ("C", i + 1, j + 1), ("E", EDGE_H, i, j + 1),
        ("C", i, j + 1), ("E", EDGE_V, i, j),
    ]
    points = []
    for slot, key in enumerate(items):
        if key[0] == "C":
            lv = terrain.level(key[1], key[2])
            points.append(BoundaryPoint(key, slot, lv, lv))
        else:
            (ia, ja), (ib, jb) = edge_corners(key[1], key[2], key[3])
            la, lb = terrain.level(ia, ja), terrain.level(ib, jb)
            if la != lb:
                points.append(BoundaryPoint(key, slot, min(la, lb), max(la, lb)))
    return points


def same_side(a, b):
    """Whether two boundary points lie on the same side of the cell (so the segment between them runs
    along the cell's boundary, not through it)."""
    for side in range(4):
        slots = {2 * side, 2 * side + 1, (2 * side + 2) % 8}
        if a.pos in slots and b.pos in slots:
            return True
    return False


def region(cycle, t):
    return [p for p in cycle if p.inside(t)]


def bands(cycle):
    """{t: [polygon point lists]} of the visible top of each level in the cell: Region(>= t) minus
    Region(>= t + 1), as convex caps."""
    lmin = min(p.lo for p in cycle)
    lmax = max(p.hi for p in cycle)
    result = {}
    for t in range(lmin, lmax + 1):
        outer = region(cycle, t)
        inner = region(cycle, t + 1)
        if len(inner) == 0:
            result[t] = [outer]
            continue
        outer_keys = {p.key for p in outer}
        caps = []
        n = len(inner)
        for k in range(n):
            a, b = inner[k], inner[(k + 1) % n]
            # The cycle points strictly between a and b, counter-clockwise, that are in the outer region.
            between = []
            for p in cycle_iter_after(cycle, a, b):
                if p.key in outer_keys:
                    between.append(p)
            if len(between) > 0:
                caps.append([a] + between + [b])
        result[t] = caps
    return result


def cycle_iter_after(cycle, a, b):
    """The points of the cycle strictly after a and strictly before b, going counter-clockwise."""
    n = len(cycle)
    ia = next(k for k, p in enumerate(cycle) if p.key == a.key)
    k = (ia + 1) % n
    out = []
    while cycle[k].key != b.key:
        out.append(cycle[k])
        k = (k + 1) % n
    return out


def chords(cycle, t):
    """Wall segments of Region(>= t) in the cell: (a, b) with the region on the left of a -> b."""
    r = region(cycle, t)
    out = []
    n = len(r)
    if n == len(cycle) or n == 0:
        return out
    for k in range(n):
        a, b = r[k], r[(k + 1) % n]
        if same_side(a, b) is False:
            out.append((a, b))
    return out


# --------------------------------------------------------------------------------------------------------
# Ramps

RAMP_AXIS = {RAMP_NORTH: (0, 1), RAMP_EAST: (1, 0), RAMP_SOUTH: (0, -1), RAMP_WEST: (-1, 0)}


def ramp_runs(terrain):
    """Validated ramp runs: {cell (i, j): {corner (i, j): height in levels}} for every ramp cell, plus a list
    of problems. A run is consecutive cells with the same direction; its height goes linearly from the low
    edge (all corners one level) to the high edge (all corners one higher level). The surface comes ONLY from
    the run's two ends: the levels stored on a ramp's side corners belong to the neighbouring ground, and
    the side walls fill between that ground and the slope."""
    heights = {}
    problems = []
    seen = set()
    ny, nx = terrain.ramps.shape
    for j in range(ny):
        for i in range(nx):
            d = int(terrain.ramps[j, i])
            if d == RAMP_NONE or (i, j) in seen:
                continue
            ax, ay = RAMP_AXIS[d]
            # Walk back to the run's first cell (the low end), then forward.
            si, sj = i, j
            while 0 <= si - ax < nx and 0 <= sj - ay < ny and int(terrain.ramps[sj - ay, si - ax]) == d:
                si, sj = si - ax, sj - ay
            cells = []
            ci, cj = si, sj
            while 0 <= ci < nx and 0 <= cj < ny and int(terrain.ramps[cj, ci]) == d:
                cells.append((ci, cj))
                ci, cj = ci + ax, cj + ay
            for c in cells:
                seen.add(c)
            k = len(cells)

            # Corners of the low and high edges, and the corner heights of each cell.
            def edge_corners_of(c, high):
                ci2, cj2 = c
                if d == RAMP_NORTH:
                    row = cj2 + 1 if high else cj2
                    return [(ci2, row), (ci2 + 1, row)]
                if d == RAMP_SOUTH:
                    row = cj2 if high else cj2 + 1
                    return [(ci2, row), (ci2 + 1, row)]
                if d == RAMP_EAST:
                    col = ci2 + 1 if high else ci2
                    return [(col, cj2), (col, cj2 + 1)]
                col = ci2 if high else ci2 + 1
                return [(col, cj2), (col, cj2 + 1)]

            low = [terrain.level(*c) for c in edge_corners_of(cells[0], False)]
            high = [terrain.level(*c) for c in edge_corners_of(cells[-1], True)]
            if len(set(low)) != 1 or len(set(high)) != 1 or high[0] <= low[0]:
                problems.append("ramp run at cell %s: low edge %s, high edge %s; drawn as a cliff" % (cells[0], low, high))
                continue
            lo, hi = low[0], high[0]
            for n, c in enumerate(cells):
                h0 = lo + (hi - lo) * n / k
                h1 = lo + (hi - lo) * (n + 1) / k
                corner_heights = {}
                for corner in edge_corners_of(c, False):
                    corner_heights[corner] = h0
                for corner in edge_corners_of(c, True):
                    corner_heights[corner] = h1
                heights[c] = corner_heights
    return heights, problems


# --------------------------------------------------------------------------------------------------------
# Mesh output

class MeshBuilder:
    """Sub-meshes by material: flat triangles (positions and UVs per corner of each triangle)."""

    def __init__(self):
        self.parts = {}

    def part(self, material):
        if material not in self.parts:
            self.parts[material] = {"positions": [], "uvs": []}
        return self.parts[material]

    def tri(self, material, p0, p1, p2, uv0, uv1, uv2):
        a = np.subtract(p1, p0)
        b = np.subtract(p2, p0)
        if np.linalg.norm(np.cross(a, b)) < 1e-10:
            return
        part = self.part(material)
        part["positions"].extend([tuple(p0), tuple(p1), tuple(p2)])
        part["uvs"].extend([tuple(uv0), tuple(uv1), tuple(uv2)])

    def quad(self, material, p0, p1, p2, p3, uv0, uv1, uv2, uv3):
        self.tri(material, p0, p1, p2, uv0, uv1, uv2)
        self.tri(material, p0, p2, p3, uv0, uv2, uv3)

    def triangle_count(self):
        return sum(len(p["positions"]) // 3 for p in self.parts.values())


def corner_xy(terrain, i, j):
    s = terrain.settings.cell_size
    return (i * s, j * s)


def point_at(terrain, bp, z):
    """World position of a boundary point at height z."""
    if bp.key[0] == "C":
        x, y = corner_xy(terrain, bp.key[1], bp.key[2])
        return (x, y, z)
    return edge_point(terrain, bp.key[1], bp.key[2], bp.key[3], z)


# Wall material slot names (per cell value).
def wall_part_name(material):
    return "wall_%d" % material


@dataclass
class WallSegment:
    t: int
    a: BoundaryPoint
    b: BoundaryPoint
    cell: tuple
    material: int
    is_top: bool


def collect_wall_segments(terrain, cells, ramp_heights):
    segments = []
    for (i, j) in cells:
        if (i, j) in ramp_heights:
            continue
        cycle = cell_cycle(terrain, i, j)
        if len({p.hi for p in cycle} | {p.lo for p in cycle}) <= 1:
            continue
        lmin = min(p.lo for p in cycle)
        lmax = max(p.hi for p in cycle)
        per_t = {t: chords(cycle, t) for t in range(lmin + 1, lmax + 1)}
        material = int(terrain.wall_material[j, i])
        for t, segs in per_t.items():
            upper = {(a.key, b.key) for a, b in per_t.get(t + 1, [])}
            for a, b in segs:
                segments.append(WallSegment(t, a, b, (i, j), material, (a.key, b.key) not in upper))
    return segments


def wall_u_table(terrain, segments):
    """Distance along the outline at each crossing point (metres), so wall textures run on without jumps.
    Chains are walked per level, highest first; a point keeps the first value it gets."""
    u = {}
    by_t = {}
    for seg in segments:
        by_t.setdefault(seg.t, []).append(seg)
    for t in sorted(by_t.keys(), reverse=True):
        segs = by_t[t]
        start = {seg.a.key: seg for seg in segs}
        end = {seg.b.key: seg for seg in segs}
        done = set()
        for seg in sorted(segs, key=lambda s: (s.cell[1], s.cell[0], s.a.key)):
            if id(seg) in done:
                continue
            # Back to the chain's start (or around the loop).
            first = seg
            guard = 0
            while first.a.key in end and id(end[first.a.key]) != id(seg) and guard < 100000:
                first = end[first.a.key]
                guard += 1
            cur = first
            run = u.get(cur.a.key, 0.0)
            while cur is not None and id(cur) not in done:
                done.add(id(cur))
                if cur.a.key in u:
                    run = u[cur.a.key]
                else:
                    u[cur.a.key] = run
                pa = point_at(terrain, cur.a, t * terrain.settings.step_height)
                pb = point_at(terrain, cur.b, t * terrain.settings.step_height)
                run += math.hypot(pb[0] - pa[0], pb[1] - pa[1])
                if cur.b.key not in u:
                    u[cur.b.key] = run
                cur = start.get(cur.b.key)
    return u


def mesh_terrain(terrain, cells=None, top_layer_at=None, fringe_layers=(0,), wall_tiling=None):
    """Meshes the given cells (default: all). top_layer_at(x, y) -> winning top layer index (for the
    fringe). wall_tiling: {material: (tx, ty)} repeats per metre for wall UVs.
    Returns (MeshBuilder, info dict)."""
    s = terrain.settings
    if cells is None:
        cells = [(i, j) for j in range(terrain.ny - 1) for i in range(terrain.nx - 1)]
    cells = list(cells)
    cell_set = set(cells)
    wall_tiling = wall_tiling or {}
    ramp_heights, problems = ramp_runs(terrain)
    mb = MeshBuilder()
    W, H = terrain.width, terrain.height

    def top_uv(p):
        return (p[0] / W, p[1] / H)

    # Tops.
    for (i, j) in cells:
        if (i, j) in ramp_heights:
            continue
        cycle = cell_cycle(terrain, i, j)
        for t, polys in bands(cycle).items():
            z = t * s.step_height
            for poly in polys:
                pts = [point_at(terrain, bp, z) for bp in poly]
                for k in range(1, len(pts) - 1):
                    mb.tri("top", pts[0], pts[k], pts[k + 1], top_uv(pts[0]), top_uv(pts[k]), top_uv(pts[k + 1]))

    # Walls (all cells, so the u table sees whole chains), then lip, fringe and caps for this chunk's cells.
    all_cells = [(i, j) for j in range(terrain.ny - 1) for i in range(terrain.nx - 1)]
    all_segments = collect_wall_segments(terrain, all_cells, ramp_heights)
    u_table = wall_u_table(terrain, all_segments)
    segments = [seg for seg in all_segments if seg.cell in cell_set]
    lip = s.lip_px / ART_PPM
    lip_face = s.lip_face_px / ART_PPM
    fringe = s.fringe_px / ART_PPM

    for seg in segments:
        zt = seg.t * s.step_height
        zb = (seg.t - 1) * s.step_height
        at, bt = point_at(terrain, seg.a, zt), point_at(terrain, seg.b, zt)
        ab, bb = point_at(terrain, seg.a, zb), point_at(terrain, seg.b, zb)
        ua = u_table.get(seg.a.key, 0.0)
        ub = ua + math.hypot(bt[0] - at[0], bt[1] - at[1])
        if seg.material != WALL_HIDDEN:
            tx, ty = wall_tiling.get(seg.material, (1.0, 1.0))
            name = wall_part_name(seg.material)
            # Outward (lower ground) is on the right of a -> b.
            mb.quad(name, ab, bb, bt, at, (ua * tx, zb * ty), (ub * tx, zb * ty), (ub * tx, zt * ty), (ua * tx, zt * ty))
        else:
            mb.quad("hidden", ab, bb, bt, at, (0, 0), (1, 0), (1, 1), (0, 1))
        if seg.is_top is False or seg.material == WALL_HIDDEN:
            continue
        da = edge_down_dir(terrain, *seg.a.key[1:])
        db = edge_down_dir(terrain, *seg.b.key[1:])
        if lip > 0.0:
            ao = (at[0] + da[0] * lip, at[1] + da[1] * lip, zt)
            bo = (bt[0] + db[0] * lip, bt[1] + db[1] * lip, zt)
            mb.quad("top", ao, bo, bt, at, top_uv(ao), top_uv(bo), top_uv(bt), top_uv(at))
            if lip_face > 0.0:
                tx, ty = wall_tiling.get(seg.material, (1.0, 1.0))
                aod = (ao[0], ao[1], zt - lip_face)
                bod = (bo[0], bo[1], zt - lip_face)
                mb.quad("lip_face_%d" % seg.material, aod, bod, bo, ao,
                        (ua * tx, (zt - lip_face) * ty), (ub * tx, (zt - lip_face) * ty), (ub * tx, zt * ty), (ua * tx, zt * ty))
        else:
            ao, bo = at, bt
        if fringe > 0.0 and top_layer_at is not None:
            # The layer just inside the shelf's edge decides (grass gets a fringe, flagstones none).
            mx, my = (at[0] + bt[0]) * 0.5, (at[1] + bt[1]) * 0.5
            ex, ey = bt[0] - at[0], bt[1] - at[1]
            ln = math.hypot(ex, ey)
            if ln > 1e-6:
                inx, iny = -ey / ln, ex / ln   # left of a -> b: into the shelf
                layer = top_layer_at(mx + inx * 0.06, my + iny * 0.06)
                if layer in fringe_layers:
                    off = 0.004
                    af = (ao[0] + da[0] * off, ao[1] + da[1] * off, zt + 0.002)
                    bf = (bo[0] + db[0] * off, bo[1] + db[1] * off, zt + 0.002)
                    afd = (af[0], af[1], zt - fringe)
                    bfd = (bf[0], bf[1], zt - fringe)
                    fu = 1.0 / 2.0   # the fringe texture is 2 m wide
                    mb.quad("fringe", afd, bfd, bf, af, (ua * fu, 0.0), (ub * fu, 0.0), (ub * fu, 1.0), (ua * fu, 1.0))
        if s.caps and seg.material == 0:
            add_caps(terrain, mb, seg, at, bt, ua)

    # Ramps.
    for (i, j) in cells:
        if (i, j) not in ramp_heights:
            continue
        add_ramp(terrain, mb, (i, j), ramp_heights, top_uv, wall_tiling)

    info = {"problems": problems, "segments": len(segments), "triangles": mb.triangle_count()}
    return mb, info


def add_caps(terrain, mb, seg, at, bt, ua):
    """Coping stones along a stone wall's top: a tight row of flat, rectangular stones aligned with the wall,
    in slots every cap_pitch metres of outline distance (u), so they don't depend on the cell or chunk. Small
    size and position jitter, low rotation, an occasional missing stone."""
    s = terrain.settings
    ex, ey = bt[0] - at[0], bt[1] - at[1]
    ln = math.hypot(ex, ey)
    if ln < 1e-6:
        return
    dx, dy = ex / ln, ey / ln
    inx, iny = -dy, dx
    pitch = s.cap_pitch
    first = math.ceil(ua / pitch - 0.5)
    last = math.floor((ua + ln) / pitch - 0.5)
    for n in range(first, last + 1):
        r = [hash01(terrain.seed, n, seg.t, k, TAG_CAP) for k in range(6)]
        if r[0] < s.cap_missing:
            continue
        centre_u = (n + 0.5) * pitch + (r[1] - 0.5) * s.cap_jitter
        f = (centre_u - ua) / ln
        if f < 0.0 or f > 1.0:
            continue
        length = pitch - s.cap_gap - r[2] * s.cap_size_jitter
        depth = s.cap_depth * (1.0 - 0.5 * s.cap_size_jitter / max(pitch, 1e-6) * r[3])
        height = s.cap_height[0] + r[4] * (s.cap_height[1] - s.cap_height[0])
        yaw = (r[5] - 0.5) * 2.0 * math.radians(s.cap_yaw_degrees)
        inset = depth * 0.3 + (r[3] - 0.5) * s.cap_jitter
        cx = at[0] + ex * f + inx * inset
        cy = at[1] + ey * f + iny * inset
        add_box(mb, "caps", (cx, cy, at[2]), (dx, dy), yaw, length, depth, height)


def add_box(mb, material, base, axis, yaw, w, d, h):
    c, s_ = math.cos(yaw), math.sin(yaw)
    ax = (axis[0] * c - axis[1] * s_, axis[0] * s_ + axis[1] * c)
    ay = (-ax[1], ax[0])
    corners = []
    for sz in (0.0, h):
        for sy in (-0.5, 0.5):
            for sx in (-0.5, 0.5):
                corners.append((base[0] + ax[0] * sx * w + ay[0] * sy * d,
                                base[1] + ax[1] * sx * w + ay[1] * sy * d,
                                base[2] + sz - 0.01))
    # index: x + 2y + 4z
    faces = [(4, 5, 7, 6), (0, 2, 3, 1), (0, 1, 5, 4), (2, 6, 7, 3), (1, 3, 7, 5), (0, 4, 6, 2)]
    for f in faces:
        p = [corners[k] for k in f]
        uv = [(q[0] * 0.2 + q[2] * 0.2, q[1] * 0.2 + q[2] * 0.2) for q in p]   # the Boulder's 0.2 repeats per metre
        mb.quad(material, p[0], p[1], p[2], p[3], uv[0], uv[1], uv[2], uv[3])


def add_ramp(terrain, mb, cell, ramp_heights, top_uv, wall_tiling):
    s = terrain.settings
    i, j = cell
    hs = ramp_heights[cell]
    corners = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
    pts = []
    for c in corners:
        x, y = corner_xy(terrain, *c)
        pts.append((x, y, hs[c] * s.step_height))
    mb.quad("top", pts[0], pts[1], pts[2], pts[3], top_uv(pts[0]), top_uv(pts[1]), top_uv(pts[2]), top_uv(pts[3]))

    # Side walls: every cell edge whose other side doesn't continue the same slope. A ramp replaces only
    # the wall across its slope; its sides meet the neighbouring ground through tapering pieces.
    sides = [((i, j), (i + 1, j), EDGE_H, (i, j), (0, -1)),
             ((i + 1, j), (i + 1, j + 1), EDGE_V, (i + 1, j), (1, 0)),
             ((i, j + 1), (i + 1, j + 1), EDGE_H, (i, j + 1), (0, 1)),
             ((i, j), (i, j + 1), EDGE_V, (i, j), (-1, 0))]
    for c0, c1, orient, (ei, ej), out in sides:
        ni, nj = i + out[0], j + out[1]
        if ni < 0 or nj < 0 or ni >= terrain.nx - 1 or nj >= terrain.ny - 1:
            continue
        r0, r1 = hs[c0] * s.step_height, hs[c1] * s.step_height
        if (ni, nj) in ramp_heights:
            nh = ramp_heights[(ni, nj)]
            n0, n1 = nh[c0] * s.step_height, nh[c1] * s.step_height
            neighbour = [(0.0, n0), (1.0, n1)]
        else:
            neighbour = edge_profile(terrain, orient, ei, ej)
        ramp_profile = (r0, r1)
        build_side_wall(terrain, mb, orient, ei, ej, out, ramp_profile, neighbour, int(terrain.wall_material[j, i]), wall_tiling)


def edge_profile(terrain, orient, i, j):
    """The regular ground's height along an edge as (param, z) points from its first corner to its second;
    vertical steps appear as two points at (almost) the same param."""
    s = terrain.settings
    (ia, ja), (ib, jb) = edge_corners(orient, i, j)
    la, lb = terrain.level(ia, ja), terrain.level(ib, jb)
    za, zb = la * s.step_height, lb * s.step_height
    if la == lb:
        return [(0.0, za), (1.0, zb)]
    x0, y0 = corner_xy(terrain, ia, ja)
    prof = [(0.0, za)]
    step = 1 if lb > la else -1
    for lv in range(la, lb + step, step):
        z = lv * s.step_height
        p = edge_point(terrain, orient, i, j, z)
        t = math.hypot(p[0] - x0, p[1] - y0) / s.cell_size
        prof.append((t, z))
    prof.append((1.0, zb))
    return prof


def build_side_wall(terrain, mb, orient, i, j, out, ramp_profile, neighbour, material, wall_tiling):
    s = terrain.settings
    (ia, ja), (ib, jb) = edge_corners(orient, i, j)
    x0, y0 = corner_xy(terrain, ia, ja)
    x1, y1 = corner_xy(terrain, ib, jb)

    def at(t, z):
        return (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z)

    def ramp_z(t):
        return ramp_profile[0] + (ramp_profile[1] - ramp_profile[0]) * t

    if material == WALL_HIDDEN:
        return
    tx, ty = wall_tiling.get(material, (1.0, 1.0))
    for k in range(len(neighbour) - 1):
        t0, n0 = neighbour[k]
        t1, n1 = neighbour[k + 1]
        if t1 - t0 < 1e-6:
            continue
        pieces = [(t0, n0, t1, n1)]
        d0, d1 = ramp_z(t0) - n0, ramp_z(t1) - n1
        if d0 * d1 < 0.0:
            f = d0 / (d0 - d1)
            tm = t0 + (t1 - t0) * f
            nm = n0 + (n1 - n0) * f
            pieces = [(t0, n0, tm, nm), (tm, nm, t1, n1)]
        for (a, na, b, nb) in pieces:
            ra, rb = ramp_z(a), ramp_z(b)
            mid = (ra - na) + (rb - nb)
            if abs(mid) < 1e-6:
                continue
            lo_a, hi_a = min(ra, na), max(ra, na)
            lo_b, hi_b = min(rb, nb), max(rb, nb)
            p = [at(a, lo_a), at(b, lo_b), at(b, hi_b), at(a, hi_a)]
            ua, ub = (x0 + y0 + (x1 - x0 + y1 - y0) * a), (x0 + y0 + (x1 - x0 + y1 - y0) * b)
            uv = [(ua * tx, lo_a * ty), (ub * tx, lo_b * ty), (ub * tx, hi_b * ty), (ua * tx, hi_a * ty)]
            # Visible from the lower side: towards the neighbour where the ramp is higher, else into the ramp.
            facing = out if mid > 0.0 else (-out[0], -out[1])
            n = np.cross(np.subtract(p[1], p[0]), np.subtract(p[2], p[0]))
            if n[0] * facing[0] + n[1] * facing[1] < 0.0:
                p = [p[1], p[0], p[3], p[2]]
                uv = [uv[1], uv[0], uv[3], uv[2]]
            mb.quad(wall_part_name(material), p[0], p[1], p[2], p[3], uv[0], uv[1], uv[2], uv[3])
