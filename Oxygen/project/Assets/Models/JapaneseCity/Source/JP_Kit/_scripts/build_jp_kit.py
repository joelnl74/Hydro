# EONLORE Japanese city kit (JP_Kit) - builder (Blender 5.x, run headless).
#
#   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python build_jp_kit.py
#
# Builds the kit's modules (a library in JP_Kit.blend, also exported to Models/JapaneseCity/Modules/ for hand-built
# variants in the city scene), the 24 building prefabs of the three districts (rich incl. the city gate, middle,
# poor incl. the harbour sheds) and, from jp_props.py, the 3D props; exports one glb per prefab / prop into
# Models/JapaneseCity/ and writes kit_summary.json (sizes, faces, lineup positions) for
# scripts/japanese_city/build_kit_showcase.py. Re-running rebuilds everything from scratch.
#
# Conventions (copied from MV_Kit/_scripts/build_kit.py, not imported: the kits stay independent):
#   1 Blender unit = 1 m, Z up, building fronts face -Y; a prefab's origin is the centre of its front wall at the
#   ground (a prop's: its foot); UVs at 32 texels per metre everywhere (axis faces box-projected, roof slopes mapped
#   along the slope so tile courses stay horizontal, turned surfaces mapped around the circumference); COL_ proxies
#   are never exported; banded vertex-colour AO in "Col" (exported as COLOR_0, multiplied with albedo by mesh.frag).
#   Material names are the texture names (the contract with the PixelLab textures in art_incoming/jp_city/textures).
# Metrics: JP_CITY_MODULAR_PREP.md section 3, locked in the Phase 1 graybox (JP_CITY_KIT_PHASE1_GRAYBOX.md).
import bpy, bmesh, math, os, sys, json, random
from mathutils import Matrix, Vector
from pathlib import Path

ROOT = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\Models\JapaneseCity")
SRC = ROOT / "Source" / "JP_Kit"
TEX = SRC / "Textures"
SCRIPTS = Path(__file__).resolve().parent
PPM = 32.0

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.name = "JP_Kit"
scene.unit_settings.system = 'METRIC'
ROOTCOL = bpy.data.collections.new("EONLORE_JapaneseCity")
scene.collection.children.link(ROOTCOL)
COL = {}
for n in ("00_Modules", "09_Props_3D", "10_Collision", "90_Prefabs"):
    c = bpy.data.collections.new(n); ROOTCOL.children.link(c); COL[n] = c
PREVIEW = bpy.data.collections.new("Preview")
scene.collection.children.link(PREVIEW)

# ------------------------------------------------------------------ materials (texture = name; size from the image)
# extra: emissive mask (+ colour and strength), alpha clip, uv rotation
MATDEFS = {
    "JP_Plaster_Warm": {}, "JP_Roof_Kawara": {}, "JP_Roof_KawaraOld": {}, "JP_Roof_Plank": {},
    "JP_Wood_CedarDark": {}, "JP_Wood_Lattice": {"emi": "JP_Wood_Lattice_EMI.png", "strength": 0.35},
    "JP_Wood_Weathered": {}, "JP_Wood_Weathered_V": {"file": "JP_Wood_Weathered.png", "rot": True},
    "JP_Wood_Bamboo": {}, "JP_Gate_DoorStudded": {}, "JP_Net_Alpha": {"alpha": True},
    "JP_Shoji_Paper": {"emi": "JP_Shoji_Paper_EMI.png", "strength": 1.2},
    "JP_Shoji_Patched": {"emi": "JP_Shoji_Patched_EMI.png", "strength": 1.0},
    "JP_Wall_MudStraw": {}, "JP_Wall_Namako": {}, "JP_Stone_Granite": {}, "JP_Stone_Rough": {},
    "JP_Paving_City": {}, "JP_Ground_AlleyDirt": {},
    "JP_Wood_Staves": {}, "JP_Clay_Ochre": {}, "JP_Glaze_Brown": {}, "JP_Cloth_Red": {}, "JP_Drum_Hide": {},
    "JP_Cloth_Indigo": {}, "JP_Roof_KawaraChannel": {}, "JP_Roof_KawaraRoll": {}, "JP_Roof_KawaraOldRoll": {},
    "JP_Mat_Iron": {}, "JP_Mat_Bronze": {}, "JP_Mat_Rope": {}, "JP_Mat_Glass": {},
    "JP_Mat_Opening": {"file": "JP_Mat_Opening.png"},     # the dark behind open shop fronts and window bars
}
GLOW = (1.0, 0.62, 0.28, 1.0)
ROOF_MATS = {"JP_Roof_Kawara", "JP_Roof_KawaraOld", "JP_Roof_Plank", "JP_Roof_KawaraChannel"}
# the round tile rolls on a kawara slope: their own texture (courses along the roll), UV'd along the roll's arc
ROLL_MAT = {"JP_Roof_Kawara": "JP_Roof_KawaraRoll", "JP_Roof_KawaraOld": "JP_Roof_KawaraOldRoll"}
MAT, TEXSIZE, ROT = {}, {}, set()
if (TEX / "JP_Mat_Opening.png").exists() is False:
    img = bpy.data.images.new("JP_Mat_Opening", 4, 4, alpha=False)
    img.pixels = [v for _ in range(16) for v in (0.035, 0.03, 0.03, 1.0)]
    img.filepath_raw = str(TEX / "JP_Mat_Opening.png"); img.file_format = 'PNG'; img.save()
