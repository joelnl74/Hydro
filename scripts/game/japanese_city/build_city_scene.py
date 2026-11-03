"""Writes Oxygen/project/Assets/Scenes/JapaneseCity.hyscene: a first full field scene built from the Japanese city kit,
to see in the game what the kit still lacks (JP_CITY_SCENE_PLAN.md zones, Nov 1 references: OT2 Ku, wuxia HD-2D).

Layout, south (near the camera) to north, three terraces joined by granite stairs:
  - Harbour (z 0): canal water, stone embankment, piers, a boat, the boathouse on stilts, net shed, fish store, racks.
  - Poor district (z 0): row house, huts and houses on an alley-dirt ground, well, laundry, kamado, fences.
  - Middle street (z 1.5): townhouses and a shop on both sides of a paved street (the south row turned away from the
    camera, so its roofs frame the street as foreground, as in the references), the fire watchtower behind.
  - Rich plaza (z 3): residence behind a moon-gate wall, the Lantern Inn, the city gate in the city wall with guards,
    the guild hall, a storehouse and the pagoda; lamp posts, banners, braziers, stalls, stone lanterns, a weeping
    cherry; forest and the sunset backdrop beyond the wall.

Ground: box shapes with the kit's paving / alley-dirt textures (Materials/JapaneseCity, written here) and box
colliders; terraces are retaining-wall modules; everything else is the kit's glbs and the PixelLab sprite cards.
Rewrites the scene on every run (don't edit it in the editor). Entity ids come from names (stable across runs).

    python scripts/game/japanese_city/build_city_scene.py [--profile Day|Dusk|Night]
Close the scene in Oxygen first. Check: Oxygen --scene JapaneseCity --play --frames 150 --capture out.png
"""

import argparse
import json
import math
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_kit_showcase as kit  # noqa: E402

kit.KEY = "JP_City:"
ASSETS, MODELS = kit.ASSETS, kit.MODELS
SCENE = os.path.join(ASSETS, "Scenes", "JapaneseCity.hyscene")
KIT_TEX = os.path.join(MODELS, "Source", "JP_Kit", "Textures")
TEX_DIR = os.path.join(ASSETS, "Textures", "JapaneseCity")
MAT_DIR = os.path.join(ASSETS, "Materials", "JapaneseCity")
TREES = os.path.join(ASSETS, "Sprites", "Trees")
PLANTS = os.path.join(ASSETS, "Sprites", "Plants")

Z_QUAY, Z_MID, Z_RICH = 0.0, 1.5, 3.0
WATER_Z = -0.8


def stable_handle(name):
    return int(kit.hashlib.sha256(("JP_City:asset:" + name).encode()).hexdigest()[:16], 16) & 0x7FFFFFFFFFFFFFFF


def write_material(path, text):
    """Writes a .hymat, keeping the normal map scripts/game/textures/make_normal_maps.py set on it (Nov 3, 2026: the
    builders rewrite their materials on every run, which dropped NormalHandle / NormalStrength)."""
    keep = ""
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            old = f.read()
        for key in ("NormalHandle", "NormalStrength"):
            m = re.search(r"^  %s: .*$" % key, old, re.M)
            if m and ("  %s:" % key) not in text:
                keep += m.group(0) + "\n"
    with open(path, "w", newline="\r\n") as f:
        f.write(text + keep)


