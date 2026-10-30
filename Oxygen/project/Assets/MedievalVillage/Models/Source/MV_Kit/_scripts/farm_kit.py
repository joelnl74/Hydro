# EONLORE Medieval Village - V9 Farming Village add-on. Executed by build_kit.py in three phases (A modules, B prefabs, C stage).
# Fitted to Scenes/FarmVillage: stream channel 2.4 m wide, water at z -0.45, bed at z -1.0, ground at z 0.
# Every new piece is tagged hydro_status = PLACEHOLDER until it has been checked in the engine camera (V9 approval gate).
import random

CH_W = 2.4        # stream channel width (water surface)
BED_Z = -1.0
WATER_Z = -0.45
DT = 0.55         # bridge deck top (humpback so the arch reads over a 2.4 m channel)

if FARM_PHASE == 'A':
    def tag(ob, status="PLACEHOLDER"):
        ob["hydro_status"] = status
        return ob

    def col_prism(parent, pts, y0, y1, suffix, kind, M=None):
        """Collision proxy from a convex XZ polygon extruded along Y (use M to turn it)."""
        M = M or Matrix.Identity(4)
        bm = bmesh.new()
        front = [bm.verts.new(M @ Vector((x, y0, z))) for x, z in pts]
        back = [bm.verts.new(M @ Vector((x, y1, z))) for x, z in pts]
        n = len(pts)
        bm.faces.new(front); bm.faces.new(list(reversed(back)))
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new([front[i], front[j], back[j], back[i]])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        name = "COL_" + parent.name + suffix
        me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
        ob = bpy.data.objects.new(name, me)
        COL["10_Collision"].objects.link(ob)
        ob.parent = parent; ob.display_type = 'WIRE'; ob.hide_render = True; ob["hydro_kind"] = kind
        return ob

    def cbox(parent, x0, y0, z0, x1, y1, z1, suffix, kind="COL"):
        c = col_box(parent, x0, y0, z0, x1, y1, z1, suffix)
        c["hydro_kind"] = kind
        return c

    def add_poly(mb, cos, mat, key):
        f = mb.bm.faces.new([mb.bm.verts.new(Vector(c)) for c in cos])
        f.material_index = mb.mi(mat)
        for loop, c in zip(f.loops, cos):
            loop[mb.uv].uv = mb._uv_for(c, key, mat)
        return f

    def slab_outline(mb, outline, z0, z1, topmat, sidemat):
        """Closed slab from a counter-clockwise (seen from above) XY outline."""
        add_poly(mb, [(x, y, z1) for x, y in outline], topmat, '+z')
        add_poly(mb, [(x, y, z0) for x, y in reversed(outline)], sidemat, '-z')
        n = len(outline)
        for i in range(n):
            (ax, ay), (bx, by) = outline[i], outline[(i + 1) % n]
            key = '-y' if abs(bx - ax) >= abs(by - ay) else '+x'
            add_poly(mb, [(ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1)], sidemat, key)

    def area(pts):
        return 0.5 * sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))

    # ---- stream -----------------------------------------------------------------
    def bridge():
        """Stone arch bridge over the 2.4 m channel. Deck runs along X (4.4 m + 1.6 m ramps each end), 2.6 m wide, origin at the
        channel's centre on the deck's centreline at ground level. Stream flows along Y underneath."""
        mb = MeshBuilder()
        a, b, zs = CH_W / 2, -BED_Z, BED_Z
        HL, HW = 2.2, 1.3
        step = 0.2
        n = int(round(2 * HL / step))
        for i in range(n):
            x0 = -HL + i * step; x1 = x0 + step; xm = (x0 + x1) / 2
            zb = zs if abs(xm) >= a else zs + b * math.sqrt(1 - (xm / a) ** 2)
            mb.box(x0, -HW, zb, x1, HW, DT, "MAT_Stone_Field", facemat={'+z': "MAT_Stone_Cut"})
        ring, nseg = 0.28, 10
        for i in range(nseg):
            t0 = math.pi * i / nseg; t1 = math.pi * (i + 1) / nseg
            e = lambda t, ra, rb: (ra * math.cos(t), zs + rb * math.sin(t))
            pts = [e(t0, a, b), e(t1, a, b), e(t1, a + ring, b + ring), e(t0, a + ring, b + ring)]
            if area(pts) < 0:
                pts.reverse()
            m = "MAT_Stone_Cut" if i % 2 == 0 else "MAT_Stone_Chimney"
            mb.prism_xz(pts, -HW - 0.07, -HW, m)
            mb.prism_xz(pts, HW, HW + 0.07, m)
        for y0 in (-HW, HW - 0.25):
            mb.box(-HL, y0, DT, HL, y0 + 0.25, DT + 0.5, "MAT_Stone_Field", facemat={'+z': "MAT_Stone_Cut"})
            mb.box(-HL - 0.05, y0 - 0.04, DT + 0.5, HL + 0.05, y0 + 0.29, DT + 0.6, "MAT_Stone_Cut")
            for x0 in (-HL - 0.05, HL - 0.4):
                mb.box(x0, y0 - 0.07, DT, x0 + 0.45, y0 + 0.32, DT + 0.8, "MAT_Stone_Cut")
        ramp = 1.6
        mb.prism_xz([(-HL - ramp, 0), (-HL, 0), (-HL, DT)], -HW, HW, "MAT_Stone_Cut", capmat="MAT_Stone_Field")
        mb.prism_xz([(HL, 0), (HL + ramp, 0), (HL, DT)], -HW, HW, "MAT_Stone_Cut", capmat="MAT_Stone_Field")
        ob = mb.to_object("MV_Bridge_Stone_A", COL["11_Terrain_Route"])
        cbox(ob, -HL, -HW + 0.25, BED_Z, HL, HW - 0.25, DT, "_WALK_Deck", "WALK")
        col_prism(ob, [(-HL - ramp, 0), (-HL, 0), (-HL, DT)], -HW + 0.25, HW - 0.25, "_WALK_RampW", "WALK")
        col_prism(ob, [(HL, 0), (HL + ramp, 0), (HL, DT)], -HW + 0.25, HW - 0.25, "_WALK_RampE", "WALK")
        cbox(ob, -HL - 0.05, -HW - 0.07, DT, HL + 0.05, -HW + 0.25, DT + 0.8, "_COL_RailS")
        cbox(ob, -HL - 0.05, HW - 0.25, DT, HL + 0.05, HW + 0.07, DT + 0.8, "_COL_RailN")
        cbox(ob, -HL, -HW, BED_Z, -a, HW, DT, "_COL_AbutW")
        cbox(ob, a, -HW, BED_Z, HL, HW, DT, "_COL_AbutE")
        return tag(ob)

    def bank_stone():
        """4 m dressed-stone stream bank, face towards -Y, from the bed (z -1.0) to the ground; irregular cap stones."""
        r = random.Random(11)
        mb = MeshBuilder()
        mb.box(0, 0, BED_Z, 4, 0.5, 0.0, "MAT_Stone_Field")
        x = 0.0
        while x < 4 - 1e-6:
            w = min(r.uniform(0.7, 1.3), 4 - x)
            mb.box(x, -0.07, 0.0, x + w, 0.55, r.uniform(0.05, 0.14), "MAT_Stone_Cut")
            x += w
        for _ in range(4):
            sx = r.uniform(0.1, 3.0)
            mb.box(sx, -0.05, r.uniform(-0.85, -0.35), sx + r.uniform(0.5, 0.9), 0.0, r.uniform(-0.3, -0.05), "MAT_Stone_Field")
        ob = mb.to_object("MV_Bank_Stone_4m_A", COL["11_Terrain_Route"])
        cbox(ob, 0, -0.05, BED_Z, 4, 0.55, 0.15, "_COL")
        return tag(ob)

    def bank_earth():
        """4 m earth bank: slopes from the waterline up 1.8 m to the ground. The terrain must be cut 1.8 m back from the water edge."""
        pts = [(0, BED_Z), (1.8, BED_Z), (1.8, 0.0), (0, -0.4)]
        mb = MeshBuilder()
        mb.prism_xz(pts, -4, 0, "MAT_Dirt_Road", M=R('Z', 90))
        ob = mb.to_object("MV_Bank_Earth_4m_A", COL["11_Terrain_Route"])
        col_prism(ob, pts, -4, 0, "_COL", "COL", M=R('Z', 90))
        return tag(ob)

    def stream_bed():
        mb = MeshBuilder()
        mb.box(-CH_W / 2 - 0.1, 0, BED_Z - 0.25, CH_W / 2 + 0.1, 4, BED_Z, "MAT_Dirt_Road", facemat={'+z': "MAT_Stone_Field"})
        return tag(mb.to_object("MV_StreamBed_4m_A", COL["11_Terrain_Route"]))

    def water_plane():
        """PLACEHOLDER water surface: replace with the engine's stylised water. Origin on the surface, centred across the channel."""
        mb = MeshBuilder()
        mb.box(-CH_W / 2, 0, 0, CH_W / 2, 4, 0.02, "MAT_Water_Stream")
        return tag(mb.to_object("MV_Water_Plane_4m_A", COL["11_Terrain_Route"]))

    def retaining_wall():
        """4 m field-terrace retaining wall, 1.0 m high, face towards -Y, irregular cap stones."""
        r = random.Random(23)
        mb = MeshBuilder()
        mb.box(-0.08, -0.1, 0, 4.08, 0.45, 0.25, "MAT_Stone_Field", facemat={'+z': "MAT_Stone_Cut"})
        mb.box(0, 0, 0.25, 4, 0.45, 1.0, "MAT_Stone_Field")
        x = 0.0
        while x < 4 - 1e-6:
            w = min(r.uniform(0.7, 1.3), 4 - x)
            mb.box(x, -0.06, 1.0, x + w, 0.51, 1.0 + r.uniform(0.04, 0.13), "MAT_Stone_Cut")
            x += w
        for _ in range(3):
            sx = r.uniform(0.2, 3.0)
            mb.box(sx, -0.05, r.uniform(0.3, 0.6), sx + r.uniform(0.5, 0.8), 0.0, r.uniform(0.65, 0.9), "MAT_Stone_Cut")
        ob = mb.to_object("MV_Retaining_Stone_4m_A", COL["11_Terrain_Route"])
        cbox(ob, -0.08, -0.1, 0, 4.08, 0.55, 1.15, "_COL")
        return tag(ob)

    def stairs_stone():
        """2 m wide stone stairs, 4 steps, 1.0 m rise over 1.4 m run, climbing towards +Y. Collision is a ramp."""
        mb = MeshBuilder()
        n, tread, riser = 4, 0.35, 0.25
        for i in range(n):
            mb.box(0, i * tread, 0, 2.0, n * tread, (i + 1) * riser, "MAT_Stone_Field", facemat={'+z': "MAT_Stone_Cut"})
        cheek = [(0, 0), (n * tread, 0), (n * tread, 1.1), (0, 0.3)]
        mb.prism_xz(cheek, 0, 0.18, "MAT_Stone_Field", M=R('Z', 90))
        mb.prism_xz(cheek, 0, 0.18, "MAT_Stone_Field", M=T(2.18, 0, 0) @ R('Z', 90))
        ob = mb.to_object("MV_Stairs_Stone_2m_A", COL["11_Terrain_Route"])
        col_prism(ob, [(0, 0), (n * tread, 0), (n * tread, n * riser)], -2.0, 0, "_WALK", "WALK", M=R('Z', 90))
        return tag(ob)

    # ---- roads (flat, irregular edges; end points are straight so the pieces join) ------------------------------
    def road_straight(seed, L=4.0, Wd=3.0, jit=0.12, step=0.5):
        r = random.Random(seed)
        n = int(L / step)
        lo = [(i * step, -Wd / 2 + (0 if i in (0, n) else r.uniform(-jit, jit))) for i in range(n + 1)]
        hi = [(i * step, Wd / 2 + (0 if i in (0, n) else r.uniform(-jit, jit))) for i in range(n, -1, -1)]
        return lo + hi

    def road_piece(name, outline, z0, z1, topmat, sidemat):
        mb = MeshBuilder()
        slab_outline(mb, outline, z0, z1, topmat, sidemat)
        return tag(mb.to_object(name, COL["11_Terrain_Route"]))

    def road_bend(R_=3.0, Wd=3.0, jit=0.1, seed=5, steps=10):
        """90 degree left bend: enters at the origin heading +X, leaves at (3, 3) heading +Y."""
        r = random.Random(seed)
        outer, inner = [], []
        for i in range(steps + 1):
            th = math.radians(90 * i / steps)
            px, py = R_ * math.sin(th), R_ - R_ * math.cos(th)
            nx, ny = -math.sin(th), math.cos(th)
            j1 = 0 if i in (0, steps) else r.uniform(-jit, jit)
            j2 = 0 if i in (0, steps) else r.uniform(-jit, jit)
            outer.append((px - nx * (Wd / 2 + j1), py - ny * (Wd / 2 + j1)))
            inner.append((px + nx * (Wd / 2 + j2), py + ny * (Wd / 2 + j2)))
        return outer + list(reversed(inner))

    # ---- thatch roof ---------------------------------------------------------------------------------------
    def roof_thatch(L):
        mb = MeshBuilder()
        roof_slab(mb, 0, L, "MAT_Roof_Thatch", eave=0.6, slab=0.30, fascia=False, ridgemat="MAT_Roof_Thatch", basemat="MAT_Roof_Thatch")
        return tag(mb.to_object("MV_Roof_GableThatch_%dm_A" % L, COL["06_Roofs"]))

    def roof_thatch_end():
        mb = MeshBuilder()
        roof_slab(mb, -0.4, 0, "MAT_Roof_Thatch", eave=0.6, slab=0.30, fascia=False, ridgemat="MAT_Roof_Thatch", basemat="MAT_Roof_Thatch")
        return tag(mb.to_object("MV_Roof_GableThatchEnd_A", COL["06_Roofs"]))

    # ---- field and farm props ------------------------------------------------------------------------------
    def fence_gate():
        mb = MeshBuilder(); m = "MAT_Wood_DarkBeam"
        for x in (0.05, 1.95):
            mb.box(x - 0.07, -0.07, 0, x + 0.07, 0.07, 1.15, m)
            mb.box(x - 0.05, -0.05, 1.15, x + 0.05, 0.05, 1.21, m)
        for zc in (0.32, 0.82):
            mb.box(0, -0.08, 0, 0.1, -0.02, 1.64, m, M=T(0.18, 0, zc + 0.05) @ R('Y', 90))
        for xi in (0.3, 0.55, 0.8, 1.05, 1.3, 1.55, 1.7):
            mb.box(xi - 0.035, -0.06, 0.15, xi + 0.035, -0.02, 1.0, m)
        dx, dz = 1.5, 0.5
        L = math.hypot(dx, dz); ang = math.degrees(math.atan2(dx, dz))
        mb.box(-0.045, -0.1, 0, 0.045, -0.06, L, m, M=T(0.22, 0, 0.32) @ R('Y', ang))
        ob = mb.to_object("MV_Fence_Gate_2m_A", COL["09_Props_3D"])
        cbox(ob, -0.02, -0.1, 0, 0.12, 0.1, 1.2, "_COL_L"); cbox(ob, 1.88, -0.1, 0, 2.02, 0.1, 1.2, "_COL_R")
        cbox(ob, 0.12, -0.1, 0, 1.88, 0.05, 1.0, "_COL_Gate", "GATE")
        return tag(ob)

    def field_plot():
        """4 x 4 m tilled plot with eight soil ridges along X (crop rows are sprites). Origin at the front-left corner."""
        mb = MeshBuilder()
        mb.box(0, 0, 0, 4, 4, 0.06, "MAT_Soil_Tilled")
        ridge = [(-0.2, 0), (0.2, 0), (0.08, 0.12), (-0.08, 0.12)]
        for i in range(8):
            mb.prism_xz(ridge, -4, 0, "MAT_Soil_Tilled", M=T(0, i * 0.5 + 0.25, 0.06) @ R('Z', 90))
        ob = mb.to_object("MV_Field_Plot_4x4_A", COL["11_Terrain_Route"])
        return tag(ob)

    def wheelbarrow():
        mb = MeshBuilder(); m = "MAT_Wood_DarkBeam"; pl = "MAT_Wood_Planks"
        mb.box(-0.28, -0.4, 0.36, 0.28, 0.35, 0.40, pl)
        mb.box(-0.34, -0.4, 0.36, -0.28, 0.4, 0.62, pl); mb.box(0.28, -0.4, 0.36, 0.34, 0.4, 0.62, pl)
        mb.box(-0.34, -0.46, 0.36, 0.34, -0.4, 0.56, pl); mb.box(-0.34, 0.35, 0.36, 0.34, 0.41, 0.66, pl)
        for x in (-0.24, 0.24):
            mb.box(x - 0.025, 0.28, 0, x + 0.025, 0.34, 0.38, m)
        for x in (-0.22, 0.22):
            mb.box(x - 0.025, -0.025, -0.025, x + 0.025, 0.85, 0.025, m, M=T(0, 0.3, 0.42) @ R('X', 12))
        n = 12; r_ = 0.26
        pts = [(math.cos(2 * math.pi * i / n) * r_, math.sin(2 * math.pi * i / n) * r_) for i in range(n)]
        mb.prism_xz(pts, -0.03, 0.03, "MAT_Wood_Door", M=T(0, -0.62, r_) @ R('Z', 90), capmat="MAT_Metal_IronDark")
        mb.box(-0.3, -0.64, r_ - 0.02, 0.3, -0.6, r_ + 0.02, m)
        ob = mb.to_object("MV_Wheelbarrow_A", COL["09_Props_3D"])
        cbox(ob, -0.4, -0.9, 0, 0.4, 1.15, 0.7, "_COL")
        return tag(ob)

    def hay_bale():
        mb = MeshBuilder()
        mb.box(-0.45, -0.22, 0, 0.45, 0.22, 0.45, "MAT_Roof_Thatch")
        for x in (-0.2, 0.2):
            mb.box(x - 0.02, -0.235, -0.005, x + 0.02, 0.235, 0.455, "MAT_Wood_DarkBeam")
        ob = mb.to_object("MV_HayBale_A", COL["09_Props_3D"])
        cbox(ob, -0.45, -0.22, 0, 0.45, 0.22, 0.45, "_COL")
        return tag(ob)

    def barrel():
        mb = MeshBuilder()
        def octa(r_):
            return [(r_ * math.cos(math.radians(45 * i + 22.5)), r_ * math.sin(math.radians(45 * i + 22.5))) for i in range(8)]
        Mx = R('X', 90)
        for y0, y1, r_ in ((0.0, 0.3, 0.29), (0.3, 0.6, 0.33), (0.6, 0.9, 0.29)):
            mb.prism_xz(octa(r_), y0, y1, "MAT_Wood_DarkBeam", M=Mx, capmat="MAT_Wood_Planks")
        for y0, y1, r_ in ((0.1, 0.15, 0.30), (0.42, 0.47, 0.345), (0.75, 0.8, 0.30)):
            mb.prism_xz(octa(r_), y0, y1, "MAT_Metal_IronDark", M=Mx, capmat="MAT_Metal_IronDark")
        ob = mb.to_object("MV_Barrel_A", COL["09_Props_3D"])
        cbox(ob, -0.34, -0.34, 0, 0.34, 0.34, 0.9, "_COL")
        return tag(ob)

    def crate():
        mb = MeshBuilder()
        mb.box(-0.35, -0.35, 0, 0.35, 0.35, 0.6, "MAT_Wood_Planks")
        for sx in (-1, 1):
            for sy in (-1, 1):
                mb.box(sx * 0.35 - 0.04 - 0.01 * sx + (0.0), sy * 0.35 - 0.04 - 0.01 * sy, 0, sx * 0.35 + 0.04 + 0.01 * sx, sy * 0.35 + 0.04 + 0.01 * sy, 0.6, "MAT_Wood_DarkBeam")
        mb.box(-0.37, -0.37, 0.56, 0.37, 0.37, 0.62, "MAT_Wood_DarkBeam")
        ob = mb.to_object("MV_Crate_A", COL["09_Props_3D"])
        cbox(ob, -0.37, -0.37, 0, 0.37, 0.37, 0.62, "_COL")
        return tag(ob)

    FARM_ROWS = [
        [("bridge", bridge()), ("bankS", bank_stone()), ("bankD", bank_earth()), ("bed", stream_bed()), ("water", water_plane())],
        [("ret", retaining_wall()), ("stairs", stairs_stone()),
         ("roadA", road_piece("MV_Road_Dirt_4m_Straight_A", road_straight(3), -0.05, 0.03, "MAT_Dirt_Road", "MAT_Dirt_Road")),
         ("roadB", road_piece("MV_Road_Dirt_4m_Straight_B", road_straight(8), -0.05, 0.03, "MAT_Dirt_Road", "MAT_Dirt_Road")),
         ("bend", road_piece("MV_Road_Dirt_Bend_A", road_bend(), -0.05, 0.03, "MAT_Dirt_Road", "MAT_Dirt_Road")),
         ("cobble", road_piece("MV_Road_Cobble_2m_A", road_straight(5, L=2.0, Wd=2.6, jit=0.05), -0.05, 0.07, "MAT_Cobble_Road", "MAT_Stone_Field"))],
        [("roofT2", roof_thatch(2)), ("roofTEnd", roof_thatch_end()),
         ("leanS", roof_leanto(roofmat="MAT_Roof_SlateDark", tag="Slate")), ("leanT", roof_leanto(roofmat="MAT_Roof_Thatch", tag="Thatch")),
         ("gate", fence_gate()), ("plot", field_plot())],
        [("barrow", wheelbarrow()), ("hay", hay_bale()), ("barrel", barrel()), ("crate", crate())],
    ]

