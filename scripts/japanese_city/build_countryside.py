"""Writes Oxygen/project/Assets/Scenes/JapaneseCountryside.hyscene: the small Japanese countryside hub outside the
city gate (Nov 1, placeholder art). It only has to connect: the city's gate at its south end ("Exit JapaneseCitySlice")
and the Shrine Glade at its north end ("Exit ShrineGlade"), a dirt road between them through rice paddies with dirt
bunds and irrigation channels, bamboo fences, a few trees, a roadside shrine with a stone lantern, one farmhouse off
the road, a campsite, and two dormant slimes (EnemyChaseScript, the Shrine Glade's) as the first encounters.

    python scripts/japanese_city/build_countryside.py [--profile Day|Dusk|Night] [--start south|north]
Check: Oxygen --scene JapaneseCountryside --play --frames 150 --capture out.png
"""

import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_kit_showcase as kit  # noqa: E402
import build_city_scene as city  # noqa: E402
import build_city_slice as slice_  # noqa: E402

kit.KEY = "JP_Country:"
ASSETS, MODELS = kit.ASSETS, kit.MODELS
SCENE = os.path.join(ASSETS, "Scenes", "JapaneseCountryside.hyscene")
TREES, PLANTS = city.TREES, city.PLANTS
SLIME_TEX, SLIME_ANIM = 3596799573489577125, 2173924343473726646      # the Shrine Glade's slime sheet


def paddy(c, parent, water, dirt, x0, x1, y0, y1):
    """A rice paddy: a shallow water box sunk 0.25 m below the ground with a dirt bund (0.6 m wide) around it, and
    rows of seedling tufts (bush sprites, small) across the water."""
    c.box(parent, dirt, x0 - 0.6, x1 + 0.6, y0 - 0.6, y1 + 0.6, 0.0, 0.26, tag="Bund", collider=False)
    c.box(parent, water, x0, x1, y0, y1, 0.0, 0.18, tag="Paddy", shadows=False, collider=False)
    c.blocker(parent, x0 - 0.3, x1 + 0.3, y0 - 0.3, y1 + 0.3, 0.0, 0.8)
    nx, ny = int((x1 - x0) / 1.2), int((y1 - y0) / 1.2)
    for i in range(nx):
        for j in range(ny):
            if (i + j) % 2:
                continue
            c.w.entity("tuft:%g,%g" % (x0 + i, y0 + j), "Seedlings", parent=parent,
                       t=(x0 + 0.6 + i * 1.2, y0 + 0.6 + j * 1.2, 0.16), s=(0.55, 0.55, 1), body=(
                "    SpriteRendererComponent:\n      Color: [0.72, 0.9, 0.5, 1]\n      TilingFactor: 1\n      SortOrder: 0\n"
                "      TextureHandle: %d\n      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n")
                % kit.handle_of(os.path.join(PLANTS, "BushDense01.png.hymeta")))


