"""Writes Oxygen/project/Assets/Scenes/JapaneseCitySlice.hyscene: the first *playable* piece of the Japanese city, laid
out for the field camera from the Nov 1 layout references (merchant street, stair street, gate plaza):

  - Merchant street (z 0): runs left to right across the screen. Shops only on the far (north) side, facing the
    camera; the near side holds low things only (stalls, a bamboo fence, stone lanterns, barrels, cherry trees).
  - Stair street: two granite flights with stone cheek walls climb straight away from the camera between the shops,
    with a landing between them (z 3.5) and walled gardens either side.
  - Gate plaza (z 7): the city gate in the city wall at the back, the Lantern Inn on the left and the guild hall on
    the right turned towards the plaza, braziers, banners, guards; meadow, forest and the backdrop beyond.

Every walkable spot is visible from the camera: nothing tall stands between the camera and a street, and invisible
walls keep Miko out of the gaps behind buildings. Camera zones (CameraZoneScript, Game/src/Scripts/Common) frame
each part: the street along its length, the stair pulled back and aiming up it, the plaza centred on the gate.

Ground and walls are box / stairs shapes with the round-2 textures (art_incoming/jp_city/textures_r2: setts,
flagstones, small castle-wall stones) and the kit's granite; buildings and props are the kit's glbs.
Rewrites the scene on every run (don't edit it in the editor). Entity ids come from names (stable across runs).

    python scripts/japanese_city/build_city_slice.py [--profile Day|Dusk|Night] [--start street|stairs|landing|plaza]
Close the scene in Oxygen first. Check: Oxygen --scene JapaneseCitySlice --play --frames 150 --capture out.png
"""

import argparse
import json
import math
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_kit_showcase as kit  # noqa: E402
import build_city_scene as city  # noqa: E402  (City, its slab / model / npc helpers)

kit.KEY = "JP_Slice:"
ASSETS, MODELS, REPO = kit.ASSETS, kit.MODELS, kit.REPO
SCENE = os.path.join(ASSETS, "Scenes", "JapaneseCitySlice.hyscene")
R2 = os.path.join(REPO, "art_incoming", "jp_city", "textures_r2")
TREES, PLANTS = city.TREES, city.PLANTS

Z0, Z1, Z2 = 0.0, 3.5, 7.0          # street, landing and gardens, gate plaza
STAIR_W, CHEEK = 5.0, 1.0           # flight width, stone cheek wall either side
CAP = 0.12                          # paving / grass cap on top of a terrace's wall box


