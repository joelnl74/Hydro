# EONLORE Japanese city kit - 3D props. Run by build_jp_kit.py (exec, shares its helpers and materials).
#
# The V9 split (decided Nov 1, 2026): props the player walks around, that cast a clear shadow or show their top from
# the field camera are 3D with evenly lit pixel textures; flat, hanging, small or organic things stay sprite cards
# (noren, paper lanterns, lantern strings, laundry, fish, nets on racks, plants, small clutter). Mixed props are a 3D
# frame carrying sprite cards (banner pole + nobori cloth, laundry pole + cloth, fish rack + fish, spear rack +
# spears). Light sources on posts or walls are 3D with an emissive core: put a point light at PROP_INFO[..]["light"].
# Every prop's origin is its foot on the ground, centred; fronts face -Y. Exported to Models/JapaneseCity/Props/.

PROP_INFO = {}
BAMBOO, STAVES, IRON, ROPE = "JP_Wood_Bamboo", "JP_Wood_Staves", "JP_Mat_Iron", "JP_Mat_Rope"
WEATHER, ROUGH = "JP_Wood_Weathered", "JP_Stone_Rough"


def prop(name, mb, col=None, **info):
    h = max((v.co.z for v in mb.bm.verts), default=1.0)
    ob = mb.to_object(name, COL["09_Props_3D"], ao=lambda co: max(0.7, 1.0 - 0.25 * max(0.0, 1.0 - co.z / 0.3)))
    if col:
        col_box(ob, *col)
    PROPS[name] = ob
    PROP_INFO[name] = dict(info, height=round(h, 2))
    return ob


def barrel_profile(r, h, bulge=0.1, n=6):
    return [(r * (1 + bulge * math.sin(math.pi * i / n)), h * i / n) for i in range(n + 1)]


def hoops(mb, r, h, zs, bulge=0.1, mat=IRON, sides=10, M=None):
    M = M or Matrix.Identity(4)
    for z in zs:
        rr = r * (1 + bulge * math.sin(math.pi * z / h)) + 0.012
        mb.revolve([(rr, z - 0.03), (rr, z + 0.03)], mat, sides, M)


def p_barrel():
    mb = MeshBuilder()
    r, h = 0.32, 0.9
    mb.revolve(barrel_profile(r, h), STAVES, 10, cap_top=WEATHER, cap_bottom=WEATHER)
    hoops(mb, r, h, (0.12, 0.3, 0.6, 0.78))
    return prop("JP_Prop_Barrel_A", mb, (-0.36, -0.36, 0, 0.36, 0.36, 0.9))


def p_sake_stack():
    """Sake barrels (komodaru) lying in a pyramid 3-2-1, straw-wrapped look from the staves + rope bands."""
    mb = MeshBuilder()
    r, L = 0.3, 0.75
    rows = [(-0.62, 0.0), (0.0, 0.0), (0.62, 0.0), (-0.31, 1), (0.31, 1), (0.0, 2)]
    for x, k in rows:
        z = r + k * r * 1.72
        M = T(x, -L / 2, z) @ R('X', -90)
        mb.revolve(barrel_profile(r, L, 0.06, 4), "JP_Wood_Weathered", 10, M, cap_top=STAVES, cap_bottom=STAVES)
        hoops(mb, r, L, (0.12, L - 0.12), 0.06, ROPE, 10, M)
    return prop("JP_Prop_SakeBarrelStack_A", mb, (-0.95, -0.4, 0, 0.95, 0.4, 1.6))