def build(c, args):
    d = c.d
    grass = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Grass.hymat"))
    road = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "VillageDirt.hymat"))
    water = kit.handle_of(os.path.join(ASSETS, "Materials", "Ground", "Water.hymat"))
    dirt = city.ground_material("JP_Ground_AlleyDirt")
    G = c.group("Ground")
    c.slab(G, grass, -40, 40, -40, 60, 0.0, 2.0)
    c.box(G, road, -2.2, 2.2, -36.0, 56.0, 0.0, 0.02, tag="Road", shadows=False, collider=False)      # the road, south to north
    c.box(G, road, 2.0, 16.0, 18.0, 21.0, 0.0, 0.02, tag="Lane", shadows=False, collider=False)       # a lane to the farmhouse
    # paddies either side of the road, irrigation channels along them
    for (x0, x1, y0, y1) in ((-24, -5, -20, -8), (-26, -5, -4, 10), (5, 22, -24, -12), (5, 24, -8, 6), (-22, -5, 14, 30), (5, 14, 26, 40)):
        paddy(c, G, water, dirt, x0, x1, y0, y1)
    for (x0, x1, y0, y1) in ((-3.6, -3.0, -30, 34), (3.0, 3.6, -30, 34)):
        c.box(G, water, x0, x1, y0, y1, -0.25, -0.15, tag="Channel", shadows=False, collider=False)
    for y in range(-30, 34, 8):                                                                       # plank footbridges
        for x0 in (-3.8, 3.0):
            c.box(G, dirt, x0, x0 + 1.0, y - 0.6, y + 0.6, 0.0, 0.08, tag="Plank", collider=False)
    # bamboo fences along the road's verges, with gaps at the bridges and the lane
    for y in range(-28, 32, 2):
        if y % 8 in (0, 2) or 16 <= y <= 22:            # gaps at the footbridges and the lane
            continue
        for x in (-4.4, 4.4):
            c.model(G, "Props/JP_Prop_FenceBamboo_2m_A", x, y + 1, 0.0, 90)
    # trees and bamboo along the edges, the roadside shrine, the farmhouse, the campsite
    T = c.group("Dressing")
    for (name, x, y, folder) in (("PineTall", -30, 36, TREES), ("PineTall", 28, 44, TREES), ("SakuraWeeping", -9, 36, TREES),
                                 ("PineTall", -32, -30, TREES), ("LindenAutumnA", 30, -30, TREES), ("OakGnarledSmall", 26, 12, TREES),
                                 ("BushDense02", -6, 12, PLANTS), ("BushDense03", 6.4, 12.4, PLANTS), ("FlowerBush02", -6.2, 32, PLANTS)):
        c.plant(T, name, x, y, 0.0, folder)
    c.model(T, "Props/JP_Prop_StoneLantern_A", 5.2, 36.0, 0.0)
    d.lantern(T, 5.2, 36.0, 1.15, 0.9, 3.0)
    c.model(T, "Props/JP_Prop_StoneLantern_A", -5.2, 36.0, 0.0)
    d.lantern(T, -5.2, 36.0, 1.15, 0.9, 3.0)
    c.building(T, "JP_Rich_CompoundGate_A", 0.0, 46.0, 0.0)                                           # the shrine road's gate
    c.building(T, "JP_Poor_House_A", 20.0, 24.0, 0.0)                                                 # the farmhouse
    for (name, x, y, r) in (("JP_Prop_Well_A", 24.5, 20.5, 0), ("JP_Prop_Handcart_A", 15.5, 22.5, 20), ("JP_Prop_Barrel_A", 17.2, 23.6, 0),
                            ("JP_Prop_Brazier_A", -14.0, 36.0, 0), ("JP_Prop_Crate_A", -15.4, 35.0, 12), ("JP_Prop_Buckets_A", -12.8, 34.6, 0),
                            ("JP_Prop_WaterJar_A", 22.8, 23.5, 0)):
        c.model(T, "Props/" + name, x, y, 0.0, r)
    d.lantern(T, -14.0, 36.0, 1.2, 1.6, 4.5, "Fire")                                                  # the campsite's fire
    d.sprite(T, "JP_Signboard_Standing_A", 3.0, -30.0, 0.0)
    # the exits: south back into the city (the gate), north up to the shrine
    X = c.group("Exits")
    for (name, y) in (("Exit JapaneseCitySlice", -34.0), ("Exit ShrineGlade", 48.5)):
        c.w.entity("exit:" + name, name, parent=X, t=(0.0, y, 0.0), body=(
            "    BoxColliderComponent:\n      Offset: [0, 0, 1]\n      Size: [4, 2.4, 2]\n      Layer: Trigger\n      IsTrigger: true\n"))
    c.blocker(X, -40, 40, -38.0, -37.0); c.blocker(X, -40, 40, 51.0, 52.0)
    c.blocker(X, -40, -38, -38, 52); c.blocker(X, 38, 40, -38, 52)
    # two slimes in the far paddies' corners, dormant until she comes near (the Shrine Glade's enemy script)
    for k, (x, y) in enumerate(((-28.0, 12.0), (26.0, -10.0))):
        c.w.entity("slime:%d" % k, "Slime Paddy %d" % (k + 1), parent=T, t=(x, y, 0.0), s=(1.5, 1.5, 1), body=(
            "    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: 0\n      TextureHandle: %d\n"
            "      Sprite: Slime_0\n      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n"
            "    SpriteAnimatorComponent:\n      AnimationsHandle: %d\n      StartClip: Bounce\n      PlayOnStart: true\n      Speed: 1\n"
            "    Shadow2DComponent:\n      Offset: [0, -0.02]\n      Size: [0.85, 0.5]\n      Strength: 0.5\n"
            "    Rigidbody2DComponent:\n      Type: Dynamic\n      FixedRotation: true\n      IsBullet: false\n      GravityScale: 0\n"
            "      LinearDamping: 10\n      AngularDamping: 0.05\n"
            "    BoxCollider2DComponent:\n      Offset: [0, 0.2]\n      Size: [0.6, 0.3]\n      Density: 1\n      Friction: 0.4\n      Restitution: 0\n      IsSensor: false\n"
            "    BoxColliderComponent:\n      Offset: [0, 0.2, 0.8]\n      Size: [0.6, 0.3, 1.6]\n      Layer: World\n      IsTrigger: false\n"
            "    NativeScriptComponent:\n      Scripts:\n        - Name: EnemyChaseScript\n          Properties:\n            AggroRange: 4\n"
            "            ChaseSpeed: 2\n            Dormant: true\n            LeashRange: 8\n            MaxFromHome: 8\n            ReturnSpeed: 1.8\n"
            "            TouchRange: 0.7\n            WanderRadius: 1.5\n            WanderSpeed: 0.7\n") % (SLIME_TEX, SLIME_ANIM))
    # one camera zone for the whole hub: a little lower than the city's street
    Zg = c.group("Camera Zones")
    c.zone(Zg, "Country", 0.0, 8.0, (80.0, 92.0), tilt=71, fov=30, scale=1.0, lead=2.5, rise=3.0, pan_min=(-6, -30), pan_max=(6, 40), blend=1.0)
    slice_.backdrop(c, G)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"])
    ap.add_argument("--start", default="south", choices=["south", "north"])
    args = ap.parse_args()
    summary = json.load(open(kit.SUMMARY))
    profiles = slice_.city_profiles()
    scene_handle = kit.handle_of(SCENE) if os.path.exists(SCENE) else kit.stable_id("scene")
    c = slice_.Slice(summary)
    w = c.w
    start = {"south": (0.0, -31.0, 0.05), "north": (0.0, 44.0, 0.05)}[args.start]
    w.entity("Player", "Player", t=start, s=(2.5, 2.34375, 3), body=kit.character_body(kit.MIKO_TEXTURE, kit.MIKO_ANIM, "IdleUp", "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 14.0, start[2] + 10.0), r=(0.977, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 30\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 300\n      OrthographicSize: 13\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [60, 60]\n            LookAhead: 0.5\n            Smoothing: 6\n"))
    w.entity("Director", "Country Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: JapaneseCountryside\n            TiltDegrees: 74\n            FieldOfView: 30\n"
        "            FocusLead: 2.5\n            FocusRise: 3\n            BoundsMin: [-36, -36]\n            BoundsMax: [36, 50]\n"
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
    header = (
        "Handle: %d\nScene: JapaneseCountryside\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
        "    SpriteSorting: ViewDepth\n    Lighting: Forward\n    DefaultSpriteAlpha: Cutout\n    Meshes: true\n    Shadows: Sun\n"
        "    SpriteShadows: AsDrawn\n    UprightSprites: Vertical\n    HDR: true\n    Bloom: true\n    DepthOfField: true\n"
        "Look:\n  ProfileHandle: %d\n  Environment: Profile\n  PostProcess: Profile\n"
        "Environment:\n  SkyAmbient: [0.42, 0.36, 0.3]\n  GroundAmbient: [0.16, 0.12, 0.08]\n  Fog: true\n  FogColor: [0.78, 0.6, 0.38]\n"
        "  FogStart: 35\n  FogEnd: 110\nEntities:\n") % (scene_handle, profiles[args.profile])
    with open(SCENE, "w", newline="\r\n") as f:
        f.write(header + "\n".join(w.lines) + "\n")
    print("wrote %s: %d entities" % (SCENE, sum(1 for l in w.lines if l.startswith("  - Entity:"))))
    if c.d.missing:
        print("missing (skipped):", sorted(set(c.d.missing)))


if __name__ == "__main__":
    main()
