"""Shared helpers for putting a field scene on a terrain (Terrain plan TR4/TR6): a corner grid in world metres,
shape and paint helpers, the ground height of a terrain (the same bands as the mesher), and text edits of a
.hyscene that keep everything the builder doesn't touch byte for byte.

The scene builders (build_farm_village.py, build_shrine_glade.py) use it like this:

    grid = Grid(x0=-40, y0=-56, width=80, height=102)      # the terrain entity sits at (x0, y0)
    grid.levels[...] = ...                                  # shape with masks over grid.X, grid.Y
    paint = Paint(grid, layers=4); paint.road([...], width=2.2, layer=2)
    scene = Scene.load(path); scene.remove_subtrees(ids); scene.shift_units(delta_z); scene.save(path)
"""

import math
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import terrain_mesher as tm  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ASSETS = os.path.join(REPO, "Oxygen", "project", "Assets")


def handle_of(relative_path):
    """The asset handle of a file under Assets: the first line of a .hymat/.hyscene, or its .hymeta's."""
    path = os.path.join(ASSETS, relative_path)
    meta = path + ".hymeta"
    source = meta if os.path.exists(meta) else path
    with open(source) as f:
        first = f.readline()
    if first.startswith("Handle:") is False:
        raise ValueError("%s has no handle" % source)
    return int(first.split()[1])


# ------------------------------------------------------------------------------------------------------------
# Noise

def hash01(*values):
    return tm.hash01(*values)


def wander(t, seed, amplitude, scale=6.0):
    """A smooth 1D wander (metres) along t (metres), from value noise: the same for the same seed."""
    t = np.asarray(t, dtype=np.float64)
    g = t / scale
    g0 = np.floor(g)
    f = g - g0
    f = f * f * (3 - 2 * f)
    g0 = g0.astype(np.int64)
    flat = g0.ravel()
    a = np.array([hash01(seed, int(k), 0, 11) for k in flat]).reshape(g0.shape)
    b = np.array([hash01(seed, int(k) + 1, 0, 11) for k in flat]).reshape(g0.shape)
    return (a * (1 - f) + b * f - 0.5) * 2.0 * amplitude


def field_noise(x, y, seed, scale):
    """2D value noise 0..1 over metres (numpy arrays)."""
    gx = np.asarray(x) / scale
    gy = np.asarray(y) / scale
    x0 = np.floor(gx)
    y0 = np.floor(gy)
    fx = gx - x0
    fy = gy - y0
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    x0 = x0.astype(np.int64)
    y0 = y0.astype(np.int64)
    cache = {}

    def h(ix, iy):
        out = np.empty(ix.shape)
        for idx, (a, b) in enumerate(zip(ix.ravel(), iy.ravel())):
            key = (int(a), int(b))
            v = cache.get(key)
            if v is None:
                v = hash01(seed, key[0], key[1], 12)
                cache[key] = v
            out.ravel()[idx] = v
        return out

    v00 = h(x0, y0)
    v10 = h(x0 + 1, y0)
    v01 = h(x0, y0 + 1)
    v11 = h(x0 + 1, y0 + 1)
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy


def smoothstep(e0, e1, v):
    t = np.clip((np.asarray(v, dtype=np.float64) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------------------------------------------------
# The grid

class Grid:
    """Corners every `cell` metres from (x0, y0); X, Y are the corners' world positions, [j, i]."""

    def __init__(self, x0, y0, width, height, cell=0.5, step=0.25):
        self.x0, self.y0, self.cell, self.step = x0, y0, cell, step
        self.nx = int(round(width / cell)) + 1
        self.ny = int(round(height / cell)) + 1
        i = np.arange(self.nx)
        j = np.arange(self.ny)
        self.X, self.Y = np.meshgrid(x0 + i * cell, y0 + j * cell)
        self.levels = np.zeros((self.ny, self.nx), dtype=np.int32)
        self.ramps = np.zeros((self.ny - 1, self.nx - 1), dtype=np.uint8)
        self.walls = np.zeros((self.ny - 1, self.nx - 1), dtype=np.uint8)
        # Cell centres.
        self.CX = (self.X[:-1, :-1] + self.X[1:, 1:]) * 0.5
        self.CY = (self.Y[:-1, :-1] + self.Y[1:, 1:]) * 0.5

    def level_of(self, z):
        return int(round(z / self.step))

    def cell_max(self):
        L = self.levels
        return np.maximum(np.maximum(L[:-1, :-1], L[:-1, 1:]), np.maximum(L[1:, :-1], L[1:, 1:]))

    def cell_min(self):
        L = self.levels
        return np.minimum(np.minimum(L[:-1, :-1], L[:-1, 1:]), np.minimum(L[1:, :-1], L[1:, 1:]))

    def mesher_terrain(self, settings, seed):
        return tm.Terrain(levels=self.levels.copy(), ramps=self.ramps.copy(), wall_material=self.walls.copy(), seed=seed, settings=settings)

    def height_at(self, terrain, x, y):
        """The ground's height at world (x, y): the band (or ramp) under it, as the engine's TerrainMesher::GetHeightAt."""
        lx = (x - self.x0) / self.cell
        ly = (y - self.y0) / self.cell
        i = int(min(max(math.floor(lx), 0), self.nx - 2))
        j = int(min(max(math.floor(ly), 0), self.ny - 2))
        cycle = tm.cell_cycle(terrain, i, j)
        if len({p.lo for p in cycle} | {p.hi for p in cycle}) <= 1:
            return cycle[0].hi * self.step
        px, py = x - self.x0, y - self.y0
        for t, polys in tm.bands(cycle).items():
            for poly in polys:
                pts = [tm.point_at(terrain, bp, t * self.step) for bp in poly]
                if len(pts) < 3:
                    continue
                inside = True
                for k in range(len(pts)):
                    p, q = pts[k], pts[(k + 1) % len(pts)]
                    if (q[0] - p[0]) * (py - p[1]) - (q[1] - p[1]) * (px - p[0]) < -1e-5:
                        inside = False
                        break
                if inside:
                    return t * self.step
        return max(p.hi for p in cycle) * self.step


# ------------------------------------------------------------------------------------------------------------
# Painting the weight map

def catmull_rom(points, spacing=0.1):
    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for k in range(1, len(pts) - 2):
        p0, p1, p2, p3 = (np.asarray(p, dtype=np.float64) for p in pts[k - 1:k + 3])
        n = max(2, int(math.ceil(np.linalg.norm(p2 - p1) / spacing)))
        for s in range(n):
            t = s / n
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    out.append(np.asarray(points[-1], dtype=np.float64))
    return np.array(out)


def distance_to_polyline(px, py, poly):
    best = np.full(px.shape, 1e9)
    for a, b in zip(poly[:-1], poly[1:]):
        d = b - a
        ll = float(d @ d)
        if ll == 0:
            continue
        t = np.clip(((px - a[0]) * d[0] + (py - a[1]) * d[1]) / ll, 0.0, 1.0)
        best = np.minimum(best, np.hypot(px - (a[0] + d[0] * t), py - (a[1] + d[1] * t)))
    return best


class Paint:
    """The weight map as floats (texels, layers) in world metres; to_bytes() normalises to RGBA8 summing to 255."""

    def __init__(self, grid, resolution=8, layers=4):
        self.grid = grid
        self.res = resolution
        w = int(round((grid.nx - 1) * grid.cell * resolution))
        h = int(round((grid.ny - 1) * grid.cell * resolution))
        tx = np.arange(w)
        ty = np.arange(h)
        TX, TY = np.meshgrid(tx, ty)
        self.PX = grid.x0 + (TX + 0.5) / resolution
        self.PY = grid.y0 + (TY + 0.5) / resolution
        self.w = np.zeros((h, w, 4))
        self.w[..., 0] = 1.0
        self.layers = layers

    def raise_to(self, layer, target):
        """Raises `layer` to at least `target` (array or scalar 0..1); the others shrink in proportion."""
        target = np.broadcast_to(np.asarray(target, dtype=np.float64), self.w.shape[:2])
        old = self.w[..., layer]
        mask = target > old
        others = self.w.sum(axis=-1) - old
        scale = np.where(others > 1e-9, (1.0 - target) / np.maximum(others, 1e-9), 0.0)
        for k in range(4):
            if k == layer:
                self.w[..., k] = np.where(mask, target, old)
            else:
                self.w[..., k] = np.where(mask, self.w[..., k] * scale, self.w[..., k])

    def road(self, points, width, layer, wander_m=0.15, soft=0.35, seed=1):
        """A layer along the Catmull-Rom curve through `points`, `width` wide, its edges wandering (the Path brush)."""
        poly = catmull_rom(points)
        lo = poly.min(axis=0) - width - 1.0
        hi = poly.max(axis=0) + width + 1.0
        box = (self.PX >= lo[0]) & (self.PX <= hi[0]) & (self.PY >= lo[1]) & (self.PY <= hi[1])
        if box.any() is False:
            return
        ys, xs = np.nonzero(box)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        px = self.PX[y0:y1, x0:x1]
        py = self.PY[y0:y1, x0:x1]
        d = distance_to_polyline(px, py, poly)
        s = float(seed % 1000)
        wobble = wander_m * (0.6 * np.sin(1.7 * px + 0.9 * py + s) + 0.4 * np.sin(0.6 * px - 1.3 * py + 2 * s))
        target = np.clip((0.5 * width + wobble - d) / soft + 0.5, 0.0, 1.0)
        full = np.zeros(self.w.shape[:2])
        full[y0:y1, x0:x1] = target
        self.raise_to(layer, full)

    def blob(self, centre, half, layer, wander_m=0.3, soft=0.35, seed=2, corner=0.6):
        """A rounded rectangle of `layer` with wandering edges."""
        cx, cy = centre
        hx, hy = half
        qx = np.abs(self.PX - cx) - (hx - corner)
        qy = np.abs(self.PY - cy) - (hy - corner)
        d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - corner
        s = float(seed % 1000)
        wobble = wander_m * (0.6 * np.sin(1.3 * self.PX + 0.7 * self.PY + s) + 0.4 * np.sin(0.5 * self.PX - 1.1 * self.PY + 2 * s))
        target = np.clip((wobble - d) / soft + 0.5, 0.0, 1.0)
        self.raise_to(layer, target)

    def where(self, mask_fn, layer, strength=1.0):
        """`layer` raised by a function of world (x, y) arrays returning 0..1."""
        self.raise_to(layer, np.clip(mask_fn(self.PX, self.PY), 0.0, 1.0) * strength)

    def to_bytes(self):
        w = np.clip(self.w, 0.0, None)
        w[..., self.layers:] = 0.0
        w = w / np.maximum(w.sum(axis=-1, keepdims=True), 1e-9)
        b = np.round(w * 255.0).astype(np.int32)
        largest = np.argmax(b, axis=-1)
        fix = 255 - b.sum(axis=-1)
        np.put_along_axis(b, largest[..., None], np.take_along_axis(b, largest[..., None], axis=-1) + fix[..., None], axis=-1)
        return np.clip(b, 0, 255).astype(np.uint8)


# ------------------------------------------------------------------------------------------------------------
# Scene text edits

ENTITY_RE = re.compile(r"^  - Entity: (\d+)\s*$")


class Scene:
    """A .hyscene as its header and entity blocks (text). Edits keep untouched blocks byte for byte."""

    def __init__(self, header, blocks, newline):
        self.header = header        # text before the first entity
        self.blocks = blocks        # list of [id, text]
        self.newline = newline

    @staticmethod
    def load(path):
        raw = open(path, "rb").read().decode("utf-8")
        newline = "\r\n" if "\r\n" in raw else "\n"
        lines = raw.replace("\r\n", "\n").split("\n")
        header = []
        blocks = []
        current = None
        for line in lines:
            m = ENTITY_RE.match(line)
            if m:
                current = [int(m.group(1)), line + "\n"]
                blocks.append(current)
            elif current is None:
                header.append(line + "\n")
            else:
                current[1] += line + "\n"
        text_header = "".join(header)
        if blocks:
            blocks[-1][1] = blocks[-1][1][:-1]  # the split added one newline too many at the end
        else:
            text_header = text_header[:-1]
        return Scene(text_header, blocks, newline)

    def save(self, path):
        text = self.header + "".join(b[1] for b in self.blocks)
        with open(path, "wb") as f:
            f.write(text.replace("\n", self.newline).encode("utf-8"))

    # Queries -------------------------------------------------------------------------------------------------

    def block(self, eid):
        for b in self.blocks:
            if b[0] == eid:
                return b
        return None

    @staticmethod
    def field(text, key):
        m = re.search(r"^\s+%s: (.*)$" % re.escape(key), text, re.M)
        return m.group(1).strip() if m else None

    def tag(self, eid):
        return self.field(self.block(eid)[1], "Tag") or ""

    def parent(self, eid):
        b = self.block(eid)
        m = re.search(r"RelationshipComponent:\n\s+Parent: (\d+)", b[1])
        return int(m.group(1)) if m else 0

    def children(self, eid):
        return [b[0] for b in self.blocks if self.parent(b[0]) == eid]

    def find(self, tag, parent=None):
        out = []
        for b in self.blocks:
            if self.tag(b[0]) == tag and (parent is None or self.parent(b[0]) == parent):
                out.append(b[0])
        return out

    def translation(self, eid):
        m = re.search(r"TransformComponent:\n\s+Translation: \[([^\]]*)\]", self.block(eid)[1])
        if m is None:
            return None
        return [float(v) for v in m.group(1).split(",")]

    def set_translation(self, eid, t):
        b = self.block(eid)
        text = "[%s]" % ", ".join(fmt_float(v) for v in t)
        b[1] = re.sub(r"(TransformComponent:\n\s+Translation: )\[[^\]]*\]", lambda m: m.group(1) + text, b[1], count=1)

    def subtree(self, eid):
        kids = {}
        for b in self.blocks:
            kids.setdefault(self.parent(b[0]), []).append(b[0])
        out = []
        stack = [eid]
        while stack:
            e = stack.pop()
            out.append(e)
            stack.extend(kids.get(e, []))
        return out

    # Edits ---------------------------------------------------------------------------------------------------

    def remove(self, ids):
        ids = set(ids)
        self.blocks = [b for b in self.blocks if b[0] not in ids]

    def remove_subtrees(self, roots):
        ids = set()
        for r in roots:
            ids.update(self.subtree(r))
        self.remove(ids)
        return len(ids)

    def insert_after(self, after_id, eid, text):
        for k, b in enumerate(self.blocks):
            if b[0] == after_id:
                self.blocks.insert(k + 1 + len(self.subtree(after_id)) - 1, [eid, text])
                return
        self.blocks.append([eid, text])


def fmt_float(v):
    v = float(v)
    if abs(v) < 5e-7:
        return "0"
    s = ("%.6f" % v).rstrip("0").rstrip(".")
    return s


def is_folder(scene, eid):
    t = scene.translation(eid)
    return t is not None and all(abs(v) < 1e-6 for v in t) and len(scene.children(eid)) > 0


def units(scene, roots):
    """The placed things under `roots`: entities that aren't zero-transform folders, the topmost ones only."""
    out = []
    stack = list(roots)
    while stack:
        e = stack.pop()
        if is_folder(scene, e):
            stack.extend(scene.children(e))
        elif scene.translation(e) is not None:
            out.append(e)
    return out
