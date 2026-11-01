# EONLORE Japanese city kit (JP_Kit) - builder (Blender 5.x, run headless).
#
#   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python build_jp_kit.py
#
# Phase 1 (this file, JP_PHASE=graybox, the default): every building of the kit as proportioned grey blocks
# (plinth, storeys, pent roof, balcony / bay window, main roof with eaves, ridge, ridge ends, openings as dark
# recesses), one prefab per building, exported as glb under the final names so the showcase scene keeps its
# handles when the textured modules replace the blocks in Phase 2. Re-running rebuilds everything from scratch.
#
# Conventions (copied from MV_Kit/_scripts/build_kit.py, not imported: the kits stay independent):
#   1 Blender unit = 1 m, Z up, building fronts face -Y, a prefab's origin is the centre of its front wall at the
#   ground; box-projected UVs in each part's own frame at 32 texels per metre; COL_ proxies are never exported;
#   banded vertex-colour AO in "Col" (exported as COLOR_0, multiplied with albedo by Hydro's mesh.frag).
# Metrics: JP_CITY_MODULAR_PREP.md section 3 (the graybox is where they get locked).
import bpy, bmesh, math, os, sys
from mathutils import Matrix, Vector
from pathlib import Path

ROOT = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\Models\JapaneseCity")
SRC = ROOT / "Source" / "JP_Kit"
TEX = SRC / "Textures"
ASSETS = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets")
PHASE = os.environ.get("JP_PHASE", "graybox")
PPM = 32.0

# ------------------------------------------------------------------ reset
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.name = "JP_Kit"
scene.unit_settings.system = 'METRIC'

ROOTCOL = bpy.data.collections.new("EONLORE_JapaneseCity")
scene.collection.children.link(ROOTCOL)
COL = {}
for n in ("10_Collision", "90_Prefabs"):
    c = bpy.data.collections.new(n); ROOTCOL.children.link(c); COL[n] = c
PREVIEW = bpy.data.collections.new("Preview")
scene.collection.children.link(PREVIEW)

# ------------------------------------------------------------------ graybox materials
# Flat 4 x 4 px colour textures (not material colours: Hydro's importer reads glb textures; these are replaced by the
# real JP_* textures in Phase 2). Colours are the values of the final materials, desaturated, so roofs read dark
# against walls as they will.
GB = {
    "GB_Wall":    (0.72, 0.69, 0.63),   # plaster / boards
    "GB_WallC":   (0.55, 0.52, 0.47),   # poor walls (weathered boards, mud)
    "GB_Wood":    (0.26, 0.22, 0.19),   # cedar frame, posts, railings
    "GB_Roof":    (0.24, 0.27, 0.31),   # kawara
    "GB_RoofC":   (0.38, 0.36, 0.33),   # plank roofs
    "GB_Stone":   (0.50, 0.50, 0.49),   # granite / rough stone
    "GB_Opening": (0.09, 0.08, 0.08),   # door and window recesses
    "GB_Glow":    (0.80, 0.62, 0.36),   # lattice fronts / shoji (lit from inside at dusk)
    "GB_Floor":   (0.42, 0.41, 0.39),
}
TEX.mkdir(parents=True, exist_ok=True)
MAT = {}
for name, rgb in GB.items():
    path = TEX / ("JP_%s.png" % name)
    img = bpy.data.images.new("JP_" + name, 4, 4, alpha=False)
    srgb = [c ** (1 / 2.2) for c in rgb]
    img.pixels = [v for _ in range(16) for v in (*srgb, 1.0)]
    img.filepath_raw = str(path); img.file_format = 'PNG'; img.save()
    m = bpy.data.materials.new("JP_" + name)
    m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes["Principled BSDF"]
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images.load(str(path), check_existing=True)
    t.interpolation = 'Closest'
    nt.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.92
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.15
    if name == "GB_Glow":
        bsdf.inputs["Emission Color"].default_value = (1.0, 0.62, 0.28, 1)
        bsdf.inputs["Emission Strength"].default_value = 0.6
    MAT[name] = m


# ------------------------------------------------------------------ mesh builder (MV kit pattern)
def T(x, y, z):
    return Matrix.Translation((x, y, z))


def R(axis, deg):
    return Matrix.Rotation(math.radians(deg), 4, axis)


class MeshBuilder:
    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []

    def mi(self, m):
        if m not in self.mats:
            self.mats.append(m)
        return self.mats.index(m)

    def poly(self, cos, mat):
        """Any planar polygon (counter-clockwise seen from outside), UV box-projected on its dominant axis, 4 m tiles."""
        vs = [self.bm.verts.new(Vector(c)) for c in cos]
        f = self.bm.faces.new(vs)
        f.material_index = self.mi(mat)
        f.normal_update()
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            x, y, z = loop.vert.co
            u, v = ((y, z), (x, z), (x, y))[ax]
            loop[self.uv].uv = (u / 4.0, v / 4.0)
        return f

    def box(self, x0, y0, z0, x1, y1, z1, mat, M=None, top=None, skip=()):
        M = M or Matrix.Identity(4)
        p = lambda x, y, z: tuple(M @ Vector((x, y, z)))
        F = {
            '-y': [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
            '+y': [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)],
            '-x': [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)],
            '+x': [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
            '+z': [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
            '-z': [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)],
        }
        for k, cos in F.items():
            if k in skip:
                continue
            self.poly([p(*c) for c in cos], top if (k == '+z' and top) else mat)
        return self

    def solid(self, bottom, top_pts, mat, bottom_mat=None, cap=True):
        """A closed solid from a bottom ring and a top ring of the same length (prism, frustum, roof wedge).
        cap=False leaves the top open (a roof skirt whose top is covered by the slopes)."""
        n = len(bottom)
        self.poly(list(reversed(bottom)), bottom_mat or mat)
        for i in range(n):
            a, b = bottom[i], bottom[(i + 1) % n]
            c, d = top_pts[(i + 1) % n], top_pts[i]
            if (Vector(c) - Vector(d)).length < 1e-6:
                self.poly([a, b, c], mat)
            else:
                self.poly([a, b, c, d], mat)
        if cap and len({tuple(round(v, 5) for v in p) for p in top_pts}) >= 3:
            self.poly(list(top_pts), mat)
        return self

    def to_object(self, name, coll, ao=None):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        layer = self.bm.verts.layers.float_color.new("Col")
        for v in self.bm.verts:
            c = ao(v.co) if ao else 1.0
            v[layer] = (c, c, c, 1.0)
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me); self.bm.free()
        me.color_attributes.active_color_name = "Col"
        me.color_attributes.render_color_index = me.color_attributes.find("Col")
        for m in self.mats:
            me.materials.append(MAT[m])
        ob = bpy.data.objects.new(name, me)
        coll.objects.link(ob)
        return ob


def col_box(parent, x0, y0, z0, x1, y1, z1, suffix=""):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = x0 + (v.co.x + 0.5) * (x1 - x0)
        v.co.y = y0 + (v.co.y + 0.5) * (y1 - y0)
        v.co.z = z0 + (v.co.z + 0.5) * (z1 - z0)
    me = bpy.data.meshes.new("COL_" + parent.name + suffix); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new("COL_" + parent.name + suffix, me)
    COL["10_Collision"].objects.link(ob)
    ob.parent = parent; ob.display_type = 'WIRE'; ob.hide_render = True
    return ob