elif FARM_PHASE == 'B':
    NEW_PREFABS = []

    def newp(name, loc):
        root = new_prefab(name, loc); root["hydro_status"] = "PLACEHOLDER"; NEW_PREFABS.append(name)
        return root

    # HD-2D pass: a stone cottage under thatch (one material all round: ashlar with quoins, stone gables).
    house("MV_Cottage_Thatch_A", 6, 1, front=["SSS"], right=["SS"], left=["SS"], back=["SSS"], roof="thatch", chimney="right", step_x=3.0,
          opens=[("win", "front", 1.0, 0, 1.25), ("door", "front", 3.0, 0, 0.0), ("winS", "right", 3.0, 0, 1.2), ("fbox", "front", 1.0, 0, 1.0)],
          loc=(28.0, 0, 0))
    PREFABS["MV_Cottage_Thatch_A"]["hydro_status"] = "PLACEHOLDER"; NEW_PREFABS.append("MV_Cottage_Thatch_A")

    for key, pname in (("bridge", "MV_Bridge"), ("bankS", "MV_Bank_Stone"), ("bankD", "MV_Bank_Earth"), ("ret", "MV_Retaining_Stone"),
                       ("stairs", "MV_Stairs_Stone"), ("roadA", "MV_Road_Dirt_Straight_A"), ("roadB", "MV_Road_Dirt_Straight_B"),
                       ("bend", "MV_Road_Dirt_Bend"), ("cobble", "MV_Road_Cobble"), ("gate", "MV_Fence_Gate"), ("plot", "MV_Field_Plot"),
                       ("barrow", "MV_Wheelbarrow"), ("hay", "MV_HayBale"), ("barrel", "MV_Barrel"), ("crate", "MV_Crate")):
        root = newp(pname, (0, -30 - 4 * len(PREFABS), 0))
        place(root, key, 0, 0, 0)

    def stream(name, bank, loc):
        root = newp(name, loc)
        place(root, "bed", 0, 0, 0)
        place(root, "water", 0, 0, WATER_Z)
        place(root, bank, -CH_W / 2, 0, 0, 90)
        place(root, bank, CH_W / 2, 4, 0, -90)

    stream("MV_Stream_Stone", "bankS", (-12.0, -60.0, 0))
    stream("MV_Stream_Earth", "bankD", (-12.0, -70.0, 0))

    root = newp("MV_Field_A", (-24.0, -60.0, 0))
    place(root, "plot", -2, 0, 0)
    place(root, "fence", -2, -0.2, 0); place(root, "gate", 0, -0.2, 0)
    for sx in (-2.2, 2.2):
        for y in (0, 2):
            place(root, "fence", sx, y, 0, 90)