for name, ex in MATDEFS.items():
    path = TEX / ex.get("file", name + ".png")
    img = bpy.data.images.load(str(path), check_existing=True)
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes["Principled BSDF"]
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = img; t.interpolation = 'Closest'
    nt.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.92
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.12
    if ex.get("alpha"):
        rnd = nt.nodes.new("ShaderNodeMath"); rnd.operation = 'ROUND'          # glTF exporter: alpha MASK, cutoff 0.5
        nt.links.new(t.outputs["Alpha"], rnd.inputs[0])
        nt.links.new(rnd.outputs[0], bsdf.inputs["Alpha"])
        try:
            m.surface_render_method = 'DITHERED'
        except Exception:
            pass
        m.use_backface_culling = False
    if "emi" in ex:
        e = nt.nodes.new("ShaderNodeTexImage"); e.image = bpy.data.images.load(str(TEX / ex["emi"]), check_existing=True)
        e.interpolation = 'Closest'
        mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = 'RGBA'; mix.blend_type = 'MULTIPLY'
        mix.inputs["Factor"].default_value = 1.0
        nt.links.new(e.outputs["Color"], mix.inputs[6]); mix.inputs[7].default_value = GLOW
        nt.links.new(mix.outputs[2], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = ex["strength"]
    MAT[name] = m
    TEXSIZE[name] = (img.size[0] / PPM, img.size[1] / PPM)
    if ex.get("rot"):
        ROT.add(name)


# ------------------------------------------------------------------ mesh builder
def T(x, y, z):
    return Matrix.Translation((x, y, z))


def R(axis, deg):
    return Matrix.Rotation(math.radians(deg), 4, axis)


class MeshBuilder:
    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []
        self.tint = {}

    def mi(self, m):
        if m not in self.mats:
            self.mats.append(m)
        return self.mats.index(m)

    def poly(self, cos, mat, uvs=None):
        """A planar polygon. UVs at 32 texels/m: given, or roof slopes along the slope (courses horizontal),
        everything else box-projected on its dominant axis (as the MV kit)."""
        vs = [self.bm.verts.new(Vector(c)) for c in cos]
        try:
            f = self.bm.faces.new(vs)
        except ValueError:
            return None
        f.material_index = self.mi(mat)
        f.normal_update()
        W, H = TEXSIZE.get(mat, (4.0, 4.0))
        n = f.normal
        for i, loop in enumerate(f.loops):
            p = loop.vert.co
            if uvs is not None:
                u, v = uvs[i]
            elif mat in ROOF_MATS and abs(n.z) < 0.995:
                t = Vector((-n.y, n.x, 0)).normalized()
                b = n.cross(t).normalized()
                if b.z < 0:
                    b = -b
                u, v = p.dot(t), p.dot(b)
            else:
                ax = max(range(3), key=lambda k: abs(n[k]))
                if ax == 1:
                    u, v = (p.x if n.y < 0 else -p.x), p.z
                elif ax == 0:
                    u, v = (p.y if n.x > 0 else -p.y), p.z
                else:
                    u, v = p.x, (p.y if n.z > 0 else -p.y)
            if mat in ROT:
                u, v = v, u
            loop[self.uv].uv = (u / W, v / H)
        return f

    def box(self, x0, y0, z0, x1, y1, z1, mat, M=None, top=None, skip=(), front=None):
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
            m = top if (k == '+z' and top) else (front if (k == '-y' and front) else mat)
            self.poly([p(*c) for c in cos], m)
        return self

    def solid(self, bottom, top_pts, mat, bottom_mat=None, cap=True, side_mat=None):
        n = len(bottom)
        self.poly(list(reversed(bottom)), bottom_mat or mat)
        for i in range(n):
            a, b = bottom[i], bottom[(i + 1) % n]
            c, d = top_pts[(i + 1) % n], top_pts[i]
            if (Vector(c) - Vector(d)).length < 1e-6:
                self.poly([a, b, c], side_mat or mat)
            else:
                self.poly([a, b, c, d], side_mat or mat)
        if cap and len({tuple(round(v, 5) for v in p) for p in top_pts}) >= 3:
            self.poly(list(top_pts), mat)
        return self

    def revolve(self, profile, mat, sides=10, M=None, cap_top=None, cap_bottom=None, phase=0.0):
        """Turned solid around local Z from a profile [(radius, z), ...] bottom to top. UV u = arc length."""
        M = M or Matrix.Identity(4)
        W, H = TEXSIZE.get(mat, (2.0, 2.0))
        ring = []
        for (r, z) in profile:
            ring.append([M @ Vector((r * math.cos(phase + 2 * math.pi * i / sides), r * math.sin(phase + 2 * math.pi * i / sides), z))
                         for i in range(sides + 1)])
        for j in range(len(profile) - 1):
            (r0, z0), (r1, z1) = profile[j], profile[j + 1]
            for i in range(sides):
                cos = [ring[j][i], ring[j][i + 1], ring[j + 1][i + 1], ring[j + 1][i]]
                vs = [self.bm.verts.new(c) for c in cos]
                try:
                    f = self.bm.faces.new(vs)
                except ValueError:
                    continue
                f.material_index = self.mi(mat)
                rm = (r0 + r1) / 2
                u0, u1 = rm * 2 * math.pi * i / sides, rm * 2 * math.pi * (i + 1) / sides
                for loop, (u, v) in zip(f.loops, ((u0, z0), (u1, z0), (u1, z1), (u0, z1))):
                    if mat in ROT:
                        u, v = v, u
                    loop[self.uv].uv = (u / W, v / H)
        for cap, j, rev in ((cap_bottom, 0, True), (cap_top, len(profile) - 1, False)):
            if cap and profile[j][0] > 1e-4:
                pts = [tuple(c) for c in ring[j][:sides]]
                self.poly(list(reversed(pts)) if rev else pts, cap)
        return self

    def tube(self, p0, p1, r, mat, sides=6, cap=None):
        """A pole between two points (bamboo, shafts, rails)."""
        p0, p1 = Vector(p0), Vector(p1)
        d = p1 - p0
        L = d.length
        q = d.normalized().to_track_quat('Z', 'Y')
        M = T(*p0) @ q.to_matrix().to_4x4()
        return self.revolve([(r, 0.0), (r, L)], mat, sides, M, cap_top=cap or mat, cap_bottom=cap or mat)

    def to_object(self, name, coll, ao=None):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        # a face-corner colour: the AO band times the material's tint (MeshBuilder.tint, per material name), so one
        # plaster texture gives cream, grey and ochre houses (kit v3, Nov 1) without extra textures
        layer = self.bm.loops.layers.float_color.new("Col")
        for f in self.bm.faces:
            t = self.tint.get(self.mats[f.material_index], (1.0, 1.0, 1.0))
            for loop in f.loops:
                c = ao(loop.vert.co) if ao else 1.0
                loop[layer] = (c * t[0], c * t[1], c * t[2], 1.0)
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


def ao_band(z_floor, z_top, floors=()):
    """Banded contact AO (2 bands): darker in the 0.4 m above the ground and above each floor line, and under the eave."""
    def f(co):
        z = co.z
        c = 1.0
        for zf in (z_floor,) + tuple(floors):
            if 0.0 <= z - zf < 0.4:
                c -= (0.22 if zf == z_floor else 0.12) * (1 - (z - zf) / 0.4)
        if 0.0 <= z_top - z < 0.5:
            c -= 0.14 * (1 - (z_top - z) / 0.5)
        return max(0.62, c)
    return f


# ------------------------------------------------------------------ metrics
TIER = {
    "A": dict(plinth=0.45, gf=3.2, uf=2.8, pent=1.0, eave=1.1, pitch=27.0),
    "B": dict(plinth=0.30, gf=3.0, uf=2.3, pent=0.9, eave=0.9, pitch=26.0),
    "C": dict(plinth=0.20, gf=2.6, uf=0.0, pent=0.0, eave=0.55, pitch=25.0),
}
DOOR_H = 2.0
SLAB = 0.18
RECESS = 0.1
POST = 0.22
CEDAR, PLASTER, KAWARA, GRANITE = "JP_Wood_CedarDark", "JP_Plaster_Warm", "JP_Roof_Kawara", "JP_Stone_Granite"
rng = random.Random(1729)


# ------------------------------------------------------------------ roof parts
ROLL_SPACING, ROLL_W, ROLL_H = 0.32, 0.2, 0.09
CHANNEL = {"JP_Roof_Kawara": "JP_Roof_KawaraChannel"}   # the slope under the rolls: the tiles two steps darker


def chan(mat):
    return CHANNEL.get(mat, mat)
ROLLED = {"JP_Roof_Kawara", "JP_Roof_KawaraOld"}


def tile_rolls(mb, poly, mat):
    """Round-tile relief (Nov 1 pass, after the OT2 / wuxia HD-2D references): the courses of a clay-tile roof are
    real geometry, a low ridge running down the slope every 0.28 m, so every row throws a line of shadow at the
    field camera instead of reading as a flat painted slab. poly: the slope face, its first two points on the eave."""
    if mat not in ROLLED:
        return
    P = [Vector(p) for p in poly]
    e0, e1 = P[0], P[1]
    u = (e1 - e0)
    if u.length < 0.3:
        return
    u.normalize()
    far = max(P[2:], key=lambda q: (q - e0).cross(u).length)
    n = u.cross(far - e0).normalized()
    if n.z < 0:
        n = -n
    v = n.cross(u).normalized()
    if (far - e0).dot(v) < 0:
        v = -v
    pts2 = [((q - e0).dot(u), (q - e0).dot(v)) for q in P]
    umin, umax = min(p[0] for p in pts2), max(p[0] for p in pts2)
    m = len(pts2)
    k = umin + ROLL_SPACING / 2
    prof = [(-ROLL_W / 2, -0.01), (-ROLL_W / 4, ROLL_H * 0.78), (0.0, ROLL_H), (ROLL_W / 4, ROLL_H * 0.78), (ROLL_W / 2, -0.01)]
    # UVs (Nov 1, after the game view showed streaked rolls): u = distance around the roll's arc, v = up the slope, both
    # in metres, so the roll texture's courses run across every roll at 32 texels/m. The slope projection used before
    # turned the steep sides of a roll sideways and squeezed the kawara pattern into chevrons.
    rmat = ROLL_MAT.get(mat, mat)
    arc = [0.0]
    for j in range(len(prof) - 1):
        arc.append(arc[-1] + math.hypot(prof[j + 1][0] - prof[j][0], prof[j + 1][1] - prof[j][1]))
    while k < umax - ROLL_SPACING / 4:
        vs = []
        for i in range(m):
            (a0, b0), (a1, b1) = pts2[i], pts2[(i + 1) % m]
            if (a0 - k) * (a1 - k) <= 0 and abs(a1 - a0) > 1e-6:
                vs.append(b0 + (b1 - b0) * (k - a0) / (a1 - a0))
        if len(vs) >= 2:
            v0, v1 = min(vs), max(vs) - 0.04
            if v1 - v0 > 0.2:
                rows = []
                for vv in (v0, v1):
                    rows.append([e0 + u * (k + du) + v * vv + n * dh for (du, dh) in prof])
                for j in range(len(prof) - 1):
                    a0, a1 = k + arc[j], k + arc[j + 1]
                    mb.poly([tuple(rows[0][j]), tuple(rows[0][j + 1]), tuple(rows[1][j + 1]), tuple(rows[1][j])], rmat,
                            uvs=[(a0, v0), (a1, v0), (a1, v1), (a0, v1)])
                mb.poly([tuple(q) for q in reversed(rows[0])], rmat, uvs=[(k + du, dh) for (du, dh) in reversed(prof)])
        k += ROLL_SPACING


def swept_ring(X0, X1, Y0, Y1, z, upturn, seg=6, power=2.4):
    """An eave ring of 4 * seg points, corners swept up by `upturn` along a curve (flat in the middle of a side)."""
    corners = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
    ring = []
    for i in range(4):
        (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 4]
        for j in range(seg):
            f = j / seg
            d = abs(2 * f - 1)
            ring.append((ax + (bx - ax) * f, ay + (by - ay) * f, z + upturn * d ** power))
    return ring


def inner_ring(X0, X1, Y0, Y1, z, seg=6):
    corners = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
    ring = []
    for i in range(4):
        (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 4]
        for j in range(seg):
            f = j / seg
            ring.append((ax + (bx - ax) * f, ay + (by - ay) * f, z))
    return ring


def tile_ends(mb, a, b, z_a, z_b, out, mat=KAWARA, step=0.27, r=0.085):
    """Row of round tile ends (small 6-sided discs) from a to b (2D points) along an eave, facing `out` (2D unit)."""
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    L = (b - a).length
    n = max(1, int(L / step))
    ang = math.degrees(math.atan2(out[1], out[0])) + 90.0      # the disc's axis turned to face `out` (local -Y)
    for i in range(n + 1):
        t = i / n
        p = a + (b - a) * t
        z = z_a + (z_b - z_a) * t
        M = T(p.x, p.y, z - r * 0.4) @ R('Z', ang) @ R('X', 90)
        mb.revolve([(r, -0.02), (r, 0.1)], mat, 6, M, cap_top=mat, cap_bottom=mat)


def ridge_end_tile(mb, x_outer, ym, z, h, outward, mat, length=0.7, width=0.56):
    """Ridge-end tile as a flared wedge leaning outward (onigawara / cloud-end outline): an ornament, never a chimney."""
    xi = x_outer - outward * length
    bot = [(min(xi, x_outer), ym - width / 2, z), (max(xi, x_outer), ym - width / 2, z),
           (max(xi, x_outer), ym + width / 2, z), (min(xi, x_outer), ym + width / 2, z)]
    xo = x_outer + outward * 0.12
    xt0, xt1 = sorted((xo, xo - outward * length * 0.4))
    top = [(xt0, ym - width * 0.42, z + h), (xt1, ym - width * 0.42, z + h), (xt1, ym + width * 0.42, z + h), (xt0, ym + width * 0.42, z + h)]
    mb.solid(bot, top, mat)


def roof_gable(mb, x0, x1, y0, y1, z, pitch, eave, mat, gable_over=None, ridge_end="plain", gable_mat=PLASTER,
               ends=True, barge=CEDAR, slab=SLAB, free=(True, True)):
    """Gable roof, ridge along X, over the wall rectangle x0..x1, y0..y1 (wall top at z). free: whether the left /
    right gable end is free (overhang, barge board, ridge-end tile) or attached to the next house of a row (the roof
    stops exactly at the party wall, so the neighbours' roofs join into one)."""
    go = eave * 0.6 if gable_over is None else gable_over
    t = math.tan(math.radians(pitch))
    ym = (y0 + y1) / 2
    rise = (ym - y0 + eave) * t
    ze = z - eave * t
    X0, X1 = x0 - (go if free[0] else 0.0), x1 + (go if free[1] else 0.0)
    for s in (-1, 1):
        ye = y0 - eave if s < 0 else y1 + eave
        bottom = [(X0, ye, ze - slab), (X1, ye, ze - slab), (X1, ym, ze - slab + rise), (X0, ym, ze - slab + rise)]
        top = [(X0, ye, ze), (X1, ye, ze), (X1, ym, ze + rise), (X0, ym, ze + rise)]
        if s > 0:
            bottom = [bottom[1], bottom[0], bottom[3], bottom[2]]; top = [top[1], top[0], top[3], top[2]]
        mb.solid(bottom, top, chan(mat), bottom_mat=CEDAR if mat != "JP_Roof_Plank" else "JP_Wood_Weathered", side_mat=CEDAR)
        tile_rolls(mb, top, mat)
        if ends:
            tile_ends(mb, (X1, ye), (X0, ye), ze, ze, (0, s))
    for gx in (x0, x1):
        # a thin closed wedge, not a lone triangle: a single face sharing the wall's top edge got its normal turned
        # inwards by the normals pass (the gable end showed from inside, Nov 1)
        o = -1.0 if gx == x0 else 1.0
        tri_in = [(gx - o * 0.04, y0 + 0.01, z + 0.01), (gx - o * 0.04, y1 - 0.01, z + 0.01), (gx - o * 0.04, ym, z + (ym - y0) * t - 0.01)]
        tri_out = [(gx + o * 0.015, py, pz) for (_, py, pz) in tri_in]
        mb.solid(tri_in, tri_out, gable_mat, bottom_mat=gable_mat, side_mat=CEDAR)
        if barge and free[0 if gx == x0 else 1]:   # barge boards under the roof edge at the free gable ends
            bx = X0 if gx == x0 else X1
            for s in (-1, 1):
                ye = y0 - eave if s < 0 else y1 + eave
                L = math.hypot(ym - ye, rise)
                ang = math.degrees(math.atan2(rise, abs(ym - ye)))
                M = T(bx, ye, ze - slab) @ R('Z', 0 if s < 0 else 180) @ R('X', ang)
                if s > 0:
                    M = T(bx, ye, ze - slab) @ R('Z', 180) @ R('X', ang)
                mb.box(-0.05, 0, -0.22, 0.05, L, 0.0, barge, M=M)
    zr = ze + rise
    rx0, rx1 = X0 - (0.05 if free[0] else 0.0), X1 + (0.05 if free[1] else 0.0)
    mb.box(rx0, ym - 0.2, zr - 0.05, rx1, ym + 0.2, zr + 0.3, mat)
    h = {"plain": 0.3, "cloud": 0.62, "none": 0.0}[ridge_end]
    if h > 0:
        for xe, o, fr in ((rx0, -1, free[0]), (rx1, 1, free[1])):
            if fr:
                ridge_end_tile(mb, xe, ym, zr + 0.22, h, o, mat, length=0.55 if ridge_end == "plain" else 0.8)
    return zr


def roof_hipgable(mb, x0, x1, y0, y1, z, pitch, eave, mat, upturn=0.25, ridge_end="cloud", seg=6):
    """Irimoya (hip-and-gable): eaves swept up toward the corners along a curve, tile rolls on every slope, tile ends
    along every eave, a latticed gable and a heavy ridge with ridge-end tiles."""
    t = math.tan(math.radians(pitch))
    ym = (y0 + y1) / 2
    hd = (y1 - y0) / 2 + eave
    rise = hd * t
    ze = z - eave * t
    X0, X1, Y0, Y1 = x0 - eave, x1 + eave, y0 - eave, y1 + eave
    inset = hd * 0.55
    zr = ze + rise
    zg = ze + inset * t
    lo = swept_ring(X0, X1, Y0, Y1, ze, upturn, seg)
    hi = inner_ring(X0 + inset, X1 - inset, Y0 + inset, Y1 - inset, zg, seg)
    n = len(lo)
    under = [(p[0], p[1], p[2] - SLAB) for p in lo]
    mb.solid(under, lo, mat, bottom_mat=CEDAR, cap=False, side_mat=CEDAR)
    for i in range(n):
        q = [lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]]
        mb.poly(q, chan(mat))
        tile_rolls(mb, q, mat)
        a, b = lo[i], lo[(i + 1) % n]
        side = i // seg
        out = [(0, -1), (1, 0), (0, 1), (-1, 0)][side]
        tile_ends(mb, a[:2], b[:2], a[2], b[2], out)
    gx0, gx1 = X0 + inset, X1 - inset
    for s_ in (-1, 1):
        ye = Y0 + inset if s_ < 0 else Y1 - inset
        q = [(gx0 - 0.2, ye, zg), (gx1 + 0.2, ye, zg), (gx1 + 0.2, ym, zr), (gx0 - 0.2, ym, zr)]
        if s_ > 0:
            q = [q[1], q[0], q[3], q[2]]
        mb.poly(q, chan(mat))
        tile_rolls(mb, q, mat)
    for gx, sgn in ((gx0, -1), (gx1, 1)):
        tri = [(gx, Y0 + inset, zg), (gx, Y1 - inset, zg), (gx, ym, zr - 0.1)]
        mb.poly(list(reversed(tri)) if sgn < 0 else tri, "JP_Wood_Lattice")      # latticed gable (kitsune-goshi)
    mb.box(gx0 - 0.3, ym - 0.24, zr - 0.05, gx1 + 0.3, ym + 0.24, zr + 0.42, mat)
    mb.box(gx0 - 0.25, ym - 0.3, zr - 0.05, gx1 + 0.25, ym + 0.3, zr + 0.12, mat)          # stepped ridge base
    h = {"plain": 0.32, "cloud": 0.8}[ridge_end]
    for xe, o in ((gx0 - 0.3, -1), (gx1 + 0.3, 1)):
        ridge_end_tile(mb, xe, ym, zr + 0.32, h, o, mat, length=0.55 if ridge_end == "plain" else 0.95, width=0.62)
    return zr


def roof_hip(mb, x0, x1, y0, y1, z, pitch, eave, mat, upturn=0.0, ends=True, seg=6):
    t = math.tan(math.radians(pitch))
    X0, X1, Y0, Y1 = x0 - eave, x1 + eave, y0 - eave, y1 + eave
    hd = min(X1 - X0, Y1 - Y0) / 2
    ze = z - eave * t
    zr = ze + hd * t
    ym, xm = (Y0 + Y1) / 2, (X0 + X1) / 2
    lo = swept_ring(X0, X1, Y0, Y1, ze, upturn, seg)
    if (X1 - X0) >= (Y1 - Y0):
        ra, rb = Vector((X0 + hd, ym, zr)), Vector((X1 - hd, ym, zr))
    else:
        ra, rb = Vector((xm, Y0 + hd, zr)), Vector((xm, Y1 - hd, zr))
    under = [(p[0], p[1], p[2] - SLAB) for p in lo]
    mb.solid(under, lo, mat, bottom_mat=CEDAR, cap=False, side_mat=CEDAR)
    n = len(lo)
    outs = [(0, -1), (1, 0), (0, 1), (-1, 0)]
    for side in range(4):
        pts = [lo[(side * seg + j) % n] for j in range(seg + 1)]
        # the ridge points this side climbs to: the front and back sides span the whole ridge, the ends meet one tip
        tips = {0: (ra, rb), 1: (rb, rb), 2: (rb, ra), 3: (ra, ra)} if (X1 - X0) >= (Y1 - Y0) else \
               {0: (ra, ra), 1: (ra, rb), 2: (rb, rb), 3: (rb, ra)}
        ta, tb = tips[side]
        face = [pts[0], pts[-1], tuple(tb), tuple(ta)] if (ta - tb).length > 1e-6 else [pts[0], pts[-1], tuple(ta)]
        for j in range(seg):
            f0, f1 = j / seg, (j + 1) / seg
            top0, top1 = ta.lerp(tb, f0), ta.lerp(tb, f1)
            q = [pts[j], pts[j + 1], tuple(top1), tuple(top0)]
            if (top1 - top0).length < 1e-6:
                q = [pts[j], pts[j + 1], tuple(top0)]
            mb.poly(q, chan(mat))
        tile_rolls(mb, face, mat)
        if ends:
            for j in range(seg):
                tile_ends(mb, pts[j][:2], pts[j + 1][:2], pts[j][2], pts[j + 1][2], outs[side])
    mb.box(min(ra.x, rb.x) - 0.15, min(ra.y, rb.y) - 0.2, zr - 0.05, max(ra.x, rb.x) + 0.15, max(ra.y, rb.y) + 0.2, zr + 0.3, mat)
    return zr