# ------------------------------------------------------------------ metrics (JP_CITY_MODULAR_PREP.md section 3)
TIER = {
    #      plinth, ground floor, upper floor, pent proj, eave, pitch
    "A": dict(plinth=0.45, gf=3.2, uf=2.8, pent=1.0, eave=1.1, pitch=27.0),
    "B": dict(plinth=0.30, gf=3.0, uf=2.3, pent=0.9, eave=0.9, pitch=26.0),
    "C": dict(plinth=0.20, gf=2.6, uf=0.0, pent=0.0, eave=0.55, pitch=25.0),
}
DOOR_H = 2.0          # the medieval kit's door leaf (MV_Door_Wood_A: 1.0 x 2.0 m); characters are 1.6-1.75 m
WALL_T = 0.25
SLAB = 0.18           # roof thickness at the eave (kawara roofs read heavy)
RECESS = 0.1          # openings sit this far back from the wall face


def ao_band(z_floor, z_top):
    """Banded contact AO: darker in the 0.4 m above a floor line and under the eave (2 bands)."""
    def f(co):
        z = co.z
        c = 1.0
        if z - z_floor < 0.4:
            c -= 0.22 * (1 - max(0.0, z - z_floor) / 0.4)
        if z_top - z < 0.5:
            c -= 0.16 * (1 - max(0.0, z_top - z) / 0.5)
        return max(0.6, c)
    return f


# ------------------------------------------------------------------ roofs
def ridge_end_tile(mb, x_outer, ym, z, h, outward, mat, length=0.7, width=0.56):
    """Ridge-end tile as a flared wedge: long at the ridge, narrowing and leaning outward as it rises (onigawara /
    cloud end outline). Low and wide, so from the field camera it reads as an ornament, not a chimney."""
    xi = x_outer - outward * length
    bot = [(min(xi, x_outer), ym - width / 2, z), (max(xi, x_outer), ym - width / 2, z),
           (max(xi, x_outer), ym + width / 2, z), (min(xi, x_outer), ym + width / 2, z)]
    xo = x_outer + outward * 0.12
    tl = length * 0.4
    xt0, xt1 = sorted((xo, xo - outward * tl))
    top = [(xt0, ym - width * 0.42, z + h), (xt1, ym - width * 0.42, z + h), (xt1, ym + width * 0.42, z + h), (xt0, ym + width * 0.42, z + h)]
    mb.solid(bot, top, mat)


def roof_gable(mb, x0, x1, y0, y1, z, pitch, eave, mat, gable_over=None, ridge_end="plain", ridge_mat="GB_Roof"):
    """Gable roof, ridge along X (parallel to the street), over the wall rectangle x0..x1, y0..y1 with its wall top at
    z. eave: overhang beyond the long walls; gable_over: beyond the gable walls (default eave * 0.6)."""
    go = eave * 0.6 if gable_over is None else gable_over
    t = math.tan(math.radians(pitch))
    ym = (y0 + y1) / 2
    rise = (ym - y0 + eave) * t
    ze = z - eave * t                    # eave line height (the roof plane passes z at the wall line)
    X0, X1 = x0 - go, x1 + go
    # each slope is a slab: bottom plane = the roof plane, thickness SLAB
    for s in (-1, 1):
        ye = y0 - eave if s < 0 else y1 + eave
        bottom = [(X0, ye, ze - SLAB), (X1, ye, ze - SLAB), (X1, ym, ze - SLAB + rise), (X0, ym, ze - SLAB + rise)]
        top = [(X0, ye, ze), (X1, ye, ze), (X1, ym, ze + rise), (X0, ym, ze + rise)]
        if s > 0:
            bottom = [bottom[1], bottom[0], bottom[3], bottom[2]]; top = [top[1], top[0], top[3], top[2]]
        mb.solid(bottom, top, mat)
    # gable infill (wall material carries up under the roof)
    for gx in (x0, x1):
        sgn = -1 if gx == x0 else 1
        tri = [(gx, y0, z), (gx, y1, z), (gx, ym, z + (ym - y0) * t)]
        if sgn < 0:
            tri = list(reversed(tri))
        mb.poly(tri, "_GABLE_")
    # ridge (heavy tile ridge) and ridge ends
    zr = ze + rise
    mb.box(X0 - 0.05, ym - 0.22, zr - 0.05, X1 + 0.05, ym + 0.22, zr + 0.32, ridge_mat)
    h = {"plain": 0.3, "cloud": 0.62, "none": 0.0}[ridge_end]
    if h > 0:
        for xe, o in ((X0 - 0.05, -1), (X1 + 0.05, 1)):
            ridge_end_tile(mb, xe, ym, zr + 0.25, h, o, ridge_mat, length=0.55 if ridge_end == "plain" else 0.8)
    return zr


def roof_hipgable(mb, x0, x1, y0, y1, z, pitch, eave, mat, upturn=0.25, ridge_end="cloud"):
    """Irimoya (hip-and-gable): a hip roof whose top third is a small gable. Corners swept up by `upturn`."""
    t = math.tan(math.radians(pitch))
    ym = (y0 + y1) / 2
    hd = (y1 - y0) / 2 + eave
    rise = hd * t
    ze = z - eave * t
    X0, X1, Y0, Y1 = x0 - eave, x1 + eave, y0 - eave, y1 + eave
    inset = hd * 0.55                         # where the hip turns into the small gable
    zr = ze + rise
    # lower hip skirt: ring at eave level (corners lifted) up to a ring at gable-start height
    zg = ze + inset * t
    ring_lo = [(X0, Y0, ze + upturn), (X1, Y0, ze + upturn), (X1, Y1, ze + upturn), (X0, Y1, ze + upturn)]
    ring_hi = [(X0 + inset, Y0 + inset, zg), (X1 - inset, Y0 + inset, zg), (X1 - inset, Y1 - inset, zg), (X0 + inset, Y1 - inset, zg)]
    # mid-edge points of the low ring stay at ze (the sweep is only at the corners)
    lo = []; hi = []
    for i in range(4):
        a, b = ring_lo[i], ring_lo[(i + 1) % 4]
        c, d = ring_hi[i], ring_hi[(i + 1) % 4]
        mid_lo = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, ze)
        mid_hi = ((c[0] + d[0]) / 2, (c[1] + d[1]) / 2, zg)
        lo += [a, mid_lo]; hi += [c, mid_hi]
    under = [(p[0], p[1], p[2] - SLAB) for p in lo]
    mb.solid(under, lo, mat, bottom_mat=mat, cap=False)        # slab edge
    n = len(lo)
    for i in range(n):
        mb.poly([lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]], mat)
    # upper gable roof on the inner rectangle
    gx0, gx1 = X0 + inset, X1 - inset
    for s in (-1, 1):
        ye = Y0 + inset if s < 0 else Y1 - inset
        q = [(gx0 - 0.2, ye, zg), (gx1 + 0.2, ye, zg), (gx1 + 0.2, ym, zr), (gx0 - 0.2, ym, zr)]
        if s > 0:
            q = [q[1], q[0], q[3], q[2]]
        mb.poly(q, mat)
    for gx, sgn in ((gx0, -1), (gx1, 1)):         # small gable faces (lattice/plaster triangle)
        tri = [(gx, Y0 + inset, zg), (gx, Y1 - inset, zg), (gx, ym, zr - 0.1)]
        if sgn < 0:
            tri = list(reversed(tri))
        mb.poly(tri, "GB_Wood")
    mb.box(gx0 - 0.3, ym - 0.24, zr - 0.05, gx1 + 0.3, ym + 0.24, zr + 0.36, mat)   # ridge
    h = {"plain": 0.3, "cloud": 0.66}[ridge_end]
    for xe, o in ((gx0 - 0.3, -1), (gx1 + 0.3, 1)):
        ridge_end_tile(mb, xe, ym, zr + 0.28, h, o, mat, length=0.55 if ridge_end == "plain" else 0.85)
    return zr


