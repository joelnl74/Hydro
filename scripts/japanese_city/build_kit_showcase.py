"""Writes Oxygen/project/Assets/Scenes/JapaneseCityKit_Showcase.hyscene: the Japanese city kit's buildings in three
rows (A rich incl. the city gate, B middle, C poor incl. the harbour sheds) on a flat floor, with Miko (playable) and a
placeholder villager at the start of every row for scale. Not a city layout: that is a later session
(JP_CITY_SCENE_PLAN.md).

The kit builder (Models/JapaneseCity/Source/JP_Kit/_scripts/build_jp_kit.py) exports one glb per building and writes
kit_summary.json with each building's place in the lineup; this script reads it. Re-running rewrites the scene (it is
owned by this script: don't edit it in the editor, change this file). Entity ids and model handles are derived from the
names, so they stay the same from run to run; a glb's .hymeta is only written when it is missing (the importer adds the
Images / Materials lists to it on first load, which must survive).

    python scripts/japanese_city/build_kit_showcase.py [--profile Day|Dusk|Night]
Close the scene in Oxygen first. Check: Oxygen --scene JapaneseCityKit_Showcase --play --frames 120 --capture out.png
"""

import argparse
import hashlib
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ASSETS = os.path.join(REPO, "Oxygen", "project", "Assets")
MODELS = os.path.join(ASSETS, "Models", "JapaneseCity")
SUMMARY = os.path.join(MODELS, "Source", "JP_Kit", "kit_summary.json")
SCENE = os.path.join(ASSETS, "Scenes", "JapaneseCityKit_Showcase.hyscene")
SPRITES = os.path.join(ASSETS, "Sprites", "Props", "JapaneseCity")
PROPS_DIR = os.path.join(MODELS, "Props")

# existing assets the showcase reuses (handles from their .hymeta / .hyanim)
MIKO_TEXTURE, MIKO_ANIM = 5407845820862445765, 4798671707330513403
SMITH_TEXTURE, SMITH_ANIM = 4053719590916208210, 8079240564453792839


KEY = "JP_Showcase:"     # entity ids are derived from KEY + name (build_city_scene.py sets its own)


def stable_id(name):
    return int(hashlib.sha256((KEY + name).encode()).hexdigest()[:16], 16) & 0x7FFFFFFFFFFFFFFF


def handle_of(path):
    """The asset handle: the first "Handle:" line of a .hymeta / .hyanim / .hymat / .hyscene (comments may come first)."""
    with open(path) as f:
        for line in f:
            if line.startswith("Handle:"):
                return int(line.split()[1])
    raise ValueError("%s has no handle" % path)


def ensure_mesh_meta(glb):
    meta = glb + ".hymeta"
    if os.path.exists(meta):
        return handle_of(meta)
    h = int(hashlib.sha256(("JP_Showcase:model:" + os.path.basename(glb)).encode()).hexdigest()[:16], 16) & 0x7FFFFFFFFFFFFFFF
    with open(meta, "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Mesh\nMeshImport:\n  Scale: 1\n  ConvertYUpToZUp: true\n  GenerateNormals: true\n" % h)
    return h


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return int.from_bytes(head[16:20], "big"), int.from_bytes(head[20:24], "big")


def f3(v):
    return "[%s]" % ", ".join(("%g" % x) for x in v)


class Writer:
    def __init__(self):
        self.lines = []

    def entity(self, name, tag, parent=None, t=(0, 0, 0), r=(0, 0, 0), s=(1, 1, 1), body=""):
        eid = stable_id(name)
        out = ["  - Entity: %d" % eid, "    TagComponent:", "      Tag: %s" % tag]
        if parent:
            out += ["    RelationshipComponent:", "      Parent: %d" % stable_id(parent)]
        out += ["    TransformComponent:", "      Translation: %s" % f3(t), "      Rotation: %s" % f3(r), "      Scale: %s" % f3(s)]
        self.lines += out
        if body:
            self.lines += body.rstrip("\n").split("\n")
        return eid


def mesh_body(handle, collider=True):
    b = ("    MeshComponent:\n      ModelHandle: %d\n      Shape: Box\n      Size: [1, 1, 1]\n      CastShadows: true\n"
         "      Materials:\n        []\n") % handle
    if collider:
        b += "    MeshColliderComponent:\n      Layer: World\n"
    return b