def pent_roof(mb, x0, x1, y_wall, z_top, proj, pitch, mat=KAWARA, sides=0.0, ends=True):
    """Pent roof (hisashi) along a front wall: slab from the wall at z_top sloping out to -Y by proj, tile-end row."""
    t = math.tan(math.radians(pitch))
    ze = z_top - proj * t
    a0, a1 = x0 - sides, x1 + sides
    bottom = [(a0, y_wall - proj, ze - 0.14), (a1, y_wall - proj, ze - 0.14), (a1, y_wall, z_top - 0.14), (a0, y_wall, z_top - 0.14)]
    top = [(a0, y_wall - proj, ze), (a1, y_wall - proj, ze), (a1, y_wall, z_top), (a0, y_wall, z_top)]
    mb.solid(bottom, top, chan(mat), bottom_mat=CEDAR, side_mat=CEDAR)
    tile_rolls(mb, top, mat)
    mb.box(a0, y_wall - proj - 0.02, ze - 0.2, a1, y_wall - proj + 0.1, ze - 0.06, CEDAR)       # fascia
    if ends:
        tile_ends(mb, (a1, y_wall - proj), (a0, y_wall - proj), ze, ze, (0, -1))
    for px in [a0 + 0.3 + i * 2.0 for i in range(int((a1 - a0) / 2.0) + 1)]:                   # rafter-end brackets
        if px < a1 - 0.2:
            mb.box(px - 0.06, y_wall - proj + 0.1, ze - 0.24, px + 0.06, y_wall, ze - 0.12, CEDAR)
    return ze


# ------------------------------------------------------------------ wall modules (write into a MeshBuilder)
def m_post(mb, x, y, z0, z1, s=POST, mat=CEDAR):
    mb.box(x - s / 2, y - s / 2, z0, x + s / 2, y + s / 2, z1, mat)


def m_plinth(mb, x0, x1, y0, y1, h, mat=GRANITE):
    mb.box(x0, y0, 0, x1, y1, h, mat)


def m_front_lattice(mb, x0, x1, y, z, h=2.6):
    """A 2 m lattice shop/house front bay (koshi): kickboard, recessed glowing lattice panel, head rail."""
    mb.box(x0, y - 0.02, z, x1, y + 0.06, z + 0.35, CEDAR)
    mb.box(x0, y + 0.02, z + 0.35, x1, y + 0.12, z + h - 0.25, "JP_Wood_Lattice")
    mb.box(x0, y - 0.04, z + h - 0.25, x1, y + 0.06, z + h, CEDAR)


def m_door_lattice(mb, cx, y, z, w=1.6, deep=0.6):
    """Entrance (Nov 1 pass, after the street references): a deep recess lit from inside (a glowing paper screen at
    the back), the lattice sliding door half open, thick cedar jambs and lintel, a granite step in front. So the door
    reads as a door, not as one more lattice bay. The noren is a sprite card hung at the head beam in the scene."""
    mb.box(cx - w / 2, y + deep, z, cx + w / 2, y + deep + 0.05, z + DOOR_H, "JP_Shoji_Paper")              # lit inside
    mb.box(cx - w / 2, y + 0.02, z - 0.01, cx + w / 2, y + deep, z + 0.01, GRANITE)                          # floor
    for sx in (-1, 1):                                                                                        # recess sides
        a, b = sorted((cx + sx * w / 2, cx + sx * (w / 2 - 0.04)))
        mb.box(a, y, z, b, y + deep, z + DOOR_H, "JP_Mat_Opening")
    mb.box(cx - w / 2, y + 0.18, z, cx + 0.06, y + 0.24, z + DOOR_H, "JP_Wood_Lattice")                     # door slid half open
    mb.box(cx - w / 2 - 0.14, y - 0.06, z, cx - w / 2, y + deep, z + DOOR_H + 0.18, CEDAR)                  # jambs
    mb.box(cx + w / 2, y - 0.06, z, cx + w / 2 + 0.14, y + deep, z + DOOR_H + 0.18, CEDAR)
    mb.box(cx - w / 2 - 0.2, y - 0.1, z + DOOR_H, cx + w / 2 + 0.2, y + deep, z + DOOR_H + 0.24, CEDAR)   # lintel
    mb.box(cx - w / 2 - 0.1, y - 0.42, z - 0.16, cx + w / 2 + 0.1, y + 0.02, z - 0.01, GRANITE)            # step


def m_door_canopy(mb, cx, w, proj, ze, pitch=34.0):
    """A small gabled roof over a door (kirizuma hisashi), gable end to the street, sitting on the pent roof: it marks
    the entrance from the field camera. Eaves at the pent roof's eave height ze, out to -(proj + 0.25)."""
    half = w / 2 + 0.4
    y0, y1 = 0.0, -(proj + 0.25)
    zc = ze + 0.04
    zr = zc + half * math.tan(math.radians(pitch))
    for sx in (-1, 1):
        xe = cx + sx * half
        top = [(xe, y1, zc), (xe, y0, zc), (cx, y0, zr), (cx, y1, zr)]
        if sx > 0:
            top = [top[1], top[0], top[3], top[2]]
        bottom = [(px, py, pz - 0.12) for (px, py, pz) in top]
        mb.solid(bottom, top, chan(KAWARA), bottom_mat=CEDAR, side_mat=CEDAR)
        tile_rolls(mb, top, KAWARA)
        L = half / math.cos(math.radians(pitch)) + 0.05                          # barge board
        M = (T(xe, y1 - 0.03, zc - 0.06) @ R('Y', -pitch)) if sx < 0 else (T(cx, y1 - 0.03, zr - 0.06) @ R('Y', pitch))
        mb.box(0.0, -0.05, -0.1, L, 0.05, 0.06, CEDAR, M=M)
    mb.box(cx - 0.11, y1 - 0.06, zr - 0.04, cx + 0.11, y0, zr + 0.12, KAWARA)                                # ridge
    gab = [(cx - half + 0.12, y1 + 0.1, zc - 0.02), (cx + half - 0.12, y1 + 0.1, zc - 0.02), (cx, y1 + 0.1, zr - 0.1)]
    mb.solid(gab, [(px, y1 + 0.04, pz) for (px, _, pz) in gab], PLASTER, bottom_mat=PLASTER, side_mat=CEDAR)
    mb.box(cx - half + 0.05, y1, zc - 0.22, cx + half - 0.05, y1 + 0.14, zc - 0.04, CEDAR)                   # tie beam


def m_window_shoji(mb, cx, y, z, w=1.3, h=1.0):
    mb.box(cx - w / 2, y - 0.015, z, cx + w / 2, y + 0.01, z + h, "JP_Shoji_Paper")
    mb.box(cx - w / 2 - 0.08, y - 0.06, z - 0.1, cx + w / 2 + 0.08, y + 0.04, z, CEDAR)       # sill
    mb.box(cx - w / 2 - 0.08, y - 0.06, z + h, cx + w / 2 + 0.08, y + 0.04, z + h + 0.1, CEDAR)
    for px in (cx - w / 2 - 0.08, cx + w / 2):
        mb.box(px, y - 0.06, z, px + 0.08, y + 0.04, z + h, CEDAR)


def m_loft_window(mb, cx, y, z, w=3.0, h=0.7):
    """Mushiko-mado: a long slatted loft window in the low upper floor (plaster with a dark lattice)."""
    mb.box(cx - w / 2, y - 0.02, z, cx + w / 2, y + 0.08, z + h, "JP_Wood_Lattice")
    mb.box(cx - w / 2 - 0.1, y - 0.06, z - 0.08, cx + w / 2 + 0.1, y + 0.02, z, CEDAR)
    mb.box(cx - w / 2 - 0.1, y - 0.06, z + h, cx + w / 2 + 0.1, y + 0.02, z + h + 0.08, CEDAR)


def m_bay_window(mb, cx, y, z, w=2.2):
    mb.box(cx - w / 2, y - 0.55, z, cx + w / 2, y, z + 0.4, CEDAR)                 # base
    mb.box(cx - w / 2 + 0.08, y - 0.5, z + 0.4, cx + w / 2 - 0.08, y - 0.1, z + 1.3, "JP_Wood_Lattice")
    for px in (cx - w / 2, cx + w / 2 - 0.1):
        mb.box(px, y - 0.55, z + 0.4, px + 0.1, y, z + 1.35, CEDAR)
    mb.box(cx - w / 2, y - 0.55, z + 1.3, cx + w / 2, y, z + 1.4, CEDAR)
    pent_roof(mb, cx - w / 2 - 0.15, cx + w / 2 + 0.15, y, z + 1.75, 0.75, 28.0, ends=True)


def m_balcony(mb, x0, x1, y, z, depth, posts_to=None, bay=2.0):
    mb.box(x0, y - depth, z, x1, y, z + 0.14, CEDAR)
    mb.box(x0, y - depth - 0.02, z + 0.14, x1, y - depth + 0.06, z + 0.95, "JP_Wood_Lattice")
    mb.box(x0, y - depth - 0.05, z + 0.95, x1, y - depth + 0.09, z + 1.03, CEDAR)            # hand rail
    for bx in (x0, x1 - 0.08):
        mb.box(bx, y - depth, z + 0.14, bx + 0.08, y, z + 1.03, CEDAR)
    if posts_to:
        n = int(round((x1 - x0) / bay))
        for i in range(n + 1):
            px = x0 + i * (x1 - x0) / n
            mb.box(px - 0.08, y - depth, z + 0.14, px + 0.08, y - depth + 0.16, posts_to, CEDAR)


def m_wall_frame(mb, x0, x1, y0, y1, z0, z1, wall=PLASTER, bays=None, band=True, open_front=False, base=0.0, attach=(False, False)):
    """A storey's walls (all four sides of the rectangle) in one wall material, cedar corner posts, posts at the
    bay lines of the sides and back, and a cedar band at the top. base > 0: a cedar board skirt (koshi-ita) of that
    height with a rail on top, so a plaster wall reads as wood below and plaster with beams above."""
    at = tuple(k for k, a in (('-x', attach[0]), ('+x', attach[1])) if a)       # party walls: no faces, no posts
    mb.box(x0, y0, z0, x1, y1, z1, wall, skip=(('-z', '-y') if open_front else ('-z',)) + at)
    if base > 0.0:
        sk = (('-z', '+z', '-y') if open_front else ('-z', '+z')) + at
        ex0, ex1 = (0.0 if attach[0] else 0.03), (0.0 if attach[1] else 0.03)
        mb.box(x0 - ex0, y0 - 0.03, z0, x1 + ex1, y1 + 0.03, z0 + base, CEDAR, skip=sk)
        mb.box(x0 - ex0 - 0.02, y0 - 0.05, z0 + base, x1 + ex1 + 0.02, y1 + 0.05, z0 + base + 0.07, CEDAR, skip=sk + ('-z',))
    for (x, y, side) in ((x0, y0, 0), (x1, y0, 1), (x0, y1, 0), (x1, y1, 1)):
        if attach[side] is False:
            m_post(mb, x, y, z0, z1, 0.24)
    if bays:
        for i in range(1, int((y1 - y0) / bays)):
            for x, side in ((x0, 0), (x1, 1)):
                if attach[side] is False:
                    m_post(mb, x, y0 + i * bays, z0, z1, 0.18)
    if band:
        mb.box(x0 - (0.0 if attach[0] else 0.04), y0 - 0.04, z1 - 0.3, x1 + (0.0 if attach[1] else 0.04), y1 + 0.04, z1 - 0.08, CEDAR, skip=('-z', '+z') + at)


# ------------------------------------------------------------------ prefabs
PREFABS = {}
RULES = {"A": {"pent": True}, "B": {"max_features": 1}, "C": {}}


def finish(mb, name, w, d, h, tier, info, floors=(), coll="90_Prefabs", col=True):
    ob = mb.to_object(name, COL[coll], ao=ao_band(0.0, h, floors))
    if col:
        col_box(ob, -w / 2, 0, 0, w / 2, d, max(2.0, min(h, 4.0)))
    info.update(tier=tier, width=w, depth=d, height=round(h, 2), faces=len(ob.data.polygons))
    PREFABS[name] = (ob, info)
    return ob