def roof_hip(mb, x0, x1, y0, y1, z, pitch, eave, mat, upturn=0.0):
    t = math.tan(math.radians(pitch))
    X0, X1, Y0, Y1 = x0 - eave, x1 + eave, y0 - eave, y1 + eave
    hd = min(X1 - X0, Y1 - Y0) / 2
    ze = z - eave * t
    zr = ze + hd * t
    ym, xm = (Y0 + Y1) / 2, (X0 + X1) / 2
    lo = [(X0, Y0, ze + upturn), (X1, Y0, ze + upturn), (X1, Y1, ze + upturn), (X0, Y1, ze + upturn)]
    if (X1 - X0) >= (Y1 - Y0):
        top = [(X0 + hd, ym, zr), (X1 - hd, ym, zr), (X1 - hd, ym, zr), (X0 + hd, ym, zr)]
    else:
        top = [(xm, Y0 + hd, zr), (xm, Y0 + hd, zr), (xm, Y1 - hd, zr), (xm, Y1 - hd, zr)]
    under = [(p[0], p[1], p[2] - SLAB) for p in lo]
    mb.solid(under, lo, mat, cap=False)
    for i in range(4):
        a, b, c, d = lo[i], lo[(i + 1) % 4], top[(i + 1) % 4], top[i]
        if (Vector(c) - Vector(d)).length < 1e-6:
            mb.poly([a, b, c], mat)
        else:
            mb.poly([a, b, c, d], mat)
    mb.box(min(p[0] for p in top) - 0.1, min(p[1] for p in top) - 0.18, zr - 0.05,
           max(p[0] for p in top) + 0.1, max(p[1] for p in top) + 0.18, zr + 0.25, mat)
    return zr


def pent_roof(mb, x0, x1, y_wall, z_top, proj, pitch, mat, sides=0.0):
    """A pent roof (hisashi) along the front wall: slab from the wall at z_top sloping out to -Y by proj."""
    t = math.tan(math.radians(pitch))
    ze = z_top - proj * t
    a0, a1 = x0 - sides, x1 + sides
    bottom = [(a0, y_wall - proj, ze - 0.14), (a1, y_wall - proj, ze - 0.14), (a1, y_wall, z_top - 0.14), (a0, y_wall, z_top - 0.14)]
    top = [(a0, y_wall - proj, ze), (a1, y_wall - proj, ze), (a1, y_wall, z_top), (a0, y_wall, z_top)]
    mb.solid(bottom, top, mat)
    mb.box(a0, y_wall - proj - 0.08, ze - 0.26, a1, y_wall - proj + 0.14, ze + 0.04, "GB_Roof")   # tile-end row
    return ze


# ------------------------------------------------------------------ prefab helpers
PREFABS = {}          # name -> (object, info)
RULES = {"A": {"pent": True}, "B": {"max_features": 1, "cusped": False}, "C": {"pent": False, "balcony": False, "cusped": False, "cloud": False}}


def finish(mb, name, w, d, h, tier, info, extra_cols=()):
    ob = mb.to_object(name, COL["90_Prefabs"], ao=ao_band(0.0, h))
    col_box(ob, -w / 2, 0, 0, w / 2, d, max(2.0, h * 0.7))
    for i, c in enumerate(extra_cols):
        col_box(ob, *c, suffix="_%d" % i)
    info.update(tier=tier, width=w, depth=d, height=round(h, 2), tris=len(ob.data.polygons))
    PREFABS[name] = (ob, info)
    return ob


def openings(mb, x0, x1, y, z0, kind, bays, door_bays=(), win_h=1.2, win_z=1.0):
    """Front openings per 2 m bay as recessed dark (or glowing) panels: 'lattice' = glowing lattice front over the
    whole bay below the head beam, 'door' = sliding door, 'window' = shoji window."""
    bw = (x1 - x0) / bays
    for i in range(bays):
        bx0, bx1 = x0 + i * bw + 0.15, x0 + (i + 1) * bw - 0.15
        if i in door_bays:
            mb.box(bx0 + 0.15, y - 0.02, z0, bx1 - 0.15, y + RECESS, z0 + DOOR_H, "GB_Opening")
        elif kind == "lattice":
            mb.box(bx0, y - 0.02, z0 + 0.35, bx1, y + RECESS, z0 + 2.3, "GB_Glow")
        elif kind == "window":
            mb.box(bx0 + 0.25, y - 0.02, z0 + win_z, bx1 - 0.25, y + RECESS, z0 + win_z + win_h, "GB_Glow")
    # posts at every bay line, standing 4 cm proud
    for i in range(bays + 1):
        px = x0 + i * bw
        mb.box(px - 0.1, y - 0.06, z0, px + 0.1, y + 0.02, z0 + 2.6 if kind == "lattice" else z0 + 2.4, "GB_Wood")