def character_body(texture, anim, clip, sprite, player=False):
    b = ("    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: %d\n"
         "      TextureHandle: %d\n      Sprite: %s\n      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n"
         "      Facing: Upright\n"
         "    SpriteAnimatorComponent:\n      AnimationsHandle: %d\n      StartClip: %s\n      PlayOnStart: true\n      Speed: 1\n"
         "    Shadow2DComponent:\n      Offset: [0, -0.04]\n      Size: [0.95, 0.6]\n      Strength: 0.6\n") % (
        96 if player else 0, texture, sprite, anim, clip)
    if player:
        b += ("    CharacterBodyComponent:\n      Radius: 0.25\n      Height: 1.4\n      MaxSlopeDegrees: 50\n      StepHeight: 0.35\n"
              "      StaysOnLedges: true\n      Layer: Player\n"
              "    NativeScriptComponent:\n      Scripts:\n        - Name: DioramaWalkScript\n          Properties:\n"
              "            RunMultiplier: 1.7\n            m_Speed: 3.5\n")
    else:
        b += ("    BoxColliderComponent:\n      Offset: [0, 0.04, 0.8]\n      Size: [0.32, 0.08, 1.6]\n      Layer: Npc\n"
              "      IsTrigger: false\n")
    return b


LANTERN = "    Light2DComponent:\n      Type: Point\n      Color: [1, 0.62, 0.28]\n      Intensity: %g\n      Radius: %g\n      Height: %g\n"
WINDOW = "    Light2DComponent:\n      Type: Point\n      Color: [1, 0.66, 0.32]\n      Intensity: 1.1\n      Radius: %g\n      Height: %g\n"


class Dresser:
    """Places the kit's 3D props, sprite cards and lights around the buildings (Phase 5 dressing)."""

    def __init__(self, w, summary):
        self.w, self.summary, self.n = w, summary, 0
        self.missing = []

    def _name(self, base):
        self.n += 1
        return "%s %d" % (base, self.n)

    def prop(self, parent, name, x, y, rot=0.0, collider=True, z=0.0):
        glb = os.path.join(PROPS_DIR, name + ".glb")
        if os.path.exists(glb) is False:
            self.missing.append(name); return None
        h = ensure_mesh_meta(glb)
        tag = self._name(name.replace("JP_Prop_", "").replace("_A", ""))
        self.w.entity("prop:" + tag, tag, parent=parent, t=(x, y, z), r=(0, 0, math.radians(rot)), body=mesh_body(h, collider))
        return self.summary["_props"].get(name, {})

    def sprite(self, parent, name, x, y, z, hanging=False, sort=0, folder=None, ppm=32.0):
        """A camera-facing card at 32 px/m; z = its foot, or with hanging=True its top."""
        png = os.path.join(folder or SPRITES, name + ".png")
        if os.path.exists(png) is False:
            self.missing.append(name); return
        pw, ph = png_size(png)
        sx, sy = pw / ppm, ph / ppm
        if os.path.exists(png + ".hymeta") is False:
            self.missing.append(name + " (no .hymeta)"); return
        h = handle_of(png + ".hymeta")
        tag = self._name(name.replace("JP_", ""))
        self.w.entity("sprite:" + tag, tag, parent=parent, t=(x, y, z - sy if hanging else z), s=(sx, sy, 1), body=(
            "    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: %d\n      TextureHandle: %d\n"
            "      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n") % (sort, h))

    def lantern(self, parent, x, y, z, intensity=1.2, radius=4.5, kind="Lantern"):
        tag = self._name("Light %s" % kind)
        self.w.entity("light:" + tag, tag, parent=parent, t=(x, y, 0), body=LANTERN % (intensity, radius, z))

    def window(self, parent, x, y, z=1.6, radius=3.2):
        tag = self._name("Light Window")
        self.w.entity("light:" + tag, tag, parent=parent, t=(x, y, 0), body=WINDOW % (radius, z))


