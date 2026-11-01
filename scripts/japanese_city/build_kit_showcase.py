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
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ASSETS = os.path.join(REPO, "Oxygen", "project", "Assets")
MODELS = os.path.join(ASSETS, "Models", "JapaneseCity")
SUMMARY = os.path.join(MODELS, "Source", "JP_Kit", "kit_summary.json")
SCENE = os.path.join(ASSETS, "Scenes", "JapaneseCityKit_Showcase.hyscene")

# existing assets the showcase reuses (handles from their .hymeta / .hyanim)
MIKO_TEXTURE, MIKO_ANIM = 5407845820862445765, 4798671707330513403
SMITH_TEXTURE, SMITH_ANIM = 4053719590916208210, 8079240564453792839


def stable_id(name):
    return int(hashlib.sha256(("JP_Showcase:" + name).encode()).hexdigest()[:16], 16) & 0x7FFFFFFFFFFFFFFF


def handle_of(path):
    with open(path) as f:
        first = f.readline()
    return int(first.split()[1])


def ensure_mesh_meta(glb):
    meta = glb + ".hymeta"
    if os.path.exists(meta):
        return handle_of(meta)
    h = stable_id("model:" + os.path.basename(glb))
    with open(meta, "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Mesh\nMeshImport:\n  Scale: 1\n  ConvertYUpToZUp: true\n  GenerateNormals: true\n" % h)
    return h


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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Day", choices=["Day", "Dusk", "Night"], help="the light at the start (keys 1 / 2 / 3 switch)")
    ap.add_argument("--start", default="C", choices=["A", "B", "C"], help="the row Miko starts in front of (for captures)")
    args = ap.parse_args()
    summary = json.load(open(SUMMARY))
    profiles = {k: handle_of(os.path.join(ASSETS, "RenderProfiles", "Field", k + ".hyrender")) for k in ("Day", "Dusk", "Night")}
    profile = profiles[args.profile]
    scene_handle = handle_of(SCENE) if os.path.exists(SCENE) else stable_id("scene")

    w = Writer()
    rows = {"A": [], "B": [], "C": []}
    for name, info in summary.items():
        if info.get("location") is None:
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