def machiya(name, tier, bays, depth, floors=2, roof="gable", upper=None, cusped=False, door_bays=(1,), steps=0,
            cloud=False, shop=False, third_bay=None):
    """A tier A/B townhouse / hall: plinth, ground floor (lattice fronts, door), pent roof, low upper floor with
    at most one feature on B (upper = 'bay' | 'balcony' | 'loft'), main roof ('gable' parallel to the street or
    'hipgable'). Origin: centre of the front wall at the ground, front -Y."""
    m = TIER[tier]
    r = RULES[tier]
    if tier == "A":
        assert r["pent"] and floors >= 1, name
    if tier == "B":
        assert cusped == False and cloud == False, "%s: tier B has no cusped gable or cloud ridge ends" % name
    W = bays * 2.0
    D = depth
    x0, x1 = -W / 2, W / 2
    mb = MeshBuilder()
    pl = m["plinth"]
    # plinth (granite) with a step band in front
    mb.box(x0 - 0.15, -0.15, 0, x1 + 0.15, D + 0.15, pl, "GB_Stone")
    if steps:
        sw = steps
        for k in range(2):
            mb.box(-sw / 2 - 0.2 * (1 - k), -0.15 - 0.45 * (2 - k), 0, sw / 2 + 0.2 * (1 - k), -0.15, pl * (k + 1) / 2, "GB_Stone")
    z = pl
    # ground floor
    gf = m["gf"]
    mb.box(x0, 0, z, x1, D, z + gf, "GB_Wall")
    if shop:   # open shop front: the whole front is a recess with a goods platform
        mb.box(x0 + 0.2, -0.02, z, x1 - 0.2, 0.9, z + 2.4, "GB_Opening")
        mb.box(x0 + 0.3, -0.5, z, x1 - 0.3, 0.2, z + 0.45, "GB_Wood")
        mb.box(x0 + 0.1, -0.9, z + 2.4, x1 - 0.1, -0.05, z + 2.5, "GB_Wood")    # raised shutters (as a lid)
        for px in (x0 + 0.1, x1 - 0.3):
            mb.box(px, -0.08, z, px + 0.2, 0.02, z + gf, "GB_Wood")
    else:
        openings(mb, x0, x1, 0.0, z, "lattice", bays, door_bays)
    z_gf_top = z + gf
    mb.box(x0 - 0.04, -0.08, z_gf_top - 0.28, x1 + 0.04, 0.02, z_gf_top - 0.02, "GB_Wood")   # head beam
    # pent roof
    if m["pent"] > 0:
        pent_roof(mb, x0, x1, 0.0, z_gf_top + 0.32, m["pent"], m["pitch"] - 4, "GB_Roof")
    feats = 0
    top = z_gf_top
    if floors >= 2:
        uf = m["uf"]
        z2 = z_gf_top
        mb.box(x0, 0, z2, x1, D, z2 + uf, "GB_Wall")
        # upper floor windows (shoji) in every bay but where a feature sits
        openings(mb, x0, x1, 0.0, z2, "window", bays, (), win_h=1.0 if tier == "B" else 1.2, win_z=0.7)
        if upper == "bay":
            feats += 1
            cx = 0.0 if third_bay is None else x0 + third_bay * 2 + 1
            mb.box(cx - 1.1, -0.55, z2 + 0.55, cx + 1.1, 0.0, z2 + 1.85, "GB_Wood")
            mb.box(cx - 0.95, -0.57, z2 + 0.75, cx + 0.95, -0.5, z2 + 1.65, "GB_Glow")
            mb.box(cx - 1.25, -0.7, z2 + 1.85, cx + 1.25, 0.0, z2 + 2.0, "GB_Roof")
        elif upper == "balcony":
            feats += 1
            bx0, bx1 = (x0 + 0.4, x1 - 0.4) if tier == "A" else (-2.0 if bays > 3 else -1.6, 2.0 if bays > 3 else 1.6)
            bd = 0.6 if tier == "A" else 0.95          # A: shallow, raised over the pent roof, which stays visible below
            bz = z2 + (0.5 if tier == "A" else 0.0)
            mb.box(bx0, -bd, bz, bx1, 0.0, bz + 0.14, "GB_Wood")                         # deck
            mb.box(bx0, -bd - 0.02, bz + 0.14, bx1, -bd + 0.06, bz + 0.95, "GB_Wood")    # railing (lattice)
            for bx in (bx0, bx1 - 0.08):
                mb.box(bx, -bd, bz + 0.14, bx + 0.08, 0.0, bz + 0.95, "GB_Wood")
            if tier == "A":
                for i in range(bays + 1):
                    px = x0 + i * 2.0
                    if bx0 - 0.01 <= px <= bx1 + 0.01:
                        mb.box(px - 0.08, -bd, bz + 0.14, px + 0.08, -bd + 0.16, z2 + uf, "GB_Wood")   # balcony posts
        elif upper == "loft":
            feats += 1
            mb.box(-1.6, -0.03, z2 + 0.6, 1.6, RECESS, z2 + 1.3, "GB_Wood")                # slatted loft window
        top = z2 + uf
    if tier == "B":
        assert feats <= r["max_features"], "%s: tier B has at most one upper feature" % name
    # main roof
    if roof == "gable":
        zr = roof_gable(mb, x0, x1, 0.0, D, top, m["pitch"], m["eave"], "GB_Roof",
                        ridge_end="cloud" if cloud else "plain")
    else:
        zr = roof_hipgable(mb, x0, x1, 0.0, D, top, m["pitch"], m["eave"], "GB_Roof", upturn=0.3 if tier == "A" else 0.0,
                           ridge_end="cloud" if cloud else "plain")
    if cusped:
        assert tier == "A"
        # cusped curved gable (karahafu) facing the street: a small cross-gable on the pent roof over the centre bays
        cw = 4.4
        zc = z_gf_top + 0.32
        t = math.tan(math.radians(32))
        hw = cw / 2 + 0.3
        for s in (-1, 1):
            q = [(0, -1.4, zc + hw * t), (0, 0.4, zc + hw * t), (s * hw, 0.4, zc + 0.05), (s * hw, -1.4, zc + 0.05)]
            if s > 0:
                q = list(reversed(q))
            sub = [(p[0], p[1], p[2] - 0.16) for p in q]
            mb.solid(sub if s < 0 else list(reversed(sub)), q if s < 0 else list(reversed(q)), "GB_Roof")
        mb.poly([(-cw / 2, -1.3, zc), (cw / 2, -1.3, zc), (0, -1.3, zc + (cw / 2) * t)], "GB_Wood")   # gable board
        mb.box(-0.25, -1.55, zc + hw * t - 0.1, 0.25, -1.2, zc + hw * t + 0.5, "GB_Roof")            # crest tile
        for px in (-cw / 2, cw / 2 - 0.22):
            mb.box(px, -1.4, pl, px + 0.22, -1.18, zc, "GB_Wood")                                     # porch posts
    # the gable placeholder material ("_GABLE_") = the wall material
    mb.mats = ["GB_Wall" if mm == "_GABLE_" else mm for mm in mb.mats]
    info = dict(bays=bays, floors=floors, roof=roof, upper=upper, cusped=cusped, ridge=round(zr, 2))
    return finish(mb, name, W, D, zr, tier, info)