def machiya(name, tier, bays, depth, floors=2, roof="gable", upper=None, cusped=False, door_bay=1, steps=0,
            cloud=False, shop=False, feature_bay=None, brackets=False, canopy=True, udatsu=False):
    """Tier A/B townhouse or hall. One wall material (plaster over a cedar frame), one stone (granite)."""
    m = TIER[tier]
    if tier == "A":
        assert m["pent"] > 0, name
    if tier == "B":
        assert cusped is False and cloud is False, "%s: tier B has no cusped gable or cloud ridge ends" % name
    W, D = bays * 2.0, depth
    x0, x1 = -W / 2, W / 2
    mb = MeshBuilder()
    pl = m["plinth"]
    m_plinth(mb, x0 - 0.15, x1 + 0.15, -0.15, D + 0.15, pl)
    if steps:
        for k in range(2):
            mb.box(-steps / 2 - 0.2 * (1 - k), -0.15 - 0.42 * (2 - k), 0, steps / 2 + 0.2 * (1 - k), -0.15, pl * (k + 1) / 2, GRANITE)
    z = pl
    gf = m["gf"]
    zt0 = z + gf
    # ground floor: side/back walls + the front made of bays
    m_wall_frame(mb, x0, x1, 0.0, D, z, z + gf, bays=2.0, open_front=True, base=0.85)
    if shop:
        m_shop_interior(mb, x0 + 0.2, x1 - 0.2, z)
        mb.box(x0 + 0.1, -1.0, z + 2.45, x1 - 0.1, -0.05, z + 2.55, CEDAR)                 # raised shutters
        mb.box(x0, -0.01, z + 2.5, x1, 0.05, zt0 - 0.28, PLASTER)
        mb.box(x0, -0.01, z, x0 + 0.2, SHOP_DEPTH, z + 2.5, PLASTER); mb.box(x1 - 0.2, -0.01, z, x1, SHOP_DEPTH, z + 2.5, PLASTER)
        for (a, b) in ((x0 - 0.02, x0 + 0.22), (x1 - 0.22, x1 + 0.02)):
            mb.box(a, -0.04, z, b, 0.0, z + 0.85, CEDAR)
        for px in (x0 + 0.2, x1 - 0.2):
            m_post(mb, px, -0.05, z, z + gf, POST)
    else:
        for i in range(bays):
            bx0, bx1 = x0 + i * 2.0, x0 + (i + 1) * 2.0
            if i == door_bay:
                mb.box(bx0, -0.02, z + DOOR_H + 0.2, bx1, 0.02, z + gf, PLASTER)
                m_door_lattice(mb, (bx0 + bx1) / 2, 0.0, z)
                mb.box(bx0, -0.01, z, bx0 + 0.1, 0.36, z + DOOR_H + 0.2, PLASTER)
                mb.box(bx1 - 0.1, -0.01, z, bx1, 0.36, z + DOOR_H + 0.2, PLASTER)
            else:
                m_front_lattice(mb, bx0 + 0.11, bx1 - 0.11, -0.02, z, 2.6)
                mb.box(bx0, -0.01, z + 2.6, bx1, 0.05, zt0 - 0.28, PLASTER)          # transom above the lattice
            m_post(mb, bx0, -0.04, z, z + gf, POST)
        m_post(mb, x1, -0.04, z, z + gf, POST)
    zt = z + gf
    mb.box(x0 - 0.06, -0.12, zt - 0.3, x1 + 0.06, 0.02, zt - 0.02, CEDAR)                    # head beam
    ze_pent = None
    if m["pent"] > 0:
        ze_pent = pent_roof(mb, x0 - 0.1, x1 + 0.1, 0.0, zt + 0.32, m["pent"], m["pitch"] - 4)
        if canopy and shop is False and cusped is False:
            m_door_canopy(mb, x0 + door_bay * 2.0 + 1.0, 1.6, m["pent"], ze_pent)
    feats = 0
    top = zt
    floor_lines = [zt]
    if floors >= 2:
        uf = m["uf"]
        z2 = zt
        m_wall_frame(mb, x0, x1, 0.0, D, z2, z2 + uf, bays=2.0)
        for i in range(bays + 1):
            m_post(mb, x0 + i * 2.0, -0.03, z2, z2 + uf - 0.22, 0.18)      # stops under the eave
        mb.box(x0, -0.07, z2 + 0.52, x1, 0.0, z2 + 0.6, CEDAR)              # rail under the windows
        fb = feature_bay if feature_bay is not None else bays // 2
        for i in range(bays):
            cx = x0 + i * 2.0 + 1.0
            if upper == "bay" and i == fb:
                continue
            if upper == "loft" and abs(cx) < 1.6:
                continue
            m_window_shoji(mb, cx, 0.0, z2 + 0.7, 1.55, 1.1 if tier == "B" else 1.25)
        if upper == "bay":
            feats += 1
            m_bay_window(mb, x0 + fb * 2.0 + 1.0, -0.02, z2 + 0.45)
        elif upper == "balcony":
            feats += 1
            if tier == "A":
                m_balcony(mb, x0 + 0.4, x1 - 0.4, 0.0, z2 + 0.5, 0.6, posts_to=z2 + uf - 0.55)   # stay under the eave
            else:
                bw = 2.0 if bays > 3 else 1.6
                m_balcony(mb, -bw, bw, 0.0, z2, 0.95)
        elif upper == "loft":
            feats += 1
            m_loft_window(mb, 0.0, -0.02, z2 + 0.75, 3.0, 0.75)
        top = z2 + uf
    if tier == "B":
        assert feats <= RULES["B"]["max_features"], "%s: tier B has at most one upper feature" % name
    # main pillars at the front corners, thicker than the bay posts, from the plinth to under the eave: they split a
    # row of houses into houses
    for px in (x0, x1):
        m_post(mb, px, -0.07, z, top - 0.2, 0.3)
    if udatsu and ze_pent is not None and floors >= 2:
        # udatsu: a plaster fire wall at each end of the upper floor, standing out past the front with a tiled cap
        for px in (x0, x1):
            # kept under the main eave (a wall standing above the roof reads as a chimney from the field camera)
            mb.box(px - 0.13, -0.55, ze_pent - 0.1, px + 0.13, 0.9, top - 0.35, PLASTER)
            mb.box(px - 0.16, -0.57, ze_pent - 0.2, px + 0.16, 0.92, ze_pent - 0.1, CEDAR)
            mb.box(px - 0.22, -0.65, top - 0.35, px + 0.22, 0.95, top - 0.22, KAWARA)
    if brackets:      # carved eave brackets as plain outlines under the main eave
        for i in range(bays + 1):
            mb.box(x0 + i * 2.0 - 0.09, -0.6, top - 0.9, x0 + i * 2.0 + 0.09, 0.0, top - 0.62, CEDAR)
    if roof == "gable":
        zr = roof_gable(mb, x0, x1, 0.0, D, top, m["pitch"], m["eave"], KAWARA, ridge_end="cloud" if cloud else "plain")
    else:
        zr = roof_hipgable(mb, x0, x1, 0.0, D, top, m["pitch"], m["eave"], KAWARA, upturn=0.6 if tier == "A" else 0.0,
                           ridge_end="cloud" if cloud else "plain")
    if cusped:
        cusped_gable(mb, zt + 0.32, pl)
    info = dict(bays=bays, floors=floors, roof=roof, upper=upper, cusped=cusped, ridge=round(zr, 2),
                door_x=(None if shop else x0 + door_bay * 2.0 + 1.0), head_z=round(z + DOOR_H + 0.2, 2),
                arm_z=round(zt - 0.12, 2), shop_floor_z=(round(z + SHOP_FLOOR, 2) if shop else None),
                pent_z=round(zt + 0.32 - m["pent"] * math.tan(math.radians(m["pitch"] - 4)), 2) if m["pent"] else None)
    return finish(mb, name, W, D, zr, tier, info, floors=floor_lines)


# ------------------------------------------------------------------ kit v3 (Nov 1): the merchant row
# A disciplined machiya grammar after the street references: every row house shares the tier-B metrics, so eaves,
# pent roofs and ridges line up along the row; the houses stand attached (attach=(left, right): no overhang, no
# gable end, no posts at a party wall, so neighbours join into one streetscape); variety comes from the facade
# only: plaster and timber tints (vertex colour), upper windows (glowing shoji or dark lattice), the door (lattice
# sliding door, plank double door, or the open shop), a door canopy or not, udatsu fire walls, the roof tiles.
ROW_PLINTH = 0.35      # the visible granite base course
ROW_SKIRT = 0.55       # the cedar board skirt (koshi-ita) on top of it, with a rail
PLASTER_TINTS = {"cream": (1.0, 0.985, 0.94), "grey": (0.93, 0.935, 0.92), "ochre": (1.0, 0.96, 0.86), "white": (1.03, 1.02, 1.0)}
WOOD_TINTS = {"dark": (1.0, 1.0, 1.0), "medium": (1.16, 1.08, 0.96), "red": (1.12, 0.98, 0.9)}


def m_window_lattice(mb, cx, y, z, w=1.5, h=1.0):
    """An upper-floor lattice window (koshi-mado): dark lattice panel in a cedar frame, a lit inside."""
    mb.box(cx - w / 2, y - 0.02, z, cx + w / 2, y + 0.08, z + h, "JP_Wood_Lattice")
    mb.box(cx - w / 2 - 0.08, y - 0.06, z - 0.1, cx + w / 2 + 0.08, y + 0.04, z, CEDAR)
    mb.box(cx - w / 2 - 0.08, y - 0.06, z + h, cx + w / 2 + 0.08, y + 0.04, z + h + 0.1, CEDAR)
    for px in (cx - w / 2 - 0.08, cx + w / 2):
        mb.box(px, y - 0.06, z, px + 0.08, y + 0.04, z + h, CEDAR)


def m_door_wood(mb, cx, y, z, w=2.2, deep=0.5):
    """A wide plank double door (the storehouse / workshop kind): the left leaf closed with battens, the right leaf
    slid open on a lit paper screen, thick jambs and lintel, a granite step."""
    mb.box(cx - w / 2, y + deep, z, cx + w / 2, y + deep + 0.05, z + DOOR_H, "JP_Shoji_Paper")
    mb.box(cx - w / 2, y + 0.02, z - 0.01, cx + w / 2, y + deep, z + 0.01, GRANITE)
    for sx in (-1, 1):
        a, b = sorted((cx + sx * w / 2, cx + sx * (w / 2 - 0.04)))
        mb.box(a, y, z, b, y + deep, z + DOOR_H, "JP_Mat_Opening")
    mb.box(cx - w / 2, y + 0.1, z, cx + 0.02, y + 0.18, z + DOOR_H, CEDAR)                                  # closed leaf
    for bz in (0.5, 1.2, 1.85):
        mb.box(cx - w / 2 + 0.04, y + 0.06, z + bz, cx - 0.02, y + 0.1, z + bz + 0.1, CEDAR)             # battens
    mb.box(cx + 0.06, y + 0.2, z, cx + w / 2 - 0.5, y + 0.28, z + DOOR_H, CEDAR)                             # open leaf, pushed back
    mb.box(cx - w / 2 - 0.14, y - 0.06, z, cx - w / 2, y + deep, z + DOOR_H + 0.18, CEDAR)
    mb.box(cx + w / 2, y - 0.06, z, cx + w / 2 + 0.14, y + deep, z + DOOR_H + 0.18, CEDAR)
    mb.box(cx - w / 2 - 0.2, y - 0.1, z + DOOR_H, cx + w / 2 + 0.2, y + deep, z + DOOR_H + 0.24, CEDAR)
    mb.box(cx - w / 2 - 0.1, y - 0.42, z - 0.16, cx + w / 2 + 0.1, y + 0.02, z - 0.01, GRANITE)


def row_house(name, bays, depth=9.0, attach=(False, False), door_bay=1, door="lattice", upper_win="shoji", n_win=None,
              canopy=True, udatsu=False, plaster="cream", wood="dark", roof_mat=KAWARA):
    m = TIER["B"]
    W, D = bays * 2.0, depth
    x0, x1 = -W / 2, W / 2
    mb = MeshBuilder()
    mb.tint = {PLASTER: PLASTER_TINTS[plaster], CEDAR: WOOD_TINTS[wood]}
    aL, aR = attach
    pl = ROW_PLINTH
    # the base course: granite, standing 0.12 proud of the wall, cut flush at a party wall
    mb.box(x0 - (0.0 if aL else 0.12), -0.12, 0, x1 + (0.0 if aR else 0.12), D + 0.12, pl, GRANITE,
           skip=('-z',) + tuple(k for k, a in (('-x', aL), ('+x', aR)) if a))
    z = pl
    gf = m["gf"]
    zt = z + gf
    # ground floor: plaster side / back walls over a board skirt, the front made of 2 m bays
    m_wall_frame(mb, x0, x1, 0.0, D, z, zt, bays=2.0, open_front=True, base=ROW_SKIRT, attach=attach)
    if door == "shop":
        m_shop_interior(mb, x0 + 0.2, x1 - 0.2, z)
        mb.box(x0 + 0.1, -1.0, z + 2.45, x1 - 0.1, -0.05, z + 2.55, CEDAR)                 # raised shutters
        mb.box(x0, -0.01, z + 2.5, x1, 0.05, zt - 0.28, PLASTER)
        mb.box(x0, -0.01, z, x0 + 0.2, SHOP_DEPTH, z + 2.5, PLASTER); mb.box(x1 - 0.2, -0.01, z, x1, SHOP_DEPTH, z + 2.5, PLASTER)
        for px in (x0 + 0.2, x1 - 0.2):
            m_post(mb, px, -0.05, z, zt, POST)
    else:
        for i in range(bays):
            bx0, bx1 = x0 + i * 2.0, x0 + (i + 1) * 2.0
            if i == door_bay:
                mb.box(bx0, -0.02, z + DOOR_H + 0.2, bx1, 0.02, zt, PLASTER)
                if door == "wood":
                    m_door_wood(mb, (bx0 + bx1) / 2, 0.0, z, w=1.7)
                else:
                    m_door_lattice(mb, (bx0 + bx1) / 2, 0.0, z)
                mb.box(bx0, -0.01, z, bx0 + 0.1, 0.36, z + DOOR_H + 0.2, PLASTER)
                mb.box(bx1 - 0.1, -0.01, z, bx1, 0.36, z + DOOR_H + 0.2, PLASTER)
            else:
                m_front_lattice(mb, bx0 + 0.11, bx1 - 0.11, -0.02, z + ROW_SKIRT - 0.35, 2.6 - ROW_SKIRT + 0.35)
                mb.box(bx0, -0.02, z, bx1, 0.06, z + ROW_SKIRT - 0.35, CEDAR)                   # skirt under the lattice
                mb.box(bx0, -0.01, z + 2.6, bx1, 0.05, zt - 0.28, PLASTER)                      # transom
            if i > 0:
                m_post(mb, bx0, -0.04, z, zt, POST)
    mb.box(x0 - (0.0 if aL else 0.06), -0.12, zt - 0.3, x1 + (0.0 if aR else 0.06), 0.02, zt - 0.02, CEDAR)   # head beam
    ze_pent = pent_roof(mb, x0 - (0.0 if aL else 0.1), x1 + (0.0 if aR else 0.1), 0.0, zt + 0.32, m["pent"], m["pitch"] - 4)
    if canopy and door != "shop":
        m_door_canopy(mb, x0 + door_bay * 2.0 + 1.0, 1.6, m["pent"], ze_pent)
    # upper floor: a quiet plaster field (no bay posts): a beam at the floor line, the band under the eave, and the
    # windows set in from both ends (plaster, window, window, plaster)
    uf = m["uf"]
    m_wall_frame(mb, x0, x1, 0.0, D, zt, zt + uf, bays=None, attach=attach)
    mb.box(x0, -0.07, zt, x1, 0.0, zt + 0.16, CEDAR)
    nw = n_win if n_win is not None else max(1, bays - 1)
    margin = 1.1
    span = W - 2 * margin
    ww = 1.5 if nw <= 2 else min(1.5, span / nw - 0.5)
    for k in range(nw):
        cx = x0 + margin + (k + 0.5) * span / nw
        if upper_win == "shoji":
            m_window_shoji(mb, cx, 0.0, zt + 0.7, ww, 1.1)
        else:
            m_window_lattice(mb, cx, 0.0, zt + 0.7, ww, 1.0)
    top = zt + uf
    # the main pillars at the front corners: inset half a post at a party wall, so the two houses' pillars stand side
    # by side as one double post (no two boxes on the same spot)
    for px, a in ((x0, aL), (x1, aR)):
        m_post(mb, px + (0.15 if (a and px == x0) else -0.15 if (a and px == x1) else 0.0), -0.07, z, top - 0.2, 0.3)
    if udatsu:
        for px, a in ((x0, aL), (x1, aR)):
            if a and px == x0:
                continue                                   # the neighbour on the left carries that party wall's udatsu
            mb.box(px - 0.13, -0.55, ze_pent - 0.1, px + 0.13, 0.9, top - 0.35, PLASTER)
            mb.box(px - 0.16, -0.57, ze_pent - 0.2, px + 0.16, 0.92, ze_pent - 0.1, CEDAR)
            mb.box(px - 0.22, -0.65, top - 0.35, px + 0.22, 0.95, top - 0.22, KAWARA)
    zr = roof_gable(mb, x0, x1, 0.0, D, top, m["pitch"], m["eave"], roof_mat, ridge_end="plain", free=(not aL, not aR))
    info = dict(bays=bays, floors=2, roof="gable", upper=upper_win, door=door, attach=list(attach), ridge=round(zr, 2),
                door_x=(None if door == "shop" else x0 + door_bay * 2.0 + 1.0), head_z=round(z + DOOR_H + 0.2, 2),
                arm_z=round(zt - 0.12, 2), shop_floor_z=(round(z + SHOP_FLOOR, 2) if door == "shop" else None),
                pent_z=round(zt + 0.32 - m["pent"] * math.tan(math.radians(m["pitch"] - 4)), 2), plaster=plaster, wood=wood)
    return finish(mb, name, W, D, zr, "B", info, floors=(zt,))