def dress(d, summary):
    """Props, cards and lights per building, relative to each building's origin (front wall centre, front -Y)."""
    def at(name):
        x, y = summary[name]["location"]
        return x, y

    def front_lights(group, name, info, every=4.0):
        x, y = at(name)
        n = max(1, int(info["width"] / every))
        for i in range(n):
            d.window(group, x - info["width"] / 2 + (i + 0.5) * info["width"] / n, y - 0.7)

    A, B, C = "Row A Rich", "Row B Middle", "Row C Poor"
    # --- the city gate: banners, braziers, drum, spear racks, barriers, guards' lanterns
    gx, gy = at("JP_Rich_CityGate_A")
    for s in (-1, 1):
        d.prop(A, "JP_Prop_BannerPole_A", gx + s * 3.4, gy - 1.6)
        d.sprite(A, "JP_Banner_Nobori_Cloth_A", gx + s * 3.4 + 0.45, gy - 1.68, 3.3, hanging=True, sort=2)
        d.prop(A, "JP_Prop_Brazier_A", gx + s * 2.6, gy - 0.9)
        d.lantern(A, gx + s * 2.6, gy - 0.9, 1.2, 1.6, 4.0, "Fire")
        d.prop(A, "JP_Prop_Barrier_A", gx + s * 5.6, gy - 2.6)
        d.prop(A, "JP_Prop_WallLantern_A", gx + s * 2.25, gy + 0.9)
        d.lantern(A, gx + s * 2.25, gy + 0.45, 2.2)
    d.prop(A, "JP_Prop_SpearRack_A", gx - 5.0, gy - 0.7)
    d.sprite(A, "JP_Spears_Leaning_A", gx + 5.4, gy - 0.4, 0.0)
    bx, by = at("JP_Rich_GuardBooth_A")
    d.prop(A, "JP_Prop_WarDrum_A", bx - 1.9, by - 0.6, 0)
    d.lantern(A, bx, by - 0.2, 2.1, 1.0, 3.0)
    # --- the Lantern Inn
    for name, long_noren in (("JP_Rich_LanternInn_A", False), ("JP_Rich_GuildHall_A", True), ("JP_Rich_Residence_A", False)):
        info = summary[name]
        x, y = at(name)
        dx = x + (info.get("door_x") or 0.0)
        head = info.get("head_z") or 2.6
        d.sprite(A, "JP_Noren_Inn_Long_A" if long_noren else "JP_Noren_Inn_A", dx, y + 0.3, head - 0.05, hanging=True, sort=1)
        pz = info.get("pent_z")
        if pz:
            for s in (-1, 1):
                d.sprite(A, "JP_Lantern_Paper_Red_A", dx + s * 1.5, y - 0.75, pz - 0.05, hanging=True, sort=3)
                d.lantern(A, dx + s * 1.5, y - 0.75, pz - 0.6, 1.0, 3.5)
        for s in (-1, 1):
            d.prop(A, "JP_Prop_StoneLantern_A", dx + s * 3.2, y - 2.0)
            d.lantern(A, dx + s * 3.2, y - 2.0, 1.15, 0.8, 2.5)
        front_lights(A, name, info)
    ix, iy = at("JP_Rich_LanternInn_A")
    d.prop(A, "JP_Prop_LampPost_A", ix - 6.2, iy - 3.0)
    d.lantern(A, ix - 6.2 + 0.62, iy - 3.0, 2.36)
    d.prop(A, "JP_Prop_LampPost_A", ix + 6.2, iy - 3.0)
    d.lantern(A, ix + 6.2 + 0.62, iy - 3.0, 2.36)
    d.prop(A, "JP_Prop_TeaBench_A", ix + 4.2, iy - 1.2)
    d.prop(A, "JP_Prop_SakeBarrelStack_A", ix - 4.6, iy - 1.0)
    d.prop(A, "JP_Prop_BannerPole_A", ix + 2.4, iy - 2.6)
    d.sprite(A, "JP_Banner_Nobori_Cloth_A", ix + 2.85, iy - 2.68, 3.3, hanging=True, sort=2)
    hx, hy = at("JP_Rich_GuildHall_A")
    d.sprite(A, "JP_Lantern_String_A", hx - 4.0, hy - 0.95, summary["JP_Rich_GuildHall_A"]["pent_z"] - 0.1, hanging=True, sort=3)
    d.sprite(A, "JP_Lantern_String_B", hx + 4.0, hy - 0.95, summary["JP_Rich_GuildHall_A"]["pent_z"] - 0.1, hanging=True, sort=3)
    sx_, sy_ = at("JP_Rich_Storehouse_A")
    d.prop(A, "JP_Prop_Crate_A", sx_ - 1.8, sy_ - 1.0, 10)
    d.prop(A, "JP_Prop_Crate_A", sx_ - 1.5, sy_ - 1.8, -6)
    d.prop(A, "JP_Prop_Barrel_A", sx_ + 1.8, sy_ - 0.9)
    # --- the middle district
    for name in ("JP_Mid_Townhouse_A", "JP_Mid_Townhouse_B", "JP_Mid_Townhouse_C", "JP_Mid_Shop_A"):
        info = summary[name]
        x, y = at(name)
        if info.get("door_x") is not None:
            d.sprite(B, "JP_Noren_Inn_A", x + info["door_x"], y + 0.3, info["head_z"] - 0.05, hanging=True, sort=1)
            d.sprite(B, "JP_Lantern_Paper_Red_A", x + info["door_x"] + 1.3, y - 0.65, info["pent_z"] - 0.05, hanging=True, sort=3)
            d.lantern(B, x + info["door_x"] + 1.3, y - 0.65, info["pent_z"] - 0.6, 1.0, 3.5)
        front_lights(B, name, info)
    tx, ty = at("JP_Mid_Townhouse_A")
    d.prop(B, "JP_Prop_RainBarrel_A", tx - 2.4, ty - 0.7)
    d.sprite(B, "JP_Signboard_Standing_A", tx + 2.1, ty - 1.0, 0.0)
    tx, ty = at("JP_Mid_Townhouse_B")
    d.prop(B, "JP_Prop_Buckets_A", tx - 3.3, ty - 0.6)
    d.prop(B, "JP_Prop_WaterJar_A", tx + 3.3, ty - 0.7)
    tx, ty = at("JP_Mid_Townhouse_C")
    d.prop(B, "JP_Prop_Signboard_A", tx + 1.8, ty - 1.2)
    d.sprite(B, "JP_Signboard_Standing_B", tx - 2.2, ty - 1.0, 0.0)
    tx, ty = at("JP_Mid_Shop_A")
    info = summary["JP_Mid_Shop_A"]
    d.sprite(B, "JP_Sign_Hanging_A", tx - 1.9, ty - 0.9, info["pent_z"] - 0.1, hanging=True, sort=3)
    d.prop(B, "JP_Prop_Handcart_A", tx + 3.6, ty - 2.2, -20)
    d.prop(B, "JP_Prop_Crate_A", tx - 2.4, ty - 0.9, 5)
    wx, wy = at("JP_Mid_FireWatchtower_A")
    d.prop(B, "JP_Prop_FenceBamboo_2m_A", wx + 2.0, wy - 0.5)
    # --- the poor district and the harbour sheds
    px, py = at("JP_Poor_House_A")
    d.prop(C, "JP_Prop_Kamado_A", px - 2.0, py - 1.0)
    d.prop(C, "JP_Prop_WaterJar_A", px + 0.6, py - 0.8)
    px, py = at("JP_Poor_House_B")
    d.prop(C, "JP_Prop_LaundryPole_A", px, py - 2.2)
    d.sprite(C, "JP_Laundry_Cloth_A", px - 0.75, py - 2.25, 1.97, hanging=True, sort=2)
    d.sprite(C, "JP_Laundry_Cloth_A", px + 0.75, py - 2.25, 1.97, hanging=True, sort=2)
    px, py = at("JP_Poor_Hut_A")
    d.prop(C, "JP_Prop_Buckets_A", px - 1.6, py - 0.7)
    px, py = at("JP_Poor_Hut_B")
    d.prop(C, "JP_Prop_Well_A", px + 4.3, py - 2.6)
    d.prop(C, "JP_Prop_FenceBamboo_2m_A", px - 2.5, py - 1.6)
    px, py = at("JP_Poor_RowHouse_A")
    d.prop(C, "JP_Prop_LaundryPole_A", px + 2.0, py - 2.4)
    d.sprite(C, "JP_Laundry_Cloth_A", px + 2.0, py - 2.45, 1.97, hanging=True, sort=2)
    d.prop(C, "JP_Prop_Barrel_A", px - 5.2, py - 0.7)
    d.prop(C, "JP_Prop_Buckets_A", px - 1.0, py - 0.7)
    for i, info_x in enumerate(summary["JP_Poor_RowHouse_A"]["door_x"]):
        if i % 2 == 0:
            d.window(C, px + info_x, py - 0.6, 1.4, 2.6)
    px, py = at("JP_Poor_Boathouse_A")
    d.prop(C, "JP_Prop_Pier_2m_A", px - 2.0, py - 3.2, 0, True, -0.42)
    d.prop(C, "JP_Prop_Pier_2m_A", px + 0.2, py - 3.2, 0, True, -0.42)
    d.prop(C, "JP_Prop_MooringPost_A", px + 2.0, py - 2.4)
    d.prop(C, "JP_Prop_Boat_A", px + 0.8, py - 5.6, 8, True, 0.15)
    d.prop(C, "JP_Prop_LampPost_A", px - 3.6, py - 1.4)
    d.lantern(C, px - 3.6 + 0.62, py - 1.4, 2.36)
    px, py = at("JP_Poor_NetShed_A")
    d.prop(C, "JP_Prop_NetFrame_A", px + 0.3, py - 2.0)
    d.prop(C, "JP_Prop_Crate_A", px - 1.6, py - 0.7, 12)
    px, py = at("JP_Poor_FishStore_A")
    d.prop(C, "JP_Prop_FishRack_A", px, py - 2.2)
    d.sprite(C, "JP_Fish_Hanging_A", px - 0.7, py - 2.45, 1.4, hanging=True, sort=2)
    d.sprite(C, "JP_Fish_Hanging_B", px + 0.7, py - 2.45, 1.4, hanging=True, sort=2)
    d.prop(C, "JP_Prop_LadderBamboo_A", px + 2.0, py - 0.15, 0, False)
    # the props library: every 3D prop in a row in front of row C, so each can be walked around
    names = sorted(summary["_props"])
    for i, n in enumerate(names):
        d.prop("Props Library", n, -42 + i * 3.1, -7.5 - (i % 2) * 2.2, 0, True, -0.42 if "Pier" in n else 0.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"], help="the light at the start (keys 1 / 2 / 3 switch)")
    ap.add_argument("--start", default="C", choices=["A", "B", "C"], help="the row Miko starts in front of (for captures)")
    args = ap.parse_args()
    summary = json.load(open(SUMMARY))
    profiles = {k: handle_of(os.path.join(ASSETS, "RenderProfiles", "Field", k + ".hyrender")) for k in ("Day", "Dusk", "Night")}
    profile = profiles[args.profile]
    scene_handle = handle_of(SCENE) if os.path.exists(SCENE) else stable_id("scene")

    w = Writer()
    rows = {"A": [], "B": [], "C": []}
    for name, info in summary.items():
        if name.startswith("_") or info.get("location") is None:
            continue
        x, y = info["location"]
        tier = info["tier"]
        rows.setdefault(tier if tier in rows else "-", []).append((x, name))

    # player in front of the chosen row, a little left of its middle; the camera follows her with the field
    # template's director (FieldSceneScript: ShrineGlade framing, day / dusk / night on 1 / 2 / 3)
    row_y = {r: summary[min(v)[1]]["location"][1] for r, v in rows.items() if r in "ABC"}
    start = (-6.0, row_y[args.start] - 2.5, 0)
    w.entity("Player", "Player", t=start, s=(2.5, 2.34375, 3), body=character_body(MIKO_TEXTURE, MIKO_ANIM, "IdleDown", "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 12.7, 8.0), r=(1.22173047, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 35\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 260\n      OrthographicSize: 12\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [60, 60]\n            LookAhead: 1.5\n            Smoothing: 8\n"))
    w.entity("Director", "Showcase Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: JapaneseCityKit_Showcase\n            TiltDegrees: 70\n            FieldOfView: 35\n"
        "            FocusLead: 3\n            FocusRise: 2.2\n            BoundsMin: [-58, -8]\n            BoundsMax: [58, 52]\n"
        "            StartPreset: %d\n            DayProfile: %d\n            DuskProfile: %d\n            NightProfile: %d\n")
        % (("Day", "Dusk", "Night").index(args.profile), profiles["Day"], profiles["Dusk"], profiles["Night"]))
    w.entity("Lighting", "Lighting")
    w.entity("Sun", "Sun", parent="Lighting", t=(-8, -6, 10), r=(1.01229095, 0, -0.663224995), body=(
        "    DirectionalLightComponent:\n      Color: [1, 0.86, 0.7]\n      Intensity: 0.8\n      IsMainLight: true\n      CastShadows: true\n"
        "      ShadowStrength: 0.85\n      ShadowDistance: 40\n      ShadowBias: 1\n"))

    # floor + buildings, grouped per row
    floor = summary.get("JP_Showcase_Floor")
    w.entity("Kit", "Kit")
    if floor:
        h = ensure_mesh_meta(os.path.join(MODELS, "JP_Showcase_Floor.glb"))
        fx, fy = floor["location"]
        w.entity("Floor", "Showcase Floor", parent="Kit", t=(fx, fy, 0), body=mesh_body(h, collider=True))
    labels = {"A": "Row A Rich", "B": "Row B Middle", "C": "Row C Poor"}
    for row in ("A", "B", "C"):
        group = labels[row]
        w.entity(group, group, parent="Kit")
        for x, name in sorted(rows[row]):
            info = summary[name]
            h = ensure_mesh_meta(os.path.join(MODELS, name + ".glb"))
            w.entity(name, name, parent=group, t=(x, info["location"][1], 0), body=mesh_body(h))
        # the city gate's door leaves are rows "A" too (their tier) and placed by their own location
        # scale pair at the start of the row: Miko is the player in row C; elsewhere a still Miko card + a villager
        x0, n0 = min(v for v in rows[row] if "_Door" not in v[1])
        sx = x0 - summary[n0]["width"] / 2 - 1.6
        y0 = summary[n0]["location"][1] - 1.5
        if row != "C":
            w.entity("Scale Miko " + row, "Scale Miko " + row, parent=group, t=(sx, y0, 0), s=(2.5, 2.34375, 1),
                     body=character_body(MIKO_TEXTURE, MIKO_ANIM, "IdleDown", "Miko_0"))
        w.entity("Scale Villager " + row, "Scale Villager " + row, parent=group, t=(sx + 1.3, y0, 0), s=(2.5, 2.34375, 1),
                 body=character_body(SMITH_TEXTURE, SMITH_ANIM, "IdleDown", "IdleDown_0"))

    w.entity("Dressing", "Dressing")
    w.entity("Props Library", "Props Library", parent="Kit")
    d = Dresser(w, summary)
    dress(d, summary)
    if d.missing:
        print("missing (skipped):", sorted(set(d.missing)))

    header = (
        "Handle: %d\nScene: JapaneseCityKit_Showcase\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
        "    SpriteSorting: ViewDepth\n    Lighting: Forward\n    DefaultSpriteAlpha: Cutout\n    Meshes: true\n    Shadows: Sun\n"
        "    SpriteShadows: AsDrawn\n    UprightSprites: Vertical\n    HDR: true\n    Bloom: true\n    DepthOfField: true\n"
        "Look:\n  ProfileHandle: %d\n  Environment: Profile\n  PostProcess: Profile\n"
        "Environment:\n  SkyAmbient: [0.42, 0.36, 0.3]\n  GroundAmbient: [0.16, 0.12, 0.08]\n  Fog: true\n  FogColor: [0.78, 0.6, 0.38]\n"
        "  FogStart: 30\n  FogEnd: 90\nEntities:\n") % (scene_handle, profile)
    with open(SCENE, "w", newline="\r\n") as f:
        f.write(header + "\n".join(w.lines) + "\n")
    n = sum(len(v) for k, v in rows.items() if k in "ABC")
    print("wrote %s: %d buildings, profile %s" % (SCENE, n, args.profile))


if __name__ == "__main__":
    main()
