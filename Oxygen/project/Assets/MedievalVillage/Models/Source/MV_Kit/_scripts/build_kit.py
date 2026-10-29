# EONLORE Medieval Village - modular kit builder (Phase 1 + village set) (Blender 5.x, run headless).
# Builds the kit modules, collision proxies, House A prefab and a preview stage,
# saves MV_Kit.blend and exports glb files. Re-running rebuilds everything from scratch.
import bpy, bmesh, math, os, sys
from mathutils import Matrix, Vector
from pathlib import Path

ROOT = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\MedievalVillage")
SRC = ROOT / "Models" / "Source" / "MV_Kit"
TEX = SRC / "Textures"
SG = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\ShrineGlade")
PPM = 32.0

# ------------------------------------------------------------------ reset
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.name = "MV_Kit"
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

# ------------------------------------------------------------------ collections
ROOTCOL = bpy.data.collections.new("EONLORE_MedievalVillage")
scene.collection.children.link(ROOTCOL)
CNAMES = ["00_Blockout", "01_Foundations", "02_Walls", "03_Beams", "04_Doors", "05_Windows", "06_Roofs",
          "07_Chimneys", "08_Balconies", "09_Props_3D", "10_Collision", "11_Terrain_Route", "20_Atlases", "90_PrefabExamples"]
COL = {}
for n in CNAMES:
    c = bpy.data.collections.new(n); ROOTCOL.children.link(c); COL[n] = c
PREVIEW = bpy.data.collections.new("Preview")
scene.collection.children.link(PREVIEW)

# ------------------------------------------------------------------ materials
# name, texture file, texture world size (W,H) in metres, extra
MATDEFS = [
    ("MAT_Stone_Field", "MV_Stone_Field.png", (4.0, 2.0), {}),
    ("MAT_Stone_Cut", "MV_Stone_Cut.png", (2.0, 2.0), {}),
    ("MAT_Plaster_Warm", "MV_Plaster_Warm.png", (2.0, 2.0), {}),
    ("MAT_Wood_DarkBeam", "MV_Wood_DarkBeam.png", (0.5, 2.0), {}),
    ("MAT_Wood_Door", "MV_Wood_Door.png", (1.0, 2.0), {}),
    ("MAT_Roof_SlateDark", "MV_Roof_SlateDark.png", (2.0, 1.5), {}),
    ("MAT_Roof_ShingleDark", "MV_Roof_ShingleDark.png", (2.0, 2.0), {}),
    ("MAT_Metal_IronDark", "MV_Metal_IronDark.png", (0.5, 0.5), {}),
    ("MAT_Cloth_WarmNeutral", "MV_Cloth_WarmNeutral.png", (1.0, 1.0), {}),
    ("MAT_Window_Glass", "MV_Window_Glass.png", (1.0, 1.0), {"emissive": "MV_Window_Glass_Emissive.png", "strength": 4.0}),
    ("MAT_Sign_Tavern", "MV_Sign_Tavern.png", (1.0, 0.75), {}),
    ("MAT_Stone_Chimney", "MV_Stone_Chimney.png", (2.0, 2.0), {}),
    ("MAT_Wood_Planks", "MV_Wood_Planks.png", (2.0, 2.0), {}),
    ("MAT_Roof_Thatch", "MV_Roof_Thatch.png", (2.0, 2.0), {}),
    ("MAT_Soil_Tilled", "MV_Soil_Tilled.png", (2.0, 2.0), {}),
    ("MAT_Water_Stream", "MV_Water_Stream.png", (2.0, 2.0), {}),
    ("MAT_Dirt_Road", "MV_Dirt_Road.png", (2.0, 2.0), {}),
]
MAT = {}
TEXSIZE = {}


def load_img(fname):
    img = bpy.data.images.load(str(TEX / fname), check_existing=True)
    # absolute while building (the glTF export needs to read it); the save makes it relative ("//Textures/...")
    return img