def r2_material(src, name, tiling=0.25):
    """A material for a round-2 texture (art_incoming/jp_city/textures_r2/<src>), copied to Textures/JapaneseCity."""
    os.makedirs(city.TEX_DIR, exist_ok=True)
    os.makedirs(city.MAT_DIR, exist_ok=True)
    png = os.path.join(city.TEX_DIR, name + ".png")
    shutil.copy2(os.path.join(R2, src), png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Repeat\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % city.stable_handle("tex:" + name))
    tex = kit.handle_of(png + ".hymeta")
    mat = os.path.join(city.MAT_DIR, name + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else city.stable_handle("mat:" + name)
    with open(mat, "w", newline="\r\n") as f:
        f.write("Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n  Tint: [1, 1, 1, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [0, 0, 0]\n  EmissionIntensity: 1\n  AlphaCutoff: 0\n  Filter: Nearest\n" % (h, tex, tiling, tiling))
    return h


def tinted_material(texture, name, tint, tiling=0.25):
    """A second material on a texture already in Textures/JapaneseCity, darkened by `tint` (step risers)."""
    tex = kit.handle_of(os.path.join(city.TEX_DIR, texture + ".png.hymeta"))
    mat = os.path.join(city.MAT_DIR, name + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else city.stable_handle("mat:" + name)
    with open(mat, "w", newline="\r\n") as f:
        f.write("Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n  Tint: [%g, %g, %g, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [0, 0, 0]\n  EmissionIntensity: 1\n  AlphaCutoff: 0\n  Filter: Nearest\n" % (h, tex, tint[0], tint[1], tint[2], tiling, tiling))
    return h


class Slice(city.City):
    def box(self, parent, mat, x0, x1, y0, y1, z0, z1, tag=None, shadows=True, collider=True):
        """A box from z0 to z1 (Hydro's Box grows up from its origin; the collider is centred)."""
        tag = tag or self._n("Block")
        h = z1 - z0
        body = ("    MeshComponent:\n      Shape: Box\n      Size: [%g, %g, %g]\n      CastShadows: %s\n      Materials:\n"
                "        - MaterialHandle: %d\n" % (x1 - x0, y1 - y0, h, "true" if shadows else "false", mat))
        if collider:
            body += ("    BoxColliderComponent:\n      Offset: [0, 0, %g]\n      Size: [%g, %g, %g]\n      Layer: World\n"
                     "      IsTrigger: false\n" % (h / 2, x1 - x0, y1 - y0, h))
        self.w.entity("box:" + tag, tag, parent=parent, t=((x0 + x1) / 2, (y0 + y1) / 2, z0), body=body)

    def terrace(self, parent, wall, cap, x0, x1, y0, y1, top, base=0.0):
        """A raised terrace: a stone wall box up to just under `top` and a paving / grass cap on it."""
        self.box(parent, wall, x0, x1, y0, y1, base, top - CAP, tag=self._n("Terrace Wall"))
        self.box(parent, cap, x0, x1, y0, y1, top - CAP, top, tag=self._n("Terrace Top"), shadows=False)

    def stairs(self, parent, mat, cx, y0, y1, z0, z1, width=STAIR_W, step_rise=0.2, riser=None):
        steps = max(1, int(round((z1 - z0) / step_rise)))
        if riser is not None:
            # a darker strip on each riser, so the steps read from the camera (same texture on treads and risers
            # otherwise makes the flight look like a ramp)
            depth, rise = (y1 - y0) / steps, (z1 - z0) / steps
            for i in range(steps):
                tag = self._n("Riser")
                self.w.entity("riser:" + tag, tag, parent=parent, t=(cx, y0 + i * depth - 0.015, z0 + i * rise), body=(
                    "    MeshComponent:\n      Shape: Box\n      Size: [%g, 0.03, %g]\n      CastShadows: false\n      Materials:\n"
                    "        - MaterialHandle: %d\n") % (width - 0.02, rise * 0.9, riser))
        tag = self._n("Stairs")
        self.w.entity("stairs:" + tag, tag, parent=parent, t=(cx, (y0 + y1) / 2, z0), body=(
            "    MeshComponent:\n      Shape: Stairs\n      Size: [%g, %g, %g]\n      Steps: %d\n      CastShadows: true\n"
            "      Materials:\n        - MaterialHandle: %d\n    MeshColliderComponent:\n      Layer: World\n")
            % (width, y1 - y0, z1 - z0, steps, mat))

    def blocker(self, parent, x0, x1, y0, y1, z0=0.0, h=3.0):
        """An invisible wall: keeps the player where the camera can see her."""
        tag = self._n("Blocker")
        self.w.entity("block:" + tag, tag, parent=parent, t=((x0 + x1) / 2, (y0 + y1) / 2, z0), body=(
            "    BoxColliderComponent:\n      Offset: [0, 0, %g]\n      Size: [%g, %g, %g]\n      Layer: World\n      IsTrigger: false\n"
            % (h / 2, x1 - x0, y1 - y0, h)))

    def zone(self, parent, name, cx, cy, size, priority=0, tilt=0.0, yaw=0.0, fov=0.0, scale=1.0, lead=3.0, rise=2.2,
             pan_min=(0.0, 0.0), pan_max=(0.0, 0.0), blend=0.8):
        """A camera zone (CameraZoneScript): its framing while the player stands in it."""
        self.w.entity("zone:" + name, "Camera Zone " + name, parent=parent, t=(cx, cy, 0), body=(
            "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraZoneScript\n          Properties:\n"
            "            Size: [%g, %g]\n            Priority: %d\n            TiltDegrees: %g\n            YawDegrees: %g\n"
            "            FieldOfView: %g\n            ViewScale: %g\n            FocusLead: %g\n            FocusRise: %g\n"
            "            PanMin: [%g, %g]\n            PanMax: [%g, %g]\n            BlendSeconds: %g\n")
            % (size[0], size[1], priority, tilt, yaw, fov, scale, lead, rise, pan_min[0], pan_min[1], pan_max[0], pan_max[1], blend))

    def shopfront(self, parent, name, x, y, z, rot=0.0, lanterns=True, sort_lantern=3):
        """A building facing the street with its noren, eave lanterns and lit windows (the kit's door / eave heights)."""
        info = self.building(parent, name, x, y, z, rot)
        d = self.d
        if rot != 0.0 or info is None:
            return info
        dx = info.get("door_x")
        dx = dx[0] if isinstance(dx, list) and dx else dx
        if dx is not None and info.get("head_z"):
            d.sprite(parent, "JP_Noren_Inn_A", x + dx, y + 0.3, z + info["head_z"] - 0.05, hanging=True, sort=1)
        if lanterns and info.get("pent_z"):
            for s_ in (-1, 1):
                lx = x + (dx or 0.0) + s_ * 1.4
                d.sprite(parent, "JP_Lantern_Paper_Red_A", lx, y - 0.65, z + info["pent_z"] - 0.05, hanging=True, sort=sort_lantern)
                d.lantern(parent, lx, y - 0.65, z + info["pent_z"] - 0.6, 1.0, 3.5)
        w = info.get("width", 6.0)
        n = max(1, int(w / 3))
        for k in range(n):
            d.window(parent, x - w / 2 + (k + 0.5) * w / n, y - 0.6, z + 1.5, 2.8)
            d.window(parent, x - w / 2 + (k + 0.5) * w / n, y - 0.4, z + 4.6, 2.4)
        return info


def build(c, args):
    d = c.d
    setts = r2_material("JP_Paving_Setts.png", "JP_Paving_Setts")
    flag = r2_material("JP_Paving_Flagstone.png", "JP_Paving_Flagstone")
    ishi = r2_material("JP_Stone_IshigakiSmall.png", "JP_Stone_IshigakiSmall")
    granite = city.ground_material("JP_Stone_Granite")
    riser = tinted_material("JP_Stone_Granite", "JP_Stone_Granite_Riser", (0.42, 0.4, 0.38))
    grass = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Grass.hymat"))
    road = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "VillageDirt.hymat"))
    sky = kit.handle_of(os.path.join(ASSETS, "Materials", "Backdrops", "SunsetBackdrop.hymat"))
    half = STAIR_W / 2
    gap = half + CHEEK                                  # 3.5: the stair corridor's half width

    # ---------------------------------------------------------------- ground, terraces, stairs
    G = c.group("Ground")
    c.slab(G, setts, -44, 44, -20, 1.5, Z0, 2.0)                                 # merchant street and its near side
    c.slab(G, setts, -44, 44, 1.5, 8.0, Z0, 2.0)                                 # under the shops (hidden)
    # landing level: gardens either side of the stair corridor, the landing between the flights
    c.terrace(G, ishi, grass, -44, -gap, 8.0, 17.5, Z1)
    c.terrace(G, ishi, grass, gap, 44, 8.0, 17.5, Z1)
    c.terrace(G, ishi, flag, -gap, gap, 8.0, 11.0, Z1)
    # gate plaza level: its front wall rises behind the gardens; a flagstone path runs from the stairs to the gate
    c.terrace(G, ishi, setts, -44, -gap, 17.5, 46.0, Z2, Z1)
    c.terrace(G, ishi, setts, gap, 44, 17.5, 46.0, Z2, Z1)
    c.terrace(G, ishi, setts, -gap, gap, 17.5, 46.0, Z2, Z1)
    c.box(G, flag, -half, half, 17.5, 33.0, Z2, Z2 + 0.02, tag="Plaza Path", shadows=False, collider=False)
    # outside the city wall: meadow and the road out of the gate, up to the backdrop
    c.slab(G, grass, -70, 70, 46.0, 110.0, Z2, 4.0)
    c.slab(G, road, -2.6, 2.6, 34.0, 110.0, Z2 + 0.02, 0.5)
    c.slab(G, grass, -70, -44, -20, 46.0, Z0, 2.0)
    c.slab(G, grass, 44, 70, -20, 46.0, Z0, 2.0)
    c.w.entity("Backdrop", "Backdrop Sunset", parent=G, t=(0, 108, -20), body=(
        "    MeshComponent:\n      Shape: Box\n      Size: [260, 0.2, 80]\n      CastShadows: false\n      Materials:\n"
        "        - MaterialHandle: %d\n" % sky))

    S = c.group("Stair Street")
    # flight 1 from the street to the landing, flight 2 from the landing to the plaza, stone cheek walls along both
    c.stairs(S, granite, 0.0, 1.0, 8.0, Z0, Z1, riser=riser)
    c.stairs(S, granite, 0.0, 11.0, 17.5, Z1, Z2, riser=riser)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_ * half, s_ * gap))
        c.box(S, ishi, x0, x1, 1.0, 8.0, Z0, Z1 + 0.5, tag=c._n("Cheek Wall"))
        c.box(S, granite, x0, x1, 1.0, 8.0, Z1 + 0.5, Z1 + 0.62, tag=c._n("Cheek Coping"), shadows=False)
        c.box(S, ishi, x0, x1, 11.0, 17.5, Z1, Z2 + 0.5, tag=c._n("Cheek Wall"))
        c.box(S, granite, x0, x1, 11.0, 17.5, Z2 + 0.5, Z2 + 0.62, tag=c._n("Cheek Coping"), shadows=False)
        # stone lanterns at the foot and the head of each flight
        for (ly, lz) in ((0.4, Z0), (8.4, Z1), (10.6, Z1), (18.0, Z2)):
            c.model(S, "Props/JP_Prop_StoneLantern_A", s_ * (gap + 0.7), ly, lz)
            d.lantern(S, s_ * (gap + 0.7), ly, lz + 1.15, 0.9, 3.0)
        c.model(S, "Modules/JP_Railing_Stone_2m", s_ * (gap + 1.2), 17.9, Z2)

    # ---------------------------------------------------------------- merchant street
    M = c.group("Merchant Street")
    left = (("JP_Mid_Townhouse_A", -6.5), ("JP_Mid_Townhouse_B", -13.5), ("JP_Mid_Townhouse_C", -20.5), ("JP_Mid_Shop_A", -26.5),
            ("JP_Mid_Townhouse_A", -32.5), ("JP_Mid_Townhouse_C", -38.5))
    right = (("JP_Mid_Townhouse_C", 6.5), ("JP_Mid_Shop_A", 12.5), ("JP_Mid_Townhouse_B", 19.5), ("JP_Mid_Townhouse_A", 26.5),
             ("JP_Mid_Townhouse_B", 33.5), ("JP_Mid_Townhouse_C", 40.5))
    for name, x in left + right:
        c.shopfront(M, name, x, 1.0, Z0)
    shop = c.summary.get("JP_Mid_Shop_A", {})
    if shop.get("pent_z"):
        for x in (-26.5, 12.5):
            d.sprite(M, "JP_Sign_Hanging_A", x - 1.9, 0.1, Z0 + shop["pent_z"] - 0.1, hanging=True, sort=3)
    # along the shopfronts: goods, water barrels, a signboard, lamp posts
    for (name, x, y, r) in (("JP_Prop_SakeBarrelStack_A", -16.6, 0.2, 0), ("JP_Prop_RainBarrel_A", -10.0, 0.3, 0),
                            ("JP_Prop_Buckets_A", -23.4, 0.2, 0), ("JP_Prop_Crate_A", 9.8, 0.2, 6), ("JP_Prop_Crate_A", 10.4, -0.3, -8),
                            ("JP_Prop_WaterJar_A", 16.0, 0.3, 0), ("JP_Prop_Signboard_A", 23.0, -0.4, 0), ("JP_Prop_TeaBench_A", 29.5, -0.5, 0),
                            ("JP_Prop_Barrel_A", -29.6, 0.2, 0), ("JP_Prop_Barrel_A", -29.0, -0.4, 0)):
        c.model(M, "Props/" + name, x, y, Z0, r)
    d.sprite(M, "JP_Signboard_Standing_A", -4.6, -0.2, Z0)
    for x in (-17.0, 16.0, -34.5, 35.5):
        c.model(M, "Props/JP_Prop_LampPost_A", x, -0.6, Z0)
        d.lantern(M, x + 0.62, -0.6, Z0 + 2.36)
    # lantern strings across the street
    for x in (-12.0, 12.0, -28.0, 28.0):
        d.sprite(M, "JP_Lantern_String_A" if x < 0 else "JP_Lantern_String_B", x, -2.6, Z0 + 4.4, hanging=True, sort=4)
        d.lantern(M, x, -2.6, Z0 + 3.6, 0.8, 3.0)

    # the near side: low things only, so the street stays in view
    N = c.group("Near Side")
    c.model(N, "Props/JP_Prop_MarketStall_A", -11.5, -9.6, Z0, 180)
    c.model(N, "Props/JP_Prop_MarketStall_B", 11.0, -9.8, Z0, 180)
    c.model(N, "Props/JP_Prop_MarketStall_A", 25.5, -9.4, Z0, 180)
    for x in range(-30, 31, 2):
        if -16 <= x <= -8 or 7 <= x <= 15 or 22 <= x <= 29 or -4 <= x <= 3:
            continue
        c.model(N, "Props/JP_Prop_FenceBamboo_2m_A", x + 1, -10.8, Z0)
    for (name, x, y, r) in (("JP_Prop_StoneLantern_A", -4.6, -9.0, 0), ("JP_Prop_StoneLantern_A", 4.4, -9.0, 0),
                            ("JP_Prop_Barrel_A", -6.6, -9.4, 0), ("JP_Prop_Barrel_A", 6.2, -9.6, 0), ("JP_Prop_Buckets_A", 18.0, -9.6, 0),
                            ("JP_Prop_Handcart_A", -20.5, -9.2, 15), ("JP_Prop_WaterJar_A", 30.6, -9.4, 0), ("JP_Prop_Crate_A", -25.0, -9.6, 10)):
        c.model(N, "Props/" + name, x, y, Z0, r)
    for x in (-4.6, 4.4):
        d.lantern(N, x, -9.0, Z0 + 1.15, 0.9, 3.0)
    for (name, x, y, folder) in (("SakuraWeeping", -27.0, -12.5, TREES), ("SakuraWeeping", 21.0, -13.0, TREES),
                                 ("FlowerBush02", -2.0, -11.0, PLANTS), ("FlowerBush03", 1.6, -11.2, PLANTS),
                                 ("BushDense01", -17.0, -11.4, PLANTS), ("BushDense02", 15.8, -11.4, PLANTS)):
        c.plant(N, name, x, y, Z0, folder)

    # ---------------------------------------------------------------- gardens on the landing level
    L = c.group("Gardens")
    for (name, x, y) in (("SakuraWeeping", -9.0, 13.5), ("SakuraWeeping", 10.0, 13.0), ("PineTall", -16.0, 15.5),
                         ("LindenAutumnA", 17.5, 15.0), ("OakGnarledSmall", 5.6, 15.8), ("LindenAutumnC", -24.0, 14.5)):
        c.plant(L, name, x, y, Z1, TREES)
    for (name, x, y) in (("BushDense03", -5.4, 9.6), ("BushDense04", 5.6, 9.6), ("FlowerBush04", -7.0, 16.6), ("FlowerBush05", 7.6, 16.6)):
        c.plant(L, name, x, y, Z1)
    for x in range(-30, 30, 2):
        if -6 <= x <= 4:
            continue
        c.model(L, "Props/JP_Prop_FenceBamboo_2m_A", x + 1, 8.6, Z1)

    # ---------------------------------------------------------------- gate plaza
    P = c.group("Gate Plaza")
    c.building(P, "JP_Rich_CityGate_A", 0.0, 33.0, Z2)
    c.building(P, "JP_Rich_CityGate_A_DoorL", -2.0, 34.05, Z2, -70)
    c.building(P, "JP_Rich_CityGate_A_DoorR", 2.0, 34.05, Z2, 70)
    for x in (-11.0, 11.0, -19.0, 19.0, -27.0, 27.0, -35.0, 35.0, -43.0, 43.0):
        c.building(P, "JP_Rich_CityWall_8m_A", x, 33.0, Z2)
    # the inn (left) and the guild hall (right) turned towards the plaza
    inn = c.building(P, "JP_Rich_LanternInn_A", -12.0, 25.0, Z2, 90.0)
    hall = c.building(P, "JP_Rich_GuildHall_A", 12.0, 25.0, Z2, -90.0)
    for (b, x, s_) in ((inn, -12.0, 1), (hall, 12.0, -1)):
        if b is None:
            continue
        head, pent = b.get("head_z", 2.6), b.get("pent_z", 3.5)
        d.sprite(P, "JP_Noren_Inn_A" if s_ > 0 else "JP_Noren_Inn_Long_A", x + s_ * 0.3, 25.0, Z2 + head - 0.05, hanging=True, sort=1)
        for k in (-1, 1):
            d.sprite(P, "JP_Lantern_Paper_Red_A", x + s_ * 0.75, 25.0 + k * 1.5, Z2 + pent - 0.05, hanging=True, sort=3)
            d.lantern(P, x + s_ * 0.75, 25.0 + k * 1.5, Z2 + pent - 0.6, 1.0, 3.5)
            d.sprite(P, "JP_Lantern_String_A" if k < 0 else "JP_Lantern_String_B", x + s_ * 1.05, 25.0 + k * 4.2, Z2 + pent - 0.1, hanging=True, sort=3)
        for k in range(4):
            d.window(P, x + s_ * 0.6, 19.5 + k * 3.6, Z2 + 1.6)
            d.window(P, x + s_ * 0.4, 19.5 + k * 3.6, Z2 + 4.8, 2.4)
    # the gate: banners, braziers, wall lanterns, spear rack, drum, barriers, guards
    for s_ in (-1, 1):
        c.model(P, "Props/JP_Prop_BannerPole_A", s_ * 3.6, 31.2, Z2)
        d.sprite(P, "JP_Banner_Nobori_Cloth_A", s_ * 3.6 + 0.45, 31.12, Z2 + 3.3, hanging=True, sort=2)
        c.model(P, "Props/JP_Prop_Brazier_A", s_ * 2.7, 32.0, Z2)
        d.lantern(P, s_ * 2.7, 32.0, Z2 + 1.2, 1.6, 4.5, "Fire")
        c.model(P, "Props/JP_Prop_WallLantern_A", s_ * 2.25, 33.9, Z2)
        d.lantern(P, s_ * 2.25, 33.45, Z2 + 2.2)
        c.model(P, "Props/JP_Prop_Barrier_A", s_ * 6.4, 30.6, Z2, s_ * 6)
        c.npc(P, "Blacksmith", s_ * 1.6, 31.4, Z2)
    c.model(P, "Props/JP_Prop_SpearRack_A", -6.0, 32.2, Z2)
    c.model(P, "Props/JP_Prop_WarDrum_A", 6.4, 32.0, Z2)
    # plaza dressing: well and tea bench near the corners, stone lanterns along the path, cherry and pine
    for (name, x, y, r) in (("JP_Prop_Well_A", -7.6, 20.2, 0), ("JP_Prop_TeaBench_A", 7.4, 20.0, 0), ("JP_Prop_SakeBarrelStack_A", -9.8, 29.8, 90),
                            ("JP_Prop_Crate_A", 9.8, 30.0, 0), ("JP_Prop_Barrel_A", 10.0, 29.2, 0)):
        c.model(P, "Props/" + name, x, y, Z2, r)
    for y in (22.0, 27.0):
        for s_ in (-1, 1):
            c.model(P, "Props/JP_Prop_StoneLantern_A", s_ * 3.4, y, Z2)
            d.lantern(P, s_ * 3.4, y, Z2 + 1.15, 0.9, 3.0)
    for (name, x, y, folder) in (("SakuraWeeping", -9.0, 18.4, TREES), ("PineTall", 9.4, 18.6, TREES),
                                 ("TopiaryA", -5.6, 19.0, PLANTS), ("TopiaryB", 5.6, 19.0, PLANTS)):
        c.plant(P, name, x, y, Z2, folder)
    # beyond the wall: the pagoda, forest
    c.building(P, "JP_Rich_Pagoda_A", -26.0, 42.0, Z2)
    for (name, x, y) in (("FirTallA", -12.0, 44.0), ("FirTallB", -4.0, 47.0), ("FirTallC", 7.0, 45.0), ("FirTallD", 15.0, 48.0),
                         ("PineTall", 24.0, 44.0), ("FirTallB", 32.0, 47.0), ("FirTallC", -36.0, 46.0), ("PineTall", 2.0, 52.0)):
        c.plant(P, name, x, y, Z2, TREES)

    # ---------------------------------------------------------------- townsfolk
    T = c.group("Townsfolk")
    for (name, x, y, z) in (("Pilgrim", -9.0, -3.4, Z0), ("Barmaid", 8.6, -2.0, Z0), ("Farmer", 17.0, -5.2, Z0), ("Gatherer", -18.0, -4.6, Z0),
                            ("FarmWife", -12.0, -7.4, Z0), ("Monk", 1.4, 5.0, Z0 + (5.0 - 1.0) / 7.0 * Z1), ("VillageElder", -1.6, 9.6, Z1),
                            ("Gatherer", -4.6, 23.0, Z2), ("Pilgrim", 5.2, 24.4, Z2)):
        c.npc(T, name, x, y, z)

    # ---------------------------------------------------------------- invisible walls: only where the camera sees her
    B = c.group("Blockers")
    c.blocker(B, -32, 32, -9.0, -8.0)                       # the near side of the street (stalls, fence)
    c.blocker(B, -33, -32, -9.0, 1.0)                       # the street's ends
    c.blocker(B, 32, 33, -9.0, 1.0)
    c.blocker(B, -12.5, -11.5, 17.5, 33.0, Z2)              # the plaza's sides: the inn and the guild hall
    c.blocker(B, 11.5, 12.5, 17.5, 33.0, Z2)
    c.blocker(B, -3.0, 3.0, 34.6, 35.6, Z2)                 # the gate: not through it yet
    c.blocker(B, -4.2, -3.5, 8.0, 11.0, Z1)                 # the landing: the gardens are only to look at
    c.blocker(B, 3.5, 4.2, 8.0, 11.0, Z1)

    # ---------------------------------------------------------------- camera zones
    Zg = c.group("Camera Zones")
    # the street: framed along its length, the camera slides with her but stays on the street's line
    c.zone(Zg, "Street", 0.0, -3.5, (66.0, 10.0), tilt=72, fov=30, scale=0.85, lead=0.5, rise=1.4,
           pan_min=(-20.0, -1.2), pan_max=(20.0, -0.6), blend=0.8)
    # the stairs: pulled back and higher, aiming up the flight so the landing and the next level show
    c.zone(Zg, "Stairs", 0.0, 9.25, (7.0, 17.5), priority=1, tilt=67, fov=30, scale=0.85, lead=3.5, rise=2.8,
           pan_min=(0.0, -6.0), pan_max=(0.0, 8.5), blend=1.0)
    # the plaza: centred on the gate, a little closer
    c.zone(Zg, "Plaza", 0.0, 25.5, (26.0, 16.5), tilt=70, yaw=args.plaza_yaw, fov=30, scale=1.0, lead=2.0, rise=2.0,
           pan_min=(-5.0, -5.0), pan_max=(5.0, -2.0), blend=0.9)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"])
    ap.add_argument("--start", default="street", choices=["street", "stairs", "landing", "plaza"])
    ap.add_argument("--plaza-yaw", type=float, default=0.0, help="turn the plaza's camera zone (degrees), to try a yawed view")
    args = ap.parse_args()
    summary = json.load(open(kit.SUMMARY))
    profiles = {k: kit.handle_of(os.path.join(ASSETS, "RenderProfiles", "Field", k + ".hyrender")) for k in ("Day", "Dusk", "Night")}
    scene_handle = kit.handle_of(SCENE) if os.path.exists(SCENE) else kit.stable_id("scene")
    c = Slice(summary)
    w = c.w
    starts = {"street": (0.0, -4.0, Z0), "stairs": (0.0, 4.0, 2.2), "landing": (0.0, 9.5, Z1 + 0.05), "plaza": (0.0, 21.0, Z2 + 0.05)}
    start = starts[args.start]
    w.entity("Player", "Player", t=start, s=(2.5, 2.34375, 3), body=kit.character_body(kit.MIKO_TEXTURE, kit.MIKO_ANIM, "IdleUp", "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 14.0, start[2] + 10.0), r=(0.977, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 30\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 300\n      OrthographicSize: 13\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [60, 60]\n            LookAhead: 0.5\n            Smoothing: 6\n"))
    w.entity("Director", "City Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: JapaneseCitySlice\n            TiltDegrees: 70\n            FieldOfView: 30\n"
        "            FocusLead: 2\n            FocusRise: 1.8\n            BoundsMin: [-24, -6]\n            BoundsMax: [24, 26]\n"
        "            YawDegrees: 0\n            ViewScale: 1\n"
        "            StartPreset: %d\n            DayProfile: %d\n            DuskProfile: %d\n            NightProfile: %d\n")
        % (("Day", "Dusk", "Night").index(args.profile), profiles["Day"], profiles["Dusk"], profiles["Night"]))
    w.entity("Lighting", "Lighting")
    w.entity("Sun", "Sun", parent="Lighting", t=(-8, -6, 10), r=(1.01229095, 0, -0.663224995), body=(
        "    DirectionalLightComponent:\n      Color: [1, 0.86, 0.7]\n      Intensity: 0.8\n      IsMainLight: true\n      CastShadows: true\n"
        "      ShadowStrength: 0.85\n      ShadowDistance: 45\n      ShadowBias: 1\n"))
    build(c, args)
    header = (
        "Handle: %d\nScene: JapaneseCitySlice\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
        "    SpriteSorting: ViewDepth\n    Lighting: Forward\n    DefaultSpriteAlpha: Cutout\n    Meshes: true\n    Shadows: Sun\n"
        "    SpriteShadows: AsDrawn\n    UprightSprites: Vertical\n    HDR: true\n    Bloom: true\n    DepthOfField: true\n"
        "Look:\n  ProfileHandle: %d\n  Environment: Profile\n  PostProcess: Profile\n"
        "Environment:\n  SkyAmbient: [0.42, 0.36, 0.3]\n  GroundAmbient: [0.16, 0.12, 0.08]\n  Fog: true\n  FogColor: [0.78, 0.6, 0.38]\n"
        "  FogStart: 35\n  FogEnd: 110\nEntities:\n") % (scene_handle, profiles[args.profile])
    with open(SCENE, "w", newline="\r\n") as f:
        f.write(header + "\n".join(w.lines) + "\n")
    print("wrote %s: %d entities, start %s, profile %s" % (SCENE, sum(1 for l in w.lines if l.startswith("  - Entity:")), args.start, args.profile))
    if c.d.missing:
        print("missing (skipped):", sorted(set(c.d.missing)))


if __name__ == "__main__":
    main()