def p_rain_barrel():
    mb = MeshBuilder()
    mb.box(-0.42, -0.42, 0, 0.42, 0.42, 0.18, ROUGH)
    M = T(0, 0, 0.18)
    mb.revolve([(0.36, 0.0), (0.38, 0.5), (0.4, 0.95)], STAVES, 10, M, cap_bottom=STAVES)
    mb.revolve([(0.34, 0.85), (0.34, 0.9)], "JP_Mat_Glass", 10, M, cap_top="JP_Mat_Glass")      # water surface
    for z in (0.15, 0.75):
        mb.revolve([(0.385 + z * 0.04, z - 0.04), (0.385 + z * 0.04, z + 0.04)], BAMBOO, 10, M)
    mb.tube((0.25, -0.2, 1.1), (0.0, 0.05, 1.18), 0.03, BAMBOO, 5)                                 # ladle
    return prop("JP_Prop_RainBarrel_A", mb, (-0.42, -0.42, 0, 0.42, 0.42, 1.1))


def p_buckets():
    mb = MeshBuilder()
    for (x, y, z, rot) in ((0.0, 0.0, 0.0, 0), (0.05, 0.02, 0.36, 12)):
        M = T(x, y, z) @ R('Z', rot)
        mb.revolve([(0.2, 0.0), (0.24, 0.34)], STAVES, 8, M, cap_bottom=STAVES)
        for zz in (0.06, 0.28):
            mb.revolve([(0.21 + zz * 0.12, zz - 0.02), (0.21 + zz * 0.12, zz + 0.02)], BAMBOO, 8, M)
    mb.tube((-0.2, 0.0, 0.7), (0.0, 0.0, 0.86), 0.015, ROPE, 4)
    mb.tube((0.0, 0.0, 0.86), (0.25, 0.0, 0.7), 0.015, ROPE, 4)
    return prop("JP_Prop_Buckets_A", mb, (-0.26, -0.26, 0, 0.26, 0.26, 0.7))


def p_crate():
    mb = MeshBuilder()
    mb.box(-0.4, -0.3, 0, 0.4, 0.3, 0.55, WEATHER)
    for x in (-0.4, 0.34):
        mb.box(x, -0.32, 0, x + 0.06, 0.32, 0.57, CEDAR)
    mb.box(-0.42, -0.32, 0.25, 0.42, -0.28, 0.32, CEDAR)
    return prop("JP_Prop_Crate_A", mb, (-0.42, -0.32, 0, 0.42, 0.32, 0.57))


def p_stone_lantern():
    """Toro: hexagonal granite lantern, 1.7 m; firebox panels glow (shoji paper)."""
    mb = MeshBuilder()
    g = GRANITE
    mb.revolve([(0.36, 0.0), (0.36, 0.18), (0.28, 0.26)], g, 6, cap_bottom=g, cap_top=g)
    mb.revolve([(0.12, 0.26), (0.11, 0.82)], g, 6)
    mb.revolve([(0.18, 0.82), (0.3, 0.92), (0.3, 1.0)], g, 6, cap_top=g)
    mb.revolve([(0.2, 1.0), (0.2, 1.3)], "JP_Shoji_Paper", 6)
    for i in range(6):
        a = 2 * math.pi * i / 6
        mb.box(-0.035, -0.035, 1.0, 0.035, 0.035, 1.3, g, M=T(0.2 * math.cos(a), 0.2 * math.sin(a), 0))
    mb.revolve([(0.18, 1.3), (0.46, 1.36), (0.44, 1.42), (0.12, 1.6), (0.0, 1.62)], g, 6, cap_bottom=g)
    mb.revolve([(0.07, 1.6), (0.1, 1.66), (0.06, 1.74), (0.0, 1.76)], g, 6)
    return prop("JP_Prop_StoneLantern_A", mb, (-0.36, -0.36, 0, 0.36, 0.36, 1.7), light=[0, 0, 1.15])