def poor_house(name, w, d, roof="kawara_old", door_x=(0.0,), win_x=(), open_front=False, units=1, stilts=0.0,
               water_bay=None, tier="C"):
    """Tier C: single storey on a low rough base (or posts on stones / stilts), simple gable parallel to the street,
    low eaves. roof: 'kawara_old' (25 deg, patched tile) or 'plank' (18 deg, planks weighed with rows of stones)."""
    m = TIER["C"]
    assert units >= 1
    x0, x1 = -w / 2, w / 2
    mb = MeshBuilder()
    base = m["plinth"]
    z = base
    if stilts > 0:
        z = stilts
        for px in [x0 + 0.1 + i * (w - 0.2) / max(1, round(w / 2)) for i in range(round(w / 2) + 1)]:
            for py in (0.1, d / 2, d - 0.1):
                mb.box(px - 0.1, py - 0.1, -1.2, px + 0.1, py + 0.1, z, "GB_Wood")
        if water_bay:
            bx0, bx1 = water_bay
            mb.box(x0, 0, z - 0.15, bx0, d, z, "GB_Wood"); mb.box(bx1, 0, z - 0.15, x1, d, z, "GB_Wood")
            mb.box(bx0, d - 1.2, z - 0.15, bx1, d, z, "GB_Wood")
        else:
            mb.box(x0, 0, z - 0.15, x1, d, z, "GB_Wood")
        mb.box(x0 - 0.1, -1.1, z - 0.15, x1 + 0.1, 0, z - 0.03, "GB_Wood")      # plank walkway in front
    else:
        # posts on flat stones: a low rough-stone base under the sills
        mb.box(x0 - 0.08, -0.08, 0, x1 + 0.08, d + 0.08, base, "GB_Stone")
    eave_z = z + m["gf"]
    # walls
    if open_front:
        mb.box(x0, d - WALL_T, z, x1, d, eave_z, "GB_WallC")
        mb.box(x0, 0, z, x0 + WALL_T, d, eave_z, "GB_WallC")
        mb.box(x1 - WALL_T, 0, z, x1, d, eave_z, "GB_WallC")
        mb.box(x0, -0.02, eave_z - 0.4, x1, 0.2, eave_z, "GB_Wood")                       # head beam over the opening
        mb.box(x0 + 0.4, 0.4, z + 0.3, x1 - 0.4, 0.5, eave_z - 0.5, "GB_Opening")         # dark interior back
    elif water_bay:
        bx0, bx1 = water_bay
        mb.box(x0, 0, z, bx0, d, eave_z, "GB_WallC"); mb.box(bx1, 0, z, x1, d, eave_z, "GB_WallC")
        mb.box(bx0, d - 0.25, z, bx1, d, eave_z, "GB_WallC")
        mb.box(bx0, -0.02, eave_z - 0.5, bx1, 0.2, eave_z, "GB_Wood")
        mb.box(bx0 + 0.1, 0.3, -1.0, bx1 - 0.1, d - 0.3, z - 0.2, "GB_Opening")            # the dark water bay
    else:
        mb.box(x0, 0, z, x1, d, eave_z, "GB_WallC")
    for dx in door_x:
        mb.box(dx - 0.55, -0.02, z, dx + 0.55, RECESS, z + DOOR_H - 0.15, "GB_Glow")       # patched paper door
        mb.box(dx - 0.65, -0.06, z, dx - 0.55, 0.02, z + DOOR_H - 0.05, "GB_Wood")
        mb.box(dx + 0.55, -0.06, z, dx + 0.65, 0.02, z + DOOR_H - 0.05, "GB_Wood")
    for wx in win_x:
        mb.box(wx - 0.4, -0.02, z + 1.0, wx + 0.4, RECESS, z + 1.6, "GB_Opening")          # barred window
        mb.box(wx - 0.55, -0.45, z + 1.72, wx + 0.55, 0.0, z + 1.8, "GB_RoofC")            # small plank awning
    # corner posts
    for px in (x0, x1):
        mb.box(px - 0.09, -0.09, z, px + 0.09, 0.09, eave_z, "GB_Wood")
    if units > 1:
        for i in range(1, units):
            px = x0 + i * w / units
            mb.box(px - 0.08, -0.08, z, px + 0.08, 0.04, eave_z, "GB_Wood")
    # roof
    if roof == "kawara_old":
        zr = roof_gable(mb, x0, x1, 0.0, d, eave_z, 25.0, m["eave"], "GB_Roof", gable_over=0.3, ridge_end="none")
    else:
        zr = roof_gable(mb, x0, x1, 0.0, d, eave_z, 18.0, m["eave"], "GB_RoofC", gable_over=0.3, ridge_end="none",
                        ridge_mat="GB_RoofC")
        # rows of river stones weighing the planks: two rows per slope, low blocks for the graybox
        t = math.tan(math.radians(18.0))
        for frac in (0.3, 0.75):
            for s in (-1, 1):
                yy = (d / 2) - s * (d / 2 + m["eave"]) * (1 - frac)
                zz = eave_z - m["eave"] * t + (d / 2 + m["eave"]) * frac * t
                mb.box(x0 - 0.2, yy - 0.12, zz - 0.02, x1 + 0.2, yy + 0.12, zz + 0.16, "GB_Stone")
    mb.mats = ["GB_WallC" if mm == "_GABLE_" else mm for mm in mb.mats]
    info = dict(roof=roof, units=units, stilts=stilts, ridge=round(zr, 2))
    return finish(mb, name, w, d, zr, tier, info)


# ------------------------------------------------------------------ special buildings
def storehouse(name):
    mb = MeshBuilder()
    w, d = 6.0, 5.0
    mb.box(-w / 2 - 0.2, -0.2, 0, w / 2 + 0.2, d + 0.2, 0.6, "GB_Stone")
    mb.box(-w / 2, 0, 0.6, w / 2, d, 6.2, "GB_Wall")
    mb.box(-0.8, -0.25, 0.6, 0.8, 0.0, 2.8, "GB_Opening")          # heavy door in a deep frame
    mb.box(-1.05, -0.45, 2.8, 1.05, 0.0, 3.1, "GB_Roof")           # little door roof
    for wx in (-1.8, 1.8):
        mb.box(wx - 0.35, -0.12, 4.4, wx + 0.35, 0.0, 5.0, "GB_Opening")
    zr = roof_gable(mb, -w / 2, w / 2, 0.0, d, 6.2, 30.0, 0.7, "GB_Roof", ridge_end="plain")
    mb.mats = ["GB_Wall" if mm == "_GABLE_" else mm for mm in mb.mats]
    return finish(mb, name, w, d, zr, "A", dict(walls="namako (one material)"))


def compound_wall(name, length):
    mb = MeshBuilder()
    mb.box(-length / 2, 0, 0, length / 2, 0.5, 0.7, "GB_Stone")
    mb.box(-length / 2, 0.05, 0.7, length / 2, 0.45, 2.1, "GB_Wall")
    t = math.tan(math.radians(28))
    for s in (-1, 1):   # small tile cap, both slopes
        y_out = -0.25 if s < 0 else 0.75
        q = [(-length / 2, y_out, 2.1), (length / 2, y_out, 2.1), (length / 2, 0.25, 2.1 + 0.5 * t), (-length / 2, 0.25, 2.1 + 0.5 * t)]
        if s > 0:
            q = [q[1], q[0], q[3], q[2]]
        mb.solid([(p[0], p[1], p[2] - 0.1) for p in q], q, "GB_Roof")
    mb.box(-length / 2, 0.15, 2.1 + 0.5 * t - 0.05, length / 2, 0.35, 2.1 + 0.5 * t + 0.15, "GB_Roof")
    ob = mb.to_object(name, COL["90_Prefabs"], ao=ao_band(0.0, 2.6))
    col_box(ob, -length / 2, 0, 0, length / 2, 0.5, 2.2)
    PREFABS[name] = (ob, dict(tier="A", width=length, depth=0.5, height=2.6, tris=len(ob.data.polygons)))
    return ob


