"""Komainu, high-poly (Nov 2, after the user's two reference statues): a seated lion-dog sculpted from overlapping
primitives (a round chest and haunches, straight front legs with heavy toed paws, a big head with a broad flat nose,
bulging brows and eyes, a grinning jaw, small ears, rows of round curls for the mane and down the back, a collar with a
bell, a tail of curls), fused into one surface by a voxel remesh, smoothed, and decimated to a few thousand triangles.
Exec'd by build_jp_kit.py; also runs on its own in Blender for a quick look (test_komainu.py)."""

import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector


def _ellipsoid(bm, c, r, rot=None, segs=16, rings=10):
    M = Matrix.Translation(c) @ (rot or Matrix.Identity(4)) @ Matrix.Diagonal((r[0], r[1], r[2], 1.0))
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0, matrix=M)


def komainu_sculpt(open_mouth=True, voxel=0.014, target_tris=4200, seed=3):
    """The statue (without its pedestal) as a list of triangles in metres: it sits on z = 0, faces -Y."""
    rnd = random.Random(seed)
    bm = bmesh.new()
    E = lambda c, r, **k: _ellipsoid(bm, c, r, **k)
    S = lambda c, r, **k: _ellipsoid(bm, c, (r, r, r), segs=10, rings=7, **k)
    rx = lambda deg: Matrix.Rotation(math.radians(deg), 4, 'X')
    # body
    E((0.0, 0.05, 0.4), (0.2, 0.24, 0.3), rot=rx(-14))                     # torso
    S((0.0, -0.09, 0.5), 0.22)                                             # chest
    E((0.0, -0.13, 0.38), (0.15, 0.1, 0.17))                               # belly front
    for sx in (-1, 1):
        E((sx * 0.15, 0.12, 0.2), (0.12, 0.2, 0.17))                       # haunch
        E((sx * 0.19, -0.05, 0.05), (0.07, 0.12, 0.05))                    # hind foot
        E((sx * 0.115, -0.2, 0.26), (0.078, 0.08, 0.23))                   # front leg, thick
        E((sx * 0.115, -0.26, 0.05), (0.085, 0.11, 0.05))                  # front paw
        for k in range(3):
            S((sx * 0.11 + (k - 1) * 0.04, -0.34, 0.03), 0.03)             # toes
    # neck and head
    E((0.0, -0.1, 0.64), (0.16, 0.16, 0.15))
    hz, hy = 0.84, -0.17
    E((0.0, hy, hz), (0.2, 0.18, 0.18))                                    # skull
    E((0.0, hy - 0.17, hz - 0.04), (0.13, 0.1, 0.09))                      # muzzle
    E((0.0, hy - 0.28, hz - 0.01), (0.085, 0.05, 0.055))                   # nose: broad and flat
    for sx in (-1, 1):
        S((sx * 0.075, hy - 0.14, hz + 0.065), 0.05)                       # brows
        S((sx * 0.078, hy - 0.17, hz + 0.03), 0.032)                       # eyes
        S((sx * 0.115, hy - 0.13, hz - 0.06), 0.055)                       # cheeks
        E((sx * 0.175, hy + 0.02, hz + 0.12), (0.035, 0.06, 0.07))         # ears
    jaw_z = hz - 0.15 if open_mouth else hz - 0.11
    E((0.0, hy - 0.13, jaw_z), (0.1, 0.07, 0.04))                          # lower jaw
    # mane: rings of round curls over the back of the head and the shoulders, curls on the crown
    for (r, z, n) in ((0.17, hz + 0.15, 7), (0.22, hz + 0.04, 9), (0.23, hz - 0.09, 9), (0.22, hz - 0.22, 9), (0.2, hz - 0.33, 7)):
        for k in range(n):
            a = math.radians(-118 + 236 * k / (n - 1))
            S((r * math.sin(a), hy + 0.05 + r * math.cos(a), z + rnd.uniform(-0.012, 0.012)), 0.052)
    for (x, y) in ((-0.07, -0.05), (0.07, -0.05), (0.0, 0.03), (-0.1, 0.06), (0.1, 0.06)):
        S((x, hy + y, hz + 0.19), 0.05)
    for k in range(5):                                                     # curls down the back
        for sx in (-1, 1):
            S((sx * 0.06, 0.14 + 0.035 * k, 0.6 - 0.09 * k), 0.05)
    for k in range(4):                                                     # tail of curls rising at the back
        S(((k % 2 - 0.5) * 0.07, 0.33 + 0.015 * k, 0.2 + 0.09 * k), 0.07 - 0.006 * k)
    # collar and bell
    for k in range(11):
        a = math.radians(-85 + 170 * k / 10)
        S((0.165 * math.sin(a), -0.1 - 0.13 * math.cos(a), 0.6 - 0.05 * math.cos(a)), 0.024)
    S((0.0, -0.25, 0.53), 0.045)

    me = bpy.data.meshes.new("KomainuSrc")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("KomainuSrc", me)
    bpy.context.scene.collection.objects.link(ob)
    rem = ob.modifiers.new("remesh", 'REMESH')
    rem.mode = 'VOXEL'
    rem.voxel_size = voxel
    rem.adaptivity = 0.0
    sm = ob.modifiers.new("smooth", 'SMOOTH')
    sm.factor = 0.6
    sm.iterations = 4
    dg = bpy.context.evaluated_depsgraph_get()
    dense = ob.evaluated_get(dg).to_mesh().copy()
    tmp = bpy.data.objects.new("KomainuDense", dense)
    bpy.context.scene.collection.objects.link(tmp)
    n = max(1, len(dense.polygons) * 2)
    dec = tmp.modifiers.new("dec", 'DECIMATE')
    dec.ratio = min(1.0, target_tris / float(n))
    dg = bpy.context.evaluated_depsgraph_get()
    low = tmp.evaluated_get(dg).to_mesh()
    low.calc_loop_triangles()
    tris = [[tuple(low.vertices[i].co) for i in t.vertices] for t in low.loop_triangles]
    tmp.evaluated_get(dg).to_mesh_clear()
    for o in (tmp, ob):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in (me, dense):
        bpy.data.meshes.remove(m)
    zmin = min(p[2] for t in tris for p in t)
    return [[(p[0], p[1], p[2] - zmin) for p in t] for t in tris]