def ground_material(name, tiling=0.25):
    """Copy a kit texture to Textures/JapaneseCity (repeat wrap) and give it a material with 32 texels per metre."""
    os.makedirs(TEX_DIR, exist_ok=True); os.makedirs(MAT_DIR, exist_ok=True)
    png = os.path.join(TEX_DIR, name + ".png")
    shutil.copy2(os.path.join(KIT_TEX, name + ".png"), png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Repeat\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % stable_handle("tex:" + name))
    tex = kit.handle_of(png + ".hymeta")
    mat = os.path.join(MAT_DIR, name + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else stable_handle("mat:" + name)
    write_material(mat, "Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n  Tint: [1, 1, 1, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [0, 0, 0]\n  EmissionIntensity: 1\n  AlphaCutoff: 0\n  Filter: Nearest\n" % (h, tex, tiling, tiling))
    return h


class City:
    def __init__(self, summary):
        self.w = kit.Writer()
        self.d = kit.Dresser(self.w, summary)
        self.summary = summary
        self.n = 0

    def _n(self, base):
        self.n += 1
        return "%s %d" % (base, self.n)

    def group(self, name, parent=None):
        self.w.entity(name, name, parent=parent)
        return name

    def slab(self, parent, mat, x0, x1, y0, y1, top, h=2.0, tint=None):
        tag = self._n("Ground")
        # Hydro's Box shape grows up from the entity origin (MeshFactory::Box: z 0..size.z); the box collider is centred
        cx, cy, cz = (x0 + x1) / 2, (y0 + y1) / 2, top - h
        self.w.entity("slab:" + tag, tag, parent=parent, t=(cx, cy, cz), body=(
            "    MeshComponent:\n      Shape: Box\n      Size: [%g, %g, %g]\n      CastShadows: false\n      Materials:\n"
            "        - MaterialHandle: %d\n" % (x1 - x0, y1 - y0, h, mat)) + (
            "    BoxColliderComponent:\n      Offset: [0, 0, %g]\n      Size: [%g, %g, %g]\n      Layer: World\n      IsTrigger: false\n"
            % (h / 2, x1 - x0, y1 - y0, h)))

    def model(self, parent, path, x, y, z=0.0, rot=0.0, collider=True, tag=None):
        glb = os.path.join(MODELS, path + ".glb")
        if os.path.exists(glb) is False:
            self.d.missing.append(path); return
        h = kit.ensure_mesh_meta(glb)
        tag = tag or self._n(os.path.basename(path).replace("JP_", ""))
        self.w.entity("model:" + tag, tag, parent=parent, t=(x, y, z), r=(0, 0, math.radians(rot)), body=kit.mesh_body(h, collider))

    def building(self, parent, name, x, y, z, rot=0.0):
        self.model(parent, name, x, y, z, rot)
        return self.summary.get(name, {})

    def plant(self, parent, name, x, y, z, folder=PLANTS, ppm=32.0):
        self.d.sprite(parent, name, x, y, z, folder=folder, ppm=ppm)

    def npc(self, parent, name, x, y, z, clip="IdleDown"):
        base = os.path.join(ASSETS, "Characters", "NPC", name, name)
        if os.path.exists(base + ".png.hymeta") is False or os.path.exists(base + ".hyanim") is False:
            self.d.missing.append("NPC " + name); return
        tex, anim = kit.handle_of(base + ".png.hymeta"), kit.handle_of(base + ".hyanim")
        tag = self._n("Npc " + name)
        self.w.entity("npc:" + tag, tag, parent=parent, t=(x, y, z), s=(2.5, 2.34375, 1),
                      body=kit.character_body(tex, anim, clip, clip + "_0"))


def build(c, args):
    S = c.summary
    paving, dirt = ground_material("JP_Paving_City"), ground_material("JP_Ground_AlleyDirt")
    water = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Water.hymat"))
    sky = kit.handle_of(os.path.join(ASSETS, "Materials", "Backdrops", "SunsetBackdrop.hymat"))
    d = c.d

    G = c.group("Ground")
    c.slab(G, water, -70, 70, -80, -52, WATER_Z, 0.6)
    c.slab(G, paving, -70, 70, -52, -44, Z_QUAY)                 # quay
    c.slab(G, dirt, -70, 70, -44, -22, Z_QUAY)                   # poor district
    # the terraces start just behind the retaining walls' faces and leave a notch where the stairs climb
    SX0, SX1 = -0.9, 2.9
    for (top, h, y0, ys, y1) in ((Z_MID, 3.0, -21.2, -19.4, 12.8), (Z_RICH, 4.0, 12.8, 14.6, 31.0)):
        c.slab(G, paving, -70, SX0, y0, ys, top, h)
        c.slab(G, paving, SX1, 70, y0, ys, top, h)
        c.slab(G, paving, -70, 70, ys, y1, top, h)
    # outside the city wall: meadow up to the backdrop, the road out of the gate
    grass = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Grass.hymat"))
    road = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "VillageDirt.hymat"))
    c.slab(G, grass, -70, 70, 31.0, 108.0, Z_RICH, 4.0)
    c.slab(G, road, -2.6, 2.6, 26.0, 108.0, Z_RICH + 0.02, 0.5)
    w = c.w
    w.entity("Backdrop", "Backdrop Sunset", parent=G, t=(0, 108, -20), body=(
        "    MeshComponent:\n      Shape: Box\n      Size: [260, 0.2, 60]\n      CastShadows: false\n      Materials:\n"
        "        - MaterialHandle: %d\n" % sky))

    # --- terraces: retaining walls with stairs gaps, embankment along the canal
    T = c.group("Terraces")
    for x in range(-68, 70, 2):
        c.model(T, "Props/JP_Prop_Embankment_2m_A", x + 1, -52.0, Z_QUAY)
    for x in range(-68, 68, 4):
        if -2 <= x + 2 <= 4:
            continue
        c.model(T, "Modules/JP_Wall_Retaining_Rough_4m", x + 2, -22.0, Z_QUAY)
    c.model(T, "Modules/JP_Stairs_Granite_3m", 1.0, -22.0, Z_QUAY)
    for x in range(-68, 68, 4):
        if -2 <= x + 2 <= 4:
            continue
        c.model(T, "Modules/JP_Wall_Retaining_Granite_4m", x + 2, 12.0, Z_MID)
    c.model(T, "Modules/JP_Stairs_Granite_3m", 1.0, 12.0, Z_MID)
    for x in (-3.4, 3.2):                                         # railings along the upper stair landing
        c.model(T, "Modules/JP_Railing_Stone_2m", x - 1, 14.9, Z_RICH)

    # --- harbour
    H = c.group("Harbour")
    c.building(H, "JP_Poor_Boathouse_A", -20.0, -55.0, -0.6)
    for x in (8.0, 10.0):
        c.model(H, "Props/JP_Prop_Pier_2m_A", x, -56.0, -0.42)
        c.model(H, "Props/JP_Prop_Pier_2m_A", x, -54.0, -0.42)
    c.model(H, "Props/JP_Prop_Boat_A", 3.5, -60.0, WATER_Z + 0.15, 6)
    c.model(H, "Props/JP_Prop_Boat_A", -9.0, -62.5, WATER_Z + 0.15, -10)
    for x in (-12.0, 2.5, 13.0, 24.0):
        c.model(H, "Props/JP_Prop_MooringPost_A", x, -52.4, Z_QUAY)
    c.building(H, "JP_Poor_NetShed_A", 8.0, -48.5, Z_QUAY)
    c.building(H, "JP_Poor_FishStore_A", 17.0, -48.5, Z_QUAY)
    c.building(H, "JP_Poor_Hut_B", 27.0, -48.5, Z_QUAY)
    c.model(H, "Props/JP_Prop_FishRack_A", 12.0, -50.6, Z_QUAY)
    d.sprite(H, "JP_Fish_Hanging_A", 11.3, -50.85, Z_QUAY + 1.4, hanging=True, sort=2)
    d.sprite(H, "JP_Fish_Hanging_B", 12.7, -50.85, Z_QUAY + 1.4, hanging=True, sort=2)
    c.model(H, "Props/JP_Prop_NetFrame_A", -10.0, -49.0, Z_QUAY)
    c.model(H, "Props/JP_Prop_NetFrame_A", -6.8, -49.4, Z_QUAY, 8)
    for (x, y, r) in ((-14.5, -46.0, 10), (-13.8, -46.6, -5), (21.5, -46.5, 0)):
        c.model(H, "Props/JP_Prop_Crate_A", x, y, Z_QUAY, r)
    c.model(H, "Props/JP_Prop_Barrel_A", 4.4, -46.5, Z_QUAY)
    c.model(H, "Props/JP_Prop_Barrel_A", 5.1, -46.0, Z_QUAY)
    for x in (-27.0, 0.0, 30.0):
        c.model(H, "Props/JP_Prop_LampPost_A", x, -51.4, Z_QUAY)
        d.lantern(H, x + 0.62, -51.4, Z_QUAY + 2.36)
    c.npc(H, "Gatherer", 6.0, -50.5, Z_QUAY)

    # --- poor district
    P = c.group("Poor District")
    for name, x in (("JP_Poor_House_A", -34.0), ("JP_Poor_Hut_A", -25.0), ("JP_Poor_RowHouse_A", -11.0),
                    ("JP_Poor_House_B", 12.0), ("JP_Poor_Hut_A", 21.0), ("JP_Poor_House_A", 30.0)):
        c.building(P, name, x, -34.0, Z_QUAY)
    c.building(P, "JP_Poor_Hut_B", -44.0, -33.0, Z_QUAY, 8)
    c.model(P, "Props/JP_Prop_Well_A", 2.0, -38.5, Z_QUAY)
    c.model(P, "Props/JP_Prop_LaundryPole_A", -11.0, -37.2, Z_QUAY)
    d.sprite(P, "JP_Laundry_Cloth_A", -11.75, -37.25, Z_QUAY + 1.97, hanging=True, sort=2)
    d.sprite(P, "JP_Laundry_Cloth_A", -10.25, -37.25, Z_QUAY + 1.97, hanging=True, sort=2)
    c.model(P, "Props/JP_Prop_LaundryPole_A", 18.0, -37.0, Z_QUAY, -6)
    d.sprite(P, "JP_Laundry_Cloth_A", 18.0, -37.15, Z_QUAY + 1.97, hanging=True, sort=2)
    for (name, x, y, r) in (("JP_Prop_Kamado_A", -32.0, -35.4, 0), ("JP_Prop_WaterJar_A", -36.5, -35.0, 0),
                            ("JP_Prop_Buckets_A", -24.0, -35.0, 0), ("JP_Prop_Buckets_A", 4.0, -37.5, 20),
                            ("JP_Prop_RainBarrel_A", 14.8, -34.8, 0), ("JP_Prop_Barrel_A", 27.0, -34.9, 0),
                            ("JP_Prop_Handcart_A", -2.5, -41.5, -35), ("JP_Prop_WaterJar_A", 32.4, -35.0, 0)):
        c.model(P, "Props/" + name, x, y, Z_QUAY, r)
    for x in (-40.0, -38.0, 36.0, 38.0):
        c.model(P, "Props/JP_Prop_FenceBamboo_2m_A", x, -35.5, Z_QUAY)
    for (name, x, y) in (("OakSunB", -46.0, -26.0), ("OakSunA", 44.0, -27.0), ("FirSmallB", -20.0, -26.0),
                         ("FirSmallC", 25.0, -25.5), ("FirSmallA", 40.0, -31.0)):
        c.plant(P, name, x, y, Z_QUAY, TREES)
    for (name, x, y) in (("BushDense01", -29.5, -35.5), ("BushDense02", 7.5, -35.4), ("FlowerBush01", 24.0, -35.3),
                         ("GrassTuft01", -6.0, -40.0), ("GrassTuft02", 9.0, -42.5), ("Fern01", -45.0, -36.0)):
        c.plant(P, name, x, y, Z_QUAY)
    for i, xx in enumerate(S.get("JP_Poor_RowHouse_A", {}).get("door_x", [])):
        if i % 2 == 0:
            d.window(P, -11.0 + xx, -34.6, Z_QUAY + 1.4, 2.6)
    c.npc(P, "Farmer", -4.0, -39.5, Z_QUAY)
    c.npc(P, "Gatherer", 15.0, -38.5, Z_QUAY)

    # --- middle street (north row faces the camera; the south row turns its back, its roofs frame the street)
    M = c.group("Middle Street")
    north = (("JP_Mid_Townhouse_A", -25.0), ("JP_Mid_Townhouse_B", -16.0), ("JP_Mid_Townhouse_C", -7.0),
             ("JP_Mid_Shop_A", 9.0), ("JP_Mid_Townhouse_A", 16.5), ("JP_Mid_Townhouse_B", 25.5), ("JP_Mid_Townhouse_C", 35.0))
    for name, x in north:
        info = c.building(M, name, x, -1.0, Z_MID)
        if info.get("door_x") is not None:
            dx = x + info["door_x"]
            d.sprite(M, "JP_Noren_Inn_A", dx, -0.7, Z_MID + info["head_z"] - 0.05, hanging=True, sort=1)
            d.sprite(M, "JP_Lantern_Paper_Red_A", dx + 1.3, -1.65, Z_MID + info["pent_z"] - 0.05, hanging=True, sort=3)
            d.lantern(M, dx + 1.3, -1.65, Z_MID + info["pent_z"] - 0.6, 1.0, 3.5)
        for k in range(max(1, int(info.get("width", 6) / 4))):
            d.window(M, x - info.get("width", 6) / 2 + (k + 0.5) * info.get("width", 6) / max(1, int(info.get("width", 6) / 4)), -1.7, Z_MID + 1.6)
    for name, x in (("JP_Mid_Townhouse_C", -26.0), ("JP_Mid_Townhouse_A", -16.5), ("JP_Mid_Townhouse_B", -7.0),
                    ("JP_Mid_Townhouse_A", 10.0), ("JP_Mid_Townhouse_C", 19.0), ("JP_Mid_Townhouse_B", 28.5)):
        c.building(M, name, x, -10.0, Z_MID, 180.0)
    c.building(M, "JP_Mid_FireWatchtower_A", -36.0, 2.0, Z_MID)
    shop_info = S.get("JP_Mid_Shop_A", {})
    if shop_info.get("pent_z"):
        d.sprite(M, "JP_Sign_Hanging_A", 7.1, -1.9, Z_MID + shop_info["pent_z"] - 0.1, hanging=True, sort=3)
    for (name, x, y, r) in (("JP_Prop_MarketStall_A", -1.2, -5.0, 0), ("JP_Prop_MarketStall_B", 2.6, -5.4, 0),
                            ("JP_Prop_RainBarrel_A", -28.6, -1.6, 0), ("JP_Prop_Buckets_A", -19.8, -1.6, 0),
                            ("JP_Prop_WaterJar_A", -12.4, -1.7, 0), ("JP_Prop_Signboard_A", 4.8, -2.5, 0),
                            ("JP_Prop_Handcart_A", 13.0, -6.5, 70), ("JP_Prop_Crate_A", 12.6, -1.8, 8),
                            ("JP_Prop_SakeBarrelStack_A", 21.5, -1.9, 0), ("JP_Prop_TeaBench_A", 30.4, -2.1, 0),
                            ("JP_Prop_StoneLantern_A", -2.6, -1.8, 0), ("JP_Prop_StoneLantern_A", 4.6, -1.8, 0)):
        c.model(M, "Props/" + name, x, y, Z_MID, r)
    d.lantern(M, -2.6, -1.8, Z_MID + 1.15, 0.8, 2.5)
    d.lantern(M, 4.6, -1.8, Z_MID + 1.15, 0.8, 2.5)
    d.sprite(M, "JP_Signboard_Standing_A", -9.8, -2.0, Z_MID)
    d.sprite(M, "JP_Lantern_String_A", 1.0, -1.2, Z_MID + 3.6, hanging=True, sort=3)
    for x in (-33.0, -11.5, 13.8, 39.5):
        c.model(M, "Props/JP_Prop_LampPost_A", x, -8.6, Z_MID)
        d.lantern(M, x + 0.62, -8.6, Z_MID + 2.36)
    for (name, x, y) in (("FirTallA", -42.0, 6.0), ("FirSmallD", -2.0, 6.5), ("FirSmallE", 4.0, 7.0), ("OakSunC", 42.0, 4.0)):
        c.plant(M, name, x, y, Z_MID, TREES)
    for (name, x, y) in (("TopiaryA", -3.4, -1.4), ("TopiaryB", 5.4, -1.4), ("FlowerBush02", -21.0, -1.6), ("FlowerBush03", 20.7, -1.6)):
        c.plant(M, name, x, y, Z_MID)
    c.npc(M, "Barmaid", 29.0, -3.5, Z_MID)
    c.npc(M, "Pilgrim", -14.0, -5.0, Z_MID)
    c.npc(M, "VillageElder", 3.5, -6.6, Z_MID)
    c.npc(M, "Farmer", -6.5, -8.2, Z_MID)
    c.npc(M, "Gatherer", 8.5, -7.6, Z_MID)
    c.npc(M, "Monk", 6.4, -3.2, Z_MID)

    # --- rich plaza
    R = c.group("Rich Plaza")
    c.building(R, "JP_Rich_Residence_A", -40.0, 22.0, Z_RICH)
    c.building(R, "JP_Rich_MoonGateWall_6m_A", -40.0, 17.0, Z_RICH)
    for x in (-48.0, -32.0):
        c.building(R, "JP_Rich_CompoundWall_4m_A", x, 17.0, Z_RICH)
    inn = c.building(R, "JP_Rich_LanternInn_A", -22.0, 20.0, Z_RICH)
    c.building(R, "JP_Rich_CityGate_A", 0.0, 25.0, Z_RICH)
    c.building(R, "JP_Rich_CityGate_A_DoorL", -2.0, 26.05, Z_RICH, -70)          # leaves swung open
    c.building(R, "JP_Rich_CityGate_A_DoorR", 2.0, 26.05, Z_RICH, 70)
    for x in (-11.0, 11.0, -19.0, 19.0, -27.0, 27.0, -35.0, 35.0, -43.0, 43.0, -51.0, 51.0):
        c.building(R, "JP_Rich_CityWall_8m_A", x, 25.0 + (6.0 if abs(x) > 12 else 0.0), Z_RICH)
    c.building(R, "JP_Rich_GuardBooth_A", 9.2, 22.4, Z_RICH)
    hall = c.building(R, "JP_Rich_GuildHall_A", 23.0, 19.0, Z_RICH)
    c.building(R, "JP_Rich_Storehouse_A", 36.0, 20.0, Z_RICH)
    c.building(R, "JP_Rich_Pagoda_A", 46.0, 22.0, Z_RICH)
    for (b, bx, by, long_noren) in ((inn, -22.0, 20.0, False), (hall, 23.0, 19.0, True)):
        if b.get("door_x") is None:
            continue
        dx = bx + b["door_x"]
        d.sprite(R, "JP_Noren_Inn_Long_A" if long_noren else "JP_Noren_Inn_A", dx, by + 0.3, Z_RICH + b["head_z"] - 0.05, hanging=True, sort=1)
        for s_ in (-1, 1):
            d.sprite(R, "JP_Lantern_Paper_Red_A", dx + s_ * 1.5, by - 0.75, Z_RICH + b["pent_z"] - 0.05, hanging=True, sort=3)
            d.lantern(R, dx + s_ * 1.5, by - 0.75, Z_RICH + b["pent_z"] - 0.6, 1.0, 3.5)
            c.model(R, "Props/JP_Prop_StoneLantern_A", dx + s_ * 3.4, by - 2.4, Z_RICH)
            d.lantern(R, dx + s_ * 3.4, by - 2.4, Z_RICH + 1.15, 0.8, 2.5)
        for k in range(4):
            d.window(R, bx - 6 + k * 4, by - 0.8, Z_RICH + 1.6)
        d.sprite(R, "JP_Lantern_String_A", bx - 4.2, by - 1.05, Z_RICH + b["pent_z"] - 0.1, hanging=True, sort=3)
        d.sprite(R, "JP_Lantern_String_B", bx + 4.2, by - 1.05, Z_RICH + b["pent_z"] - 0.1, hanging=True, sort=3)
    # the gate: banners, braziers, wall lanterns, barriers, spears, drum, guards
    for s_ in (-1, 1):
        c.model(R, "Props/JP_Prop_BannerPole_A", s_ * 3.5, 23.2, Z_RICH)
        d.sprite(R, "JP_Banner_Nobori_Cloth_A", s_ * 3.5 + 0.45, 23.12, Z_RICH + 3.3, hanging=True, sort=2)
        c.model(R, "Props/JP_Prop_Brazier_A", s_ * 2.7, 24.1, Z_RICH)
        d.lantern(R, s_ * 2.7, 24.1, Z_RICH + 1.2, 1.6, 4.0, "Fire")
        c.model(R, "Props/JP_Prop_Barrier_A", s_ * 6.0, 21.4, Z_RICH, s_ * 8)
        c.model(R, "Props/JP_Prop_WallLantern_A", s_ * 2.25, 25.9, Z_RICH)
        d.lantern(R, s_ * 2.25, 25.45, Z_RICH + 2.2)
        c.npc(R, "Blacksmith", s_ * 2.6, 22.9, Z_RICH)
    c.model(R, "Props/JP_Prop_SpearRack_A", -5.6, 23.9, Z_RICH)
    c.model(R, "Props/JP_Prop_WarDrum_A", 7.4, 23.4, Z_RICH)
    d.sprite(R, "JP_Spears_Leaning_A", 5.4, 24.2, Z_RICH)
    # plaza dressing
    for x in (-30.0, -13.0, 12.0, 30.0):
        c.model(R, "Props/JP_Prop_LampPost_A", x, 15.6, Z_RICH)
        d.lantern(R, x + 0.62, 15.6, Z_RICH + 2.36)
    for (name, x, y, r) in (("JP_Prop_MarketStall_B", -10.5, 17.6, 0), ("JP_Prop_MarketStall_A", 13.8, 17.4, 0),
                            ("JP_Prop_TeaBench_A", -27.8, 17.6, 0), ("JP_Prop_SakeBarrelStack_A", -15.6, 18.7, 0),
                            ("JP_Prop_Crate_A", 33.4, 18.6, 12), ("JP_Prop_Crate_A", 34.0, 17.9, -4), ("JP_Prop_Barrel_A", 38.6, 18.8, 0),
                            ("JP_Prop_RainBarrel_A", 30.0, 18.4, 0), ("JP_Prop_Well_A", -6.0, 18.0, 0)):
        c.model(R, "Props/" + name, x, y, Z_RICH, r)
    for s_ in (-1, 1):
        c.model(R, "Props/JP_Prop_BannerPole_A", s_ * 14.8, 19.8, Z_RICH)
        d.sprite(R, "JP_Banner_Nobori_Cloth_A", s_ * 14.8 + 0.45, 19.72, Z_RICH + 3.3, hanging=True, sort=2)
    c.plant(R, "SakuraWeeping", -33.5, 19.0, Z_RICH, TREES)
    c.plant(R, "SakuraWeeping", 41.5, 18.6, Z_RICH, TREES)
    for (name, x, y) in (("FirTallA", -14.0, 35.0), ("FirTallB", -6.0, 37.0), ("FirTallC", 8.0, 36.0), ("FirTallD", 16.0, 38.0),
                         ("FirTallB", -30.0, 40.0), ("FirTallA", 30.0, 41.0), ("PineTall", 0.0, 42.0), ("FirTallC", -46.0, 39.0),
                         ("FirTallD", 50.0, 40.0)):
        c.plant(R, name, x, y, Z_RICH, TREES)
    for (name, x, y) in (("TopiaryA", -18.8, 18.4), ("TopiaryB", -25.2, 18.4), ("TopiaryA", 19.8, 17.6), ("TopiaryB", 26.4, 17.6),
                         ("FlowerBush04", -44.0, 16.4), ("FlowerBush05", -35.8, 16.4), ("BushDense03", 4.2, 15.2), ("BushDense04", -4.0, 15.2)):
        c.plant(R, name, x, y, Z_RICH)
    c.npc(R, "Monk", -36.5, 15.4, Z_RICH)
    c.npc(R, "Gatherer", 18.0, 15.6, Z_RICH)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"])
    ap.add_argument("--start", default="mid", choices=["harbour", "poor", "mid", "rich"], help="where Miko starts (for captures)")
    args = ap.parse_args()
    summary = json.load(open(kit.SUMMARY))
    profiles = {k: kit.handle_of(os.path.join(ASSETS, "RenderProfiles", "Field", k + ".hyrender")) for k in ("Day", "Dusk", "Night")}
    scene_handle = kit.handle_of(SCENE) if os.path.exists(SCENE) else kit.stable_id("scene")
    c = City(summary)
    w = c.w
    starts = {"harbour": (-2.0, -48.0, Z_QUAY), "poor": (-2.0, -40.0, Z_QUAY), "mid": (1.0, -6.5, Z_MID), "rich": (0.0, 18.0, Z_RICH)}
    start = starts[args.start]
    w.entity("Player", "Player", t=start, s=(2.5, 2.34375, 3), body=kit.character_body(kit.MIKO_TEXTURE, kit.MIKO_ANIM, "IdleDown", "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 12.7, start[2] + 8.0), r=(1.22173047, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 35\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 300\n      OrthographicSize: 12\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [60, 60]\n            LookAhead: 1.5\n            Smoothing: 8\n"))
    w.entity("Director", "City Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: JapaneseCity\n            TiltDegrees: 70\n            FieldOfView: 35\n"
        "            FocusLead: 3\n            FocusRise: 2.2\n            BoundsMin: [-46, -54]\n            BoundsMax: [46, 22]\n"
        "            StartPreset: %d\n            DayProfile: %d\n            DuskProfile: %d\n            NightProfile: %d\n")
        % (("Day", "Dusk", "Night").index(args.profile), profiles["Day"], profiles["Dusk"], profiles["Night"]))
    w.entity("Lighting", "Lighting")
    w.entity("Sun", "Sun", parent="Lighting", t=(-8, -6, 10), r=(1.01229095, 0, -0.663224995), body=(
        "    DirectionalLightComponent:\n      Color: [1, 0.86, 0.7]\n      Intensity: 0.8\n      IsMainLight: true\n      CastShadows: true\n"
        "      ShadowStrength: 0.85\n      ShadowDistance: 40\n      ShadowBias: 1\n"))
    build(c, args)
    header = (
        "Handle: %d\nScene: JapaneseCity\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
        "    SpriteSorting: ViewDepth\n    Lighting: Forward\n    DefaultSpriteAlpha: Cutout\n    Meshes: true\n    Shadows: Sun\n"
        "    SpriteShadows: AsDrawn\n    UprightSprites: Vertical\n    HDR: true\n    Bloom: true\n    DepthOfField: true\n"
        "Look:\n  ProfileHandle: %d\n  Environment: Profile\n  PostProcess: Profile\n"
        "Environment:\n  SkyAmbient: [0.42, 0.36, 0.3]\n  GroundAmbient: [0.16, 0.12, 0.08]\n  Fog: true\n  FogColor: [0.78, 0.6, 0.38]\n"
        "  FogStart: 30\n  FogEnd: 90\nEntities:\n") % (scene_handle, profiles[args.profile])
    with open(SCENE, "w", newline="\r\n") as f:
        f.write(header + "\n".join(w.lines) + "\n")
    print("wrote %s: %d entities, start %s, profile %s" % (SCENE, sum(1 for l in w.lines if l.startswith("  - Entity:")), args.start, args.profile))
    if c.d.missing:
        print("missing (skipped):", sorted(set(c.d.missing)))


if __name__ == "__main__":
    main()
