"""EONLORE Medieval Village kit - trimmed hedge modules (Oct 30, 2026).

Builds MV_Hedge_A/B/C (2 m middle pieces that join seamlessly end to end) and MV_Hedge_End (rounded at +X; turn
it 180 degrees for the left end of a row) and writes them as .glb next to the other kit models.

Why 3D (StyleGuide, "Hedges"): from the field camera a long trimmed hedge needs a lit top plane and a darker front;
upright sprite cards in a row show no top and read as one flat green box.

Shape: 2.0 m long (X), 0.88 m deep (Y), about 1.2 m tall (Z), standing on its base at the origin, front facing -Y
(towards the field camera), the same frame as build_kit.py (exported Y-up; Hydro's import turns it back to Z-up).
The cross-section is a rounded box; every vertex except the base and the two joint slices is pushed in and out
along its normal (lumps of 0.25-0.6 m), so neighbours never repeat and the joints always match.

Materials (32 texels per metre, nearest, seamless 2 m tiles from textures_hedge.py):
  MAT_Hedge_Top   faces pointing up      MV_Hedge_Top.png
  MAT_Hedge_Side  the rest               MV_Hedge_Side.png
  MAT_Hedge_Leaf  cut-out cards growing out of the top edge (alpha mask), MV_Hedge_Leaf.png, 1 x 0.5 m
Contact shading is baked into COLOR_0 (Hydro multiplies albedo by it): darker toward the foot and in the hollows.

No Blender needed: numpy + Pillow, writes glTF 2.0 binary directly.
Run: python build_hedge.py [<models dir>] [<textures dir>]
     defaults: ../../../ (MedievalVillage/Models) and ../Textures (MV_Kit/Textures); missing textures are generated.
"""
import json
import math
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

PPM = 32.0
LENGTH = 2.0
TILE_M = 2.0            # the 64 px textures cover 2 m
LEAF_W, LEAF_H = 1.0, 0.5

# Cross-section (y, z), front (-y) -> top -> back. Base points (first, last) never move.
PROFILE = [(-0.38, 0.0), (-0.43, 0.22), (-0.45, 0.55), (-0.44, 0.88), (-0.38, 1.08), (-0.24, 1.18), (-0.08, 1.22),
           (0.08, 1.22), (0.24, 1.18), (0.38, 1.08), (0.44, 0.88), (0.45, 0.55), (0.43, 0.22), (0.38, 0.0)]
SLICES = 17             # every 0.125 m
LUMP = 0.11             # how far lumps push in or out (m)


# ------------------------------------------------------------------ geometry
def profile_normals(prof):
    """Outward 2D normals of the profile (y, z) at each point."""
    n = []
    for i in range(len(prof)):
        a = np.array(prof[max(i - 1, 0)]); b = np.array(prof[min(i + 1, len(prof) - 1)])
        t = b - a
        nn = np.array([-t[1], t[0]])        # rotate the tangent: front -> top -> back runs clockwise seen from +X
        nn /= np.linalg.norm(nn) + 1e-9
        if nn @ np.array(prof[i]) - nn @ np.array([0.0, 0.6]) < 0:   # point away from the middle
            nn = -nn
        n.append(nn)
    return np.array(n)


def lumps(seed, xs, rows):
    """Smooth noise per profile row along x: a few sines with random phase and length, zero at both joints."""
    r = np.random.default_rng(seed)
    d = np.zeros((rows, len(xs)))
    for j in range(rows):
        for _ in range(3):
            wl = r.uniform(0.5, 1.2); ph = r.uniform(0, 2 * math.pi); amp = r.uniform(0.3, 0.6)
            d[j] += amp * np.sin(2 * math.pi * xs / wl + ph)
        d[j] += r.normal(0, 0.18, len(xs))
    d /= max(np.abs(d).max(), 1e-6)
    # neighbouring rows share some of their shape so lumps read as round clumps, not stripes
    d = 0.5 * d + 0.25 * np.roll(d, 1, 0) + 0.25 * np.roll(d, -1, 0)
    return d