ROW_TYPES = {
    # name: (bays, door_bay, door, upper window, canopy, udatsu, plaster, wood, roof)
    "House_A": (3, 1, "lattice", "shoji", True, False, "cream", "dark", KAWARA),
    "House_B": (4, 2, "wood", "lattice", False, False, "grey", "medium", KAWARA),
    "House_C": (3, 0, "lattice", "shoji", True, True, "ochre", "dark", "JP_Roof_KawaraOld"),
    "Shop_A": (3, 1, "shop", "shoji", False, False, "cream", "dark", KAWARA),
    "Shop_B": (3, 1, "shop", "lattice", False, True, "white", "red", KAWARA),
}
ROW_ATTACH = {"L": (False, True), "M": (True, True), "R": (True, False), "F": (False, False)}


SHOP_DEPTH, SHOP_FLOOR = 2.6, 0.45      # the open shop's room: depth behind the front, raised floor height


def m_shop_interior(mb, a, b, z):
    """The open shop's room (Nov 1, replaces the black box): a raised wooden floor with a tatami-coloured top that
    starts just in front of the facade (the merchant stands or sits on it, goods along its front edge), plaster
    side walls, a dark cedar back wall with three shelves of jars and cloth bolts and a lit paper door in the middle,
    and a plank ceiling. The scene puts a warm light, the merchant and a noren in it (info: shop_floor_z)."""
    yd, fz = SHOP_DEPTH, z + SHOP_FLOOR
    mb.box(a, -0.3, z, b, yd, fz, CEDAR, top="JP_Clay_Ochre", skip=('-z',))                 # raised floor
    mb.box(a, -0.32, fz - 0.1, b, -0.18, fz + 0.02, CEDAR)                                 # its front edge beam
    mb.box(a, yd - 0.05, z, b, yd, z + 2.5, CEDAR)                                         # back wall
    mb.box(a, 0.0, z + 2.45, b, yd, z + 2.5, CEDAR, skip=('+z',))                          # ceiling
    for px in (a + 0.02, b - 0.14):                                                         # corner posts
        mb.box(px, yd - 0.17, fz, px + 0.12, yd - 0.05, z + 2.45, CEDAR)
    # a sliding paper door to the back room in the middle of the back wall, lit from behind
    mb.box(-0.8, yd - 0.07, fz, 0.8, yd - 0.05, z + 2.3, "JP_Shoji_Paper")
    mb.box(-0.86, yd - 0.1, z + 2.3, 0.86, yd - 0.05, z + 2.38, CEDAR)
    # shelves either side of the door, with goods
    jar_cols = ("JP_Glaze_Brown", "JP_Clay_Ochre", "JP_Glaze_Brown")
    bolt_cols = ("JP_Cloth_Indigo", "JP_Cloth_Red", "JP_Clay_Ochre", "JP_Cloth_Indigo")
    for (sa, sb) in ((a + 0.2, -1.0), (1.0, b - 0.2)):
        for k, hz in enumerate((0.55, 1.05, 1.55)):
            sz = fz + hz
            mb.box(sa, yd - 0.45, sz - 0.05, sb, yd - 0.05, sz, CEDAR)
            x, i = sa + 0.12, 0
            while x < sb - 0.15:
                if (k + i) % 3 == 2:                                                        # a stack of cloth bolts
                    c = bolt_cols[(i + 2 * k) % len(bolt_cols)]
                    mb.box(x - 0.1, yd - 0.4, sz, x + 0.12, yd - 0.12, sz + 0.12, c)
                    mb.box(x - 0.08, yd - 0.38, sz + 0.12, x + 0.1, yd - 0.14, sz + 0.23, bolt_cols[(i + 2 * k + 1) % len(bolt_cols)])
                    x += 0.32
                else:                                                                       # a lidded jar
                    r, h = (0.1, 0.26) if (i + k) % 2 else (0.13, 0.32)
                    mb.revolve([(r * 0.7, 0.0), (r, h * 0.35), (r * 0.95, h * 0.75), (r * 0.55, h)], jar_cols[(i + k) % 3], 8,
                               T(x, yd - 0.26, sz), cap_bottom=jar_cols[(i + k) % 3], cap_top=CEDAR)
                    x += 2 * r + 0.08
                i += 1
    # goods along the front edge of the floor: two jar groups, a cloth stack, a tray of small bowls
    for (gx, kind) in ((a + 0.5, "jars"), (a + 1.3, "bolts"), (b - 1.4, "tray"), (b - 0.55, "jars")):
        gy = -0.02
        if kind == "jars":
            for dx, dy, r, h, c in ((0.0, 0.0, 0.16, 0.42, "JP_Glaze_Brown"), (0.3, 0.12, 0.12, 0.3, "JP_Clay_Ochre"),
                                    (-0.26, 0.14, 0.1, 0.26, "JP_Glaze_Brown")):
                mb.revolve([(r * 0.7, 0.0), (r, h * 0.35), (r * 0.95, h * 0.75), (r * 0.55, h)], c, 8, T(gx + dx, gy + dy, fz),
                           cap_bottom=c, cap_top=CEDAR)
        elif kind == "bolts":
            for j, c in enumerate(("JP_Cloth_Indigo", "JP_Cloth_Red", "JP_Clay_Ochre")):
                mb.box(gx - 0.25 + 0.02 * j, gy - 0.12, fz + 0.12 * j, gx + 0.25 - 0.02 * j, gy + 0.22, fz + 0.12 * (j + 1), c)
        else:
            mb.box(gx - 0.4, gy - 0.15, fz, gx + 0.4, gy + 0.2, fz + 0.05, CEDAR)
            for j in range(4):
                mb.revolve([(0.05, 0.0), (0.09, 0.07)], "JP_Clay_Ochre", 8, T(gx - 0.3 + 0.2 * j, gy + 0.02, fz + 0.05),
                           cap_bottom="JP_Clay_Ochre")


def cusped_gable(mb, zc, plinth, cw=4.4):
    """Karahafu: a cross-gable on the pent roof over the entrance with a bell-curved, upturned front edge; the gable
    board under it and two porch posts down to the plinth."""
    hw = cw / 2 + 0.35
    H = 1.5
    n = 10
    prof = []
    for i in range(n + 1):
        x = -hw + 2 * hw * i / n
        k = abs(x) / hw
        prof.append((x, zc + H * (1 - k ** 1.7) + 0.18 * max(0.0, k - 0.8) / 0.2))      # flicks up at the ends
    y0, y1 = -1.45, 0.4
    for i in range(n):
        (xa, za), (xb, zb) = prof[i], prof[i + 1]
        mb.poly([(xa, y0, za), (xb, y0, zb), (xb, y1, zb), (xa, y1, za)], KAWARA)                    # top
        mb.poly([(xa, y1, za - 0.16), (xb, y1, zb - 0.16), (xb, y0, zb - 0.16), (xa, y0, za - 0.16)], CEDAR)   # under
        mb.poly([(xa, y0, za - 0.16), (xb, y0, zb - 0.16), (xb, y0, zb), (xa, y0, za)], CEDAR)       # front edge (bargeboard)
    tile_ends(mb, (hw, y0), (-hw, y0), prof[-1][1], prof[0][1], (0, -1))
    fan = [(-cw / 2, y0 + 0.12, zc)] + [(x, y0 + 0.12, z - 0.2) for (x, z) in prof if abs(x) <= cw / 2] + [(cw / 2, y0 + 0.12, zc)]
    mb.poly(fan, "JP_Wood_Lattice")
    mb.box(-0.3, y0 - 0.1, zc + H - 0.15, 0.3, y0 + 0.3, zc + H + 0.45, KAWARA)            # crest tile (onigawara)
    for px in (-cw / 2, cw / 2 - 0.24):
        mb.box(px, y0 + 0.05, plinth, px + 0.24, y0 + 0.29, zc, CEDAR)
        mb.box(px - 0.06, y0 - 0.01, plinth, px + 0.3, y0 + 0.35, plinth + 0.3, GRANITE)   # post base stones
    mb.box(-cw / 2, y0 + 0.02, zc - 0.32, cw / 2, y0 + 0.32, zc - 0.02, CEDAR)              # tie beam


def poor_house(name, w, d, roof="plank", wall="JP_Wood_Weathered_V", door_x=(0.0,), win_x=(), open_front=False,
               units=1, stilts=0.0, water_bay=None):
    """Tier C: single storey on posts on flat stones (or stilts), simple gable parallel to the street, low eaves."""
    m = TIER["C"]
    x0, x1 = -w / 2, w / 2
    mb = MeshBuilder()
    z = m["plinth"]
    post_xs = [x0 + i * w / max(1, round(w / 2)) for i in range(round(w / 2) + 1)]
    if stilts > 0:
        z = stilts
        for px in post_xs:
            for py in (0.1, d / 2, d - 0.1):
                mb.box(px - 0.1, py - 0.1, -1.4, px + 0.1, py + 0.1, z, "JP_Wood_Weathered")
        floors = [(x0, d)] if water_bay is None else [(x0, water_bay[0]), (water_bay[1], x1)]
        for (a, b) in floors:
            mb.box(a, 0, z - 0.15, b, d, z, "JP_Wood_Weathered")
        if water_bay:
            mb.box(water_bay[0], d - 1.2, z - 0.15, water_bay[1], d, z, "JP_Wood_Weathered")
        mb.box(x0 - 0.1, -1.1, z - 0.15, x1 + 0.1, 0, z - 0.03, "JP_Wood_Weathered")          # walkway
        for px in (x0, x1):
            mb.box(px - 0.08, -1.08, -1.4, px + 0.08, -0.92, z - 0.15, "JP_Wood_Weathered")
    else:
        mb.box(x0 - 0.06, -0.06, 0, x1 + 0.06, d + 0.06, z, "JP_Stone_Rough")                   # low rough base
        for px in post_xs:
            mb.box(px - 0.2, -0.22, 0, px + 0.2, 0.18, z + 0.05, "JP_Stone_Rough")              # flat stones under posts
    eave_z = z + m["gf"]
    if open_front:
        mb.box(x0, d - 0.2, z, x1, d, eave_z, wall)
        mb.box(x0, 0, z, x0 + 0.2, d, eave_z, wall)
        mb.box(x1 - 0.2, 0, z, x1, d, eave_z, wall)
        mb.box(x0 + 0.2, 0.2, z, x1 - 0.2, d - 0.2, z + 0.05, "JP_Wood_Weathered")
        mb.box(x0 + 0.21, d - 0.21, z + 0.05, x1 - 0.21, d - 0.2, eave_z - 0.3, "JP_Mat_Opening")
        mb.box(x0, -0.04, eave_z - 0.35, x1, 0.2, eave_z, CEDAR)
    elif water_bay:
        bx0, bx1 = water_bay
        mb.box(x0, 0, z, bx0, d, eave_z, wall); mb.box(bx1, 0, z, x1, d, eave_z, wall)
        mb.box(bx0, d - 0.2, z, bx1, d, eave_z, wall)
        mb.box(bx0, -0.04, eave_z - 0.45, bx1, 0.2, eave_z, CEDAR)
        mb.box(bx0 + 0.05, d - 0.21, -1.0, bx1 - 0.05, d - 0.2, eave_z - 0.45, "JP_Mat_Opening")
    else:
        mb.box(x0, 0, z, x1, d, eave_z, wall, skip=('-z',))
    for dx in door_x:
        mb.box(dx - 0.5, -0.02, z, dx + 0.5, 0.06, z + DOOR_H - 0.15, "JP_Shoji_Patched")
        for px in (dx - 0.6, dx + 0.5):
            mb.box(px, -0.06, z, px + 0.1, 0.04, z + DOOR_H - 0.05, CEDAR)
        mb.box(dx - 0.6, -0.06, z + DOOR_H - 0.15, dx + 0.6, 0.04, z + DOOR_H - 0.03, CEDAR)
    for wx in win_x:
        mb.box(wx - 0.4, -0.01, z + 1.0, wx + 0.4, 0.04, z + 1.6, "JP_Mat_Opening")
        for k in range(5):
            bx = wx - 0.36 + k * 0.18
            mb.box(bx - 0.025, -0.05, z + 1.0, bx + 0.025, -0.01, z + 1.6, CEDAR)              # bars
        mb.box(wx - 0.5, -0.06, z + 0.92, wx + 0.5, 0.04, z + 1.0, CEDAR)
        mb.box(wx - 0.55, -0.45, z + 1.72, wx + 0.55, 0.0, z + 1.79, "JP_Wood_Weathered")    # little plank awning
    for px in post_xs:
        m_post(mb, px, 0.0, z, eave_z, 0.16)
        m_post(mb, px, d, z, eave_z, 0.16)
    if units > 1:
        for i in range(1, units):
            m_post(mb, x0 + i * w / units, 0.0, z, eave_z, 0.16)
    mb.box(x0 - 0.04, -0.06, eave_z - 0.18, x1 + 0.04, d + 0.06, eave_z, CEDAR, skip=('-z', '+z'))
    gm = wall
    if roof == "kawara_old":
        zr = roof_gable(mb, x0, x1, 0.0, d, eave_z, 25.0, m["eave"], "JP_Roof_KawaraOld", gable_over=0.3, ridge_end="none",
                        gable_mat=gm, ends=False)
    else:
        pitch = 18.0
        zr = roof_gable(mb, x0, x1, 0.0, d, eave_z, pitch, m["eave"], "JP_Roof_Plank", gable_over=0.3, ridge_end="none",
                        gable_mat=gm, ends=False, barge="JP_Wood_Weathered", slab=0.1)
        t = math.tan(math.radians(pitch))
        for frac in (0.3, 0.72):                           # rows of river stones weighing the planks (+ a pole under them)
            for s in (-1, 1):
                run = (d / 2 + m["eave"]) * frac
                yy = (-m["eave"] + run) if s < 0 else (d + m["eave"] - run)
                zz = eave_z - m["eave"] * t + run * t
                mb.tube((x0 - 0.25, yy, zz + 0.04), (x1 + 0.25, yy, zz + 0.04), 0.045, "JP_Wood_Weathered", 5)
                x = x0 - 0.15
                while x < x1 + 0.15:
                    r = 0.11 + rng.random() * 0.06
                    M = T(x, yy + (rng.random() - 0.5) * 0.08, zz) @ R('Z', rng.random() * 60)
                    mb.revolve([(r * 0.7, 0.0), (r, r * 0.45), (r * 0.6, r * 0.9), (0.0, r * 1.05)], "JP_Stone_Rough", 6, M,
                               cap_bottom="JP_Stone_Rough")
                    x += r * 2.1
    info = dict(roof=roof, wall=wall, units=units, stilts=stilts, ridge=round(zr, 2), door_x=list(door_x),
                head_z=round(z + DOOR_H - 0.03, 2))
    return finish(mb, name, w, d, zr, "C", info)


