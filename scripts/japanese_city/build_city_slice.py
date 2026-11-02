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
import random
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
R3 = os.path.join(REPO, "art_incoming", "jp_city", "textures_r3")          # PixelLab batch 1 (Nov 2), seams closed
STEP12 = os.path.join(REPO, "art_incoming", "jp_city", "step12_prepared")   # prep_step12.py's output (decals, backdrops)
CITY_PROFILES = os.path.join(ASSETS, "RenderProfiles", "City")
TREES, PLANTS = city.TREES, city.PLANTS

Z0, Z1, Z2 = 0.0, 3.5, 7.0          # street, landing and gardens, gate plaza
STAIR_W, CHEEK = 5.0, 1.0           # flight width, stone cheek wall either side
CAP = 0.12                          # paving / grass cap on top of a terrace's wall box


GOODS = os.path.join(REPO, "art_incoming", "jp_city", "goods")       # the delivered goods / shop sprites (Nov 2)


PROPS_R1 = os.path.join(REPO, "art_incoming", "jp_city", "props_r1")  # cut from the four prop sheets (Nov 2)


def tree_bed(c, parent, stone, earth, x, y, z, w, d, h=0.45, rnd=None):
    """A raised planting bed round a tree's roots where it stands on paving (Nov 2: trees on bare stone looked dropped
    there): a low stone curb, w x d metres and h tall, with earth inside a little below its top, and a few ground plants
    in the corners. Returns the earth's height (stand the tree on it)."""
    t = 0.28
    for (x0, x1, y0, y1) in ((x - w / 2, x + w / 2, y - d / 2, y - d / 2 + t), (x - w / 2, x + w / 2, y + d / 2 - t, y + d / 2),
                             (x - w / 2, x - w / 2 + t, y - d / 2 + t, y + d / 2 - t), (x + w / 2 - t, x + w / 2, y - d / 2 + t, y + d / 2 - t)):
        c.box(parent, stone, x0, x1, y0, y1, z, z + h, tag=c._n("Tree Bed Curb"))
    top = z + h - 0.07
    c.box(parent, earth, x - w / 2 + t, x + w / 2 - t, y - d / 2 + t, y + d / 2 - t, z, top, tag=c._n("Tree Bed Earth"), shadows=False)
    rnd = rnd or random.Random(int(x * 7 + y * 13))
    for (cx, cy) in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        if rnd.random() < 0.75:
            name = "JP_Shrub_" + rnd.choice(("Mondo", "Fern", "MossStone", "Sasa", "Mondo_V2", "Fern_V2"))
            c.plant(parent, name, x + cx * (w / 2 - t - 0.35), y + cy * (d / 2 - t - 0.3), top)
    return top


def prop_card(c, parent, name, x, y, z, scale=1.0, sort=1, shadow=0.45, hanging=False, tag=None):
    """A trade-district sprite (props_r1/<name>.png: outline recoloured, halved to 32 px/m) standing upright at
    (x, y) with its foot at z (hanging=True: its top), `scale` times its 32 px/m size, a soft contact shadow under it.
    `tag` names it for the field director ("Sign <row>": examinable)."""
    src = os.path.join(PROPS_R1, name + ".png")
    if os.path.exists(src) is False:
        c.d.missing.append(name)
        return
    os.makedirs(kit.SPRITES, exist_ok=True)
    png = os.path.join(kit.SPRITES, name + ".png")
    shutil.copy2(src, png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % city.stable_handle("sprite:" + name))
    pw, ph = kit.png_size(png)
    sx, sy = pw / 32.0 * scale, ph / 32.0 * scale
    tag = tag or c._n(name.replace("JP_", ""))
    if hanging:
        z -= sy
    body = ("    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: %d\n      TextureHandle: %d\n"
            "      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n") % (sort, kit.handle_of(png + ".hymeta"))
    if shadow:
        body += ("    Shadow2DComponent:\n      Offset: [0, -0.02]\n      Size: [%g, %g]\n      Strength: %g\n"
                 % (round(sx * 0.85, 2), round(min(0.5, sx * 0.35), 2), shadow))
    c.w.entity("card:" + tag, tag, parent=parent, t=(x, y, z), s=(sx, sy, 1), body=body)


# the open shops' trades, left to right along the street (Nov 2): goods on the raised floor either side of the
# merchant (sprites, or a 3D piece), and one display on the street in front of the shop. Each trade has its noren
# colour (crest only, no text: indigo cloth, brown pottery, green tea and sake, white paper) and its merchant's row
# in Npcs.hytable ("Npc <row>": the field director's talk); the sake shop hangs a sugidama (cedar ball) by its eave.
SHOP_TRADES = [
    dict(floor=(("JP_Goods_ClothStacks", -1.45), ("JP_Goods_Furoshiki", 1.8)), front=("card", "JP_Display_Umbrellas"),
         noren="JP_Noren_Cloth_A", merchant="ClothMerchant"),
    dict(floor=(("JP_Goods_JarsRow", -1.45), ("JP_Goods_BowlTray", 1.8)), front=("model", "JP_Prop_StoragePots_A"),
         noren="JP_Noren_Pottery_A", merchant="PotteryMerchant"),
    dict(floor=(("model:JP_Prop_TeaChests_A", -1.45), ("JP_Goods_SakeTray", 1.8)), front=("model", "JP_Prop_SakeBarrelStraw_A"),
         noren="JP_Noren_Tea_A", merchant="SakeMerchant", sugidama=True),
    dict(floor=(("JP_Goods_PaperBundles", -1.45), ("JP_Goods_DriedGoodsBoxes", 1.8)), front=("card", "JP_Display_HatStack"),
         noren="JP_Noren_Paper_A", merchant="PaperMerchant"),
]


def goods_sprite(name):
    """A delivered goods sprite copied into Sprites/Props/JapaneseCity with a sprite .hymeta (32 px/m, clamped)."""
    src = os.path.join(GOODS, name + ".png")
    if os.path.exists(src) is False:
        return False
    os.makedirs(kit.SPRITES, exist_ok=True)
    png = os.path.join(kit.SPRITES, name + ".png")
    shutil.copy2(src, png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % city.stable_handle("sprite:" + name))
    return True


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