def build_module(seed, round_end=False):
    xs = np.linspace(-LENGTH / 2, LENGTH / 2, SLICES)
    prof = np.array(PROFILE)
    nrm = profile_normals(PROFILE)
    rows = len(prof)
    d = lumps(seed, xs, rows) * LUMP
    joint = np.abs(np.abs(xs) - LENGTH / 2) < 1e-6
    d[:, joint] = 0.0
    d[0, :] = 0.0; d[-1, :] = 0.0                                       # the base stays put
    # round end: the last 0.5 m at +X shrinks toward a dome (taper in y, drop in z)
    taper = np.ones(len(xs))
    if round_end:
        t = np.clip((xs - (LENGTH / 2 - 0.55)) / 0.55, 0, 1)
        taper = np.sqrt(np.clip(1 - t ** 2, 0, 1)) * 0.75 + 0.25 * (1 - t)
        taper[-1] = 0.0
    grid = np.zeros((len(xs), rows, 3))
    for i, x in enumerate(xs):
        for j in range(rows):
            y, z = prof[j] + nrm[j] * d[j, i]
            k = taper[i]
            if round_end:
                y *= max(k, 0.0)
                z = z * (0.35 + 0.65 * k) if j not in (0, rows - 1) else 0.0
            grid[i, j] = (x, y, z)
    return grid, d


def mesh_from_grid(grid, d):
    """Quads between slices, an end cap at -X (and +X when it isn't collapsed); smooth normals; top/side split."""
    ns, nr, _ = grid.shape
    V = grid.reshape(-1, 3)
    vid = lambda i, j: i * nr + j
    quads = []
    for i in range(ns - 1):
        for j in range(nr - 1):
            quads.append((vid(i, j), vid(i + 1, j), vid(i + 1, j + 1), vid(i, j + 1)))
    tris = []
    for a, b, c, e in quads:
        tris += [(a, b, c), (a, c, e)]
    tris = np.array(tris)
    # orient outward (away from the hedge's centre line)
    cen = V[tris].mean(1)
    fn = np.cross(V[tris[:, 1]] - V[tris[:, 0]], V[tris[:, 2]] - V[tris[:, 0]])
    out = cen - np.stack([cen[:, 0], np.zeros(len(cen)), np.full(len(cen), 0.55)], 1)
    flip = (fn * out).sum(1) < 0
    tris[flip] = tris[flip][:, [0, 2, 1]]
    fn = np.cross(V[tris[:, 1]] - V[tris[:, 0]], V[tris[:, 2]] - V[tris[:, 0]])
    # smooth vertex normals
    vn = np.zeros_like(V)
    for k in range(3):
        np.add.at(vn, tris[:, k], fn)
    vn /= np.linalg.norm(vn, axis=1, keepdims=True) + 1e-9
    # contact shading: dark at the foot, a little darker in hollows
    dd = d.T.reshape(-1)                                                # (slices, rows) order, like V
    z = V[:, 2]
    ao = 0.7 + 0.3 * np.clip(z / 0.5, 0, 1)
    ao *= 0.9 + 0.1 * np.clip(1 + dd / LUMP, 0, 1)
    fnu = fn / (np.linalg.norm(fn, axis=1, keepdims=True) + 1e-9)
    is_top = fnu[:, 2] > 0.6
    groups = {}
    for name, sel in (("MAT_Hedge_Top", is_top), ("MAT_Hedge_Side", ~is_top)):
        T = tris[sel]
        used, inv = np.unique(T.reshape(-1), return_inverse=True)
        P = V[used]; N = vn[used]; C = ao[used]
        if name == "MAT_Hedge_Top":
            uv = np.stack([P[:, 0] / TILE_M, -P[:, 1] / TILE_M], 1)
        else:
            # project front/back on XZ; the round end's cap faces mostly +X, project those on YZ
            side_x = np.abs(N[:, 0]) > 0.7
            u = np.where(side_x, P[:, 1], P[:, 0] * np.sign(-N[:, 1] + 1e-6))
            uv = np.stack([u / TILE_M, -P[:, 2] / TILE_M], 1)
        groups[name] = (P, N, uv, C, inv.reshape(-1, 3))
    # flat end caps (fan from the middle of the base line), side material
    caps = []
    for i, sgn in ((0, -1.0), (ns - 1, 1.0)):
        ring = grid[i]
        if np.abs(ring[:, 1]).max() < 1e-4:
            continue                                                    # collapsed (round end)
        centre = np.array([ring[0, 0], 0.0, 0.5])
        P = np.vstack([centre[None], ring])
        N = np.tile([sgn, 0.0, 0.0], (len(P), 1))
        uv = np.stack([P[:, 1] * -sgn / TILE_M, -P[:, 2] / TILE_M], 1)
        C = 0.7 + 0.3 * np.clip(P[:, 2] / 0.5, 0, 1)
        idx = []
        for j in range(1, len(P) - 1):
            idx.append((0, j, j + 1) if sgn > 0 else (0, j + 1, j))
        # close the base edge
        idx.append((0, len(P) - 1, 1) if sgn > 0 else (0, 1, len(P) - 1))
        caps.append((P, N, uv, C, np.array(idx)))
    if caps:
        P0, N0, U0, C0, I0 = groups["MAT_Hedge_Side"]
        for P, N, uv, C, I in caps:
            I0 = np.vstack([I0, I + len(P0)])
            P0 = np.vstack([P0, P]); N0 = np.vstack([N0, N]); U0 = np.vstack([U0, uv]); C0 = np.concatenate([C0, C])
        groups["MAT_Hedge_Side"] = (P0, N0, U0, C0, I0)
    return groups