def compound_gate(name):
    mb = MeshBuilder()
    w = 3.0
    for px in (-w / 2 - 0.3, w / 2):
        mb.box(px, 0.0, 0, px + 0.3, 0.5, 3.0, "GB_Wood")
    mb.box(-w / 2 - 0.4, -0.05, 2.75, w / 2 + 0.4, 0.55, 3.0, "GB_Wood")
    mb.box(-w / 2, 0.2, 0, w / 2, 0.3, 2.6, "GB_Opening")
    zr = roof_gable(mb, -w / 2 - 0.3, w / 2 + 0.3, 0.0, 0.5, 3.0, 28.0, 0.8, "GB_Roof", gable_over=0.5)
    mb.mats = ["GB_Wood" if mm == "_GABLE_" else mm for mm in mb.mats]
    ob = mb.to_object(name, COL["90_Prefabs"], ao=ao_band(0.0, zr))
    for px in (-w / 2 - 0.3, w / 2):
        col_box(ob, px, 0, 0, px + 0.3, 0.5, 3.0, suffix="_%d" % int(px * 10))
    PREFABS[name] = (ob, dict(tier="A", width=w + 0.6, depth=0.5, height=round(zr, 2), tris=len(ob.data.polygons)))
    return ob


def city_gate(name):
    """Yagura gate (MODEL_Gate_Front_A): two battered granite bases flanking a 4 m x 4.5 m passage, a plaster upper
    storey with arrow slits and a barred central window across the top, a hip roof with upturned corners and ridge
    ornaments, a pent roof over the passage. The two door leaves are separate exports (JP_Rich_CityGate_A_DoorL/R)."""
    mb = MeshBuilder()
    pw, ph = 4.0, 4.5
    bw, bd = 5.0, 7.0
    for s in (-1, 1):
        xi = s * pw / 2
        xo = s * (pw / 2 + bw)
        lo = [(min(xi, xo), 0, 0), (max(xi, xo), 0, 0), (max(xi, xo), bd, 0), (min(xi, xo), bd, 0)]
        batter = 0.9
        hi = [(min(xi, xo) + (batter if s < 0 else 0), batter, ph), (max(xi, xo) - (batter if s > 0 else 0), batter, ph),
              (max(xi, xo) - (batter if s > 0 else 0), bd - batter, ph), (min(xi, xo) + (batter if s < 0 else 0), bd - batter, ph)]
        mb.solid(lo, hi, "GB_Stone")
    # passage ceiling beam and lintel
    mb.box(-pw / 2 - 0.2, 0.6, ph - 0.1, pw / 2 + 0.2, bd - 0.6, ph + 0.3, "GB_Wood")
    for px in (-pw / 2, pw / 2 - 0.35):
        mb.box(px, 0.9, 0, px + 0.35, 1.25, ph, "GB_Wood")                       # door posts
    # upper storey
    ux0, ux1 = -(pw / 2 + bw) + 0.9, (pw / 2 + bw) - 0.9
    uy0, uy1 = 0.9, bd - 0.9
    uz0, uz1 = ph + 0.3, ph + 0.3 + 3.2
    mb.box(ux0, uy0, uz0, ux1, uy1, uz1, "GB_Wall")
    mb.box(ux0 - 0.05, uy0 - 0.08, uz0, ux1 + 0.05, uy0 + 0.02, uz0 + 0.3, "GB_Wood")      # sill beam
    for sx in (-4.2, -2.6, 2.6, 4.2):                                                        # triangular arrow slits
        mb.poly([(sx - 0.25, uy0 - 0.01, uz0 + 1.1), (sx + 0.25, uy0 - 0.01, uz0 + 1.1), (sx, uy0 - 0.01, uz0 + 1.55)], "GB_Opening")
    mb.box(-1.4, uy0 - 0.02, uz0 + 0.9, 1.4, uy0 + RECESS, uz0 + 2.2, "GB_Opening")          # barred central window
    pent_roof(mb, -pw / 2 - 1.2, pw / 2 + 1.2, uy0, uz0 - 0.05, 1.3, 24.0, "GB_Roof")         # roof over the passage
    zr = roof_hip(mb, ux0, ux1, uy0, uy1, uz1, 30.0, 1.2, "GB_Roof", upturn=0.35)
    ridge_x = ((ux1 - ux0) + 2.4) / 2 - (uy1 - uy0 + 2.4) / 2                                # hip ridge half-length
    for xe, o in ((-ridge_x - 0.1, -1), (ridge_x + 0.1, 1)):                                  # ridge ornaments (shachi outline)
        ridge_end_tile(mb, xe, (uy0 + uy1) / 2, zr + 0.2, 0.9, o, "GB_Roof", length=0.8, width=0.5)
    mb.mats = ["GB_Wall" if mm == "_GABLE_" else mm for mm in mb.mats]
    W = pw + 2 * bw
    ob = finish(mb, name, W, bd, zr + 1.1, "A", dict(passage="%.1f x %.1f m" % (pw, ph)))
    # collision: the two bases (the passage stays open)
    for c in ob.children[:]:
        bpy.data.objects.remove(c)
    for s in (-1, 1):
        x0, x1 = sorted((s * pw / 2, s * (pw / 2 + bw)))
        col_box(ob, x0, 0, 0, x1, bd, uz1, suffix="_%s" % ("L" if s < 0 else "R"))
    # door leaves (open-able): hinge at the passage side posts
    for side in ("L", "R"):
        db = MeshBuilder()
        db.box(0.0 if side == "L" else -pw / 2, 0.0, 0, pw / 2 if side == "L" else 0.0, 0.15, ph - 0.12, "GB_Wood")
        for z in (0.8, 2.2, 3.6):
            db.box(0.05 if side == "L" else -pw / 2 + 0.05, -0.04, z, pw / 2 - 0.05 if side == "L" else -0.05, 0.0, z + 0.14, "GB_Stone")   # iron bands
        dob = db.to_object(name + "_Door" + side, COL["90_Prefabs"])
        col_box(dob, 0.0 if side == "L" else -pw / 2, 0, 0, pw / 2 if side == "L" else 0.0, 0.15, ph - 0.12)
        PREFABS[dob.name] = (dob, dict(tier="A", width=pw / 2, depth=0.15, height=ph - 0.12, tris=len(dob.data.polygons),
                                       note="origin at the hinge; place at x = -2 (L) / +2 (R), y = 1.05 from the gate origin"))
    return ob


def gate_wall(name, length=4.0):
    mb = MeshBuilder()
    mb.box(-length / 2, 0, 0, length / 2, 1.6, 1.4, "GB_Stone")
    mb.box(-length / 2, 0.3, 1.4, length / 2, 1.3, 3.6, "GB_Wall")
    mb.box(-0.3, 0.28, 2.4, 0.3, 0.4, 2.9, "GB_Opening")          # small square window
    zr = roof_gable(mb, -length / 2, length / 2, 0.3, 1.3, 3.6, 28.0, 0.5, "GB_Roof", gable_over=0.0, ridge_end="none")
    mb.mats = ["GB_Wall" if mm == "_GABLE_" else mm for mm in mb.mats]
    ob = mb.to_object(name, COL["90_Prefabs"], ao=ao_band(0.0, zr))
    col_box(ob, -length / 2, 0, 0, length / 2, 1.6, 3.6)
    PREFABS[name] = (ob, dict(tier="A", width=length, depth=1.6, height=round(zr, 2), tris=len(ob.data.polygons)))
    return ob