def p_well():
    mb = MeshBuilder()
    mb.revolve([(0.85, 0.0), (0.85, 0.8), (0.65, 0.8), (0.65, 0.3)], ROUGH, 8, cap_top="JP_Mat_Glass")
    for x in (-0.95, 0.95):
        mb.box(x - 0.08, -0.08, 0, x + 0.08, 0.08, 2.2, CEDAR)
    mb.box(-1.1, -0.07, 2.05, 1.1, 0.07, 2.2, CEDAR)
    mb.revolve([(0.12, -0.04), (0.12, 0.04)], CEDAR, 8, T(0, 0, 1.92) @ R('X', 90), cap_top=CEDAR, cap_bottom=CEDAR)   # pulley
    mb.tube((0.0, 0.0, 1.8), (0.0, 0.0, 1.0), 0.012, ROPE, 4)
    mb.revolve([(0.12, 0.0), (0.14, 0.24)], STAVES, 8, T(0, 0, 0.78), cap_bottom=STAVES)
    zr = roof_gable(mb, -1.2, 1.2, -0.6, 0.6, 2.25, 30.0, 0.25, KAWARA, gable_over=0.15, ridge_end="none", gable_mat=CEDAR, slab=0.1)
    return prop("JP_Prop_Well_A", mb, (-0.9, -0.9, 0, 0.9, 0.9, 1.0))


def p_tea_bench():
    mb = MeshBuilder()
    mb.box(-0.9, -0.3, 0.42, 0.9, 0.3, 0.48, CEDAR)
    mb.box(-0.86, -0.27, 0.48, 0.86, 0.27, 0.5, "JP_Cloth_Red")                               # felt mat (small accent)
    for x in (-0.78, 0.7):
        for y in (-0.22, 0.14):
            mb.box(x, y, 0, x + 0.08, y + 0.08, 0.42, CEDAR)
    return prop("JP_Prop_TeaBench_A", mb, (-0.9, -0.3, 0, 0.9, 0.3, 0.5))


def p_handcart():
    mb = MeshBuilder()
    r = 0.42
    mb.box(-0.55, -0.7, r + 0.05, 0.55, 0.6, r + 0.14, WEATHER)
    for x in (-0.57, 0.51):
        mb.box(x, -0.7, r + 0.14, x + 0.06, 0.6, r + 0.4, WEATHER)
    for x in (-0.7, 0.64):
        M = T(x, 0, r) @ R('Y', 90)
        mb.revolve([(r, 0.0), (r, 0.06)], CEDAR, 12, M, cap_top=CEDAR, cap_bottom=CEDAR)
    mb.box(-0.7, -0.04, r - 0.04, 0.7, 0.04, r + 0.04, CEDAR)
    for x in (-0.4, 0.4):
        mb.tube((x, -0.7, r + 0.1), (x * 0.8, -2.1, 0.06), 0.035, CEDAR, 4)
    mb.box(-0.4, -2.1, 0.0, 0.4, -2.04, 0.12, CEDAR)
    return prop("JP_Prop_Handcart_A", mb, (-0.75, -2.1, 0, 0.75, 0.6, 0.9))


def p_bamboo_fence():
    mb = MeshBuilder()
    for x in (0.05, 1.95):
        mb.box(x - 0.06, -0.06, 0, x + 0.06, 0.06, 1.2, CEDAR)
    for z in (0.3, 0.75, 1.1):
        mb.tube((0.0, -0.08, z), (2.0, -0.08, z), 0.03, BAMBOO, 5)
    x = 0.12
    while x < 1.9:
        mb.tube((x, 0.0, 0.0), (x, 0.0, 1.05 + 0.08 * math.sin(x * 9)), 0.035, BAMBOO, 5)
        x += 0.09
    return prop("JP_Prop_FenceBamboo_2m_A", mb, (0, -0.1, 0, 2.0, 0.1, 1.2))