def leaf_cards(seed, grid, round_end=False):
    """2-3 cut-out cards standing out of the front top edge, leaning back toward the camera's view."""
    r = np.random.default_rng(seed + 100)
    lean = math.radians(35.0)
    P, N, U, C, I = [], [], [], [], []
    n = 4 if not round_end else 2
    xs = np.sort(r.uniform(-0.55, 0.55 if not round_end else 0.1, n))
    for k, cx in enumerate(xs):
        w = LEAF_W
        y0 = r.uniform(-0.3, -0.18)
        top = 1.22 + r.uniform(-0.04, 0.02)
        z0 = top - 0.2                                                   # the card's foot sinks into the hedge
        # card plane: width along X, height up and leaning back (+Y) by `lean`
        up = np.array([0.0, math.sin(lean), math.cos(lean)])
        base = np.array([cx, y0, z0])
        q = [base + np.array([-w / 2, 0, 0]), base + np.array([w / 2, 0, 0]),
             base + np.array([w / 2, 0, 0]) + up * LEAF_H, base + np.array([-w / 2, 0, 0]) + up * LEAF_H]
        nrm = np.cross(q[1] - q[0], q[3] - q[0]); nrm /= np.linalg.norm(nrm)
        if nrm[1] > 0:
            nrm = -nrm
        o = len(P)
        P += q; N += [nrm] * 4
        U += [(0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)]           # image top at the card's top
        C += [0.86, 0.86, 1.0, 1.0]
        tri = [(o, o + 1, o + 2), (o, o + 2, o + 3)]
        a, b, c = np.array(q[0]), np.array(q[1]), np.array(q[2])
        if np.cross(b - a, c - a) @ nrm < 0:
            tri = [(t[0], t[2], t[1]) for t in tri]
        I += tri
    return np.array(P), np.array(N), np.array(U), np.array(C), np.array(I)


# ------------------------------------------------------------------ glb writer
def to_yup(P):
    return np.stack([P[:, 0], P[:, 2], -P[:, 1]], 1)