def make_mat(name, fname, extra):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = load_img(fname); tex.interpolation = 'Closest'
    tex.location = (-400, 200)
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.92
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.15
    if "emissive" in extra:
        et = nt.nodes.new("ShaderNodeTexImage"); et.image = load_img(extra["emissive"]); et.interpolation = 'Closest'
        et.location = (-400, -150)
        nt.links.new(et.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = extra["strength"]
    return m


for name, fname, size, extra in MATDEFS:
    MAT[name] = make_mat(name, fname, extra)
    TEXSIZE[name] = size

# ------------------------------------------------------------------ mesh building helpers
class MeshBuilder:
    """Collects primitives (in a canonical local frame, UV'd there, then transformed)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []

    def mi(self, matname):
        if matname not in self.mats:
            self.mats.append(matname)
        return self.mats.index(matname)

    def _uv_for(self, co, key, mat, fit=None):
        W, H = TEXSIZE[mat]
        x, y, z = co
        if key == '-y': u, v = x, z
        elif key == '+y': u, v = -x, z
        elif key == '+x': u, v = y, z
        elif key == '-x': u, v = -y, z
        elif key == '+z': u, v = x, y
        else: u, v = x, -y
        if fit is not None:
            (u0, u1, v0, v1) = fit
            return ((u - u0) / (u1 - u0), (v - v0) / (v1 - v0))
        return (u / W, v / H)

    def box(self, x0, y0, z0, x1, y1, z1, mat, M=None, facemat=None, fit=(), skip=()):
        """Axis-aligned box in canonical frame. facemat: {key: matname}. fit: keys whose UV fills 0..1."""
        M = M or Matrix.Identity(4)
        facemat = facemat or {}
        P = {}
        for ix, X in enumerate((x0, x1)):
            for iy, Y in enumerate((y0, y1)):
                for iz, Z in enumerate((z0, z1)):
                    P[(ix, iy, iz)] = (X, Y, Z)
        faces = {
            '-y': [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)],
            '+y': [(1, 1, 0), (0, 1, 0), (0, 1, 1), (1, 1, 1)],
            '-x': [(0, 1, 0), (0, 0, 0), (0, 0, 1), (0, 1, 1)],
            '+x': [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)],
            '+z': [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)],
            '-z': [(0, 1, 0), (1, 1, 0), (1, 0, 0), (0, 0, 0)],
        }
        for key, idx in faces.items():
            if key in skip:
                continue
            m = facemat.get(key, mat)
            cos = [P[i] for i in idx]
            fitr = None
            if key in fit:
                us = [self._uv_for(c, key, m, (0, 1, 0, 1))[0] for c in cos]
                vs = [self._uv_for(c, key, m, (0, 1, 0, 1))[1] for c in cos]
                fitr = (min(us), max(us), min(vs), max(vs))
            verts = [self.bm.verts.new(M @ Vector(c)) for c in cos]
            f = self.bm.faces.new(verts)
            f.material_index = self.mi(m)
            for loop, c in zip(f.loops, cos):
                loop[self.uv].uv = self._uv_for(c, key, m, fitr)
        return self

    def prism_xz(self, pts, y0, y1, mat, M=None, capmat=None):
        """Extrude a convex XZ polygon (counter-clockwise seen from -Y) between y0 and y1."""
        M = M or Matrix.Identity(4)
        capmat = capmat or mat
        front = [(x, y0, z) for x, z in pts]
        back = [(x, y1, z) for x, z in reversed(pts)]
        for cos, key in ((front, '-y'), (back, '+y')):
            f = self.bm.faces.new([self.bm.verts.new(M @ Vector(c)) for c in cos])
            f.material_index = self.mi(mat)
            for loop, c in zip(f.loops, cos):
                loop[self.uv].uv = self._uv_for(c, key, mat)
        n = len(pts)
        for i in range(n):
            (ax, az), (bx, bz) = pts[i], pts[(i + 1) % n]
            cos = [(ax, y0, az), (ax, y1, az), (bx, y1, bz), (bx, y0, bz)]
            cos = [cos[0], cos[3], cos[2], cos[1]]
            dx, dz = bx - ax, bz - az
            key = ('+z' if dx < 0 else '-z') if abs(dx) > abs(dz) else ('-x' if dz < 0 else '+x')
            f = self.bm.faces.new([self.bm.verts.new(M @ Vector(c)) for c in cos])
            f.material_index = self.mi(capmat)
            for loop, c in zip(f.loops, cos):
                loop[self.uv].uv = self._uv_for(c, key, capmat)
        return self

    def to_object(self, name, coll, loc=(0, 0, 0)):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me); self.bm.free()
        for m in self.mats:
            me.materials.append(MAT[m])
        ob = bpy.data.objects.new(name, me)
        ob.location = loc
        coll.objects.link(ob)
        return ob


def R(axis, deg):
    return Matrix.Rotation(math.radians(deg), 4, axis)


def T(x, y, z):
    return Matrix.Translation((x, y, z))


def col_box(parent, x0, y0, z0, x1, y1, z1, suffix=""):
    """Collision proxy: simple box, no material, parented to its module."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = x0 + (v.co.x + 0.5) * (x1 - x0)
        v.co.y = y0 + (v.co.y + 0.5) * (y1 - y0)
        v.co.z = z0 + (v.co.z + 0.5) * (z1 - z0)
    me = bpy.data.meshes.new("COL_" + parent.name + suffix)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new("COL_" + parent.name + suffix, me)
    COL["10_Collision"].objects.link(ob)
    ob.parent = parent
    ob.display_type = 'WIRE'
    ob.hide_render = True
    return ob


# ------------------------------------------------------------------ module dimensions (metres)
WALL_T = 0.25          # wall thickness; outer face at local y=0, inside towards +Y
WALL_H = 3.0           # storey height
FND_H = 0.6            # foundation height
BEAM = 0.18            # timber section
BEAM_OUT = 0.05        # how far beams stand proud of the plaster
PITCH = 50.0           # roof pitch (steep)
SPAN = 4.0             # roof span (building depth) of the gable modules
EAVE = 0.45            # eave overhang beyond wall face
GABLE_RISE = (SPAN / 2) * math.tan(math.radians(PITCH))

LIB = []  # (object, library position)


def lib(ob, x, y):
    ob.location = (x, y, 0)
    LIB.append(ob)
    return ob


# ---- A. foundation -------------------------------------------------------------
def foundation(L):
    mb = MeshBuilder()
    mb.box(-0.05, -0.06, 0, L + 0.05, WALL_T + 0.05, FND_H, "MAT_Stone_Field", facemat={'+z': "MAT_Stone_Cut"})
    ob = mb.to_object("MV_Foundation_Stone_%dm_A" % L, COL["01_Foundations"])
    col_box(ob, -0.05, -0.06, 0, L + 0.05, WALL_T + 0.05, FND_H)
    return ob


def doorstep():
    mb = MeshBuilder()
    mb.box(-0.75, -0.55, 0, 0.75, 0.0, 0.3, "MAT_Stone_Cut")
    mb.box(-0.85, -0.95, 0, 0.85, -0.5, 0.15, "MAT_Stone_Field")
    ob = mb.to_object("MV_Foundation_DoorStep_A", COL["01_Foundations"])
    col_box(ob, -0.85, -0.95, 0, 0.85, 0, 0.3)
    return ob


# ---- B. walls ------------------------------------------------------------------
def wall_plaster(L):
    mb = MeshBuilder()
    mb.box(0, 0, 0, L, WALL_T, WALL_H, "MAT_Plaster_Warm", facemat={'+z': "MAT_Wood_DarkBeam"})
    ob = mb.to_object("MV_Wall_Plaster_%dm_A" % L, COL["02_Walls"])
    col_box(ob, 0, 0, 0, L, WALL_T, WALL_H)
    return ob


def beam(mb, x0, z0, x1, z1, w=BEAM):
    """Front-face timber from (x0,z0) to (x1,z1) on the wall face plane (y<0 = proud)."""
    dx, dz = x1 - x0, z1 - z0
    L = math.hypot(dx, dz)
    ang = math.degrees(math.atan2(dx, dz))  # angle from +Z towards +X
    M = T(x0, 0, z0) @ R('Y', ang)
    # canonical: vertical beam centred on x, grain along local Z
    mb.box(-w / 2, -BEAM_OUT, 0, w / 2, 0.02, L, "MAT_Wood_DarkBeam", M=M)


def wall_timber(variant):
    """2 m timber-frame panel. A: cross-braced. B: opening panel (window or door overlay)."""
    L = 2.0
    mb = MeshBuilder()
    mb.box(0, 0, 0, L, WALL_T, WALL_H, "MAT_Plaster_Warm", facemat={'+z': "MAT_Wood_DarkBeam"})
    h = BEAM / 2
    beam(mb, h, 0, h, WALL_H)                         # left post (right one comes from the neighbour/corner)
    beam(mb, 0, WALL_H - h, L, WALL_H - h)            # top plate (horizontal: rotated vertical beam)
    if variant == "A":
        beam(mb, 0, h, L, h)                          # sill
        beam(mb, L / 2, 0, L / 2, WALL_H)             # centre post
        beam(mb, 0, 1.55, L, 1.55, w=0.15)            # mid rail
        beam(mb, BEAM, BEAM, L / 2 - h, 1.48, w=0.14)  # braces (lower panels)
        beam(mb, L - BEAM / 2, BEAM, L / 2 + h, 1.48, w=0.14)
        beam(mb, BEAM, WALL_H - BEAM, L / 2 - h, 1.62, w=0.14)  # upper panels, mirrored
        beam(mb, L - BEAM / 2, WALL_H - BEAM, L / 2 + h, 1.62, w=0.14)
    else:
        beam(mb, 0, h, 0.32, h)                       # sill stubs either side of the opening
        beam(mb, 1.68, h, L, h)
        beam(mb, 0, 2.42, L, 2.42, w=0.14)            # lintel rail above windows / doors
    ob = mb.to_object("MV_Wall_Timber_2m_%s" % variant, COL["02_Walls"])
    col_box(ob, 0, 0, 0, L, WALL_T, WALL_H)
    return ob


def wall_gable():
    """Triangular gable infill for a SPAN-deep building, sits on top of the side walls."""
    mb = MeshBuilder()
    pts = [(0, 0), (SPAN, 0), (SPAN / 2, GABLE_RISE)]
    mb.prism_xz(pts, 0, WALL_T, "MAT_Plaster_Warm")
    h = BEAM / 2
    beam(mb, SPAN / 2, 0, SPAN / 2, GABLE_RISE - 0.15)   # king post
    cz = 1.05
    cx = cz / math.tan(math.radians(PITCH))
    beam(mb, cx + 0.1, cz, SPAN - cx - 0.1, cz, w=0.16)   # collar
    beam(mb, 0.25, 0.12, SPAN / 2 - h, cz - 0.05, w=0.13)  # struts
    beam(mb, SPAN - 0.25, 0.12, SPAN / 2 + h, cz - 0.05, w=0.13)
    ob = mb.to_object("MV_Wall_Gable_4m_A", COL["02_Walls"])
    col_box(ob, 0, 0, 0, SPAN, WALL_T, GABLE_RISE * 0.6)
    return ob


# ---- E. beams --------------------------------------------------------------------
def corner_beam():
    mb = MeshBuilder()
    s = 0.26
    mb.box(-0.06, -0.06, 0, s - 0.06, s - 0.06, WALL_H, "MAT_Wood_DarkBeam")
    return mb.to_object("MV_Beam_Corner_A", COL["03_Beams"])


def floor_beam():
    mb = MeshBuilder()
    # horizontal floor beam 2 m, built as a vertical beam rotated onto X
    mb.box(-0.11, -0.08, 0, 0.11, 0.02, 2.0, "MAT_Wood_DarkBeam", M=T(0, 0, 0.11) @ R('Y', 90))
    return mb.to_object("MV_Beam_Floor_2m_A", COL["03_Beams"])


# ---- C. door ---------------------------------------------------------------------
def door():
    mb = MeshBuilder()
    W, H = 1.0, 2.0
    fw = 0.14
    mb.box(-W / 2, -0.06, 0, W / 2, -0.02, H, "MAT_Wood_Door", fit=('-y',))       # door leaf
    mb.box(-W / 2 - fw, -0.10, 0, -W / 2, 0.02, H + fw, "MAT_Wood_DarkBeam")       # jambs
    mb.box(W / 2, -0.10, 0, W / 2 + fw, 0.02, H + fw, "MAT_Wood_DarkBeam")
    mb.box(-W / 2 - fw - 0.08, -0.12, H, W / 2 + fw + 0.08, 0.02, H + fw + 0.04, "MAT_Wood_DarkBeam",
           M=Matrix.Identity(4))                                                      # lintel
    return mb.to_object("MV_Door_Wood_A", COL["04_Doors"])


# ---- D. window -------------------------------------------------------------------
def window_small():
    mb = MeshBuilder()
    W, H = 0.8, 0.9
    fw = 0.1
    mb.box(-W / 2, -0.04, 0, W / 2, -0.01, H, "MAT_Window_Glass", fit=('-y',))
    mb.box(-W / 2 - fw, -0.09, -fw, -W / 2, 0.02, H + fw, "MAT_Wood_DarkBeam")
    mb.box(W / 2, -0.09, -fw, W / 2 + fw, 0.02, H + fw, "MAT_Wood_DarkBeam")
    mb.box(-W / 2 - fw, -0.10, H, W / 2 + fw, 0.02, H + fw + 0.02, "MAT_Wood_DarkBeam")
    mb.box(-W / 2 - fw - 0.06, -0.16, -fw - 0.06, W / 2 + fw + 0.06, 0.02, -fw + 0.0, "MAT_Stone_Cut")  # sill
    # flower-box-ready ledge stays simple; window boxes are a separate prop later
    return mb.to_object("MV_Window_Small_A", COL["05_Windows"])


# ---- 7. roof --------------------------------------------------------------------
SLAB_T = 0.16


def roof_slab(mb, x0, x1, roofmat, span=SPAN, eave=EAVE, pitch=PITCH, slab=SLAB_T, ridge=True, fascia=True,
              ridgemat=None, basemat="MAT_Wood_DarkBeam"):
    """Both slopes between x0..x1. Local roof frame: front wall face y=0, back y=span, wall top z=0."""
    t = math.tan(math.radians(pitch)); c = math.cos(math.radians(pitch))
    rise = (span / 2) * t
    slope_len = (span / 2 + eave) / c + 0.12  # a touch past the ridge
    # slab canonical: x along ridge, y up the slope from the eave, z = thickness (bottom at z=0)
    eave_y, eave_z = -eave, -eave * t
    Mf = T(0, eave_y, eave_z) @ R('X', pitch)
    Mb = T((x0 + x1), span, 0) @ R('Z', 180) @ Mf  # the back slope: turned about the ridge line's centre
    for M in (Mf, Mb):
        mb.box(x0, 0, 0, x1, slope_len, slab, basemat, M=M, facemat={'+z': roofmat})
    if ridge:  # square timber turned 45 degrees, roof-dark faces
        s = 0.14 * (slab / SLAB_T) ** 0.5
        Mr = T(0, span / 2, rise + slab / c - 0.06) @ R('X', 45)
        mb.box(x0, -s, -s, x1, s, s, ridgemat or roofmat, M=Mr)
    if fascia:  # thick outer edge along both eaves
        for M in (Matrix.Identity(4), T(x0 + x1, span, 0) @ R('Z', 180)):
            mb.box(x0, eave_y - 0.06, eave_z - 0.18, x1, eave_y + 0.04, eave_z + 0.06, "MAT_Wood_DarkBeam", M=M)
    return Mf, Mb, slope_len


def roof_gable(L, roofmat="MAT_Roof_SlateDark", tag="Gable"):
    mb = MeshBuilder()
    roof_slab(mb, 0, L, roofmat)
    return mb.to_object("MV_Roof_%s_%dm_A" % (tag, L), COL["06_Roofs"])


def roof_gable_end(roofmat="MAT_Roof_SlateDark", tag="GableEnd"):
    """Gable termination: 0.4 m overhang past the gable wall + barge boards. Place at the wall line; rotate 180 for the far end."""
    mb = MeshBuilder()
    OV = 0.4
    Mf, Mb, slope_len = roof_slab(mb, -OV, 0, roofmat)
    for M, xo in ((Mf, -OV), (Mb, 0.0)):  # barge boards under the outer roof edge
        mb.box(xo - 0.05, 0.0, -0.22, xo + 0.06, slope_len - 0.08, SLAB_T + 0.03, "MAT_Wood_DarkBeam", M=M)
    return mb.to_object("MV_Roof_%s_A" % tag, COL["06_Roofs"])


def roof_leanto(L=2.0, run=2.2, pitch=22.0, roofmat="MAT_Roof_ShingleDark", tag=""):
    """Single slope from a wall (local y=0 face, top edge z=0) out to -Y; hangs on any wall. Posts are MV_Post_Timber_A."""
    mb = MeshBuilder()
    t = math.tan(math.radians(pitch))
    slope = run / math.cos(math.radians(pitch))
    # canonical slab: y from the outer eave (0) up to the wall (slope); rotate so +y climbs towards +Y world
    M = T(0, -run, -run * t) @ R('X', pitch)
    mb.box(0, 0, 0, L, slope + 0.05, 0.12, "MAT_Wood_DarkBeam", M=M, facemat={'+z': roofmat})
    mb.box(0, -run - 0.05, -run * t - 0.16, L, -run + 0.05, -run * t + 0.04, "MAT_Wood_DarkBeam")  # fascia
    mb.box(0, -0.12, -0.2, L, 0.0, 0.02, "MAT_Wood_DarkBeam")                                      # wall plate
    return mb.to_object("MV_Roof_LeanTo%s_%dm_A" % (tag, L), COL["06_Roofs"])


def post_timber(h=2.5):
    mb = MeshBuilder()
    mb.box(-0.09, -0.09, 0, 0.09, 0.09, h, "MAT_Wood_DarkBeam")
    mb.box(-0.16, -0.16, 0, 0.16, 0.16, 0.22, "MAT_Stone_Cut")   # pad stone
    ob = mb.to_object("MV_Post_Timber_A", COL["03_Beams"])
    col_box(ob, -0.16, -0.16, 0, 0.16, 0.16, h)
    return ob


# ---- 8. chimney ----------------------------------------------------------------
def chimney(total_h, tag="A"):
    mb = MeshBuilder()
    m = "MAT_Stone_Chimney"
    base_h = min(3.9, total_h - 2.6)
    mb.box(-0.6, 0, 0, 0.6, 0.75, base_h, m, facemat={'+z': "MAT_Stone_Cut"})          # breast
    mb.box(-0.5, 0.0, base_h, 0.5, 0.68, base_h + 0.35, m, facemat={'+z': "MAT_Stone_Cut"})  # shoulder
    mb.box(-0.36, 0.06, base_h + 0.35, 0.36, 0.62, total_h, m)                           # stack
    mb.box(-0.45, -0.03, total_h, 0.45, 0.71, total_h + 0.12, "MAT_Stone_Cut")           # cap
    mb.box(-0.22, 0.2, total_h + 0.12, 0.22, 0.48, total_h + 0.3, "MAT_Metal_IronDark")  # pot
    ob = mb.to_object("MV_Chimney_Stone_%s" % tag, COL["07_Chimneys"])
    col_box(ob, -0.6, 0, 0, 0.6, 0.75, base_h)
    return ob


# ---- more walls / openings (Phase 2) -------------------------------------------
def wall_material(L, kind):
    """Plain 2 m panels: 'stone' (dressed cool stone ground floors), 'plank' (barns, sheds)."""
    mb = MeshBuilder()
    if kind == "stone":
        mb.box(0, 0, 0, L, WALL_T, WALL_H, "MAT_Stone_Chimney", facemat={'+z': "MAT_Stone_Cut"})
        name = "MV_Wall_Stone_%dm_A" % L
    else:
        mb.box(0, 0, 0, L, WALL_T, WALL_H, "MAT_Wood_Planks", facemat={'+z': "MAT_Wood_DarkBeam"})
        beam(mb, BEAM / 2, 0, BEAM / 2, WALL_H)                 # left post
        beam(mb, 0, WALL_H - BEAM / 2, L, WALL_H - BEAM / 2)    # top plate
        beam(mb, 0, 0.12, L, 0.12, w=0.16)                     # sole plate
        name = "MV_Wall_Plank_%dm_A" % L
    ob = mb.to_object(name, COL["02_Walls"])
    col_box(ob, 0, 0, 0, L, WALL_T, WALL_H)
    return ob


def wall_gable_plank():
    mb = MeshBuilder()
    pts = [(0, 0), (SPAN, 0), (SPAN / 2, GABLE_RISE)]
    mb.prism_xz(pts, 0, WALL_T, "MAT_Wood_Planks")
    beam(mb, SPAN / 2, 0, SPAN / 2, GABLE_RISE - 0.15)
    beam(mb, 0.6, 0.9, SPAN - 0.6, 0.9, w=0.15)
    ob = mb.to_object("MV_Wall_Gable_Plank_4m_A", COL["02_Walls"])
    return ob


def door_barn():
    """Double plank door, 2.4 x 2.5 m, Z-braced; overlay like MV_Door_Wood_A (origin bottom centre at the wall face)."""
    mb = MeshBuilder()
    W, H = 2.4, 2.5
    mb.box(-W / 2, -0.07, 0, W / 2, -0.02, H, "MAT_Wood_Planks")
    for x0 in (-W / 2, 0.0):
        x1 = x0 + W / 2
        for z in (0.25, H - 0.25):
            mb.box(x0 + 0.08, -0.11, z - 0.08, x1 - 0.08, -0.06, z + 0.08, "MAT_Wood_DarkBeam", M=Matrix.Identity(4))
        # diagonal brace across each leaf
        dx, dz = (x1 - x0) - 0.3, H - 0.66
        L = math.hypot(dx, dz); ang = math.degrees(math.atan2(dx, dz))
        mb.box(-0.07, -0.11, 0, 0.07, -0.06, L, "MAT_Wood_DarkBeam", M=T(x0 + 0.15, 0, 0.33) @ R('Y', ang))
    mb.box(-0.03, -0.12, 0.2, 0.03, -0.06, H - 0.2, "MAT_Wood_DarkBeam")                      # meeting stile
    mb.box(-W / 2 - 0.16, -0.12, 0, -W / 2, 0.02, H + 0.16, "MAT_Wood_DarkBeam")                # frame
    mb.box(W / 2, -0.12, 0, W / 2 + 0.16, 0.02, H + 0.16, "MAT_Wood_DarkBeam")
    mb.box(-W / 2 - 0.25, -0.14, H, W / 2 + 0.25, 0.02, H + 0.2, "MAT_Wood_DarkBeam")
    for x in (-0.35, 0.35):
        mb.box(x - 0.03, -0.13, 1.1, x + 0.03, -0.07, 1.35, "MAT_Metal_IronDark")              # handles
    return mb.to_object("MV_Door_Barn_A", COL["04_Doors"])


def window_shutter():
    """MV_Window_Small_A with two open plank shutters either side."""
    mb = MeshBuilder()
    W, H, fw = 0.8, 0.9, 0.1
    mb.box(-W / 2, -0.04, 0, W / 2, -0.01, H, "MAT_Window_Glass", fit=('-y',))
    mb.box(-W / 2 - fw, -0.09, -fw, -W / 2, 0.02, H + fw, "MAT_Wood_DarkBeam")
    mb.box(W / 2, -0.09, -fw, W / 2 + fw, 0.02, H + fw, "MAT_Wood_DarkBeam")
    mb.box(-W / 2 - fw, -0.10, H, W / 2 + fw, 0.02, H + fw + 0.02, "MAT_Wood_DarkBeam")
    mb.box(-W / 2 - fw - 0.06, -0.16, -fw - 0.06, W / 2 + fw + 0.06, 0.02, -fw, "MAT_Stone_Cut")
    for side in (-1, 1):   # shutters folded back against the wall
        x0 = side * (W / 2 + fw + 0.02)
        x1 = x0 + side * 0.44
        mb.box(min(x0, x1), -0.06, -0.02, max(x0, x1), -0.02, H + 0.02, "MAT_Wood_Door")
        for z in (0.15, H - 0.15):
            mb.box(min(x0, x1) + 0.03, -0.08, z - 0.03, max(x0, x1) - 0.03, -0.05, z + 0.03, "MAT_Metal_IronDark")
    return mb.to_object("MV_Window_Shutter_A", COL["05_Windows"])


def flower_box():
    mb = MeshBuilder()
    mb.box(-0.5, -0.3, 0, 0.5, 0.0, 0.22, "MAT_Wood_Planks")
    mb.box(-0.46, -0.27, 0.22, 0.46, -0.03, 0.24, "MAT_Cloth_WarmNeutral")  # soil line (flowers are sprites)
    return mb.to_object("MV_FlowerBox_A", COL["05_Windows"])


# ---- props (3D, set dressing that is walked around) ---------------------------
def fence():
    mb = MeshBuilder()
    m = "MAT_Wood_DarkBeam"
    for i, x in enumerate((0.05, 1.0, 1.95)):
        h = 1.05 if i != 1 else 0.98
        mb.box(x - 0.06, -0.06, 0, x + 0.06, 0.06, h, m)
        mb.box(x - 0.04, -0.04, h, x + 0.04, 0.04, h + 0.06, m)   # cap nub (silhouette)
    for z in (0.35, 0.78):
        mb.box(0.0, -0.1, -0.05, 0.1, -0.06, 2.0, m, M=T(0, 0, z) @ R('Y', 90))
    ob = mb.to_object("MV_Fence_Wood_2m_A", COL["09_Props_3D"])
    col_box(ob, 0, -0.1, 0, 2.0, 0.06, 1.0)
    return ob


def hanging_sign():
    """Wall bracket sticking out along +X from a wall face at x=0; board faces -Y / +Y."""
    mb = MeshBuilder()
    m = "MAT_Wood_DarkBeam"
    mb.box(-0.06, -0.06, 0, 0.06, 0.06, 1.05, m, M=T(0, 0, 0) @ R('Y', 90))              # arm along +X
    mb.box(-0.04, -0.04, 0, 0.04, 0.04, 0.8, m, M=T(0.02, 0, -0.55) @ R('Y', 45))         # diagonal brace
    mb.box(-0.1, -0.1, -0.6, 0.0, 0.1, 0.1, m)                                             # wall plate
    for x in (0.38, 0.92):
        mb.box(x - 0.015, -0.015, -0.2, x + 0.015, 0.015, -0.06, "MAT_Metal_IronDark")    # hangers
    mb.box(0.27, -0.025, -0.95, 1.03, 0.025, -0.2, "MAT_Sign_Tavern", fit=('-y', '+y'),
           facemat={'+x': m, '-x': m, '+z': m, '-z': m})
    return mb.to_object("MV_Sign_Hanging_A", COL["09_Props_3D"])


def well():
    """Village well: a ring of eight cut-stone blocks, water, two posts, a winch beam and a little shingle roof. Origin at the centre."""
    mb = MeshBuilder()
    r, n = 0.78, 8
    for i in range(n):
        a = 360.0 / n * i
        M = R('Z', a) @ T(0, -r, 0)
        mb.box(-0.34, -0.17, 0, 0.34, 0.17, 0.82, "MAT_Stone_Cut", M=M)
        mb.box(-0.36, -0.2, 0.82, 0.36, 0.2, 0.92, "MAT_Stone_Cut", M=M)   # coping
    mb.box(-0.62, -0.62, 0.5, 0.62, 0.62, 0.52, "MAT_Metal_IronDark", skip=('-z',))  # dark water
    for x in (-0.95, 0.95):
        mb.box(x - 0.08, -0.08, 0.0, x + 0.08, 0.08, 2.15, "MAT_Wood_DarkBeam")
    mb.box(-1.05, -0.06, 1.55, 1.05, 0.06, 1.67, "MAT_Wood_DarkBeam", M=Matrix.Identity(4))  # winch beam
    mb.box(-0.12, -0.12, 0.95, 0.12, 0.12, 1.2, "MAT_Wood_Planks")                               # bucket
    mb.box(-0.01, -0.01, 1.2, 0.01, 0.01, 1.56, "MAT_Cloth_WarmNeutral")                          # rope
    mb.box(1.03, -0.03, 1.45, 1.12, 0.03, 1.75, "MAT_Metal_IronDark")                             # crank
    # roof: a short shingle gable along X, its own frame centred on y
    sub = MeshBuilder()
    roof_slab(sub, -1.25, 1.25, "MAT_Roof_ShingleDark", span=1.5, eave=0.3, pitch=38, slab=0.1)
    for v in sub.bm.verts:
        v.co += Vector((0, -0.75, 2.12))
    # merge sub into mb
    sub_mats = sub.mats
    me_tmp = bpy.data.meshes.new("_tmp"); sub.bm.to_mesh(me_tmp); sub.bm.free()
    bm2 = mb.bm
    uv2 = mb.uv
    for p in me_tmp.polygons:
        vs = [bm2.verts.new(me_tmp.vertices[i].co) for i in p.vertices]
        f = bm2.faces.new(vs)
        f.material_index = mb.mi(sub_mats[p.material_index])
        for loop, li in zip(f.loops, p.loop_indices):
            loop[uv2].uv = me_tmp.uv_layers[0].data[li].uv
    bpy.data.meshes.remove(me_tmp)
    ob = mb.to_object("MV_Well_A", COL["09_Props_3D"])
    col_box(ob, -1.0, -1.0, 0, 1.0, 1.0, 1.0)
    return ob


def lantern_post():
    """2.7 m timber post on a stone pad with an arm and a hanging iron lantern (emissive glass). Origin at the post's foot.
    The light sits at (0.62, 0, 2.05) from the origin: put a point light there in the scene."""
    mb = MeshBuilder()
    mb.box(-0.2, -0.2, 0, 0.2, 0.2, 0.3, "MAT_Stone_Cut")
    mb.box(-0.08, -0.08, 0.3, 0.08, 0.08, 2.75, "MAT_Wood_DarkBeam")
    mb.box(-0.1, -0.1, 2.75, 0.1, 0.1, 2.85, "MAT_Wood_DarkBeam")
    mb.box(-0.05, -0.05, 0, 0.05, 0.05, 0.78, "MAT_Wood_DarkBeam", M=T(0.0, 0, 2.55) @ R('Y', 90))  # arm along +X
    mb.box(-0.03, -0.03, 0, 0.03, 0.03, 0.5, "MAT_Wood_DarkBeam", M=T(0.04, 0, 2.18) @ R('Y', 45))  # brace
    lx = 0.62
    mb.box(lx - 0.01, -0.01, 2.36, lx + 0.01, 0.01, 2.55, "MAT_Metal_IronDark")                  # hanger
    mb.box(lx - 0.15, -0.15, 2.28, lx + 0.15, 0.15, 2.36, "MAT_Metal_IronDark")                  # cap
    mb.box(lx - 0.07, -0.07, 2.36, lx + 0.07, 0.07, 2.42, "MAT_Metal_IronDark")
    mb.box(lx - 0.11, -0.11, 1.9, lx + 0.11, 0.11, 2.28, "MAT_Window_Glass", fit=('-y', '+y', '-x', '+x'))  # glass
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.box(lx + sx * 0.12 - 0.02, sy * 0.12 - 0.02, 1.88, lx + sx * 0.12 + 0.02, sy * 0.12 + 0.02, 2.3, "MAT_Metal_IronDark")
    mb.box(lx - 0.13, -0.13, 1.84, lx + 0.13, 0.13, 1.9, "MAT_Metal_IronDark")                   # base
    ob = mb.to_object("MV_LanternPost_A", COL["09_Props_3D"])
    col_box(ob, -0.2, -0.2, 0, 0.2, 0.2, 2.8)
    return ob


def market_stall():
    """2.4 x 1.2 m stall: plank counter, four posts, a cloth awning sloping to the front. Origin front centre at the ground.
    Produce goes on the counter as sprites (top at z 0.9)."""
    mb = MeshBuilder()
    W, D = 2.4, 1.2
    mb.box(-W / 2, 0.0, 0, W / 2, 0.75, 0.9, "MAT_Wood_Planks", facemat={'+z': "MAT_Wood_DarkBeam"})
    mb.box(-W / 2 - 0.05, -0.05, 0.86, W / 2 + 0.05, 0.8, 0.94, "MAT_Wood_DarkBeam")
    for x in (-W / 2 + 0.06, W / 2 - 0.06):
        mb.box(x - 0.06, -0.08, 0, x + 0.06, 0.04, 2.15, "MAT_Wood_DarkBeam")
        mb.box(x - 0.06, D - 0.06, 0, x + 0.06, D + 0.06, 2.55, "MAT_Wood_DarkBeam")
    # awning: a cloth slab from the back posts' tops down to the front ones, overhanging to the front
    pitch = math.degrees(math.atan2(0.4, D))
    run = D + 0.45
    L = run / math.cos(math.radians(pitch))
    M = T(0, -0.45, 2.15 - 0.45 * math.tan(math.radians(pitch))) @ R('X', pitch)
    mb.box(-W / 2 - 0.15, 0, 0, W / 2 + 0.15, L, 0.05, "MAT_Cloth_WarmNeutral", M=M)
    z_front = 2.15 - 0.45 * math.tan(math.radians(pitch))
    mb.box(-W / 2 - 0.15, -0.5, z_front - 0.3, W / 2 + 0.15, -0.45, z_front + 0.02, "MAT_Cloth_WarmNeutral")  # valance
    ob = mb.to_object("MV_MarketStall_A", COL["09_Props_3D"])
    col_box(ob, -W / 2, 0, 0, W / 2, D, 1.0)
    return ob


def cart():
    """Hand cart: plank bed with side boards on two disc wheels, shafts resting on the ground. Origin at the axle's centre on the ground."""
    mb = MeshBuilder()
    r = 0.45
    mb.box(-0.6, -0.9, r + 0.05, 0.6, 0.6, r + 0.17, "MAT_Wood_Planks")
    for x in (-0.62, 0.56):
        mb.box(x, -0.9, r + 0.17, x + 0.06, 0.6, r + 0.5, "MAT_Wood_Planks")
    for y in (-0.92, 0.56):
        mb.box(-0.62, y, r + 0.17, 0.62, y + 0.06, r + 0.45, "MAT_Wood_Planks")
    # wheels: 12-sided discs in the YZ plane (prism along X), iron rim = slightly bigger dark disc behind
    n = 12
    pts = [(math.cos(2 * math.pi * i / n) * r, math.sin(2 * math.pi * i / n) * r) for i in range(n)]
    rim = [(math.cos(2 * math.pi * i / n) * (r + 0.03), math.sin(2 * math.pi * i / n) * (r + 0.03)) for i in range(n)]
    for x in (-0.72, 0.66):
        Mw = T(x, 0, r) @ R('Z', 90)
        mb.prism_xz(pts, -0.05, 0.0, "MAT_Wood_Door", M=Mw, capmat="MAT_Metal_IronDark")
        mb.prism_xz(rim, -0.04, -0.01, "MAT_Metal_IronDark", M=Mw)
    mb.box(-0.72, -0.04, r - 0.04, 0.72, 0.04, r + 0.04, "MAT_Wood_DarkBeam")      # axle
    for x in (-0.45, 0.45):                                                          # shafts down to the ground in front
        L = math.hypot(1.3, r + 0.1)
        ang = math.degrees(math.atan2(r + 0.1, 1.3))
        mb.box(x - 0.04, 0, -0.04, x + 0.04, L, 0.04, "MAT_Wood_DarkBeam", M=T(0, -2.2, 0.04) @ R('X', ang))
    ob = mb.to_object("MV_Cart_A", COL["09_Props_3D"])
    col_box(ob, -0.75, -2.2, 0, 0.75, 0.6, 1.0)
    return ob


SCRIPTS = Path(__file__).resolve().parent
def farm(phase):
    g = globals(); g["FARM_PHASE"] = phase
    exec(compile((SCRIPTS / "farm_kit.py").read_text(encoding="utf-8"), "farm_kit.py", "exec"), g)


farm("A")

# ------------------------------------------------------------------ library
mods = {}
LIBROWS = [
    [("fnd4", foundation(4)), ("fnd2", foundation(2)), ("step", doorstep())],
    [("wp2", wall_plaster(2)), ("wp4", wall_plaster(4)), ("wtA", wall_timber("A")), ("wtB", wall_timber("B")),
     ("ws2", wall_material(2, "stone")), ("wk2", wall_material(2, "plank"))],
    [("gable", wall_gable()), ("gableK", wall_gable_plank()), ("corner", corner_beam()), ("fbeam", floor_beam()), ("post", post_timber())],
    [("door", door()), ("doorBarn", door_barn()), ("win", window_small()), ("winS", window_shutter()), ("fbox", flower_box())],
    [("roof2", roof_gable(2)), ("roofEnd", roof_gable_end()), ("roofK2", roof_gable(2, "MAT_Roof_ShingleDark", "GableShingle")),
     ("roofKEnd", roof_gable_end("MAT_Roof_ShingleDark", "GableShingleEnd")), ("lean", roof_leanto())],
    [("chim", chimney(7.0, "A")), ("chimB", chimney(10.0, "B"))],
    [("fence", fence()), ("sign", hanging_sign()), ("well", well()), ("lantern", lantern_post()), ("stall", market_stall()), ("cart", cart())],
]
LIBROWS += FARM_ROWS
LIBRARY_ORIGIN = Vector((-40, 0, 0))
for row, items in enumerate(LIBROWS):
    for col, (key, ob) in enumerate(items):
        mods[key] = lib(ob, col * 5.0, row * 6.0)
for ob in LIB:
    ob.location += LIBRARY_ORIGIN
    ob.hide_render = True  # library stays out of preview renders (still in the file)

# ------------------------------------------------------------------ prefab assembler (instances of the modules)
PREFABS = {}


def new_prefab(name, loc=(0, 0, 0)):
    root = bpy.data.objects.new(name, None)
    COL["90_PrefabExamples"].objects.link(root)
    root.empty_display_type = 'PLAIN_AXES'
    root.location = loc
    PREFABS[name] = root
    return root


def place(root, key, x, y, z, rot=0, sx=1.0):
    src = mods[key]
    ob = bpy.data.objects.new(src.name + "." + root.name, src.data)  # linked mesh data = true reuse
    COL["90_PrefabExamples"].objects.link(ob)
    ob.parent = root
    ob.location = (x, y, z)
    ob.rotation_euler = (0, 0, math.radians(rot))
    ob.scale = (sx, 1, 1)
    for ch in src.children:  # carry collision proxies
        c = bpy.data.objects.new(ch.name.replace(src.name, ob.name), ch.data)
        COL["90_PrefabExamples"].objects.link(c)
        c.parent = ob; c.display_type = 'WIRE'; c.hide_render = True; c["hydro_kind"] = ch.get("hydro_kind", "COL")
    return ob


PANEL = {"A": "wtA", "B": "wtB", "P": "wp2", "S": "ws2", "K": "wk2"}


def house(name, W, floors, front, right, left=None, back=None, roof="slate", chimney=None, opens=(), leanto=None,
          sign=False, step_x=None, plank_gable=False, loc=(0, 0, 0)):
    """A rectangular building W x 4 m (W even), its origin at the front wall's centre at the ground, front facing -Y.
    floors: storeys. front/right/left/back: per storey, a string of 2 m panel letters (A braced timber, B timber opening,
    P plaster, S stone, K plank), left to right as seen from outside; left/back default to braced timber. opens: (key, side,
    x along that wall, storey, z in storey). chimney: 'left'/'right'. leanto: ('right'|'front', x along the wall (left end),
    length in 2 m pieces).
    Material rule (Oct 29, 2026): houses are timber frame over plaster (A, B) all round, barns and sheds planks (K);
    stone is for foundations, chimneys and steps only, so S and plain P panels are left in the library but unused."""
    D = SPAN
    root = new_prefab(name, loc)
    ox = -W / 2.0
    F = FND_H
    nf = len(front)
    WT = F + WALL_H * nf

    def P(key, x, y, z, rot=0, sx=1.0):
        return place(root, key, ox + x, y, z, rot, sx)

    # foundations (ring of 2 m pieces)
    for i in range(int(W // 2)):
        P("fnd2", 2 * i, 0, 0); P("fnd2", W - 2 * i, D, 0, 180)
    for i in range(int(D // 2)):
        P("fnd2", 0, D - 2 * i, 0, -90); P("fnd2", W, 2 * i, 0, 90)
    left = left or ["A" * int(D // 2)] * nf
    back = back or ["A" * int(W // 2)] * nf
    for s in range(nf):
        z = F + WALL_H * s
        for i, k in enumerate(front[s]):
            P(PANEL[k], 2 * i, 0, z)
        for i, k in enumerate(right[s]):
            P(PANEL[k], W, 2 * i, z, 90)
        for i, k in enumerate(back[s]):
            P(PANEL[k], W - 2 * i, D, z, 180)
        for i, k in enumerate(left[s]):
            P(PANEL[k], 0, D - 2 * i, z, -90)
        for (cx, cy, r) in ((0, 0, 0), (W, 0, 90), (W, D, 180), (0, D, -90)):
            P("corner", cx, cy, z, r)
        if s > 0:  # a timber floor band between storeys, front and right side
            for i in range(int(W // 2)):
                P("fbeam", 2 * i, -0.02, z - 0.11)
            for i in range(int(D // 2)):
                P("fbeam", W + 0.02, 2 * i, z - 0.11, 90)
    gable = "gableK" if plank_gable else "gable"
    P(gable, 0, D, WT, -90); P(gable, W, 0, WT, 90)
    rk, ek = {"slate": ("roof2", "roofEnd"), "thatch": ("roofT2", "roofTEnd")}.get(roof, ("roofK2", "roofKEnd"))
    lk = {"slate": "leanS", "thatch": "leanT"}.get(roof, "lean")   # lean-to uses the house's own roof material
    for i in range(int(W // 2)):
        P(rk, 2 * i, 0, WT)
    P(ek, 0, 0, WT); P(ek, W, D, WT, 180)
    for (key, side, x, storey, zz) in opens:
        z = F + WALL_H * storey + zz
        if side == "front":
            P(key, x, 0, z)
        elif side == "right":
            P(key, W, x, z, 90)
        elif side == "left":
            P(key, 0, D - x, z, -90)
        elif side == "gable_right":
            P(key, W, x, WT + zz, 90)
    if step_x is not None:
        P("step", step_x, -0.06, 0)
    if chimney:
        ck = "chim" if nf == 1 else "chimB"
        if chimney == "left":
            P(ck, 0, 2.7, 0, 90)
        else:
            P(ck, W, 1.3, 0, -90)
    if leanto:
        side, x0, n = leanto
        top = F + 2.75 if side == "front" else F + 2.9
        for i in range(n):
            if side == "right":
                P(lk, W, x0 + 2 * i, top, 90)
            else:
                P(lk, x0 + 2 * i, 0, top)
        run, pitch = 2.2, 22.0
        drop = run * math.tan(math.radians(pitch))
        post_h = top - drop - 0.16
        for i in range(n + 1):
            if side == "right":
                p = P("post", W + run - 0.12, x0 + 2 * i, 0)
            else:
                p = P("post", x0 + 2 * i, -run + 0.12, 0)
            p.scale = (1, 1, post_h / 2.5)
    if sign:
        P("sign", W + 0.02, 0.55, F + 2.55)
    return root


W6, F = 6.0, FND_H
# House A: the Phase 1 cottage (same layout as before), now from the assembler.
house("MV_House_A", 6, 1, front=["BBA"], right=["AB"], chimney="left", sign=True, step_x=3.0,
      opens=[("win", "front", 1.0, 0, 1.25), ("door", "front", 3.0, 0, 0.0), ("win", "right", 3.0, 0, 1.25),
             ("win", "gable_right", 2.0, 0, 0.35)], loc=(3.0, 0, 0))
# House B: two storeys of timber frame over plaster with a floor band, a lean-to on the right (store / woodshed).
house("MV_House_B", 6, 2, front=["BBB", "BAB"], right=["AA", "AB"], left=["AA", "AA"], back=["AAA", "AAA"],
      chimney="left", step_x=3.0, leanto=("right", 0.0, 2),
      opens=[("door", "front", 3.0, 0, 0.0), ("winS", "front", 1.0, 0, 1.2), ("winS", "front", 5.0, 0, 1.2),
             ("win", "front", 1.0, 1, 1.0), ("win", "front", 5.0, 1, 1.0), ("win", "right", 3.0, 1, 1.0),
             ("fbox", "front", 1.0, 0, 1.0), ("fbox", "front", 5.0, 0, 1.0)], loc=(3.0, 14.0, 0))
# Barn: plank walls, shingle roof, big double door, lean-to on the right for hay and firewood.
house("MV_Barn_A", 8, 1, front=["KKKK"], right=["KK"], left=["KK"], back=["KKKK"], roof="shingle", plank_gable=True,
      leanto=("right", 0.0, 2), opens=[("doorBarn", "front", 4.0, 0, 0.0), ("winS", "front", 1.2, 0, 1.3)], loc=(16.0, 0, 0))
# Workshop / shop: timber frame over plaster, shuttered windows, an awning over the front (slate, like its roof).
house("MV_Shop_A", 6, 1, front=["BBB"], right=["AB"], roof="slate", chimney="right", step_x=3.0, leanto=("front", 0.0, 3),
      opens=[("winS", "front", 1.0, 0, 1.2), ("door", "front", 3.0, 0, 0.0), ("winS", "front", 5.0, 0, 1.2),
             ("winS", "right", 3.0, 0, 1.2), ("fbox", "front", 1.0, 0, 1.0)], loc=(16.0, 14.0, 0))

# Single-module prefabs placed directly in scenes.
for key, pname in (("fence", "MV_Fence"), ("well", "MV_Well"), ("lantern", "MV_LanternPost"), ("stall", "MV_MarketStall"), ("cart", "MV_Cart")):
    root = new_prefab(pname, (0, -30 - 4 * len(PREFABS), 0))
    place(root, key, 0, 0, 0)


farm("B")


# ------------------------------------------------------------------ preview stage (not exported)


PV_TINT = (0.62, 0.66, 0.62, 1.0)


def img_plane(name, path, w_m, h_m, loc, rot_z=0.0, tilt=90.0, emit=0.0, coll=PREVIEW, uv_repeat=None):
    img = bpy.data.images.load(str(path), check_existing=True)
    m = bpy.data.materials.new("PV_" + name)
    m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes["Principled BSDF"]
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = img; t.interpolation = 'Closest'
    mul = nt.nodes.new("ShaderNodeMix"); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'
    mul.inputs["Factor"].default_value = 1.0
    mul.inputs[7].default_value = PV_TINT
    nt.links.new(t.outputs["Color"], mul.inputs[6])
    nt.links.new(mul.outputs[2], bsdf.inputs["Base Color"])
    nt.links.new(t.outputs["Alpha"], bsdf.inputs["Alpha"])
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    try:
        m.surface_render_method = 'DITHERED'
    except Exception:
        pass
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new("UVMap")
    vs = [bm.verts.new(v) for v in ((-w_m / 2, 0, 0), (w_m / 2, 0, 0), (w_m / 2, 0, h_m), (-w_m / 2, 0, h_m))]
    f = bm.faces.new(vs)
    ru, rv = uv_repeat or (1, 1)
    for l, uv in zip(f.loops, ((0, 0), (ru, 0), (ru, rv), (0, rv))):
        l[uvl].uv = uv
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); me.materials.append(m)
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob)
    ob.location = loc
    ob.rotation_euler = (math.radians(tilt - 90), 0, math.radians(rot_z))
    return ob


def ground(name, path, size, loc, tile_m):
    ob = img_plane(name, path, size[0], size[1], loc, tilt=0.0, uv_repeat=(size[0] / tile_m, size[1] / tile_m))
    ob.rotation_euler = (0, 0, 0)
    # img_plane builds in XZ; turn it flat
    me = ob.data
    for v in me.vertices:
        v.co = Vector((v.co.x, v.co.z - size[1] / 2, 0))
    return ob


ground("PV_Grass", SG / "Textures/Grass.png", (120, 110), (3, 10, 0), 10.0)
pathob = ground("PV_Dirt", SG / "Textures/Dirt.png", (3.2, 30), (3.0, -14.5, 0.01), 6.0)

CAM_YAW = 0.0  # camera looks along +Y; billboards face it


def sprite(fname, x, y, z=0.0, scale=1.0):
    p = SG / "Foliage" / fname
    img = bpy.data.images.load(str(p), check_existing=True)
    w, h = img.size[0] / PPM * scale, img.size[1] / PPM * scale
    ob = img_plane("PV_" + fname[:-4] + "_%d_%d" % (int(x * 10), int(y * 10)), p, w, h, (x, y, z))
    ob.rotation_euler = (math.radians(-12), 0, 0)  # lean back toward the camera pitch a little
    return ob


# forest wall behind + framing
for i, (f, x, y) in enumerate([("OakLargeA.png", -6.5, 8), ("Oak1.png", -1.0, 11), ("PineTall.png", 3.5, 10.5),
                               ("Oak1.png", 9.0, 10), ("OakLargeB.png", 13, 6.5), ("Pine1.png", 6.8, 13),
                               ("OakMediumA.png", -9.5, 3), ("PineA.png", 11.5, 2.5), ("Pine1.png", -4.0, 13.5)]):
    sprite(f, x, y)
for f, x, y in [("RoundBush.png", -1.4, 1.2), ("BushA.png", 6.9, -0.6), ("BushB.png", -0.9, -1.0),
                ("Fern.png", 6.6, 3.8), ("FernWide.png", -0.7, 3.2), ("FlowersA.png", 1.6, -1.1),
                ("WildflowerPatch.png", 4.6, -1.3), ("GrassA.png", 5.2, -0.9), ("GrassA.png", 0.6, -1.4),
                ("MossBush.png", -3.5, -1.8), ("Fern.png", 8.2, 0.5), ("FlowersA.png", -2.2, -2.2),
                ("GrassA.png", 2.2, -3.2), ("WildflowerPatch.png", 8.6, -3.0)]:
    sprite(f, x, y)
# foreground framing leaves (out of focus band nearest camera)
sprite("OakLargeB.png", -8.5, -9.0, -1.5, 1.0)
sprite("RoundBush.png", 10.5, -8.0, -0.2, 1.6)

# NPC placeholders (simple 1 x 1.75 m cards)
# (NPC cards come with the Phase 3 street; the shrine maiden sheet is an atlas, not a single frame)

farm("C")

# ------------------------------------------------------------------ lighting (dusk, matches Shrine Glade)
world = bpy.data.worlds.new("MV_World"); scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
bg.inputs["Color"].default_value = (0.030, 0.052, 0.062, 1)   # cool green-blue forest ambient
bg.inputs["Strength"].default_value = 0.75

sun_d = bpy.data.lights.new("PV_Sun", 'SUN')
sun_d.color = (1.0, 0.66, 0.42); sun_d.energy = 1.5; sun_d.angle = math.radians(2)
sun = bpy.data.objects.new("PV_Sun", sun_d); PREVIEW.objects.link(sun)
sun.rotation_euler = (math.radians(60), 0, math.radians(-42))  # from upper left of camera

for name, loc, e in [("PV_WinLight_Front", (1.0, -0.9, F + 1.6), 60), ("PV_WinLight_Side", (W6 + 0.9, 3.0, F + 1.6), 50),
                     ("PV_DoorLight", (3.0, -1.4, F + 2.4), 70)]:
    ld = bpy.data.lights.new(name, 'POINT'); ld.color = (1.0, 0.64, 0.23); ld.energy = e
    ld.shadow_soft_size = 0.3
    lo = bpy.data.objects.new(name, ld); PREVIEW.objects.link(lo); lo.location = loc

# ------------------------------------------------------------------ camera (Shrine Glade-like framing)
cam_d = bpy.data.cameras.new("PV_GameCam"); cam_d.lens = 50; cam_d.sensor_width = 36
cam = bpy.data.objects.new("PV_GameCam", cam_d); PREVIEW.objects.link(cam)
pitch = 28.0
dist = 29.0
target = Vector((3.0, 1.0, 2.6))
dirv = Vector((0, math.cos(math.radians(pitch)), -math.sin(math.radians(pitch))))
cam.location = target - dirv * dist
cam.rotation_euler = (math.radians(90 - pitch), 0, 0)
scene.camera = cam

cam2_d = bpy.data.cameras.new("PV_ThreeQuarterCam"); cam2_d.lens = 50
cam2 = bpy.data.objects.new("PV_ThreeQuarterCam", cam2_d); PREVIEW.objects.link(cam2)
t2 = Vector((3.0, 2.0, 3.0))
d2 = Vector((-0.55, 0.75, -0.38)).normalized()
cam2.location = t2 - d2 * 22
cam2.rotation_euler = d2.to_track_quat('-Z', 'Y').to_euler()

cam3_d = bpy.data.cameras.new("PV_PrefabsCam"); cam3_d.lens = 40
cam3 = bpy.data.objects.new("PV_PrefabsCam", cam3_d); PREVIEW.objects.link(cam3)
t3 = Vector((10.0, 8.0, 2.0))
d3 = Vector((-0.25, 0.8, -0.55)).normalized()
cam3.location = t3 - d3 * 42
cam3.rotation_euler = d3.to_track_quat('-Z', 'Y').to_euler()

# ------------------------------------------------------------------ render settings
r = scene.render
r.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE_NEXT'
r.resolution_x, r.resolution_y, r.resolution_percentage = 1280, 720, 100
r.film_transparent = False
try:
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.look = 'AgX - Medium High Contrast'
except Exception:
    pass
for k, v in (("use_shadows", True), ("taa_render_samples", 32), ("use_raytracing", False)):
    try:
        setattr(scene.eevee, k, v)
    except Exception:
        pass

try:
    ng = bpy.data.node_groups.new("MV_Comp", "CompositorNodeTree")
    scene.compositing_node_group = ng
    rl = ng.nodes.new("CompositorNodeRLayers")
    gl = ng.nodes.new("CompositorNodeGlare")
    out = ng.nodes.new("NodeGroupOutput")
    ng.interface.new_socket("Image", in_out='OUTPUT', socket_type='NodeSocketColor')
    try:
        gl.glare_type = 'BLOOM'
    except Exception:
        pass
    for k, v in (("Threshold", 1.2), ("Strength", 0.35), ("Size", 0.5)):
        if k in gl.inputs:
            gl.inputs[k].default_value = v
    ng.links.new(rl.outputs["Image"], gl.inputs["Image"])
    ng.links.new(gl.outputs[0], out.inputs[0])
    BLOOM = "ok"
except Exception as e:
    BLOOM = "failed: %s" % e
scene.view_settings.exposure = -0.3


# ------------------------------------------------------------------ export the prefabs (glb, one per prefab)
# Collision proxies are left out: Hydro's importer flattens every node into one render mesh; colliders are
# scene components. Each prefab's root is moved to the origin for its export.
OUT = ROOT / "Models"
EXPORTED = []
# Re-exported on every run (the rest keep their glbs unless MV_REEXPORT is set): the buildings, after the Oct 29, 2026
# material pass (timber frame and plaster or planks for walls, stone only for foundations and chimneys).
ROOF_FIX = ("MV_House_A", "MV_House_B", "MV_Shop_A", "MV_Barn_A", "MV_Cottage_Thatch_A", "MV_Well")
for pname, root in PREFABS.items():
    if pname not in NEW_PREFABS and pname not in ROOF_FIX and not os.environ.get("MV_REEXPORT"):
        continue  # existing glbs stay as they are
    bpy.ops.object.select_all(action='DESELECT')
    for ob in [root] + list(root.children_recursive):
        if ob.name.startswith("COL_") or ob.display_type == 'WIRE':
            continue
        ob.select_set(True)
    saved = root.location.copy(); root.location = (0, 0, 0)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.gltf(filepath=str(OUT / (pname + ".glb")), export_format='GLB', use_selection=True,
                              export_yup=True, export_apply=False, export_materials='EXPORT',
                              export_image_format='AUTO', export_cameras=False, export_lights=False)
    root.location = saved
    EXPORTED.append(pname)

# ------------------------------------------------------------------ save
SRC.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(SRC / "MV_Kit.blend"), relative_remap=True)

info = {
    "modules": [o.name for o in LIB],
    "exported": EXPORTED,
    "prefabs": list(PREFABS),
    "gable_rise": round(GABLE_RISE, 2),
    "bloom": BLOOM,
    "tris": sum(len(o.data.polygons) for o in LIB if o.type == 'MESH'),
}
print("RESULT::" + repr(info))