def p_laundry_pole():
    """Two forked posts carrying a bamboo pole; the cloth is JP_Laundry_Cloth cards hung at PROP_INFO card_z."""
    mb = MeshBuilder()
    for x in (-1.5, 1.5):
        mb.box(x - 0.2, -0.2, 0, x + 0.2, 0.2, 0.12, ROUGH)
        mb.tube((x, 0, 0.12), (x, 0, 1.9), 0.05, WEATHER, 5)
        mb.tube((x, 0, 1.75), (x - 0.15, 0, 2.05), 0.03, WEATHER, 4)
        mb.tube((x, 0, 1.75), (x + 0.15, 0, 2.05), 0.03, WEATHER, 4)
    mb.tube((-1.75, 0, 1.97), (1.75, 0, 1.95), 0.035, BAMBOO, 6)
    return prop("JP_Prop_LaundryPole_A", mb, (-1.6, -0.2, 0, 1.6, 0.2, 2.0), card_z=1.97, card_span=3.0)


def p_banner_pole():
    """Nobori stand: stone base, lacquered pole and crossbar; the cloth is the banner card (cut from
    JP_Banner_Nobori_A) placed with its top at card_top, its left edge at the pole."""
    mb = MeshBuilder()
    mb.revolve([(0.26, 0.0), (0.24, 0.22), (0.16, 0.3)], ROUGH, 8, cap_bottom=ROUGH, cap_top=ROUGH)
    mb.tube((0, 0, 0.3), (0, 0, 3.4), 0.04, CEDAR, 6)
    mb.tube((0, 0, 3.25), (0.75, 0, 3.25), 0.025, CEDAR, 5)
    return prop("JP_Prop_BannerPole_A", mb, (-0.26, -0.26, 0, 0.26, 0.26, 3.4), card_top=3.25, card_x=0.05)