def write_glb(path, name, groups, textures, alpha_mask=("MAT_Hedge_Leaf",)):
    bin_ = bytearray()
    views, accessors = [], []

    def add_view(data, target=None):
        while len(bin_) % 4:
            bin_.append(0)
        off = len(bin_); bin_.extend(data)
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
        if target:
            v["target"] = target
        views.append(v)
        return len(views) - 1

    def add_acc(arr, typ, comp=5126, target=34962, minmax=False):
        arr = np.ascontiguousarray(arr, dtype=np.float32 if comp == 5126 else np.uint32)
        v = add_view(arr.tobytes(), target)
        a = {"bufferView": v, "componentType": comp, "count": int(arr.shape[0]), "type": typ}
        if minmax:
            a["min"] = arr.min(0).tolist(); a["max"] = arr.max(0).tolist()
        accessors.append(a)
        return len(accessors) - 1

    images, texs, mats, prims = [], [], [], []
    for mname, (P, N, UV, C, I) in groups.items():
        if len(I) == 0:
            continue
        png = open(textures[mname], "rb").read()
        iv = add_view(png)
        images.append({"bufferView": iv, "mimeType": "image/png", "name": os.path.splitext(os.path.basename(textures[mname]))[0]})
        texs.append({"sampler": 0, "source": len(images) - 1})
        m = {"name": mname, "pbrMetallicRoughness": {"baseColorTexture": {"index": len(texs) - 1},
                                                     "metallicFactor": 0.0, "roughnessFactor": 0.92}}
        if mname in alpha_mask:
            m["alphaMode"] = "MASK"; m["alphaCutoff"] = 0.5
        mats.append(m)
        col = np.hstack([np.repeat(C[:, None], 3, 1), np.ones((len(C), 1))])
        prims.append({"attributes": {"POSITION": add_acc(to_yup(P), "VEC3", minmax=True),
                                     "NORMAL": add_acc(to_yup(N), "VEC3"),
                                     "TEXCOORD_0": add_acc(UV, "VEC2"),
                                     "COLOR_0": add_acc(col, "VEC4")},
                      "indices": add_acc(I.reshape(-1), "SCALAR", comp=5125, target=34963),
                      "material": len(mats) - 1})
    gltf = {"asset": {"version": "2.0", "generator": "EONLORE build_hedge.py"},
            "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": name, "mesh": 0}],
            "meshes": [{"name": name, "primitives": prims}],
            "materials": mats, "textures": texs, "images": images,
            "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
            "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(bin_)}]}
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(bin_) % 4:
        bin_.append(0)
    total = 12 + 8 + len(js) + 8 + len(bin_)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A)); f.write(js)
        f.write(struct.pack("<II", len(bin_), 0x004E4942)); f.write(bytes(bin_))


# ------------------------------------------------------------------ main
MODULES = [("MV_Hedge_A", 101, False), ("MV_Hedge_B", 202, False), ("MV_Hedge_C", 303, False), ("MV_Hedge_End", 404, True)]


def main(models_dir, tex_dir):
    need = ["MV_Hedge_Top.png", "MV_Hedge_Side.png", "MV_Hedge_Leaf.png"]
    if any(not os.path.exists(os.path.join(tex_dir, n)) for n in need):
        import textures_hedge
        textures_hedge.main(tex_dir)
    textures = {"MAT_Hedge_Top": os.path.join(tex_dir, need[0]), "MAT_Hedge_Side": os.path.join(tex_dir, need[1]),
                "MAT_Hedge_Leaf": os.path.join(tex_dir, need[2])}
    report = {}
    for name, seed, round_end in MODULES:
        grid, d = build_module(seed, round_end)
        groups = mesh_from_grid(grid, d)
        groups["MAT_Hedge_Leaf"] = leaf_cards(seed, grid, round_end)
        path = os.path.join(models_dir, name + ".glb")
        write_glb(path, name, groups, textures)
        tris = sum(len(g[4]) for g in groups.values())
        report[name] = tris
        print("%-14s %4d triangles -> %s" % (name, tris, path))
    return report


if __name__ == "__main__":
    models = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(os.path.join(HERE, "..", "..", ".."))
    tex = sys.argv[2] if len(sys.argv) > 2 else os.path.normpath(os.path.join(HERE, "..", "Textures"))
    main(models, tex)