def storehouse(name):
    mb = MeshBuilder()
    w, d = 6.0, 5.0
    m_plinth(mb, -w / 2 - 0.2, w / 2 + 0.2, -0.2, d + 0.2, 0.6)
    mb.box(-w / 2, 0, 0.6, w / 2, d, 6.2, "JP_Wall_Namako", skip=('-z',))
    mb.box(-w / 2 - 0.05, -0.05, 5.7, w / 2 + 0.05, d + 0.05, 6.2, CEDAR, skip=('-z', '+z'))
    mb.box(-0.8, -0.3, 0.6, 0.8, 0.0, 2.8, CEDAR)                                     # heavy door in a deep frame
    for zz in (1.1, 2.2):
        mb.box(-0.75, -0.33, zz, 0.75, -0.29, zz + 0.1, "JP_Mat_Iron")
    pent_roof(mb, -1.1, 1.1, 0.0, 3.25, 0.6, 28.0)
    for wx in (-1.8, 1.8):
        mb.box(wx - 0.4, -0.16, 4.3, wx + 0.4, 0.0, 5.0, CEDAR)
    zr = roof_gable(mb, -w / 2, w / 2, 0.0, d, 6.2, 30.0, 0.7, KAWARA, ridge_end="plain", gable_mat="JP_Wall_Namako")
    return finish(mb, name, w, d, zr, "A", dict(walls="namako"), floors=(0.6,))


def compound_wall(name, length=4.0):
    mb = MeshBuilder()
    mb.box(-length / 2, 0, 0, length / 2, 0.5, 0.7, GRANITE)
    mb.box(-length / 2, 0.05, 0.7, length / 2, 0.45, 2.1, PLASTER, skip=('-z',))
    zr = roof_gable(mb, -length / 2, length / 2, 0.05, 0.45, 2.1, 28.0, 0.3, KAWARA, gable_over=0.0, ridge_end="none",
                    gable_mat=PLASTER, barge=None, slab=0.1)
    ob = finish(mb, name, length, 0.5, zr, "A", dict(), col=False)
    col_box(ob, -length / 2, 0, 0, length / 2, 0.5, 2.2)
    return ob


def compound_gate(name):
    mb = MeshBuilder()
    w = 3.0
    for px in (-w / 2 - 0.3, w / 2):
        mb.box(px, 0.0, 0, px + 0.3, 0.5, 3.0, CEDAR)
        mb.box(px - 0.05, -0.05, 0, px + 0.35, 0.55, 0.25, GRANITE)
    mb.box(-w / 2 - 0.45, -0.05, 2.75, w / 2 + 0.45, 0.55, 3.0, CEDAR)
    for side in (-1, 1):                                     # two leaves, half open
        mb.box(min(0, side * 1.45), 0.22, 0.02, max(0, side * 1.45), 0.3, 2.6, CEDAR)
    zr = roof_gable(mb, -w / 2 - 0.3, w / 2 + 0.3, 0.0, 0.5, 3.0, 28.0, 0.8, KAWARA, gable_over=0.5, gable_mat=CEDAR)
    ob = finish(mb, name, w + 0.6, 0.5, zr, "A", dict(), col=False)
    for px in (-w / 2 - 0.3, w / 2):
        col_box(ob, px, 0, 0, px + 0.3, 0.5, 3.0, suffix="_%d" % int(px * 10))
    return ob


def city_gate(name):
    """Yagura gate: battered granite bases flanking a 4 x 4.5 m passage, plaster upper storey with arrow slits and a
    barred window, hip roof with upturned corners and shachi ridge ornaments, pent roof over the passage. The door
    leaves are separate exports (JP_Rich_CityGate_A_DoorL / _DoorR) so they can open."""
    mb = MeshBuilder()
    pw, ph = 4.0, 4.5
    bw, bd = 5.0, 7.0
    for s in (-1, 1):
        xi, xo = s * pw / 2, s * (pw / 2 + bw)
        a, b = min(xi, xo), max(xi, xo)
        # Nov 1: battered on the outside (+Y) only, like the city walls; the face to the city and the sides where the
        # walls join stay vertical, so a wall section fits flush against the base (there was a 0.9 m gap at the top)
        bat = 0.9
        lo = [(a, 0, 0), (b, 0, 0), (b, bd, 0), (a, bd, 0)]
        hi = [(a, WALL_BAT_IN, ph), (b, WALL_BAT_IN, ph), (b, bd - bat, ph), (a, bd - bat, ph)]   # same profile as the walls
        mb.solid(lo, hi, GRANITE)
    mb.box(-pw / 2 - 0.2, 0.6, ph - 0.1, pw / 2 + 0.2, bd - 0.6, ph + 0.35, CEDAR)
    for px in (-pw / 2, pw / 2 - 0.35):
        mb.box(px, 0.9, 0, px + 0.35, 1.25, ph, CEDAR)
    mb.box(-pw / 2, 1.25, ph - 0.6, pw / 2, bd - 0.9, ph - 0.1, CEDAR)                 # passage ceiling
    ux0, ux1 = -(pw / 2 + bw) + 0.9, (pw / 2 + bw) - 0.9
    uy0, uy1 = 0.9, bd - 0.9
    uz0, uz1 = ph + 0.35, ph + 0.35 + 3.2
    m_wall_frame(mb, ux0, ux1, uy0, uy1, uz0, uz1, bays=None)
    mb.box(ux0 - 0.06, uy0 - 0.1, uz0, ux1 + 0.06, uy0 + 0.02, uz0 + 0.3, CEDAR)
    for sx in (-4.2, -2.6, 2.6, 4.2):
        mb.poly([(sx - 0.25, uy0 - 0.01, uz0 + 1.1), (sx + 0.25, uy0 - 0.01, uz0 + 1.1), (sx, uy0 - 0.01, uz0 + 1.55)], "JP_Mat_Opening")
        mb.box(sx - 0.25, uy0 - 0.012, uz0 + 0.55, sx + 0.25, uy0 - 0.005, uz0 + 0.9, "JP_Mat_Opening")     # square loophole
    mb.box(-1.4, uy0 - 0.01, uz0 + 0.9, 1.4, uy0 + 0.06, uz0 + 2.2, "JP_Mat_Opening")
    for k in range(9):
        bx = -1.3 + k * 0.325
        mb.box(bx - 0.04, uy0 - 0.08, uz0 + 0.9, bx + 0.04, uy0 - 0.01, uz0 + 2.2, CEDAR)
    mb.box(-1.5, uy0 - 0.1, uz0 + 0.82, 1.5, uy0 + 0.0, uz0 + 0.92, CEDAR)
    mb.box(-1.5, uy0 - 0.1, uz0 + 2.2, 1.5, uy0 + 0.0, uz0 + 2.3, CEDAR)
    pent_roof(mb, -pw / 2 - 1.3, pw / 2 + 1.3, uy0, uz0 - 0.05, 1.3, 24.0)
    zr = roof_hip(mb, ux0, ux1, uy0, uy1, uz1, 30.0, 1.2, KAWARA, upturn=0.6)
    # (Nov 1: the tall shachi ridge ends are gone; they read as two horns sticking up from the gate)
    W = pw + 2 * bw
    ob = finish(mb, name, W, bd, zr + 1.1, "A", dict(passage="%.1f x %.1f m" % (pw, ph)), floors=(ph,), col=False)
    for s in (-1, 1):
        a, b = sorted((s * pw / 2, s * (pw / 2 + bw)))
        col_box(ob, a, 0, 0, b, bd, uz1, suffix="_%s" % ("L" if s < 0 else "R"))
    for side in ("L", "R"):
        db = MeshBuilder()
        a, b = (0.0, pw / 2) if side == "L" else (-pw / 2, 0.0)
        db.box(a, 0.0, 0, b, 0.15, ph - 0.12, "JP_Gate_DoorStudded")
        dob = finish(db, name + "_Door" + side, pw / 2, 0.15, ph - 0.12, "A",
                     dict(note="origin at the hinge; place at x = -2 (L) / +2 (R), y = 1.05 from the gate origin"), col=False)
        col_box(dob, a, 0, 0, b, 0.15, ph - 0.12)
    return ob


def gate_wall(name, length=4.0):
    mb = MeshBuilder()
    mb.box(-length / 2, 0, 0, length / 2, 1.6, 1.4, GRANITE)
    mb.box(-length / 2, 0.3, 1.4, length / 2, 1.3, 3.6, PLASTER, skip=('-z',))
    mb.box(-0.3, 0.27, 2.4, 0.3, 0.32, 2.9, "JP_Mat_Opening")
    for k in range(3):
        mb.box(-0.2 + k * 0.2 - 0.025, 0.24, 2.4, -0.2 + k * 0.2 + 0.025, 0.28, 2.9, CEDAR)
    zr = roof_gable(mb, -length / 2, length / 2, 0.3, 1.3, 3.6, 28.0, 0.5, KAWARA, gable_over=0.0, ridge_end="none", barge=None)
    ob = finish(mb, name, length, 1.6, zr, "A", dict(), col=False)
    col_box(ob, -length / 2, 0, 0, length / 2, 1.6, 3.6)
    return ob


def guard_booth(name):
    mb = MeshBuilder()
    w, d = 2.4, 1.8
    mb.box(-w / 2, 0, 0, w / 2, d, 0.15, GRANITE)
    mb.box(-w / 2, d - 0.12, 0.15, w / 2, d, 2.4, CEDAR)
    mb.box(-w / 2, 0, 0.15, -w / 2 + 0.12, d, 2.4, CEDAR)
    mb.box(w / 2 - 0.12, 0, 0.15, w / 2, d, 2.4, CEDAR)
    mb.box(-w / 2, -0.02, 0.15, w / 2, 0.1, 1.0, "JP_Wood_Lattice")
    mb.box(-w / 2, -0.06, 1.0, w / 2, 0.14, 1.08, CEDAR)
    zr = roof_gable(mb, -w / 2, w / 2, 0.0, d, 2.4, 24.0, 0.45, KAWARA, gable_over=0.3, ridge_end="none", gable_mat=CEDAR)
    return finish(mb, name, w, d, zr, "A", dict(open_front=True))


def fire_watchtower(name):
    mb = MeshBuilder()
    s, hp = 2.4, 9.0
    for px in (-s / 2, s / 2 - 0.25):
        for py in (0.0, s - 0.25):
            mb.box(px, py, 0, px + 0.25, py + 0.25, hp + 2.2, CEDAR)
            mb.box(px - 0.1, py - 0.1, 0, px + 0.35, py + 0.35, 0.3, GRANITE)
    for z in (3.0, 6.0):
        for py in (-0.02, s - 0.12):
            mb.box(-s / 2, py, z, s / 2, py + 0.14, z + 0.16, CEDAR)
        for px in (-s / 2 - 0.02, s / 2 - 0.12):
            mb.box(px, 0, z, px + 0.14, s, z + 0.16, CEDAR)
    for (a, b) in (((-s / 2 + 0.1, -0.05, 0.3), (s / 2 - 0.1, -0.05, 3.0)), ((s / 2 - 0.1, -0.05, 3.2), (-s / 2 + 0.1, -0.05, 6.0))):
        mb.tube(a, b, 0.06, CEDAR, 4)                                                    # cross braces
    mb.box(-s / 2 - 0.4, -0.4, hp, s / 2 + 0.4, s + 0.4, hp + 0.18, CEDAR)
    mb.box(-s / 2 - 0.4, -0.44, hp + 0.18, s / 2 + 0.4, -0.36, hp + 1.1, "JP_Wood_Lattice")
    mb.revolve([(0.2, 0.0), (0.32, 0.05), (0.28, 0.45), (0.12, 0.6), (0.0, 0.65)], "JP_Mat_Bronze", 8, T(0, s / 2, hp + 1.3))   # bell
    for k, z in enumerate([0.3 + i * 0.35 for i in range(int(hp / 0.35))]):           # ladder (bamboo rungs)
        mb.box(0.25, -0.32, z, 0.85, -0.27, z + 0.05, "JP_Wood_Bamboo")
    for px in (0.25, 0.8):
        mb.box(px, -0.34, 0, px + 0.06, -0.26, hp, "JP_Wood_Bamboo")
    zr = roof_hip(mb, -s / 2, s / 2, 0.0, s, hp + 2.2, 32.0, 0.5, KAWARA, upturn=0.1)
    return finish(mb, name, s, s, zr, "B", dict(platform=hp))