def iron_lantern(mb, cx, cy, z_top, s=0.17):
    """A hanging iron lantern: pyramid cap, four glowing paper panels in an iron frame, ring. Returns its centre."""
    zc = z_top - 0.12
    mb.revolve([(0.02, z_top - 0.02), (0.04, z_top + 0.04)], IRON, 6, T(cx, cy, 0))
    mb.revolve([(s * 1.45, zc - 0.03), (0.0, zc + 0.12)], IRON, 4, T(cx, cy, 0) @ R('Z', 45), cap_bottom=IRON)
    mb.box(cx - s, cy - s, zc - 0.42, cx + s, cy + s, zc - 0.03, "JP_Shoji_Paper")
    for (dx, dy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        mb.box(cx + dx * s - 0.02, cy + dy * s - 0.02, zc - 0.44, cx + dx * s + 0.02, cy + dy * s + 0.02, zc - 0.03, IRON)
    mb.box(cx - s - 0.02, cy - s - 0.02, zc - 0.48, cx + s + 0.02, cy + s + 0.02, zc - 0.42, IRON)
    return (cx, cy, zc - 0.24)


def p_lamp_post():
    mb = MeshBuilder()
    mb.box(-0.2, -0.2, 0, 0.2, 0.2, 0.25, GRANITE)
    mb.box(-0.08, -0.08, 0.25, 0.08, 0.08, 2.9, CEDAR)
    mb.revolve([(0.11, 2.9), (0.0, 3.02)], CEDAR, 4, R('Z', 45))
    mb.box(-0.05, -0.05, 2.62, 0.75, 0.05, 2.72, CEDAR)
    mb.tube((0.06, 0.0, 2.25), (0.45, 0.0, 2.64), 0.03, CEDAR, 4)
    light = iron_lantern(mb, 0.62, 0.0, 2.6)
    return prop("JP_Prop_LampPost_A", mb, (-0.2, -0.2, 0, 0.2, 0.2, 2.9), light=[round(v, 2) for v in light])


def p_wall_lantern():
    """An iron lantern on a cedar bracket, for walls and gate posts: origin on the wall face, sticks out along -Y."""
    mb = MeshBuilder()
    mb.box(-0.06, -0.04, 1.9, 0.06, 0.0, 2.5, CEDAR)
    mb.box(-0.04, -0.55, 2.42, 0.04, 0.0, 2.5, CEDAR)
    light = iron_lantern(mb, 0.0, -0.45, 2.42, 0.13)
    return prop("JP_Prop_WallLantern_A", mb, None, light=[round(v, 2) for v in light])


def p_pier():
    mb = MeshBuilder()
    for y in (0.15, 1.85):
        for x in (-0.95, 0.95):
            mb.tube((x, y, -1.6), (x, y, 0.55), 0.09, WEATHER, 6)
            mb.revolve([(0.1, 0.3), (0.1, 0.36)], ROPE, 6, T(x, y, 0))
    mb.box(-1.05, 0.0, 0.3, 1.05, 2.0, 0.42, WEATHER)
    for x in (-1.08, 0.98):
        mb.box(x, 0.0, 0.22, x + 0.1, 2.0, 0.32, WEATHER)
    return prop("JP_Prop_Pier_2m_A", mb, (-1.05, 0, -1.6, 1.05, 2.0, 0.42), deck_z=0.42)


def p_mooring_post():
    mb = MeshBuilder()
    mb.tube((0, 0, -0.5), (0, 0, 1.0), 0.13, WEATHER, 7)
    mb.revolve([(0.13, 1.0), (0.1, 1.08), (0.0, 1.1)], WEATHER, 7)
    mb.revolve([(0.2, 0.55), (0.22, 0.6), (0.2, 0.66), (0.14, 0.66), (0.14, 0.55)], ROPE, 8)
    mb.revolve([(0.32, 0.0), (0.36, 0.05), (0.32, 0.1), (0.18, 0.1), (0.18, 0.0)], ROPE, 8, T(0.45, -0.15, 0))   # coil
    return prop("JP_Prop_MooringPost_A", mb, (-0.15, -0.15, 0, 0.15, 0.15, 1.1))


def p_boat():
    """Small open wooden boat (about 4.5 m): hull lofted from sections, two thwarts. Origin at the waterline centre."""
    mb = MeshBuilder()
    st = [(-2.25, 0.05, 0.55, 0.35), (-1.5, 0.45, 0.42, 0.18), (0.0, 0.62, 0.38, 0.12), (1.4, 0.5, 0.42, 0.18), (2.25, 0.04, 0.62, 0.45)]
    secs = []
    for x, hw, top, keel in st:
        secs.append([(x, -hw, top), (x, -hw * 0.8, -keel * 0.5), (x, 0.0, -keel), (x, hw * 0.8, -keel * 0.5), (x, hw, top)])
    for i in range(len(secs) - 1):
        a, b = secs[i], secs[i + 1]
        for j in range(4):
            mb.poly([a[j], b[j], b[j + 1], a[j + 1]], WEATHER)
            mb.poly([a[j + 1], b[j + 1], b[j], a[j]], WEATHER)              # inside (double-sided hull)
    for x in (-0.7, 0.7):
        mb.box(x - 0.12, -0.5, 0.18, x + 0.12, 0.5, 0.24, CEDAR)
    return prop("JP_Prop_Boat_A", mb, (-2.25, -0.62, -0.3, 2.25, 0.62, 0.6))


def p_net_frame():
    mb = MeshBuilder()
    for x in (-1.2, 1.2):
        mb.tube((x, 0, 0), (x, 0, 2.2), 0.045, BAMBOO, 6)
    mb.tube((-1.4, 0, 2.15), (1.4, 0, 2.15), 0.04, BAMBOO, 6)
    mb.box(-1.15, -0.01, 0.35, 1.15, 0.01, 2.1, "JP_Net_Alpha")
    return prop("JP_Prop_NetFrame_A", mb, (-1.25, -0.1, 0, 1.25, 0.1, 2.2))


def p_fish_rack():
    """A-frame drying rack with two bamboo rails; JP_Fish_Hanging cards hang from rail_z (front rail at y -0.25)."""
    mb = MeshBuilder()
    for x in (-1.2, 1.2):
        mb.tube((x, -0.45, 0), (x, 0.0, 1.8), 0.04, BAMBOO, 5)
        mb.tube((x, 0.45, 0), (x, 0.0, 1.8), 0.04, BAMBOO, 5)
    for y, z in ((-0.25, 1.4), (0.25, 1.4), (0.0, 1.78)):
        mb.tube((-1.35, y, z), (1.35, y, z), 0.03, BAMBOO, 5)
    return prop("JP_Prop_FishRack_A", mb, (-1.25, -0.45, 0, 1.25, 0.45, 1.8), rail_z=1.4, rail_y=-0.25)


def p_spear_rack():
    mb = MeshBuilder()
    for x in (-0.7, 0.7):
        mb.box(x - 0.06, -0.06, 0, x + 0.06, 0.06, 1.6, CEDAR)
    mb.box(-0.8, -0.08, 1.35, 0.8, 0.08, 1.45, CEDAR)
    mb.box(-0.8, -0.2, 0.12, 0.8, 0.2, 0.22, CEDAR)
    for k in range(5):
        x = -0.48 + k * 0.24
        mb.tube((x, 0.0, 0.22), (x, 0.0, 2.3), 0.02, "JP_Mat_Iron", 4)
        mb.revolve([(0.035, 2.3), (0.0, 2.55)], "JP_Mat_Glass", 4, T(x, 0, 0))
    return prop("JP_Prop_SpearRack_A", mb, (-0.8, -0.2, 0, 0.8, 0.2, 1.6))


def p_barrier():
    mb = MeshBuilder()
    for x in (-1.1, 1.1):
        mb.tube((x - 0.35, 0, 0), (x + 0.35, 0, 1.0), 0.05, CEDAR, 5)
        mb.tube((x + 0.35, 0, 0), (x - 0.35, 0, 1.0), 0.05, CEDAR, 5)
    mb.tube((-1.6, 0, 0.8), (1.6, 0, 0.8), 0.06, CEDAR, 6)
    mb.tube((-1.6, 0, 0.45), (1.6, 0, 0.45), 0.05, CEDAR, 6)
    return prop("JP_Prop_Barrier_A", mb, (-1.6, -0.2, 0, 1.6, 0.2, 1.0))


def p_war_drum():
    mb = MeshBuilder()
    M = T(0, 0.3, 1.05) @ R('X', 90)
    mb.revolve(barrel_profile(0.45, 0.6, 0.08, 4), "JP_Cloth_Red", 12, M, cap_top="JP_Drum_Hide", cap_bottom="JP_Drum_Hide")
    for x in (-0.55, 0.47):
        mb.box(x, -0.05, 0, x + 0.08, 0.65, 1.1, CEDAR)
    mb.box(-0.6, -0.05, 0.4, 0.6, 0.65, 0.5, CEDAR)
    return prop("JP_Prop_WarDrum_A", mb, (-0.6, -0.1, 0, 0.6, 0.7, 1.5))


def p_brazier():
    mb = MeshBuilder()
    for a in (0, 120, 240):
        x, y = 0.35 * math.cos(math.radians(a)), 0.35 * math.sin(math.radians(a))
        mb.tube((x, y, 0), (x * 0.4, y * 0.4, 0.75), 0.025, IRON, 4)
    mb.revolve([(0.12, 0.7), (0.32, 0.82), (0.36, 0.95), (0.3, 0.95), (0.1, 0.78)], IRON, 10)
    mb.revolve([(0.28, 0.88), (0.0, 0.92)], "JP_Clay_Ochre", 8)                         # embers bed
    return prop("JP_Prop_Brazier_A", mb, (-0.36, -0.36, 0, 0.36, 0.36, 0.95), light=[0, 0, 1.2])


def p_kamado():
    mb = MeshBuilder()
    mb.box(-0.55, -0.35, 0, 0.55, 0.35, 0.6, "JP_Clay_Ochre", top="JP_Clay_Ochre")
    for x in (-0.25, 0.25):
        mb.box(x - 0.12, -0.37, 0.12, x + 0.12, -0.33, 0.34, "JP_Mat_Opening")
        mb.revolve([(0.16, 0.6), (0.2, 0.72), (0.16, 0.8), (0.0, 0.8)], IRON, 8, T(x, 0.02, 0))
    return prop("JP_Prop_Kamado_A", mb, (-0.55, -0.35, 0, 0.55, 0.35, 0.8))


def p_water_jar():
    mb = MeshBuilder()
    mb.revolve([(0.22, 0.0), (0.38, 0.25), (0.4, 0.45), (0.3, 0.7), (0.26, 0.74)], "JP_Glaze_Brown", 10, cap_bottom="JP_Glaze_Brown")
    mb.revolve([(0.3, 0.74), (0.3, 0.79)], WEATHER, 10, cap_top=WEATHER)
    mb.tube((-0.05, -0.15, 0.82), (0.3, 0.1, 0.84), 0.02, BAMBOO, 4)
    return prop("JP_Prop_WaterJar_A", mb, (-0.4, -0.4, 0, 0.4, 0.4, 0.8))


def p_signboard():
    """Standing shop signboard (kanban) on two legs under a small tile cap. The front board is flat and faces the
    camera: the JP_Signboard_Standing sprite can be laid on it later; for now plain cedar with a lattice panel."""
    mb = MeshBuilder()
    for x in (-0.38, 0.3):
        mb.box(x, -0.06, 0, x + 0.08, 0.06, 1.7, CEDAR)
        mb.box(x - 0.05, -0.18, 0, x + 0.13, 0.18, 0.08, CEDAR)
    mb.box(-0.4, -0.04, 0.45, 0.4, 0.04, 1.6, "JP_Wood_Weathered")
    mb.box(-0.3, -0.06, 0.6, 0.3, -0.03, 1.45, "JP_Wood_Lattice")
    roof_gable(mb, -0.42, 0.42, -0.05, 0.05, 1.72, 30.0, 0.15, KAWARA, gable_over=0.08, ridge_end="none", gable_mat=CEDAR, barge=None, slab=0.06)
    return prop("JP_Prop_Signboard_A", mb, (-0.45, -0.2, 0, 0.45, 0.2, 1.8))


def p_embankment():
    mb = MeshBuilder()
    mb.solid([(-1.0, 0.0, -1.6), (1.0, 0.0, -1.6), (1.0, 1.0, -1.6), (-1.0, 1.0, -1.6)],
             [(-1.0, 0.35, 0.0), (1.0, 0.35, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0)], ROUGH)
    mb.box(-1.0, 0.3, -0.05, 1.0, 1.0, 0.0, GRANITE)
    return prop("JP_Prop_Embankment_2m_A", mb, (-1.0, 0.0, -1.6, 1.0, 1.0, 0.0))


def p_ladder():
    mb = MeshBuilder()
    for x in (-0.22, 0.22):
        mb.tube((x, 0, 0), (x, 0.0, 2.6), 0.03, BAMBOO, 5)
    for k in range(8):
        mb.tube((-0.22, 0, 0.3 + k * 0.3), (0.22, 0, 0.3 + k * 0.3), 0.02, BAMBOO, 4)
    return prop("JP_Prop_LadderBamboo_A", mb, None)


for fn in (p_barrel, p_sake_stack, p_rain_barrel, p_buckets, p_crate, p_stone_lantern, p_well, p_tea_bench, p_handcart,
           p_bamboo_fence, p_laundry_pole, p_banner_pole, p_lamp_post, p_wall_lantern, p_pier, p_mooring_post, p_boat,
           p_net_frame, p_fish_rack, p_spear_rack, p_barrier, p_war_drum, p_brazier, p_kamado, p_water_jar, p_signboard,
           p_embankment, p_ladder):
    fn()