def guard_booth(name):
    mb = MeshBuilder()
    w, d = 2.4, 1.8
    mb.box(-w / 2, 0, 0, w / 2, d, 0.15, "GB_Stone")
    mb.box(-w / 2, d - 0.12, 0.15, w / 2, d, 2.4, "GB_Wood")
    mb.box(-w / 2, 0, 0.15, -w / 2 + 0.12, d, 2.4, "GB_Wood")
    mb.box(w / 2 - 0.12, 0, 0.15, w / 2, d, 2.4, "GB_Wood")
    mb.box(-w / 2, -0.02, 0.15, w / 2, 0.1, 1.0, "GB_Wood")        # half wall at the front (counter)
    zr = roof_gable(mb, -w / 2, w / 2, 0.0, d, 2.4, 24.0, 0.45, "GB_Roof", gable_over=0.3, ridge_end="none")
    mb.mats = ["GB_Wood" if mm == "_GABLE_" else mm for mm in mb.mats]
    return finish(mb, name, w, d, zr, "A", dict(open_front=True))


def fire_watchtower(name):
    mb = MeshBuilder()
    s = 2.4
    hplat = 9.0
    for px in (-s / 2, s / 2 - 0.25):
        for py in (0.0, s - 0.25):
            mb.box(px, py, 0, px + 0.25, py + 0.25, hplat + 2.2, "GB_Wood")
            mb.box(px - 0.1, py - 0.1, 0, px + 0.35, py + 0.35, 0.3, "GB_Stone")
    for z in (3.0, 6.0):
        mb.box(-s / 2, -0.02, z, s / 2, 0.12, z + 0.16, "GB_Wood")
        mb.box(-s / 2, s - 0.12, z, s / 2, s + 0.02, z + 0.16, "GB_Wood")
    mb.box(-s / 2 - 0.4, -0.4, hplat, s / 2 + 0.4, s + 0.4, hplat + 0.18, "GB_Wood")      # platform
    mb.box(-s / 2 - 0.4, -0.44, hplat + 0.18, s / 2 + 0.4, -0.36, hplat + 1.1, "GB_Wood")  # railing front
    mb.box(-0.3, 0.2, hplat + 1.2, 0.3, 0.8, hplat + 1.9, "GB_Stone")                   # bell
    mb.box(0.3, -0.3, 0, 0.75, -0.2, hplat, "GB_Wood")                                    # ladder (as a board)
    zr = roof_hip(mb, -s / 2, s / 2, 0.0, s, hplat + 2.2, 32.0, 0.5, "GB_Roof", upturn=0.1)
    return finish(mb, name, s, s, zr, "B", dict(platform=hplat))


def floor_tile(name, w, d, mat):
    mb = MeshBuilder()
    mb.box(-w / 2, 0, -0.1, w / 2, d, 0.0, mat)
    ob = mb.to_object(name, COL["90_Prefabs"])
    PREFABS[name] = (ob, dict(tier="-", width=w, depth=d, height=0.0, tris=len(ob.data.polygons)))
    return ob


# ------------------------------------------------------------------ the kit's buildings
ROWS = {"A": [], "B": [], "C": []}


def add(row, ob, gap=3.0):
    ROWS[row].append((ob, gap))


# Tier A (rich, incl. the city gate)
add("A", city_gate("JP_Rich_CityGate_A"), 2.0)
add("A", guard_booth("JP_Rich_GuardBooth_A"))
add("A", gate_wall("JP_Rich_GateWall_4m_A"), 2.0)
add("A", machiya("JP_Rich_LanternInn_A", "A", 7, 10.0, roof="hipgable", upper="balcony", cusped=True, door_bays=(3,), steps=4.4, cloud=True))
add("A", machiya("JP_Rich_GuildHall_A", "A", 7, 12.0, roof="hipgable", upper=None, cusped=True, door_bays=(3,), steps=6.0, cloud=True))
add("A", machiya("JP_Rich_Residence_A", "A", 5, 10.0, roof="hipgable", upper="balcony", door_bays=(2,), steps=2.4, cloud=True))
add("A", compound_wall("JP_Rich_CompoundWall_4m_A", 4.0), 0.0)
add("A", compound_gate("JP_Rich_CompoundGate_A"), 0.0)
add("A", storehouse("JP_Rich_Storehouse_A"))
# Tier B (middle: the hero-inn grammar, small)
add("B", machiya("JP_Mid_Townhouse_A", "B", 3, 9.0, upper="bay", door_bays=(1,)))
add("B", machiya("JP_Mid_Townhouse_B", "B", 4, 10.0, upper="balcony", door_bays=(2,)))
add("B", machiya("JP_Mid_Townhouse_C", "B", 3, 8.0, upper="loft", door_bays=(0,)))
add("B", machiya("JP_Mid_Shop_A", "B", 3, 9.0, upper=None, shop=True))
add("B", fire_watchtower("JP_Mid_FireWatchtower_A"))
# Tier C (poor, incl. the harbour sheds)
add("C", poor_house("JP_Poor_House_A", 6.0, 5.0, "kawara_old", door_x=(-1.0,), win_x=(1.6,)))
add("C", poor_house("JP_Poor_House_B", 6.0, 6.0, "plank", door_x=(1.2,), win_x=(-1.4,)))
add("C", poor_house("JP_Poor_Hut_A", 4.0, 4.0, "plank", door_x=(0.6,), win_x=(-1.1,)))
add("C", poor_house("JP_Poor_Hut_B", 5.0, 4.0, "plank", door_x=(-1.2,), win_x=(1.0,)))
add("C", poor_house("JP_Poor_RowHouse_A", 12.0, 5.0, "plank", door_x=(-4.8, -1.8, 1.2, 4.2), win_x=(-3.6, -0.6, 2.4, 5.4), units=4))
add("C", poor_house("JP_Poor_Boathouse_A", 6.0, 7.0, "plank", door_x=(-2.2,), win_x=(), stilts=0.6, water_bay=(-1.2, 2.4)))
add("C", poor_house("JP_Poor_NetShed_A", 5.0, 4.0, "plank", door_x=(), open_front=True))
add("C", poor_house("JP_Poor_FishStore_A", 5.0, 4.0, "plank", door_x=(0.0,), win_x=(-1.7,)))

FLOOR = floor_tile("JP_Showcase_Floor", 120.0, 70.0, "GB_Floor")

# ------------------------------------------------------------------ lay the rows out (the showcase layout)
ROW_Y = {"C": 0.0, "B": 22.0, "A": 44.0}
LAYOUT = {}
for row, items in ROWS.items():
    widths = [PREFABS[o.name][1]["width"] for o, _ in items]
    total = sum(widths) + sum(g for _, g in items[:-1])
    x = -total / 2
    for (o, gap), w in zip(items, widths):
        o.location = (x + w / 2, ROW_Y[row], 0)
        LAYOUT[o.name] = (round(x + w / 2, 2), ROW_Y[row])
        x += w + gap