def r3_material(name, tiling=(0.25, 0.25), tint=(1, 1, 1), mat_name=None):
    """A material on a batch-1 texture (art_incoming/jp_city/textures_r3/<name>.png, copied to Textures/JapaneseCity).
    Tiling is per box-face metre: (0.5, 5.7) shows the image once over 2 m across and 0.175 m up (a step riser)."""
    os.makedirs(city.TEX_DIR, exist_ok=True)
    os.makedirs(city.MAT_DIR, exist_ok=True)
    png = os.path.join(city.TEX_DIR, name + ".png")
    shutil.copy2(os.path.join(R3, name + ".png"), png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Repeat\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % city.stable_handle("tex:" + name))
    tex = kit.handle_of(png + ".hymeta")
    mname = mat_name or name
    mat = os.path.join(city.MAT_DIR, mname + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else city.stable_handle("mat:" + mname)
    with open(mat, "w", newline="\r\n") as f:
        f.write("Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n  Tint: [%g, %g, %g, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [0, 0, 0]\n  EmissionIntensity: 1\n  AlphaCutoff: 0\n  Filter: Nearest\n"
                % (h, tex, tint[0], tint[1], tint[2], tiling[0], tiling[1]))
    return h


# the step risers: the delivered step block, its full height on each 0.175 m riser (lighter top edge, darker foot)
RISER_TILING = (0.5, 1.0 / 0.175)


def tinted_material(texture, name, tint, tiling=0.25):
    """A second material on a texture already in Textures/JapaneseCity, darkened by `tint` (step risers)."""
    tex = kit.handle_of(os.path.join(city.TEX_DIR, texture + ".png.hymeta"))
    mat = os.path.join(city.MAT_DIR, name + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else city.stable_handle("mat:" + name)
    with open(mat, "w", newline="\r\n") as f:
        f.write("Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n  Tint: [%g, %g, %g, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [0, 0, 0]\n  EmissionIntensity: 1\n  AlphaCutoff: 0\n  Filter: Nearest\n" % (h, tex, tint[0], tint[1], tint[2], tiling, tiling))
    return h


def step_material(name, w_px, h_px, cutoff=0.5, emission=None, tint=(1, 1, 1), wrap="Repeat"):
    """A material on a prepared step 1 / 2 image (STEP12/<name>.png, copied to Textures/JapaneseCity). Tiling makes
    one image cover w_px / 32 by h_px / 32 metres of a box face; emission makes a backdrop light itself."""
    os.makedirs(city.TEX_DIR, exist_ok=True)
    os.makedirs(city.MAT_DIR, exist_ok=True)
    png = os.path.join(city.TEX_DIR, name + ".png")
    shutil.copy2(os.path.join(STEP12, name + ".png"), png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: %s\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % (city.stable_handle("tex:" + name), wrap))
    tex = kit.handle_of(png + ".hymeta")
    mat = os.path.join(city.MAT_DIR, name + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else city.stable_handle("mat:" + name)
    em = ("  EmissiveHandle: %d\n" % tex) if emission else ""
    ec = emission or (0, 0, 0)
    with open(mat, "w", newline="\r\n") as f:
        f.write("Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n%s  Tint: [%g, %g, %g, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [%g, %g, %g]\n  EmissionIntensity: 1\n  AlphaCutoff: %g\n  Filter: Nearest\n"
                % (h, tex, em, tint[0], tint[1], tint[2], 32.0 / w_px, 32.0 / h_px, ec[0], ec[1], ec[2], cutoff))
    return h, tex


def city_profiles():
    """RenderProfiles/City/<Day|Dusk|Night>.hyrender: the field profiles made warmer for the city, with the fog pushed
    back (the backdrop layers sit 70 to 140 m behind the street) and a stronger depth of field and bloom."""
    os.makedirs(CITY_PROFILES, exist_ok=True)
    tweaks = {
        "Day": dict(SkyAmbient="[0.6, 0.6, 0.62]", GroundAmbient="[0.22, 0.2, 0.17]", FogColor="[0.74, 0.76, 0.8]",
                    BloomIntensity="0.5", BloomThreshold="1"),
        "Dusk": dict(SkyAmbient="[0.27, 0.2, 0.2]", GroundAmbient="[0.1, 0.07, 0.055]", FogColor="[0.72, 0.48, 0.4]",
                     BloomIntensity="0.85", BloomThreshold="0.9"),
        "Night": dict(SkyAmbient="[0.05, 0.06, 0.1]", GroundAmbient="[0.02, 0.018, 0.025]", FogColor="[0.09, 0.1, 0.18]",
                      BloomIntensity="1.0", BloomThreshold="0.85"),
    }
    # (Nov 2: fog from 30 m for air between the street and the gardens; the near band blurs like the far one)
    common = dict(FogStart="30", FogEnd="260", DofNearStart="1", DofFarStart="9", DofRange="9", DofMaxRadius="0.9")
    handles = {}
    for k, extra in tweaks.items():
        src = open(os.path.join(ASSETS, "RenderProfiles", "Field", k + ".hyrender")).read().splitlines()
        dst = os.path.join(CITY_PROFILES, k + ".hyrender")
        h = kit.handle_of(dst) if os.path.exists(dst) else city.stable_handle("profile:" + k)
        vals = dict(common, **extra)
        out = []
        for line in src:
            key = line.strip().split(":")[0]
            if line.startswith("Handle:"):
                line = "Handle: %d" % h
            elif key in vals:
                line = line[: line.index(key)] + key + ": " + vals[key]
            out.append(line)
        with open(dst, "w", newline="\r\n") as f:
            f.write("\n".join(out) + "\n")
        handles[k] = h
    return handles


class Slice(city.City):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.routes = {}        # route key -> its waypoints' positions (route_walker starts on one)
        self.shop_x = []        # the open shops' centres along the street, left to right (cloth, pottery, sake, paper)

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

    def decal(self, parent, mat, cx, cy, w, h, z, rot=0.0, tag=None):
        """A flat decal: a 1 cm box of w x h metres lying at z (its image's top towards the box's +Y; rot turns it)."""
        tag = tag or self._n("Decal")
        self.w.entity("decal:" + tag, tag, parent=parent, t=(cx, cy, z), r=(0, 0, math.radians(rot)), body=(
            "    MeshComponent:\n      Shape: Box\n      Size: [%g, %g, 0.01]\n      CastShadows: false\n      Materials:\n"
            "        - MaterialHandle: %d\n" % (w, h, mat)))

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

    def stairs_along_wall(self, parent, mat, x_bottom, x_top, yc, z0, z1, width=2.0, step_rise=0.2, riser=None):
        """A flight that climbs sideways along a wall (left-right on screen) from x_bottom up to x_top, x_top < x_bottom
        (the Stairs shape climbs along its local +Y; turned +90 degrees it climbs along -X)."""
        steps = max(1, int(round((z1 - z0) / step_rise)))
        run = x_bottom - x_top
        depth, rise = run / steps, (z1 - z0) / steps
        if riser is not None:
            for i in range(steps):
                tag = self._n("Riser")
                self.w.entity("riser:" + tag, tag, parent=parent, t=(x_bottom - i * depth + 0.015, yc, z0 + i * rise),
                              r=(0, 0, math.radians(90.0)), body=(
                    "    MeshComponent:\n      Shape: Box\n      Size: [%g, 0.03, %g]\n      CastShadows: false\n      Materials:\n"
                    "        - MaterialHandle: %d\n") % (width - 0.02, rise * 0.9, riser))
        tag = self._n("Stairs")
        self.w.entity("stairs:" + tag, tag, parent=parent, t=((x_bottom + x_top) / 2, yc, z0), r=(0, 0, math.radians(90.0)), body=(
            "    MeshComponent:\n      Shape: Stairs\n      Size: [%g, %g, %g]\n      Steps: %d\n      CastShadows: true\n"
            "      Materials:\n        - MaterialHandle: %d\n    MeshColliderComponent:\n      Layer: World\n")
            % (width, run, z1 - z0, steps, mat))

    def walker(self, parent, name, x, y, z, axis="X", length=10.0, speed=1.1, clip="IdleDown", work=None, delay=0.0,
               pause=(2.0, 6.0), stop=0.5, loop=False):
        """A townsperson who walks (NpcWanderScript): back and forth along `axis` through (x, y), `length` m in all,
        pausing at the ends; work="Work" plays the sheet's Work clip at every other stop (the monk's sweeping)."""
        base = os.path.join(ASSETS, "Characters", "NPC", name, name)
        if os.path.exists(base + ".png.hymeta") is False or os.path.exists(base + ".hyanim") is False:
            self.d.missing.append("NPC " + name); return
        tex, anim = kit.handle_of(base + ".png.hymeta"), kit.handle_of(base + ".hyanim")
        tag = self._n("Npc " + name)
        body = kit.character_body(tex, anim, clip, clip + "_0") + (
            "    NativeScriptComponent:\n      Scripts:\n        - Name: NpcWanderScript\n          Properties:\n"
            "            PathLength: %g\n            Axis: %s\n            Speed: %g\n            PauseMin: %g\n            PauseMax: %g\n"
            "            StopChance: %g\n            WorkClip: %s\n            WorkEvery: %d\n            StartDelay: %g\n            Loop: %s\n"
            "            Player: %d\n            Clearance: 0.9\n            Patience: 3\n"
            % (length, axis, speed, pause[0], pause[1], stop, work or '""', 2 if work else 1, delay, "true" if loop else "false",
               kit.stable_id("Player")))
        self.w.entity("npc:" + tag, tag, parent=parent, t=(x, y, z), s=(2.5, 2.34375, 1), body=body)

    def route(self, parent, name, points):
        """A route for townsfolk (NpcRouteScript): an entity "Route <name>" whose children are its waypoints
        (WaypointScript), in walking order. A point is (x, y, z), or dict(at=(x, y, z), wait=(min, max), chance=1,
        facing="Up", action="Work"): stand there wait seconds (with that chance; else walk on), facing that way,
        playing Action<Facing> or Idle<Facing>. Returns the route's key for route_walker."""
        key = "route:" + name
        self.w.entity(key, "Route " + name, parent=parent)
        for k, p in enumerate(points):
            p = p if isinstance(p, dict) else dict(at=p)
            wmin, wmax = p.get("wait", (0.0, 0.0))
            self.w.entity("%s:%d" % (key, k), "Waypoint %d" % (k + 1), parent=key, t=p["at"], body=(
                "    NativeScriptComponent:\n      Scripts:\n        - Name: WaypointScript\n          Properties:\n"
                "            WaitMin: %g\n            WaitMax: %g\n            StopChance: %g\n            Facing: %s\n"
                "            Action: %s\n" % (wmin, wmax, p.get("chance", 1.0), p.get("facing") or '""', p.get("action") or '""')))
        self.routes[key] = [(p if isinstance(p, dict) else dict(at=p))["at"] for p in points]
        return key

    def route_walker(self, parent, sheet, route, mode="Loop", speed=1.1, start=0, delay=0.0):
        """A townsperson going round `route` (a key from route()), placed on its waypoint `start`."""
        base = os.path.join(ASSETS, "Characters", "NPC", sheet, sheet)
        if os.path.exists(base + ".png.hymeta") is False or os.path.exists(base + ".hyanim") is False:
            self.d.missing.append("NPC " + sheet); return
        tex, anim = kit.handle_of(base + ".png.hymeta"), kit.handle_of(base + ".hyanim")
        tag = self._n("Npc " + sheet)
        body = kit.character_body(tex, anim, "IdleDown", "IdleDown_0") + (
            "    NativeScriptComponent:\n      Scripts:\n        - Name: NpcRouteScript\n          Properties:\n"
            "            Route: %d\n            Mode: %s\n            Speed: %g\n            StartAt: %d\n            StartDelay: %g\n"
            "            Player: %d\n            Clearance: 0.9\n            Patience: 3\n"
            % (kit.stable_id(route), mode, speed, start, delay, kit.stable_id("Player")))
        self.w.entity("npc:" + tag, tag, parent=parent, t=self.routes[route][start], s=(2.5, 2.34375, 1), body=body)

    def townsperson(self, parent, sheet, x, y, z, clip="IdleDown", row=None, look=None):
        """A standing NPC: tag "Npc <row>" when it has a talk row (Npcs.hytable), else numbered like npc(); look="Down
        Left Down Right" makes it look around (NpcLookAroundScript, facing the player when she comes close)."""
        base = os.path.join(ASSETS, "Characters", "NPC", sheet, sheet)
        if os.path.exists(base + ".png.hymeta") is False or os.path.exists(base + ".hyanim") is False:
            self.d.missing.append("NPC " + sheet); return
        tex, anim = kit.handle_of(base + ".png.hymeta"), kit.handle_of(base + ".hyanim")
        tag = ("Npc " + row) if row else self._n("Npc " + sheet)
        key = self._n("npc:" + (row or sheet))
        body = kit.character_body(tex, anim, clip, clip + "_0")
        if look:
            body += ("    NativeScriptComponent:\n      Scripts:\n        - Name: NpcLookAroundScript\n          Properties:\n"
                     "            Facings: %s\n            HoldMin: 2.5\n            HoldMax: 6\n            Player: %d\n"
                     "            WatchRange: 2.5\n" % (look, kit.stable_id("Player")))
        self.w.entity(key, tag, parent=parent, t=(x, y, z), s=(2.5, 2.34375, 1), body=body)

    def blocker(self, parent, x0, x1, y0, y1, z0=0.0, h=3.0):
        """An invisible wall: keeps the player where the camera can see her."""
        tag = self._n("Blocker")
        self.w.entity("block:" + tag, tag, parent=parent, t=((x0 + x1) / 2, (y0 + y1) / 2, z0), body=(
            "    BoxColliderComponent:\n      Offset: [0, 0, %g]\n      Size: [%g, %g, %g]\n      Layer: World\n      IsTrigger: false\n"
            % (h / 2, x1 - x0, y1 - y0, h)))

    def bracket_lantern(self, parent, x, y, z, rot=0.0, sort=3):
        """A paper lantern card on a wall bracket (JP_Prop_LanternBracket_A): (x, y, z) is the arm's root on the wall,
        rot turns the bracket (0 = sticking out along -Y)."""
        self.model(parent, "Props/JP_Prop_LanternBracket_A", x, y, z, rot, collider=False)
        info = self.summary.get("_props", {}).get("JP_Prop_LanternBracket_A", {})
        dy, dz = info.get("drop", [-0.53, -0.2])
        a = math.radians(rot)
        lx, ly = x - dy * math.sin(a), y + dy * math.cos(a)
        self.d.sprite(parent, "JP_Lantern_Paper_Red_A", lx, ly, z + dz, hanging=True, sort=sort)
        self.d.lantern(parent, lx, ly, z + dz - 0.5, 1.0, 3.5)

    def lantern_line(self, parent, x0, y, z, spans, every=2, sort=4):
        """Lantern poles every 8 m from x0 with a sagging rope between each pair and paper lantern cards on the rope's
        cords (every `every` cords): the hanging lanterns hang from something."""
        props = self.summary.get("_props", {})
        pole = props.get("JP_Prop_LanternPole_A", {})
        rope = props.get("JP_Prop_LanternRope_8m_A", {})
        span = rope.get("span", 8.0)
        rz = pole.get("rope_z", 4.33)
        for i in range(spans + 1):
            self.model(parent, "Props/JP_Prop_LanternPole_A", x0 + i * span, y, z)
        for i in range(spans):
            cx = x0 + (i + 0.5) * span
            self.model(parent, "Props/JP_Prop_LanternRope_8m_A", cx, y, z + rz, collider=False)
            for k, (dx, dz) in enumerate(rope.get("drops", [])):
                if k % every:
                    continue
                self.d.sprite(parent, "JP_Lantern_Paper_Red_A", cx + dx, y, z + rz + dz, hanging=True, sort=sort)
                if k % (2 * every) == 0:
                    self.d.lantern(parent, cx + dx, y, z + rz + dz - 0.5, 0.9, 3.2)

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
        if dx is not None and info.get("head_z") and info.get("door") != "wood":
            d.sprite(parent, "JP_Noren_Inn_A", x + dx, y + 0.3, z + info["head_z"] - 0.05, hanging=True, sort=1)
        # restrained lanterns (v3): a pair at the open shops, one by a lattice door, none at the plank doors
        if lanterns and info.get("arm_z"):
            if info.get("door") == "shop":
                for s_ in (-1, 1):
                    self.bracket_lantern(parent, x + s_ * 2.3, y - 0.12, z + info["arm_z"], 0.0, sort_lantern)
            elif dx is not None and info.get("door") != "wood":
                self.bracket_lantern(parent, x + dx + 1.45, y - 0.12, z + info["arm_z"], 0.0, sort_lantern)
        w = info.get("width", 6.0)
        n = max(1, int(w / 3))
        for k in range(n):
            d.window(parent, x - w / 2 + (k + 0.5) * w / n, y - 0.6, z + 1.5, 2.8)
            d.window(parent, x - w / 2 + (k + 0.5) * w / n, y - 0.4, z + 4.6, 2.4)
        return info


def backdrop(c, parent):
    """Step 2: three painted layers behind the city (ChatGPT, prepared by prep_step12.py), lit by their own emission
    and hazed by the fog: the upper town's rooftops, the far hills with the castle and pagoda, the dusk sky."""
    layers = (("JP_Backdrop_Mid", 1086, 362, 120.0, 72.0, Z2 - 4.0, (0.62, 0.56, 0.52), (0.45, 0.45, 0.45)),
              ("JP_Backdrop_Far", 1086, 294, 220.0, 105.0, Z2 - 9.0, (0.72, 0.68, 0.7), (0.3, 0.3, 0.3)),
              ("JP_Backdrop_Sky_Dusk", 1086, 362, 380.0, 140.0, -30.0, (0.78, 0.72, 0.7), (0.2, 0.2, 0.2)))   # toned down (Nov 2)
    for (name, wp, hp, width, y, z, emission, tint) in layers:
        height = width * hp / wp
        mat, _ = step_material(name, 32.0 * width, 32.0 * height, 0.5, emission, tint)      # one image per face
        c.w.entity("backdrop:" + name, "Backdrop " + name.replace("JP_Backdrop_", ""), parent=parent, t=(0, y, z), body=(
            "    MeshComponent:\n      Shape: Box\n      Size: [%g, 0.1, %g]\n      CastShadows: false\n      Materials:\n"
            "        - MaterialHandle: %d\n" % (width, height, mat)))


def ground_dressing(c):
    """Step 1: the ground stops being one repeated tile. Worn, mossy setts in patches, dirt, puddles, a damp band at the
    foot of the house fronts, moss at the foot of stone walls, petals under the cherries and maple leaves under the
    autumn trees. Decals are 1 cm boxes in layers (worn 6 mm, dirt 9, puddles 11, grime 13, moss 15, leaves 30)."""
    rnd = random.Random(7)
    D = c.group("Ground Dressing")
    worn = [step_material("JP_Decal_SettsWorn_%d" % k, 128, 128)[0] for k in (1, 2, 3)]
    dirt = step_material("JP_Decal_DirtPatch", 128, 128)[0]
    puddle = step_material("JP_Decal_Puddle", 96, 48)[0]
    grime = step_material("JP_Decal_WallGrime", 128, 32)[0]
    moss = step_material("JP_Decal_MossEdge", 128, 32)[0]
    petals = step_material("JP_Decal_Petals", 128, 128)[0]
    leaves = step_material("JP_Decal_MapleLeaves", 128, 128)[0]

    # worn setts: more towards the house fronts, the street's ends and the near side, none in the middle of the path
    spots = []
    for _ in range(26):
        x = rnd.uniform(-30, 30)
        y = rnd.choice((rnd.uniform(-1.6, -0.9), rnd.uniform(-8.0, -5.2), rnd.uniform(-16.0, -10.5)))
        spots.append((x, y, Z0))
    for _ in range(10):
        spots.append((rnd.uniform(-10.5, 10.5), rnd.uniform(19.0, 31.0), Z2))
    for (x, y, z) in spots:
        c.decal(D, rnd.choice(worn), x, y, 4.0, 4.0, z + 0.006, rnd.choice((0.0, 180.0)))
    for (x, y, z) in [(rnd.uniform(-28, 28), rnd.uniform(-7.5, -5.0), Z0) for _ in range(5)] + \
                     [(rnd.uniform(-28, 28), rnd.uniform(-15, -10.5), Z0) for _ in range(4)] + \
                     [(rnd.choice((-8.0, 8.0)), rnd.uniform(20, 30), Z2) for _ in range(3)]:
        c.decal(D, dirt, x, y, 4.0, 4.0, z + 0.009, rnd.uniform(0, 360))
    for (x, y, z) in ((-14.5, -1.6, Z0), (9.0, -1.7, Z0), (24.0, -1.5, Z0), (-6.0, 21.0, Z2), (7.5, 28.5, Z2)):
        c.decal(D, puddle, x, y, 3.0, 1.5, z + 0.011, rnd.choice((0.0, 180.0)))

    # the damp band along the house fronts (top edge against the plinths), moss in places and at the stair foot
    for (x0, x1) in ((-44.0, -3.5), (3.5, 44.0)):
        c.decal(D, grime, (x0 + x1) / 2, 0.35, x1 - x0, 1.0, Z0 + 0.013, 0.0, tag=c._n("Grime House Fronts"))
        for _ in range(4):
            L = rnd.choice((2.0, 3.0, 4.0))
            x = rnd.uniform(x0 + L / 2, x1 - L / 2)
            c.decal(D, moss, x, 0.35, L, 1.0, Z0 + 0.015)
    for x in (-3.0, 3.0):
        c.decal(D, moss, x, 0.5, 1.4, 1.0, Z0 + 0.015)
    # the plaza: grime at the foot of the inn (its front faces +X) and the guild hall (faces -X), moss along the wall
    c.decal(D, grime, -11.6, 25.0, 15.0, 1.0, Z2 + 0.013, 90.0)
    c.decal(D, grime, 11.6, 25.0, 15.0, 1.0, Z2 + 0.013, -90.0)
    for (x0, x1) in ((-11.5, -3.0), (3.0, 11.5)):
        c.decal(D, moss, (x0 + x1) / 2, 32.5, x1 - x0, 1.0, Z2 + 0.015)

    # petals under the cherries (and blown onto the street and the landing), maple leaves under the autumn trees
    for (x, y, z) in ((-27.0, -12.5, Z0), (21.0, -13.0, Z0), (-9.0, 13.5, Z1), (10.0, 13.0, Z1), (-9.0, 18.4, Z2)):
        c.decal(D, petals, x, y, 8.0, 8.0, z + 0.03)
    c.decal(D, petals, 0.0, -1.6, 8.0, 3.0, Z0 + 0.03)
    c.decal(D, petals, 0.0, 9.5, 7.0, 3.0, Z1 + 0.03)
    for (x, y, z, w_, h_) in ((17.5, 15.0, Z1, 8.0, 5.0), (-24.0, 14.5, Z1, 8.0, 5.0), (12.0, -6.5, Z0, 4.0, 4.0), (-18.0, -4.8, Z0, 4.0, 3.0)):
        c.decal(D, leaves, x, y, w_, h_, z + 0.03)


def foreground_branches(w):
    """Blurred branches in the frame's corners, as in the references: sprites parented to the camera, 6 m in front of
    it (the depth of field blurs them). Flat, so they lie in the camera's own XY plane and face it exactly (a Billboard
    would be turned by the camera's rotation a second time). A camera looks along its local -Z with +Y up the screen."""
    for (name, w_px, h_px, width, local, flip) in (("JP_FG_CherryBranch", 593, 291, 2.4, (-1.75, 1.15, -6.0), False),
                                                   ("JP_FG_MapleBranch", 635, 313, 1.9, (2.2, -1.35, -6.0), True)):
        _, tex = step_material(name, w_px, h_px, wrap="Clamp")
        height = width * h_px / w_px
        w.entity("fg:" + name, "Foreground " + name.replace("JP_FG_", ""), parent="Camera", t=local, s=(width, height, 1), body=(
            "    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: 200\n"
            "      TextureHandle: %d\n      SpriteIndex: 0\n      FlipX: %s\n      FlipY: false\n      Facing: Flat\n      AlphaMode: Cutout\n")
            % (tex, "true" if flip else "false"))


def build(c, args):
    d = c.d
    setts = r2_material("JP_Paving_Setts.png", "JP_Paving_Setts")
    flag = r2_material("JP_Paving_Flagstone.png", "JP_Paving_Flagstone")
    ishi = r2_material("JP_Stone_IshigakiSmall.png", "JP_Stone_IshigakiSmall")
    granite = city.ground_material("JP_Stone_Granite")
    gutter = tinted_material("JP_Stone_Granite", "JP_Stone_Granite_Gutter", (0.3, 0.29, 0.28))
    riser = r3_material("JP_Stone_StepBlock", RISER_TILING, (0.85, 0.85, 0.85), "JP_Stone_StepBlock_Riser")
    grass = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Grass.hymat"))
    road = r3_material("JP_Ground_PackedDirt_V")        # packed earth, its ruts along the road (+Y)
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
    c.terrace(G, ishi, setts, -50, -gap, 17.5, 46.0, Z2, Z1)       # out to x 50, past the wall's ends (47)
    c.terrace(G, ishi, setts, gap, 50, 17.5, 46.0, Z2, Z1)
    c.terrace(G, ishi, setts, -gap, gap, 17.5, 46.0, Z2, Z1)
    c.box(G, flag, -half, half, 17.5, 33.0, Z2, Z2 + 0.02, tag="Plaza Path", shadows=False, collider=False)
    # outside the city wall: meadow and the road out of the gate, up to the backdrop
    c.slab(G, grass, -70, 70, 46.0, 110.0, Z2, 4.0)
    c.slab(G, road, -2.6, 2.6, 34.0, 110.0, Z2 + 0.02, 0.5)
    c.slab(G, grass, -70, -44, -20, 17.5, Z0, 2.0)
    c.slab(G, grass, 44, 70, -20, 17.5, Z0, 2.0)
    c.slab(G, grass, -70, -50, 17.5, 46.0, Z0, 2.0)
    c.slab(G, grass, 50, 70, 17.5, 46.0, Z0, 2.0)
    backdrop(c, G)
    ground_dressing(c)

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

    # ---------------------------------------------------------------- merchant street (kit v3, Nov 1)
    # Two continuous rows of attached machiya (JP_Row_*: shared eave and ridge lines, no gaps, no overlaps) either side
    # of the stair corridor; each row read left to right, placed by its houses' widths. Variety is the facade only:
    # plaster / timber tints, windows, doors, open shops, canopies, udatsu, roof tiles, and what stands in front.
    M = c.group("Merchant Street")
    left_row = ("House_C", "House_B", "Shop_A", "House_A", "House_B", "Shop_B")                 # x -43.5 .. -3.5
    right_row = ("House_A", "Shop_B", "House_C", "House_B", "Shop_A", "House_A")                # x 3.5 .. 41.5
    shops = []

    def place_row(types, x_start, x_end):
        """Houses laid from x_start towards x_end (both rows read left to right along +x)."""
        x = x_start
        n = len(types)
        for i, t in enumerate(types):
            w = c.summary.get("JP_Row_%s_F" % t, {}).get("width", 6.0)
            suffix = "L" if i == 0 else ("R" if i == n - 1 else "M")
            name = "JP_Row_%s_%s" % (t, suffix)
            cx = x + w / 2
            info = c.shopfront(M, name, cx, 1.0, Z0)
            if t.startswith("Shop"):
                shops.append((cx, info))
                c.shop_x.append(cx)
            x += w
        return x
    place_row(left_row, -3.5 - sum(c.summary.get("JP_Row_%s_F" % t, {}).get("width", 6.0) for t in left_row), -3.5)
    place_row(right_row, 3.5, None)
    for cx, info in shops:
        # (Nov 2: the hanging board is gone from the shops; the trade's noren below says what each one sells)
        # the open shops: the merchant inside on the raised floor behind his goods, a warm light in the room
        floor_z = info.get("shop_floor_z") or 0.8
        shop_n = len([1 for (x_, _) in shops if x_ <= cx])
        trade = SHOP_TRADES[(shop_n - 1) % len(SHOP_TRADES)]
        c.townsperson(M, "Merchant", cx + 0.5, 1.0 + 1.0, Z0 + floor_z, "IdleDown", row=trade["merchant"])
        # the trade's short noren across the left bay of the open front (crest only)
        if os.path.exists(os.path.join(PROPS_R1, trade["noren"] + ".png")) and info.get("head_z"):
            prop_card(c, M, trade["noren"], cx - 1.75, 1.0 - 0.15, Z0 + info["head_z"] - 0.02, sort=2, shadow=0, hanging=True)
        if trade.get("sugidama") and info.get("arm_z"):
            prop_card(c, M, "JP_Sugidama_A", cx + 2.85, -0.35, Z0 + info["arm_z"] + 0.1, sort=3, shadow=0, hanging=True)
        d.lantern(M, cx, 1.0 + 1.0, Z0 + 2.2, 1.2, 4.0)
        # the painted back wall (shelves full of goods) just in front of the kit's back wall, and goods along the
        # front edge of the raised floor either side of the merchant
        if goods_sprite("JP_Shop_BackWall_A"):
            d.sprite(M, "JP_Shop_BackWall_A", cx, 1.0 + 2.6 - 0.1, Z0 + floor_z, sort=0)
        # (Nov 2: the sweets, hats, vegetables and the first pottery / fish cards are out: too big, too bright;
        # the trade-district sheets replace them)
        for (gname, gx) in trade["floor"]:
            if gname.startswith("model:"):
                c.model(M, "Props/" + gname[6:], cx + gx, 1.0 + 0.05, Z0 + floor_z, 0, collider=False)
            else:
                prop_card(c, M, gname, cx + gx, 1.0 - 0.1, Z0 + floor_z, scale=0.75, shadow=0.3)
        kind, fname = trade["front"]
        if kind == "model":
            c.model(M, "Props/" + fname, cx - 3.6, 0.15, Z0, 0)
        else:
            prop_card(c, M, fname, cx - 3.6, 0.3, Z0)
    # along the shopfronts: goods, water barrels, a signboard, lamp posts
    # (Nov 2: the trade-district props: rice bales, straw-wrapped sake, covered crates, rolled mats, a bench with a
    # mat are 3D; firewood, baskets, brooms, the plant shelf, persimmons and the basket rack are cards beside them)
    for (name, x, y, r) in (("JP_Prop_SakeBarrelStack_A", -24.4, 0.2, 0), ("JP_Prop_RainBarrel_A", -7.2, 0.3, 0),
                            ("JP_Prop_Buckets_A", -36.6, 0.2, 0), ("JP_Prop_CratesCovered_A", 17.9, 0.0, 4),
                            ("JP_Prop_WaterJar_A", 22.2, 0.3, 0), ("JP_Prop_Signboard_A", 31.0, -0.4, 0), ("JP_Prop_BenchMat_A", 40.5, -0.5, 0),
                            ("JP_Prop_RiceBales_A", -41.3, 0.0, 0), ("JP_Prop_WaterJar_A", -31.5, 0.3, 0),
                            ("JP_Prop_SakeBarrelStraw_A", 36.2, 0.3, 0), ("JP_Prop_RolledMats_A", 32.3, 0.4, 0),
                            ("JP_Prop_LacquerBoxes_A", -30.6, 0.35, 8)):
        c.model(M, "Props/" + name, x, y, Z0, r)
    # the Shrine Glade's 3D broom (Models/Props/MonkBroom) leans by the rain barrel instead of the broom card
    c.model(M, "../Props/MonkBroom", -6.3, 0.35, Z0, 0, collider=False)
    for (name, x, y) in (("JP_Prop_BasketStack", -23.0, 0.3), ("JP_Display_PlantShelf", 23.4, 0.4),
                         ("JP_Prop_Firewood", 37.4, 0.25), ("JP_Display_Persimmons", -35.4, 0.4), ("JP_Display_BasketRack", 19.4, 0.35),
                         ("JP_Display_SandalRack", -40.0, 0.45)):
        prop_card(c, M, name, x, y, Z0)
    d.sprite(M, "JP_Signboard_Standing_A", -4.6, -0.2, Z0)
    for x in (-17.0, 16.0, -34.5, 35.5):
        c.model(M, "Props/JP_Prop_LampPost_A", x, -0.6, Z0)
        d.lantern(M, x + 0.62, -0.6, Z0 + 2.36)
    # a lantern line along the shopfronts: poles every 8 m with a sagging rope, lanterns on its cords
    c.lantern_line(M, -25.5, -1.45, Z0, 7, every=3)
    # the street's floor: setts, a flagstone path down the middle, a stone curb and gutter along the shopfronts
    c.box(M, flag, -34.0, 34.0, -4.1, -2.5, Z0, Z0 + 0.02, tag="Street Path", shadows=False, collider=False)
    c.box(M, flag, -34.0, 34.0, -0.8, -0.15, Z0, Z0 + 0.05, tag="Street Curb", shadows=False, collider=False)
    c.box(M, gutter, -34.0, 34.0, -1.05, -0.8, Z0, Z0 + 0.012, tag="Street Gutter", shadows=False, collider=False)

    # the near side: low things only, so the street stays in view
    N = c.group("Near Side")
    # the stalls face the street (counter towards the camera, as in the references), goods on the counters: jars on
    # the indigo pottery stall, a sake barrel stack and crates by the red produce stall (the goods sprites come later)
    cz = c.summary.get("_props", {}).get("JP_Prop_MarketStall_A", {}).get("counter_z", 0.86)
    c.model(N, "Props/JP_Prop_MarketStall_A", -11.5, -9.6, Z0, 0)
    c.model(N, "Props/JP_Prop_MarketStall_B", 11.0, -9.8, Z0, 0)
    c.model(N, "Props/JP_Prop_MarketStall_A", 25.5, -9.4, Z0, 0)
    # what each stall sells (Nov 2): straw goods, pottery, rice and grain, laid on the counter
    for (gname, sx, sy) in (("JP_StallTop_StrawGoods", -11.5, -9.6), ("JP_StallTop_Pottery", 11.0, -9.8), ("JP_StallTop_RiceGrain", 25.5, -9.4)):
        prop_card(c, N, gname, sx, sy + 0.45, Z0 + cz, sort=2, shadow=0)
    c.model(N, "Props/JP_Prop_CratesCovered_A", -13.5, -8.6, Z0, 8, collider=False)
    prop_card(c, N, "JP_Prop_BasketStack", -9.5, -8.5, Z0)
    c.model(N, "Props/JP_Prop_RiceBales_A", 23.4, -8.6, Z0, -10, collider=False)
    c.model(N, "Props/JP_Prop_Buckets_A", 27.4, -8.4, Z0, 0, collider=False)
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
    # (v3: no trees on the near side; a weeping cherry there hid the whole street. Low bushes only.)
    # (Nov 2: the delivered Japanese garden shrubs, install_jp_trees.py, instead of the field's rose and puff bushes)
    for (name, x, y, folder) in (("JP_Shrub_Azalea_A", -2.0, -11.0, PLANTS), ("JP_Shrub_Camellia", 1.6, -11.2, PLANTS),
                                 ("JP_Shrub_Azalea_B", -27.0, -11.6, PLANTS), ("JP_Shrub_Nandina", 21.0, -11.6, PLANTS),
                                 ("JP_Shrub_Azalea_B_V2", -17.0, -11.4, PLANTS), ("JP_Shrub_Azalea_A_V2", 15.8, -11.4, PLANTS),
                                 ("JP_Shrub_Sasa", -15.9, -11.6, PLANTS), ("JP_Shrub_Sasa_V2", 17.0, -11.7, PLANTS)):
        c.plant(N, name, x, y, Z0, folder)

    # ---------------------------------------------------------------- gardens on the landing level
    L = c.group("Gardens")
    for (name, x, y) in (("SakuraWeeping", -9.0, 13.5), ("SakuraWeeping", 10.0, 13.0), ("PineTall", -16.0, 15.5),
                         ("LindenAutumnA", 17.5, 15.0), ("JP_Tree_BlackPine_Small", 5.6, 15.8), ("LindenAutumnC", -24.0, 14.5)):
        c.plant(L, name, x, y, Z1, TREES)
    for (name, x, y) in (("JP_Shrub_Azalea_B", -5.4, 9.6), ("JP_Shrub_Azalea_B_V2", 5.6, 9.6), ("JP_Shrub_Camellia_V2", -7.0, 16.6),
                         ("JP_Shrub_Nandina_V2", 7.6, 16.6), ("JP_Shrub_Fern", -4.4, 9.9), ("JP_Shrub_MossStone", 4.5, 9.9)):
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
        if s_ > 0:
            d.sprite(P, "JP_Noren_Inn_A", x + s_ * 0.3, 25.0, Z2 + head - 0.05, hanging=True, sort=1)
        else:
            prop_card(c, P, "JP_Noren_Hall_Long_A", x + s_ * 0.3, 25.0, Z2 + head - 0.05, sort=1, shadow=0, hanging=True)
        arm = b.get("arm_z", head + 0.6)
        for k in (-1, 1):
            for off in (1.5, 4.5):
                if s_ > 0 and off == 1.5:
                    continue        # the inn: a pair of big paper lanterns either side of the door instead (below)
                c.bracket_lantern(P, x + s_ * 0.12, 25.0 + k * off, Z2 + arm, 90.0 * s_)
        # what each one is, without text (Nov 2): the inn hangs two big red lanterns by its door and stands a blank
        # board kanban out front; the guild hall hangs a crest board over its door
        if s_ > 0:
            for k in (-1, 1):
                d.sprite(P, "JP_Lantern_Paper_Red_A", x + 0.55, 25.0 + k * 1.75, Z2 + arm + 0.55, hanging=True, sort=3, ppm=22.0)
                d.lantern(P, x + 0.9, 25.0 + k * 1.75, Z2 + arm - 0.3, 1.5, 4.0)
            d.sprite(P, "JP_Signboard_Standing_A", x + 1.4, 22.2, Z2, sort=1)
        else:
            d.sprite(P, "JP_Sign_Hanging_A", x - 0.45, 25.0, Z2 + pent + 0.05, hanging=True, sort=2, ppm=22.0)
        for k in range(4):
            d.window(P, x + s_ * 0.6, 19.5 + k * 3.6, Z2 + 1.6)
            d.window(P, x + s_ * 0.4, 19.5 + k * 3.6, Z2 + 4.8, 2.4)
    # upper-district rooftops behind the plaza wall, left and right of the plaza (scenery: the layer of roofs the
    # references show behind the merchant row; the plaza's side blockers keep the player off them)
    for s_ in (-1, 1):
        x = 19.0
        for t, suf in (("House_A", "L"), ("House_C", "M"), ("House_B", "R")):
            w = c.summary.get("JP_Row_%s_F" % t, {}).get("width", 6.0)
            cx = s_ * (x + w / 2)
            info = c.shopfront(P, "JP_Row_%s_%s" % (t, suf if s_ > 0 else {"L": "R", "R": "L"}.get(suf, suf)), cx, 19.5, Z2, lanterns=False)
            x += w
    # the gate: banners, braziers, wall lanterns, spear rack, drum, barriers, guards
    for s_ in (-1, 1):
        c.model(P, "Props/JP_Prop_BannerPole_A", s_ * 3.6, 31.2, Z2)
        d.sprite(P, "JP_Banner_Nobori_Cloth_A", s_ * 3.6 + 0.45, 31.12, Z2 + 3.3, hanging=True, sort=2)
        c.model(P, "Props/JP_Prop_Brazier_A", s_ * 2.7, 32.0, Z2)
        d.lantern(P, s_ * 2.7, 32.0, Z2 + 1.2, 1.6, 4.5, "Fire")
        c.model(P, "Props/JP_Prop_WallLantern_A", s_ * 2.25, 33.9, Z2)
        d.lantern(P, s_ * 2.25, 33.45, Z2 + 2.2)
        c.model(P, "Props/JP_Prop_Barrier_A", s_ * 6.4, 30.6, Z2, s_ * 6)
        c.townsperson(P, "GateGuard", s_ * 1.6, 31.4, Z2, "IdleDown", row="GateGuard",
                      look="Down Left Down Right" if s_ < 0 else "Down Right Down Left")
    # the wall walk (city side, 4.5 m up): banner poles, spear racks and leaning spears, so the top of the wall is manned
    walk_z, walk_y = Z2 + 4.5, 33.0 + 4.6
    for s_ in (-1, 1):
        for k, x in enumerate((8.6, 16.0, 23.0, 31.0, 39.0)):
            px = s_ * x
            if k % 2 == 0:
                c.model(P, "Props/JP_Prop_BannerPole_A", px, walk_y, walk_z)
                d.sprite(P, "JP_Banner_Nobori_Cloth_A", px + 0.45, walk_y - 0.08, walk_z + 3.3, hanging=True, sort=2)
            else:
                c.model(P, "Props/JP_Prop_SpearRack_A", px, walk_y, walk_z)
        d.sprite(P, "JP_Spears_Leaning_A", s_ * 12.2, walk_y + 0.3, walk_z)
        d.sprite(P, "JP_Spears_Leaning_A", s_ * 27.4, walk_y + 0.3, walk_z)
    # stairs up the wall on the city side (left of the gate), and a chest at the far end of the walk, out of sight from
    # the street: the reward for climbing up. The chest is the old TreasureChest sheet (frame 0 closed); a trigger zone
    # logs when Miko reaches it, ready for the interaction code
    granite_s = city.ground_material("JP_Stone_Granite")
    riser_s = r3_material("JP_Stone_StepBlock", RISER_TILING, (0.85, 0.85, 0.85), "JP_Stone_StepBlock_Riser")
    # gangi-style: the flight runs left-right against the wall's foot (rising to the left, away from the gate), so it
    # doesn't cover the inn; the top tread meets the front edge of the walk (y 33.6)
    stair_s = tinted_material("JP_Stone_Granite", "JP_Stone_Granite_Light", (1.3, 1.24, 1.12))   # lighter than the wall
    c.stairs_along_wall(P, stair_s, -4.4, -11.8, 32.6, Z2, walk_z, width=2.0, riser=riser_s)
    chest_tex = kit.handle_of(os.path.join(ASSETS, "Sprites", "Props", "TreasureChest.png.hymeta"))
    c.w.entity("chest:WallWalk", "Chest Wall Walk", parent=P, t=(-41.0, 37.6, walk_z), s=(1, 1, 1), body=(
        "    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: 0\n      TextureHandle: %d\n"
        "      Sprite: Chest_0\n      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n"
        "    BoxColliderComponent:\n      Offset: [0, 0, 0.35]\n      Size: [0.9, 0.6, 0.7]\n      Layer: World\n      IsTrigger: false\n") % chest_tex)
    c.w.entity("trigger:WallWalkChest", "Trigger Wall Walk Chest", parent=P, t=(-41.0, 36.6, walk_z), body=(
        "    BoxColliderComponent:\n      Offset: [0, 0, 0.8]\n      Size: [2.4, 2.0, 1.6]\n      Layer: World\n      IsTrigger: true\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: TriggerZoneScript\n          Properties:\n            Watched: Player\n"))
    d.lantern(P, -41.0, 37.2, walk_z + 0.6, 0.6, 2.0)
    c.model(P, "Props/JP_Prop_SpearRack_A", 8.8, 32.2, Z2)          # moved off the wall stairs
    c.model(P, "Props/JP_Prop_WarDrum_A", 6.4, 32.0, Z2)
    # plaza dressing: well and tea bench near the corners, stone lanterns along the path, cherry and pine
    for (name, x, y, r) in (("JP_Prop_Well_A", -7.6, 20.2, 0), ("JP_Prop_TeaBench_A", 7.4, 20.0, 0), ("JP_Prop_SakeBarrelStack_A", -8.0, 29.4, 90),
                            ("JP_Prop_Crate_A", 9.8, 30.0, 0), ("JP_Prop_Barrel_A", 10.0, 29.2, 0)):
        c.model(P, "Props/" + name, x, y, Z2, r)
    # (Nov 2, the gate plaza sheet): the noticeboard beside the gate, a pair of paper lanterns under the gatehouse's
    # pent roof, a palanquin parked by the inn, a horse post and trough by the guild hall, the bamboo spout and stone
    # basin at the inn's door
    prop_card(c, P, "JP_Gate_Noticeboard", -5.3, 31.9, Z2, sort=1, tag="Sign CityNotices")
    prop_card(c, P, "JP_Gate_Palanquin", -8.8, 22.6, Z2, sort=1, tag="Sign Palanquin")
    prop_card(c, P, "JP_Gate_HorsePost", 8.5, 23.4, Z2, sort=1, tag="Sign HorsePost")
    prop_card(c, P, "JP_Field_Kakehi", -9.4, 27.6, Z2, sort=1)
    lp = os.path.join(PROPS_R1, "JP_Gate_LanternPair.png")
    if os.path.exists(lp):
        lh = kit.png_size(lp)[1] / 32.0
        prop_card(c, P, "JP_Gate_LanternPair", 0.0, 32.75, Z2 + 4.15 - lh, sort=3, shadow=0)
        d.lantern(P, 0.0, 32.6, Z2 + 3.4, 1.0, 4.0)
    # a jizo by the landing, at the foot of the second flight
    prop_card(c, P, "JP_Field_Jizo_B", -3.0, 10.6, Z1, sort=1)
    for y in (22.0, 27.0):
        for s_ in (-1, 1):
            c.model(P, "Props/JP_Prop_StoneLantern_A", s_ * 3.4, y, Z2)
            d.lantern(P, s_ * 3.4, y, Z2 + 1.15, 0.9, 3.0)
    earth = r3_material("JP_Ground_PackedDirt_B", (0.25, 0.25), (0.86, 0.82, 0.76), "JP_Ground_TempleEarth")
    for (name, x, y, w, dd) in (("SakuraWeeping", -9.0, 18.4, 3.2, 2.4), ("PineTall", 9.4, 18.6, 3.0, 2.4)):   # in stone beds
        c.plant(P, name, x, y, tree_bed(c, P, ishi, earth, x, y, Z2, w, dd), TREES)
        c.model(P, "Props/JP_Prop_BenchMat_A", x, y - dd / 2 - 0.7, Z2)                   # a bench before each bed
    for (name, x, y, folder) in (("JP_Shrub_Azalea_B", -5.6, 19.0, PLANTS), ("JP_Shrub_Azalea_B_V2", 5.6, 19.0, PLANTS)):
        c.plant(P, name, x, y, Z2, folder)
    # beyond the wall: the pagoda, forest
    c.building(P, "JP_Rich_Pagoda_A", -26.0, 42.0, Z2)
    # (Nov 2: Japanese cedars instead of the field's firs, which the farm village keeps)
    for (name, x, y) in (("JP_Tree_Sugi_A_Large", -12.0, 44.0), ("JP_Tree_Sugi_B_Large", -4.0, 47.0), ("JP_Tree_Sugi_A_Large", 7.0, 45.0),
                         ("JP_Tree_Sugi_B_Large", 15.0, 48.0), ("PineTall", 24.0, 44.0), ("JP_Tree_Sugi_A_Large", 32.0, 47.0),
                         ("JP_Tree_Sugi_B_Large", -36.0, 46.0), ("PineTall", 2.0, 52.0)):
        c.plant(P, name, x, y, Z2, TREES)

    # ---------------------------------------------------------------- townsfolk
    T = c.group("Townsfolk")
    # the Edo townsfolk (Nov 1): the merchant at his shop, people in pairs facing each other, the old woman resting by
    # the stairs; the pilgrim and the monk stay (travellers on the way to the shrine)
    # (the two street merchants stand inside their shops now, see the merchant street; a customer at the right one)
    for (name, x, y, z, clip) in (("Townswoman", 13.6, -0.95, Z0, "IdleUp"),
                                  ("Townsman", 17.4, -2.6, Z0, "IdleRight"), ("Townswoman", 18.7, -2.6, Z0, "IdleLeft"),
                                  ("Pilgrim", -9.0, -3.4, Z0, "IdleDown"), ("Townsman", 23.5, -6.4, Z0, "IdleDown"),
                                  ("Monk", 1.4, 5.0, Z0 + (5.0 - 1.0) / 7.0 * Z1, "IdleDown"),
                                  ("Townswoman", -4.6, 23.0, Z2, "IdleRight"), ("Townsman", -3.4, 23.2, Z2, "IdleLeft"),
                                  ("Pilgrim", 5.2, 24.4, Z2, "IdleDown"),
                                  ("TownChild", 3.4, -6.6, Z0, "IdleLeft"),
                                  ("TownChild", -8.4, 20.6, Z2, "IdleRight")):
        c.npc(T, name, x, y, z, clip)
    # the walkers (Nov 2, routes): who walks where, after the layout references: shoppers browse the open shops and
    # the stalls, a merchant stops to talk with the chatting pair, the porter carries his load up the stair street
    # and out of the gate, a traveller reads the noticeboard and rests by the well, the monk sweeps the plaza in
    # stages, the guard patrols the wall walk looking out over the parapet, two old women take their time.
    # (NpcRouteScript + WaypointScript; the lanes keep clear of the standing townsfolk and the props.)
    cloth, pottery, sake, paper = (c.shop_x + [-24.5, -6.5, 12.5, 30.5][len(c.shop_x):])[:4]
    shop_stop = dict(wait=(3.0, 6.0), facing="Up", chance=0.85)
    stall_stop = dict(wait=(2.0, 5.0), facing="Down", chance=0.6)
    west = c.route(T, "ShopperWest", [
        (-30.0, -4.8, Z0),
        dict(shop_stop, at=(cloth - 0.8, -0.6, Z0)),
        (-17.0, -1.2, Z0),
        dict(stall_stop, at=(-10.6, -7.3, Z0)),
        dict(shop_stop, at=(pottery - 0.6, -0.7, Z0)),
        dict(at=(-4.2, -3.0, Z0), wait=(1.0, 3.0), facing="Right", chance=0.3),
        (-14.0, -5.6, Z0),
        dict(at=(-24.0, -6.8, Z0), wait=(2.0, 4.0), facing="Down", chance=0.4)])
    c.route_walker(T, "Townswoman", west, "Loop", 1.0, start=0, delay=1.0)
    east = c.route(T, "ShopperEast", [
        (5.5, -3.2, Z0),
        dict(shop_stop, at=(sake - 1.6, -0.7, Z0), chance=0.9),
        dict(stall_stop, at=(11.0, -7.3, Z0), chance=0.7),
        (21.5, -4.6, Z0),
        dict(shop_stop, at=(paper - 0.6, -0.7, Z0)),
        dict(stall_stop, at=(27.2, -7.4, Z0)),
        (21.0, -4.4, Z0)])
    c.route_walker(T, "Townswoman", east, "Loop", 1.05, start=3, delay=0.5)
    talk = c.route(T, "MerchantTalk", [
        dict(at=(17.6, -3.7, Z0), wait=(4.0, 8.0), facing="Up"),          # with the chatting pair
        dict(at=(31.0, -3.8, Z0), wait=(2.0, 4.0), facing="Down", chance=0.6),
        dict(at=(36.5, -1.0, Z0), wait=(2.0, 4.0), facing="Up")])
    c.route_walker(T, "Merchant", talk, "PingPong", 1.0, start=1, delay=2.0)
    stall = c.route(T, "StallBrowse", [
        dict(at=(-18.0, -6.0, Z0), wait=(4.0, 8.0), facing="Down"),
        dict(at=(-12.6, -7.3, Z0), wait=(5.0, 9.0), facing="Down"),
        dict(at=(-8.6, -5.6, Z0), wait=(3.0, 6.0), facing="Left")])
    c.route_walker(T, "OldWoman", stall, "PingPong", 0.5, start=0, delay=3.0)
    rest = c.route(T, "LandingRest", [
        dict(at=(1.6, 9.2, Z1), wait=(6.0, 12.0), facing="Down"),
        dict(at=(2.6, 10.3, Z1), wait=(5.0, 10.0), facing="Right")])
    c.route_walker(T, "OldWoman", rest, "PingPong", 0.4)
    # the porter: from the street's west end (off screen) up both flights, across the plaza and out of the gate,
    # then again from the west end
    porter = c.route(T, "PorterRounds", [
        (-42.0, -4.4, Z0), (-1.4, -4.0, Z0), (-1.2, 1.0, Z0), (-1.2, 8.0, Z1), (-1.2, 11.0, Z1), (-1.2, 17.5, Z2),
        (-0.6, 30.5, Z2), (0.0, 37.6, Z2)])
    c.route_walker(T, "Porter", porter, "Restart", 1.4)
    reader = c.route(T, "NoticeReader", [
        (1.6, 18.6, Z2),
        (-1.6, 28.6, Z2),
        dict(at=(-5.0, 30.7, Z2), wait=(4.0, 7.0), facing="Up"),         # reading the noticeboard
        dict(at=(-6.6, 21.0, Z2), wait=(3.0, 5.0), facing="Left"),       # by the well
        dict(at=(6.4, 20.8, Z2), wait=(3.0, 6.0), facing="Right", chance=0.7)])   # by the tea bench
    c.route_walker(T, "Merchant", reader, "Loop", 0.9, start=2, delay=4.0)
    sweep = dict(wait=(4.0, 9.0), facing="Down", action="Work")
    sweeper = c.route(T, "PlazaSweep", [dict(sweep, at=(-6.0, 26.0, Z2)), dict(sweep, at=(-2.0, 25.6, Z2), chance=0.7),
                                        dict(sweep, at=(2.0, 25.8, Z2)), dict(sweep, at=(6.0, 26.4, Z2), chance=0.7)])
    c.route_walker(T, "TempleMonk", sweeper, "PingPong", 0.6, delay=0.5)
    lookout = dict(wait=(3.0, 6.0), facing="Up")
    patrol = c.route(T, "WallPatrol", [
        dict(at=(-13.5, 36.2, Z2 + 4.5), wait=(2.0, 4.0), facing="Right"),
        dict(lookout, at=(-21.0, 36.2, Z2 + 4.5), chance=0.6),
        dict(lookout, at=(-30.0, 36.2, Z2 + 4.5), chance=0.4),
        dict(lookout, at=(-38.0, 36.2, Z2 + 4.5)),
        dict(at=(-44.5, 36.2, Z2 + 4.5), wait=(2.0, 4.0), facing="Left")])
    c.route_walker(T, "GateGuard", patrol, "PingPong", 0.8, start=1)

    # ---------------------------------------------------------------- invisible walls: only where the camera sees her
    B = c.group("Blockers")
    c.blocker(B, -32, 32, -9.0, -8.0)                       # the near side of the street (stalls, fence)
    c.blocker(B, -33, -32, -9.0, 1.0)                       # the street's ends
    c.blocker(B, 32, 33, -9.0, 1.0)
    c.blocker(B, -12.5, -11.5, 17.5, 33.0, Z2)              # the plaza's sides: the inn and the guild hall
    c.blocker(B, 11.5, 12.5, 17.5, 33.0, Z2)
    # the gate passage leads out: an "Exit <scene>" zone (FieldDirectorScript: E to leave, fade, scene push) to the
    # countryside hub; the road beyond is blocked a little further so she can't wander off the slice
    c.w.entity("exit:Gate", "Exit JapaneseCountryside", parent=B, t=(0.0, 36.0, Z2), body=(
        "    BoxColliderComponent:\n      Offset: [0, 0, 1]\n      Size: [3.6, 2.4, 2]\n      Layer: Trigger\n      IsTrigger: true\n"))
    c.blocker(B, -3.0, 3.0, 38.6, 39.6, Z2)
    c.w.entity("arrive:countryside", "Arrive JapaneseCountryside", parent=B, t=(0.0, 32.4, Z2))
    # the street's west end leads to the temple district (TempleDistrict.hyscene, build_temple_district.py): the lane
    # goes on west and up the temple hill
    c.w.entity("exit:Temple", "Exit TempleDistrict", parent=B, t=(-31.2, -4.0, Z0), body=(
        "    BoxColliderComponent:\n      Offset: [0, 0, 1]\n      Size: [1.2, 8, 2]\n      Layer: Trigger\n      IsTrigger: true\n"))
    c.w.entity("arrive:temple", "Arrive TempleDistrict", parent=B, t=(-29.0, -4.0, Z0))
    # the way to the temple, so the street's west end reads as a way on: a torii over the street, stone lanterns
    # either side, a standing board that says where it goes ("Sign TempleRoad")
    c.model(B, "../Architecture/ToriiGate", -29.6, -4.2, Z0, 90.0, collider=False)
    for y in (-8.0, -0.6):
        c.model(B, "Props/JP_Prop_StoneLantern_A", -28.4, y, Z0)
        c.d.lantern(B, -28.4, y, Z0 + 1.15, 1.0, 3.4)
    c.d.sprite(B, "JP_Signboard_Standing_A", -27.0, -7.6, Z0, sort=1)
    c.w.entity("sign:TempleRoad", "Sign TempleRoad", parent=B, t=(-27.0, -7.4, Z0))
    c.blocker(B, -47.6, -47.0, 33.0, 39.5, Z2 + 4.5)        # the far end of the wall walk
    # the city-side edge of the walk (open only where the stairs arrive), so she can't step off the wall
    c.blocker(B, -47.0, -11.8, 33.3, 33.6, Z2 + 4.5, 1.5)
    c.blocker(B, -10.6, -7.0, 33.6, 33.9, Z2 + 4.5, 1.5)
    c.blocker(B, -12.2, -11.8, 31.6, 33.6, Z2 + 4.5, 1.5)       # the top end of the wall stairs
    c.blocker(B, -4.2, -3.5, 8.0, 11.0, Z1)                 # the landing: the gardens are only to look at
    c.blocker(B, 3.5, 4.2, 8.0, 11.0, Z1)

    # ---------------------------------------------------------------- camera zones
    Zg = c.group("Camera Zones")
    # the street: framed along its length, the camera slides with her but stays on the street's line
    c.zone(Zg, "Street", 0.0, -3.5, (66.0, 10.0), tilt=args.street_tilt, fov=30, scale=args.street_scale, lead=0.5,
           rise=args.street_rise,
           pan_min=(-20.0, -1.2), pan_max=(20.0, -0.6), blend=1.4)
    # the stairs: pulled back and higher, aiming up the flight so the landing and the next level show
    c.zone(Zg, "Stairs", 0.0, 9.25, (7.0, 17.5), priority=1, tilt=72, fov=30, scale=0.9, lead=3.5, rise=3.2,
           pan_min=(0.0, -6.0), pan_max=(0.0, 8.5), blend=1.6)
    # the wall walk (left of the gate): a little higher, sliding along the wall
    c.zone(Zg, "WallWalk", -27.0, 36.3, (40.0, 6.6), priority=2, tilt=68, fov=30, scale=0.95, lead=1.0, rise=2.0,
           pan_min=(-16.0, -1.0), pan_max=(16.0, 0.0), blend=1.4)
    # the plaza: centred on the gate, a little closer
    c.zone(Zg, "Plaza", 0.0, 25.5, (26.0, 16.5), tilt=77, yaw=args.plaza_yaw, fov=30, scale=1.0, lead=2.0, rise=3.6,
           pan_min=(-5.0, -5.0), pan_max=(5.0, -2.0), blend=1.4)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"])
    ap.add_argument("--start", default="street", choices=["street", "stairs", "landing", "plaza", "wall"])
    ap.add_argument("--street-tilt", type=float, default=77.0)
    ap.add_argument("--street-rise", type=float, default=3.8)
    ap.add_argument("--street-scale", type=float, default=0.95)
    ap.add_argument("--start-pos", default="", help="x,y,z: start the player here instead (overrides --start)")
    ap.add_argument("--fg", action="store_true", help="add the foreground branches over the frame's corners (off since Nov 2:\n"
                    "an overlay on the camera felt wrong)")
    ap.add_argument("--plaza-yaw", type=float, default=0.0, help="turn the plaza's camera zone (degrees), to try a yawed view")
    args = ap.parse_args()
    summary = json.load(open(kit.SUMMARY))
    # the actors' contact shadows 0.6 -> 0.75 (Nov 2; this scene only: patched here, not at import, so the countryside
    # keeps its own)
    character_body = kit.character_body
    kit.character_body = lambda *a, **k: character_body(*a, **k).replace("      Strength: 0.6\n", "      Strength: 0.75\n")
    profiles = city_profiles()
    scene_handle = kit.handle_of(SCENE) if os.path.exists(SCENE) else kit.stable_id("scene")
    c = Slice(summary)
    w = c.w
    starts = {"street": (0.0, -4.0, Z0), "stairs": (0.0, 4.0, 2.2), "landing": (0.0, 9.5, Z1 + 0.05), "plaza": (0.0, 21.0, Z2 + 0.05),
              "wall": (-38.0, 36.2, Z2 + 4.55)}
    start = tuple(float(v) for v in args.start_pos.split(",")) if args.start_pos else starts[args.start]
    w.entity("Player", "Player", t=start, s=(2.5, 2.34375, 3), body=kit.character_body(kit.MIKO_TEXTURE, kit.MIKO_ANIM, "IdleUp", "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 14.0, start[2] + 10.0), r=(0.977, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 30\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 300\n      OrthographicSize: 13\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [60, 60]\n            LookAhead: 0.5\n            Smoothing: 6\n"
        "        - Name: OccluderFadeScript\n          Properties:\n            Target: %d\n            FadedOpacity: 0.3\n            SpriteFadedOpacity: 0.12\n"
        "            FadeSpeed: 4\n            Radius: 0.3\n            HeadHeight: 1.5\n            MaxSize: 30\n" % kit.stable_id("Player")))
    w.entity("Director", "City Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: JapaneseCitySlice\n            TiltDegrees: 70\n            FieldOfView: 30\n"
        "            FocusLead: 2\n            FocusRise: 1.8\n            BoundsMin: [-24, -6]\n            BoundsMax: [24, 26]\n"
        "            YawDegrees: 0\n            ViewScale: 1\n"
        "            DuskSunColor: [1, 0.74, 0.5]\n            DuskSunIntensity: 0.95\n            DuskLanterns: 1.3\n            DuskWindows: 1.2\n"
        "            NightLanterns: 1.6\n            NightWindows: 1.3\n"
        "            StartPreset: %d\n            DayProfile: %d\n            DuskProfile: %d\n            NightProfile: %d\n")
        % (("Day", "Dusk", "Night").index(args.profile), profiles["Day"], profiles["Dusk"], profiles["Night"]))
    w.entity("FieldDirector", "Field Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldDirectorScript\n          Properties:\n"
        "            InteractRange: 1.7\n            MaxReachHeight: 1\n            FacingDegrees: 80\n            CloseRange: 0.6\n"
        "            SwitchMargin: 0.3\n            EyeHeight: 1\n            ItemHeight: 0.6\n            MessageTime: 3\n"
        "            LeaveTime: 0.6\n            ArriveTime: 0.6\n            RestTime: 1.2\n"))
    w.entity("Lighting", "Lighting")
    w.entity("Sun", "Sun", parent="Lighting", t=(-8, -6, 10), r=(1.01229095, 0, -0.663224995), body=(
        "    DirectionalLightComponent:\n      Color: [1, 0.86, 0.7]\n      Intensity: 0.8\n      IsMainLight: true\n      CastShadows: true\n"
        "      ShadowStrength: 0.85\n      ShadowDistance: 45\n      ShadowBias: 1\n"))
    build(c, args)
    if args.fg:
        foreground_branches(w)
    header = (
        "Handle: %d\nScene: JapaneseCitySlice\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
        "    SpriteSorting: ViewDepth\n    Lighting: Forward\n    DefaultSpriteAlpha: Cutout\n    Meshes: true\n    Shadows: Sun\n"
        "    SpriteShadows: AsDrawn\n    UprightSprites: Vertical\n    HDR: true\n    Bloom: true\n    DepthOfField: true\n"
        "Look:\n  ProfileHandle: %d\n  Environment: Profile\n  PostProcess: Profile\n"
        "Environment:\n  SkyAmbient: [0.42, 0.36, 0.3]\n  GroundAmbient: [0.16, 0.12, 0.08]\n  Fog: true\n  FogColor: [0.78, 0.6, 0.38]\n"
        "  FogStart: 30\n  FogEnd: 110\nEntities:\n") % (scene_handle, profiles[args.profile])
    with open(SCENE, "w", newline="\r\n") as f:
        f.write(header + "\n".join(w.lines) + "\n")
    print("wrote %s: %d entities, start %s, profile %s" % (SCENE, sum(1 for l in w.lines if l.startswith("  - Entity:")), args.start, args.profile))
    if c.d.missing:
        print("missing (skipped):", sorted(set(c.d.missing)))


if __name__ == "__main__":
    main()