def floor_tile(name, w, d, mat):
    mb = MeshBuilder()
    mb.box(-w / 2, 0, -0.1, w / 2, d, 0.0, mat)
    ob = mb.to_object(name, COL["90_Prefabs"])
    PREFABS[name] = (ob, dict(tier="-", width=w, depth=d, height=0.0, faces=len(ob.data.polygons)))
    return ob



# ------------------------------------------------------------------ city pieces (Nov 1 pass 2: for the city scene)
def pagoda(name, tiers=3):
    """A three-tier pagoda as the rich district's skyline landmark: swept hip roofs with tile rolls, cedar storeys
    with lattice panels, a bronze-ringed finial (sorin)."""
    mb = MeshBuilder()
    m_plinth(mb, -4.0, 4.0, -4.0, 4.0, 0.7)
    mb.box(-1.4, -4.9, 0, 1.4, -4.0, 0.35, GRANITE)                                   # step
    z = 0.7
    s0 = 5.2
    for i in range(tiers):
        s_ = s0 - 1.1 * i
        h = 3.0 if i == 0 else 2.3
        mb.box(-s_ / 2, -s_ / 2, z, s_ / 2, s_ / 2, z + h, PLASTER if i == 0 else CEDAR, skip=('-z',))
        for (px, py) in ((-s_ / 2, -s_ / 2), (s_ / 2, -s_ / 2), (s_ / 2, s_ / 2), (-s_ / 2, s_ / 2)):
            m_post(mb, px, py, z, z + h, 0.26)
        mb.box(-s_ / 2 + 0.5, -s_ / 2 - 0.03, z + 0.3, s_ / 2 - 0.5, -s_ / 2 + 0.05, z + h - 0.4, "JP_Wood_Lattice")
        if i > 0:                                                                     # a little balcony rail per tier
            mb.box(-s_ / 2 - 0.35, -s_ / 2 - 0.35, z, s_ / 2 + 0.35, s_ / 2 + 0.35, z + 0.12, CEDAR)
            mb.box(-s_ / 2 - 0.35, -s_ / 2 - 0.37, z + 0.12, s_ / 2 + 0.35, -s_ / 2 - 0.31, z + 0.7, "JP_Wood_Lattice")
        top = z + h
        eave = 1.35 - 0.12 * i
        zr = roof_hip(mb, -s_ / 2, s_ / 2, -s_ / 2, s_ / 2, top, 28.0, eave, KAWARA, upturn=0.55)
        z = top + 0.9 if i < tiers - 1 else zr
    mb.tube((0, 0, z - 0.2), (0, 0, z + 3.4), 0.09, "JP_Mat_Iron", 8)
    for k in range(7):
        zz = z + 0.6 + k * 0.32
        mb.revolve([(0.26 - k * 0.02, zz), (0.26 - k * 0.02, zz + 0.08)], "JP_Mat_Bronze", 8, cap_top="JP_Mat_Bronze", cap_bottom="JP_Mat_Bronze")
    mb.revolve([(0.05, z + 3.0), (0.2, z + 3.15), (0.0, z + 3.45)], "JP_Mat_Bronze", 8)
    return finish(mb, name, 8.0, 8.0, z + 3.45, "A", dict(tiers=tiers), floors=(0.7,))


WALL_BAT_IN = 0.6      # the city-side face of the walls and the gate bases leans back 0.6 m over 4.5 m (about 7.6 degrees)


def city_wall(name, length=8.0, h=4.5, d=7.0):
    """City wall section to flank the gate (turned round Nov 1): the face to the city (-Y, origin: its foot centre) is
    vertical; the stone base is battered on the outside (+Y); the plaster parapet with loopholes and its tile cap
    stands on the outer edge, with the granite walk in front of it on the city side. Height and depth match the gate's
    stone bases (4.5 m, 7 m), so the sections sit flush against the gate and each other."""
    mb = MeshBuilder()
    bat = 0.9
    x0, x1 = -length / 2, length / 2
    yo = d - bat                                                   # the outer edge of the walk
    bi = WALL_BAT_IN                                               # the city-side face leans back too (Nov 1)
    mb.solid([(x0, 0, 0), (x1, 0, 0), (x1, d, 0), (x0, d, 0)], [(x0, bi, h), (x1, bi, h), (x1, yo, h), (x0, yo, h)], GRANITE)
    mb.box(x0, bi - 0.06, h - 0.12, x1, yo, h, GRANITE, skip=('-z',))                   # coping along the walk
    py0, py1 = yo - 0.45, yo
    mb.box(x0, py0, h, x1, py1, h + 1.3, PLASTER, skip=('-z',))
    for k in range(int(length / 2)):
        cx = x0 + 1.0 + k * 2.0
        for (y, e) in ((py0 - 0.01, -0.007), (py1 + 0.01, 0.007)):          # the holes show on both faces
            if k % 2 == 0:
                tri = [(cx - 0.2, y, h + 0.45), (cx + 0.2, y, h + 0.45), (cx, y, h + 0.85)]
                mb.poly(tri if e < 0 else list(reversed(tri)), "JP_Mat_Opening")
            else:
                mb.box(cx - 0.18, min(y, y + e), h + 0.45, cx + 0.18, max(y, y + e), h + 0.8, "JP_Mat_Opening")
    roof_gable(mb, x0, x1, py0, py1, h + 1.3, 30.0, 0.35, KAWARA, gable_over=0.0, ridge_end="none", barge=None, slab=0.1)
    ob = finish(mb, name, length, d, h + 1.6, "A", dict(walk_z=h), col=False)
    col_box(ob, x0, 0, 0, x1, d, h)                                # the walk is walkable (top at h)
    col_box(ob, x0, py0, h, x1, d, h + 1.3, suffix="_Parapet")
    return ob


def moon_gate_wall(name, length=6.0, height=2.6, r=1.2, seg=28):
    """Compound wall with a round moon gate: plaster on a granite base course, tile cap. The opening is built as a
    ring of quads between the circle and the wall's outline, so it needs no boolean."""
    mb = MeshBuilder()
    t0, t1 = 0.0, 0.45
    cz = r + 0.05
    x0, x1 = -length / 2, length / 2
    angles = [2 * math.pi * i / seg for i in range(seg)]
    for cx_, cz_ in ((x0, 0.0), (x1, 0.0), (x1, height), (x0, height)):
        angles.append(math.atan2(cz_ - cz, cx_) % (2 * math.pi))
    angles = sorted(set(round(a, 6) for a in angles))

    def outline(a):
        dx, dz = math.cos(a), math.sin(a)
        ts = []
        if abs(dx) > 1e-6:
            ts += [(x1 / dx) if dx > 0 else (x0 / dx)]
        if abs(dz) > 1e-6:
            ts += [((height - cz) / dz) if dz > 0 else ((0.0 - cz) / dz)]
        k = min(t for t in ts if t > 0)
        return (dx * k, cz + dz * k)
    circ = [(r * math.cos(a), cz + r * math.sin(a)) for a in angles]
    outl = [outline(a) for a in angles]
    n = len(angles)
    for y, flip in ((t0, False), (t1, True)):
        for i in range(n):
            j = (i + 1) % n
            q = [(circ[i][0], y, circ[i][1]), (outl[i][0], y, outl[i][1]), (outl[j][0], y, outl[j][1]), (circ[j][0], y, circ[j][1])]
            mb.poly(list(reversed(q)) if flip else q, PLASTER)
    for i in range(n):                                                     # the opening's reveal (cedar frame)
        j = (i + 1) % n
        mb.poly([(circ[i][0], t0, circ[i][1]), (circ[j][0], t0, circ[j][1]), (circ[j][0], t1, circ[j][1]), (circ[i][0], t1, circ[i][1])], GRANITE)
    for xe in (x0, x1):
        mb.poly([(xe, t0, 0), (xe, t1, 0), (xe, t1, height), (xe, t0, height)][::(1 if xe == x1 else -1)], PLASTER)
    mb.box(x0, t0 - 0.02, 0, -r - 0.02, t1 + 0.02, 0.5, GRANITE)
    mb.box(r + 0.02, t0 - 0.02, 0, x1, t1 + 0.02, 0.5, GRANITE)
    roof_gable(mb, x0, x1, t0, t1, height, 28.0, 0.3, KAWARA, gable_over=0.0, ridge_end="none", gable_mat=PLASTER, barge=None, slab=0.1)
    ob = finish(mb, name, length, t1, height + 0.4, "A", dict(opening=r * 2), col=False)
    col_box(ob, x0, 0, 0, -r, t1, height, suffix="_L"); col_box(ob, r, 0, 0, x1, t1, height, suffix="_R")
    return ob


def retaining_wall(name, length=4.0, h=1.5, mat=GRANITE, cap=True):
    """Terrace retaining wall: a slightly battered face (the lower terrace in front, the upper one behind), cap stones."""
    mb = MeshBuilder()
    x0, x1 = -length / 2, length / 2
    mb.solid([(x0, 0, 0), (x1, 0, 0), (x1, 0.9, 0), (x0, 0.9, 0)], [(x0, 0.18, h), (x1, 0.18, h), (x1, 0.9, h), (x0, 0.9, h)], mat)
    if cap:
        mb.box(x0, 0.08, h, x1, 0.95, h + 0.12, GRANITE)
    ob = mb.to_object(name, COL["00_Modules"], ao=lambda co: max(0.68, 0.7 + 0.3 * min(1.0, co.z / 0.45)))
    col_box(ob, x0, 0, 0, x1, 0.9, h)
    MODULES[name] = ob
    return ob


def stairs(name, width=3.0, rise=1.5, steps=6, mat=GRANITE):
    """Granite stairs climbing to +Y (away from the camera) between terraces, with side cheeks. Origin: bottom front
    centre. Step rise 0.25 m (the player's step height is 0.35)."""
    mb = MeshBuilder()
    run = 0.38
    sr = rise / steps
    for i in range(steps):
        mb.box(-width / 2, i * run, 0, width / 2, steps * run + 0.3, (i + 1) * sr, mat)
    for sx in (-1, 1):
        a, b = sorted((sx * width / 2, sx * (width / 2 + 0.4)))
        mb.solid([(a, 0, 0), (b, 0, 0), (b, steps * run + 0.3, 0), (a, steps * run + 0.3, 0)],
                 [(a, 0, 0.35), (b, 0, 0.35), (b, steps * run + 0.3, rise + 0.35), (a, steps * run + 0.3, rise + 0.35)], mat)
    ob = mb.to_object(name, COL["00_Modules"], ao=lambda co: 1.0)
    MODULES[name] = ob
    return ob


def stone_railing(name, length=2.0):
    mb = MeshBuilder()
    for x in (0.0, length):
        mb.box(x - 0.11, -0.11, 0, x + 0.11, 0.11, 0.95, GRANITE)
        mb.box(x - 0.14, -0.14, 0.95, x + 0.14, 0.14, 1.05, GRANITE)
    mb.box(0, -0.07, 0.72, length, 0.07, 0.86, GRANITE)
    mb.box(0, -0.06, 0.12, length, 0.06, 0.24, GRANITE)
    for k in range(1, 4):
        mb.box(k * length / 4 - 0.04, -0.04, 0.24, k * length / 4 + 0.04, 0.04, 0.72, GRANITE)
    ob = mb.to_object(name, COL["00_Modules"])
    col_box(ob, 0, -0.12, 0, length, 0.12, 1.0)
    MODULES[name] = ob
    return ob


def market_stall(name, cloth="JP_Cloth_Red"):
    """Street stall (kit v3, after the street references): a warm cedar counter facing the street, goods on it (sprite
    cards or small props at counter_z), four poles and a cloth canopy that sags between three ribs, its front edge a
    little higher than the back so the counter stays in view from the field camera, a scalloped valance in front."""
    mb = MeshBuilder()
    mb.tint = {CEDAR: WOOD_TINTS["medium"]}
    W, D = 2.4, 0.9
    mb.box(-W / 2, 0.0, 0.12, W / 2, D, 0.78, CEDAR, top=CEDAR)                              # counter body
    mb.box(-W / 2 - 0.06, -0.06, 0.78, W / 2 + 0.06, D + 0.06, 0.86, CEDAR)                   # counter top
    for x in (-W / 2 + 0.08, W / 2 - 0.08):                                                   # legs
        mb.box(x - 0.06, 0.04, 0, x + 0.06, 0.16, 0.14, CEDAR); mb.box(x - 0.06, D - 0.16, 0, x + 0.06, D - 0.04, 0.14, CEDAR)
    for x in (-W / 2 - 0.1, W / 2 + 0.1):                                                     # poles
        mb.box(x - 0.05, -0.35, 0, x + 0.05, -0.25, 2.25, CEDAR)
        mb.box(x - 0.05, D + 0.25, 0, x + 0.05, D + 0.35, 2.05, CEDAR)
    y0, y1, zf, zb = -0.6, D + 0.5, 2.3, 2.1
    ribs = (-W / 2 - 0.2, 0.0, W / 2 + 0.2)
    for rx in ribs:                                                                            # ribs the cloth hangs on
        mb.box(rx - 0.03, y0, zf - 0.08, rx + 0.03, y1, zf - 0.02, "JP_Wood_Bamboo")
    sag = 0.14
    for k in range(2):                                                                         # two sagging cloth panels
        a, b = ribs[k], ribs[k + 1]
        for j in range(4):                                                                     # 4 strips across, a curve
            fa, fb = j / 4, (j + 1) / 4
            xa, xb = a + (b - a) * fa, a + (b - a) * fb
            da, db = sag * math.sin(math.pi * fa), sag * math.sin(math.pi * fb)
            top = [(xa, y0, zf - da), (xb, y0, zf - db), (xb, y1, zb - db), (xa, y1, zb - da)]
            mb.solid([(px, py, pz - 0.04) for (px, py, pz) in top], top, cloth, bottom_mat=cloth, side_mat=cloth)
    for j in range(6):                                                                         # scalloped valance
        xa = -W / 2 - 0.2 + j * (W + 0.4) / 6
        xb = xa + (W + 0.4) / 6
        mb.box(xa + 0.01, y0 - 0.03, zf - 0.34, xb - 0.01, y0 + 0.01, zf - 0.02, cloth)
        mb.box(xa + 0.06, y0 - 0.03, zf - 0.42, xb - 0.06, y0 + 0.01, zf - 0.34, cloth)
    ob = mb.to_object(name, COL["09_Props_3D"], ao=lambda co: max(0.7, 1.0 - 0.25 * max(0.0, 1.0 - co.z / 0.3)))
    col_box(ob, -W / 2 - 0.15, -0.4, 0, W / 2 + 0.15, D + 0.4, 0.86)
    PROPS[name] = ob
    PROP_INFO[name] = dict(counter_z=0.86, counter_y=D / 2)
    return ob