gate = bpy.data.objects["JP_Rich_CityGate_A"]
for side, dx in (("L", -2.0), ("R", 2.0)):
    d = bpy.data.objects["JP_Rich_CityGate_A_Door" + side]
    d.location = (gate.location.x + dx, gate.location.y + 1.05, 0)
    LAYOUT[d.name] = (round(d.location.x, 2), round(d.location.y, 2))
FLOOR.location = (0, -12, 0)
LAYOUT[FLOOR.name] = (0.0, -12.0)

# ------------------------------------------------------------------ preview: scale cards, light, cameras (not exported)
CHAR_PPM = 38.4


def card(name, path, x, y):
    img = bpy.data.images.load(str(path), check_existing=True)
    w, h = img.size[0] / CHAR_PPM, img.size[1] / CHAR_PPM
    m = bpy.data.materials.new("PV_" + name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = img; t.interpolation = 'Closest'
    nt.links.new(t.outputs["Color"], b.inputs["Base Color"]); nt.links.new(t.outputs["Alpha"], b.inputs["Alpha"])
    b.inputs["Roughness"].default_value = 1.0
    try:
        m.surface_render_method = 'DITHERED'
    except Exception:
        pass
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new("UVMap")
    f = bm.faces.new([bm.verts.new(v) for v in ((-w / 2, 0, 0), (w / 2, 0, 0), (w / 2, 0, h), (-w / 2, 0, h))])
    for l, uv in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        l[uvl].uv = uv
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); me.materials.append(m)
    ob = bpy.data.objects.new(name, me); PREVIEW.objects.link(ob); ob.location = (x, y, 0)
    return ob


PV = SRC / "_preview"
for row, y in ROW_Y.items():
    first = ROWS[row][0][0]
    x = first.location.x - PREFABS[first.name][1]["width"] / 2 - 1.6
    if (PV / "Miko_Idle.png").exists():
        card("PV_Miko_" + row, PV / "Miko_Idle.png", x, y - 1.5)
        card("PV_Villager_" + row, PV / "Blacksmith_Idle.png", x + 1.3, y - 1.5)

world = bpy.data.worlds.new("JP_World"); scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
bg.inputs["Color"].default_value = (0.32, 0.36, 0.42, 1); bg.inputs["Strength"].default_value = 0.9
sun_d = bpy.data.lights.new("PV_Sun", 'SUN'); sun_d.color = (1.0, 0.86, 0.7); sun_d.energy = 3.0; sun_d.angle = math.radians(2)
sun = bpy.data.objects.new("PV_Sun", sun_d); PREVIEW.objects.link(sun)
sun.rotation_euler = (math.radians(52), 0, math.radians(-38))      # key from the upper left of the camera

GAME_FOV = 35.0       # ShrineGlade.hyscene: perspective 35 deg
CAMS = []


def camera(name, target, pitch, dist, yaw=0.0, fov=GAME_FOV):
    cd = bpy.data.cameras.new(name); cd.sensor_fit = 'VERTICAL'; cd.angle = math.radians(fov)
    cam = bpy.data.objects.new(name, cd); PREVIEW.objects.link(cam)
    d = Vector((math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)), math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)),
                -math.sin(math.radians(pitch))))
    cam.location = Vector(target) - d * dist
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    CAMS.append(name)
    return cam


# per row: V1 = the game camera (ShrineGlade framing: about 32 deg down, 35 deg FOV), V2 = closer and lower (street
# level read of the facades), V3 = higher and wider (roofscape, the whole row)
for row, y in ROW_Y.items():
    xs = [o.location.x for o, _ in ROWS[row]]
    cx = (min(xs) + max(xs)) / 2
    span = max(xs) - min(xs) + 16
    camera("PV_%s_V1_Game" % row, (cx, y + 3, 3.0), 32.0, span * 1.25)
    camera("PV_%s_V2_Low" % row, (cx, y + 2, 3.5), 18.0, span * 1.15)
    camera("PV_%s_V3_High" % row, (cx, y + 4, 2.0), 48.0, span * 1.45)
camera("PV_All", (0, 22, 2.0), 40.0, 125.0)
# close-ups of the hero inn and the gate at the game camera's real framing (16 m wide view)
inn = bpy.data.objects["JP_Rich_LanternInn_A"].location
camera("PV_Inn_Game", (inn.x, inn.y + 2, 3.5), 32.0, 34.0)
camera("PV_Gate_Game", (gate.location.x, gate.location.y + 2, 4.0), 32.0, 36.0)
camera("PV_Townhouses_Game", (bpy.data.objects["JP_Mid_Townhouse_B"].location.x, ROW_Y["B"] + 2, 3.0), 32.0, 34.0)
camera("PV_Poor_Game", (bpy.data.objects["JP_Poor_RowHouse_A"].location.x, 2, 2.0), 32.0, 30.0)
scene.camera = bpy.data.objects["PV_All"]

r = scene.render
r.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE_NEXT'
r.resolution_x, r.resolution_y, r.resolution_percentage = 1280, 720, 100
try:
    scene.view_settings.view_transform = 'AgX'
except Exception:
    pass
for k, v in (("use_shadows", True), ("taa_render_samples", 16)):
    try:
        setattr(scene.eevee, k, v)
    except Exception:
        pass

# ------------------------------------------------------------------ export (one glb per prefab, origin at 0)
GLTF_COLOR_ARGS = {}
try:
    _props = bpy.ops.export_scene.gltf.get_rna_type().properties
    if "export_vertex_color" in _props:
        GLTF_COLOR_ARGS["export_vertex_color"] = 'ACTIVE'
        if "export_active_vertex_color_when_no_material" in _props:
            GLTF_COLOR_ARGS["export_active_vertex_color_when_no_material"] = True
    elif "export_colors" in _props:
        GLTF_COLOR_ARGS["export_colors"] = True
except Exception:
    pass
OUT = Path(os.environ["JP_OUT"]) if os.environ.get("JP_OUT") else ROOT
OUT.mkdir(parents=True, exist_ok=True)
EXPORTED = []
for pname, (ob, info) in PREFABS.items():
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    saved = ob.location.copy(); ob.location = (0, 0, 0)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.gltf(filepath=str(OUT / (pname + ".glb")), export_format='GLB', use_selection=True,
                              export_yup=True, export_apply=False, export_materials='EXPORT',
                              export_image_format='AUTO', export_cameras=False, export_lights=False, **GLTF_COLOR_ARGS)
    ob.location = saved
    EXPORTED.append(pname)

SRC.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.environ.get("JP_SAVE") or str(SRC / "JP_Kit.blend"), relative_remap=True)

import json
summary = {n: dict(i, location=LAYOUT.get(n)) for n, (o, i) in PREFABS.items()}
(SRC / "kit_summary.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
print("RESULT::" + json.dumps({"phase": PHASE, "exported": EXPORTED, "cams": CAMS}))