elif FARM_PHASE == 'C':
    X0 = 100.0

    def stage(pname, x, y, z=0.0, rot=0.0):
        src = PREFABS[pname]
        root = bpy.data.objects.new("PVI_" + pname, None); PREVIEW.objects.link(root)
        root.location = (x, y, z); root.rotation_euler = (0, 0, math.radians(rot))
        for ch in src.children:
            o = bpy.data.objects.new("PVI_" + ch.name, ch.data); PREVIEW.objects.link(o)
            o.parent = root
            o.location = ch.location; o.rotation_euler = ch.rotation_euler; o.scale = ch.scale
        return root

    ground("PV_FarmGrassW", SG / "Textures/Grass.png", (34.8, 100), ((64 + X0 - CH_W / 2) / 2, 5, 0), 10.0)
    ground("PV_FarmGrassE", SG / "Textures/Grass.png", (38.8, 100), ((X0 + CH_W / 2 + 140) / 2, 5, 0), 10.0)
    for y0 in range(-40, 48, 4):
        stage("MV_Stream_Stone", X0, y0)
    stage("MV_Bridge", X0, 2.0)
    # road: south lane -> bend -> west approach -> bridge -> east approach
    stage("MV_Road_Cobble", X0 - 3.8 - 2.0, 2.0); stage("MV_Road_Cobble", X0 + 3.8, 2.0)
    stage("MV_Road_Dirt_Straight_A", X0 - 9.8, 2.0); stage("MV_Road_Dirt_Straight_B", X0 + 5.8, 2.0)
    stage("MV_Road_Dirt_Straight_A", X0 + 9.8, 2.0)
    stage("MV_Road_Dirt_Bend", X0 - 9.8, 2.0, 0, 180)
    stage("MV_Road_Dirt_Straight_B", X0 - 12.8, -5.0, 0, 90); stage("MV_Road_Dirt_Straight_A", X0 - 12.8, -9.0, 0, 90)
    # buildings (one roof material per house)
    stage("MV_Cottage_Thatch_A", X0 - 8.0, 9.0)
    stage("MV_House_B", X0 - 21.0, 8.0)
    stage("MV_Barn_A", X0 + 14.0, 12.0)
    stage("MV_Shop_A", X0 - 28.0, 6.0)
    # terrace behind the cottage
    stage("MV_Retaining_Stone", X0 - 13.0, 15.0); stage("MV_Retaining_Stone", X0 - 7.0, 15.0); stage("MV_Stairs_Stone", X0 - 9.0, 15.0)
    ground("PV_FarmTerrace", SG / "Textures/Grass.png", (14, 8), (X0 - 8.0, 19.5, 1.002), 10.0)
    # fields, props
    stage("MV_Field_A", X0 + 7.0, 6.0); stage("MV_Field_A", X0 + 13.0, 5.0)
    stage("MV_Well", X0 - 6.0, 6.0)
    stage("MV_Wheelbarrow", X0 + 4.4, 7.5, 0, 20); stage("MV_HayBale", X0 + 19.0, 13.0); stage("MV_HayBale", X0 + 19.0, 14.2)
    stage("MV_Barrel", X0 - 10.6, 8.3); stage("MV_Crate", X0 - 9.6, 8.2); stage("MV_LanternPost", X0 - 4.4, 0.0)
    ll = bpy.data.lights.new("PV_FarmLantern", 'POINT'); ll.color = (1.0, 0.64, 0.23); ll.energy = 90; ll.shadow_soft_size = 0.3
    lo = bpy.data.objects.new("PV_FarmLantern", ll); PREVIEW.objects.link(lo); lo.location = (X0 - 4.4 + 0.62, 0, 2.05)
    for nm, loc, e in (("PV_FarmWin", (X0 - 10.0, 8.1, 2.2), 60), ("PV_FarmDoor", (X0 - 8.0, 7.6, 3.0), 70)):
        ld = bpy.data.lights.new(nm, 'POINT'); ld.color = (1.0, 0.64, 0.23); ld.energy = e; ld.shadow_soft_size = 0.3
        o = bpy.data.objects.new(nm, ld); PREVIEW.objects.link(o); o.location = loc
    for f, x, y in [("OakLargeA.png", X0 - 24, 24), ("Oak1.png", X0 - 14, 25), ("PineTall.png", X0 - 4, 24), ("OakLargeB.png", X0 + 8, 25),
                    ("Pine1.png", X0 + 18, 24), ("OakMediumA.png", X0 + 26, 20), ("PineA.png", X0 - 32, 18), ("Oak1.png", X0 + 3, 30),
                    ("RoundBush.png", X0 - 2.5, 1.0), ("BushA.png", X0 + 3.2, -1.4), ("FernWide.png", X0 + 2.6, 5.5), ("GrassA.png", X0 - 3.0, 4.6),
                    ("FlowersA.png", X0 - 6.5, 3.2), ("WildflowerPatch.png", X0 + 1.5, -3.0), ("GrassA.png", X0 + 3.4, 4.3),
                    ("MossBush.png", X0 - 4.6, -3.0), ("Fern.png", X0 + 2.2, -5.0), ("BushB.png", X0 + 16.0, 3.0)]:
        sprite(f, x, y)
    fc_d = bpy.data.cameras.new("PV_FarmCam"); fc_d.lens = 50
    fc = bpy.data.objects.new("PV_FarmCam", fc_d); PREVIEW.objects.link(fc)
    _p = 30.0
    fc.location = Vector((X0 - 3, 6, 1.0)) - Vector((0, math.cos(math.radians(_p)), -math.sin(math.radians(_p)))) * 33
    fc.rotation_euler = (math.radians(90 - _p), 0, 0)