# ------------------------------------------------------------------ module library (for hand-built variants)
MODULES = {}


def module(name, fn, coll="00_Modules", col=None):
    mb = MeshBuilder()
    fn(mb)
    ob = mb.to_object(name, COL[coll])
    if col:
        col_box(ob, *col)
    MODULES[name] = ob
    return ob


module("JP_Post_Cedar_3m", lambda mb: m_post(mb, 0, 0, 0, 3.0))
module("JP_Plinth_Granite_2m", lambda mb: m_plinth(mb, 0, 2.0, -0.15, 0.6, 0.3))
module("JP_Front_Lattice_2m", lambda mb: m_front_lattice(mb, 0.11, 1.89, -0.02, 0.0, 2.6))
module("JP_Door_LatticeSlide_2m", lambda mb: m_door_lattice(mb, 1.0, 0.0, 0.0))
module("JP_Window_Shoji_A", lambda mb: m_window_shoji(mb, 0.0, 0.0, 0.0))
module("JP_Window_Loft_3m", lambda mb: m_loft_window(mb, 0.0, -0.02, 0.0))
module("JP_BayWindow_Lattice_2m", lambda mb: m_bay_window(mb, 0.0, 0.0, 0.0))
module("JP_Balcony_2m", lambda mb: m_balcony(mb, 0.0, 2.0, 0.0, 0.0, 0.95))
module("JP_Wall_Plaster_2m", lambda mb: m_wall_frame(mb, 0, 2.0, 0.0, 0.25, 0.0, 3.0), col=(0, 0, 0, 2.0, 0.25, 3.0))
module("JP_PentRoof_2m", lambda mb: pent_roof(mb, 0.0, 2.0, 0.0, 0.0, 0.9, 22.0))
module("JP_Roof_Kawara_Gable_6m", lambda mb: roof_gable(mb, -3.0, 3.0, 0.0, 9.0, 0.0, 26.0, 0.9, KAWARA))
module("JP_RidgeEnd_Plain", lambda mb: ridge_end_tile(mb, 0.0, 0.0, 0.0, 0.3, 1, KAWARA, 0.55))
module("JP_RidgeEnd_Cloud", lambda mb: ridge_end_tile(mb, 0.0, 0.0, 0.0, 0.66, 1, KAWARA, 0.85))
module("JP_Gable_Cusped", lambda mb: cusped_gable(mb, 0.0, -3.0))
retaining_wall("JP_Wall_Retaining_Granite_4m")
retaining_wall("JP_Wall_Retaining_Rough_4m", mat="JP_Stone_Rough")
retaining_wall("JP_Wall_Retaining_Granite_4m_Low", h=1.0)
stairs("JP_Stairs_Granite_3m")
stairs("JP_Stairs_Granite_3m_Low", rise=1.0, steps=4)
stone_railing("JP_Railing_Stone_2m")

# ------------------------------------------------------------------ buildings
ROWS = {"A": [], "B": [], "C": []}


def add(row, ob, gap=3.0):
    ROWS[row].append((ob, gap))


add("A", city_gate("JP_Rich_CityGate_A"), 2.0)
add("A", guard_booth("JP_Rich_GuardBooth_A"))
add("A", gate_wall("JP_Rich_GateWall_4m_A"), 2.0)
add("A", machiya("JP_Rich_LanternInn_A", "A", 7, 10.0, roof="hipgable", upper="balcony", cusped=True, door_bay=3, steps=4.4, cloud=True, brackets=True))
add("A", machiya("JP_Rich_GuildHall_A", "A", 7, 12.0, roof="hipgable", upper=None, cusped=True, door_bay=3, steps=6.0, cloud=True, brackets=True))
add("A", machiya("JP_Rich_Residence_A", "A", 5, 10.0, roof="hipgable", upper="balcony", door_bay=2, steps=2.4, cloud=True))
add("A", compound_wall("JP_Rich_CompoundWall_4m_A"), 0.0)
add("A", compound_gate("JP_Rich_CompoundGate_A"), 0.0)
add("A", storehouse("JP_Rich_Storehouse_A"))
add("A", pagoda("JP_Rich_Pagoda_A"))
add("A", city_wall("JP_Rich_CityWall_8m_A"))
add("A", moon_gate_wall("JP_Rich_MoonGateWall_6m_A"))
add("B", machiya("JP_Mid_Townhouse_A", "B", 3, 9.0, upper="bay", door_bay=1, feature_bay=2, udatsu=True))
add("B", machiya("JP_Mid_Townhouse_B", "B", 4, 10.0, upper="balcony", door_bay=2))
add("B", machiya("JP_Mid_Townhouse_C", "B", 3, 8.0, upper="loft", door_bay=0, udatsu=True))
add("B", machiya("JP_Mid_Shop_A", "B", 3, 9.0, upper=None, shop=True))
add("B", fire_watchtower("JP_Mid_FireWatchtower_A"))
for _t, (_b, _db, _door, _uw, _can, _ud, _pl, _wd, _rf) in ROW_TYPES.items():
    for _suf, _at in ROW_ATTACH.items():
        add("B", row_house("JP_Row_%s_%s" % (_t, _suf), _b, attach=_at, door_bay=_db, door=_door, upper_win=_uw, canopy=_can,
                           udatsu=_ud, plaster=_pl, wood=_wd, roof_mat=_rf), 0.5)
add("C", poor_house("JP_Poor_House_A", 6.0, 5.0, "kawara_old", door_x=(-1.0,), win_x=(1.6,)))
add("C", poor_house("JP_Poor_House_B", 6.0, 6.0, "plank", wall="JP_Wall_MudStraw", door_x=(1.2,), win_x=(-1.4,)))
add("C", poor_house("JP_Poor_Hut_A", 4.0, 4.0, "plank", door_x=(0.6,), win_x=(-1.1,)))
add("C", poor_house("JP_Poor_Hut_B", 5.0, 4.0, "plank", wall="JP_Wall_MudStraw", door_x=(-1.2,), win_x=(1.0,)))
add("C", poor_house("JP_Poor_RowHouse_A", 12.0, 5.0, "plank", door_x=(-4.8, -1.8, 1.2, 4.2), win_x=(-3.6, -0.6, 2.4, 5.4), units=4))
add("C", poor_house("JP_Poor_Boathouse_A", 6.0, 7.0, "plank", door_x=(-2.2,), win_x=(), stilts=0.6, water_bay=(-1.2, 2.4)))
add("C", poor_house("JP_Poor_NetShed_A", 5.0, 4.0, "plank", door_x=(), open_front=True))
add("C", poor_house("JP_Poor_FishStore_A", 5.0, 4.0, "plank", door_x=(0.0,), win_x=(-1.7,)))
FLOOR = floor_tile("JP_Showcase_Floor", 120.0, 70.0, "JP_Paving_City")

# ------------------------------------------------------------------ 3D props (jp_props.py)
PROPS = {}
exec(compile((SCRIPTS / "jp_props.py").read_text(encoding="utf-8"), "jp_props.py", "exec"), globals())
market_stall("JP_Prop_MarketStall_A", "JP_Cloth_Red")
market_stall("JP_Prop_MarketStall_B", "JP_Cloth_Indigo")

# ------------------------------------------------------------------ lineup
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
for i, (pname, ob) in enumerate(PROPS.items()):          # props library row in front of row C (not in the scene)
    ob.location = (-40 + (i % 14) * 4.0, -9.0 - (i // 14) * 4.0, 0)
for i, (mname, ob) in enumerate(MODULES.items()):
    ob.location = (-40 + i * 5.0, -30.0, 0)

# ------------------------------------------------------------------ preview (not exported)
CHAR_PPM = 38.4


def card(name, path, x, y):
    img = bpy.data.images.load(str(path), check_existing=True)
    w, h = img.size[0] / CHAR_PPM, img.size[1] / CHAR_PPM
    m = bpy.data.materials.new("PV_" + name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = img; t.interpolation = 'Closest'
    nt.links.new(t.outputs["Color"], b.inputs["Base Color"]); nt.links.new(t.outputs["Alpha"], b.inputs["Alpha"])
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
bg.inputs["Color"].default_value = (0.3, 0.33, 0.4, 1); bg.inputs["Strength"].default_value = 0.8
sun_d = bpy.data.lights.new("PV_Sun", 'SUN'); sun_d.color = (1.0, 0.82, 0.62); sun_d.energy = 3.2; sun_d.angle = math.radians(2)
sun = bpy.data.objects.new("PV_Sun", sun_d); PREVIEW.objects.link(sun)
sun.rotation_euler = (math.radians(52), 0, math.radians(-38))
CAMS = []


def camera(name, target, pitch, dist, fov=35.0):
    cd = bpy.data.cameras.new(name); cd.sensor_fit = 'VERTICAL'; cd.angle = math.radians(fov)
    cam = bpy.data.objects.new(name, cd); PREVIEW.objects.link(cam)
    d = Vector((0, math.cos(math.radians(pitch)), -math.sin(math.radians(pitch))))
    cam.location = Vector(target) - d * dist
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    CAMS.append(name)
    return cam


for row, y in ROW_Y.items():
    xs = [o.location.x for o, _ in ROWS[row]]
    cx = (min(xs) + max(xs)) / 2
    span = max(xs) - min(xs) + 16
    camera("PV_%s_V1_Game" % row, (cx, y + 3, 3.0), 32.0, span * 1.25)
camera("PV_All", (0, 22, 2.0), 40.0, 125.0)
for nm, ob in (("Inn", "JP_Rich_LanternInn_A"), ("Gate", "JP_Rich_CityGate_A"), ("Townhouses", "JP_Mid_Townhouse_B"), ("Poor", "JP_Poor_RowHouse_A")):
    o = bpy.data.objects[ob]
    camera("PV_%s_Game" % nm, (o.location.x, o.location.y + 2, 3.0), 32.0, 30.0)
camera("PV_Props", (-14.0, -12.0, 0.6), 30.0, 34.0)
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

# ------------------------------------------------------------------ export
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
(OUT / "Modules").mkdir(parents=True, exist_ok=True)
(OUT / "Props").mkdir(parents=True, exist_ok=True)


def export(ob, path):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    # its collision boxes (COL_<name>..., Hydro's MeshImporter reads them as collision proxies): until Nov 1 these
    # weren't selected, so every kit model collided with its bounding box (the wall walk was unreachable)
    for ch in ob.children_recursive:
        if ch.name.startswith("COL_"):
            ch.select_set(True)
    saved = ob.location.copy(); ob.location = (0, 0, 0)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True, export_yup=True,
                              export_apply=False, export_materials='EXPORT', export_image_format='AUTO',
                              export_cameras=False, export_lights=False, **GLTF_COLOR_ARGS)
    ob.location = saved


for pname, (ob, info) in PREFABS.items():
    export(ob, OUT / (pname + ".glb"))
for pname, ob in PROPS.items():
    export(ob, OUT / "Props" / (pname + ".glb"))
for mname, ob in MODULES.items():
    export(ob, OUT / "Modules" / (mname + ".glb"))

# Hydro's importer finds a re-exported model's materials by name but keeps the .hymat it made the first time and the
# texture copies in <model>.textures/ (MeshAssetImport.cpp): bring both in line with this build, so new PixelLab
# textures and emission changes show without deleting anything.
SYNCED = 0
for folder in (OUT, OUT / "Props", OUT / "Modules"):
    for meta in folder.glob("*.glb.hymeta"):
        model = meta.name[:-len(".glb.hymeta")]
        for hm in folder.glob(model + "_JP_*.hymat"):
            mat = hm.stem[len(model) + 1:]
            ex = MATDEFS.get(mat)
            if ex is None:
                continue
            lines = [ln for ln in hm.read_bytes().decode("utf-8").replace("\r", "").split("\n") if ln.strip()]
            col, inten = ((GLOW[0], GLOW[1], GLOW[2]), ex["strength"]) if "emi" in ex else ((0, 0, 0), 1)
            new = []
            for ln in lines:
                if ln.strip().startswith("EmissionColor:"):
                    ln = "  EmissionColor: [%g, %g, %g]" % col
                elif ln.strip().startswith("EmissionIntensity:"):
                    ln = "  EmissionIntensity: %g" % inten
                new.append(ln)
            data = ("\r\n".join(new) + "\r\n").encode("utf-8")
            if data != hm.read_bytes():
                hm.write_bytes(data); SYNCED += 1
        texdir = folder / (model + ".textures")
        if texdir.is_dir():
            for f in texdir.glob("*.png"):
                base = f.name.split("_", 1)[1] if "_" in f.name else f.name
                src = TEX / base
                if src.exists() and src.read_bytes() != f.read_bytes():
                    f.write_bytes(src.read_bytes()); SYNCED += 1

SRC.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.environ.get("JP_SAVE") or str(SRC / "JP_Kit.blend"), relative_remap=True)
summary = {n: dict(i, location=LAYOUT.get(n)) for n, (o, i) in PREFABS.items()}
summary["_props"] = {n: dict(PROP_INFO.get(n, {}), faces=len(o.data.polygons)) for n, o in PROPS.items()}
summary["_modules"] = {n: len(o.data.polygons) for n, o in MODULES.items()}
(SRC / "kit_summary.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
print("RESULT::" + json.dumps({"prefabs": len(PREFABS), "props": len(PROPS), "modules": len(MODULES), "synced": SYNCED, "cams": CAMS}))
