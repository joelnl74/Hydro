#!/usr/bin/env python3
"""Turns Assets/Scenes/Battle.hyscene into the forest arena: a clearing of the Shrine Glade's forest, in its style.

    python scripts/battle/build_arena.py [--preview out.png]

What it does (idempotent: run it again after changing it):
  - The stage: a 3D floor (grass, a mossy earth clearing, a flagstone path), the torii and four stone lanterns
    with their lights, the sacred tree and the blossom tree behind, painted oaks on both sides, rows of pines
    and the sunset card far back, bushes, ferns, flowers, rocks and petals around the edge, drifting motes.
    Everything uses the Shrine Glade's assets, materials, sun, render settings and render profiles (Dusk).
  - The actors: the shrine maiden with Characters/Miko (BattleIdle, Walk*, Kick, Punch, Death), the slimes and wisps,
    all standing upright on the floor. The camera is the battle's stage camera (stage.py), lower than the
    field's, looking into the clearing; the maiden stands right, the enemies left.
  - The UI: the battle's HUD sprites keep their layout; they become Overlay billboards on the camera's view
    plane, so they stay on screen whatever the camera does.
  - The strike effects: "FX Strike" plays Battle/FX_Strikes (fx_strikes.py), "FX Heal" the heal.
  - The hierarchy: the stage under "Arena Stage" (floor, lanterns as units of stone lantern + light, landmarks,
    trees, pines, undergrowth, sky), the battle's own entities in Party, Party HUD, Enemy Slot 0-2, HUD and FX
    folders (at the origin, since this builder reads and writes their positions as world positions). Rebuilt on every run, so
    regroup by changing BATTLE_GROUPS / ARENA_GROUPS below, not in the editor.

Entities the builder makes are tagged "Arena ..." and replaced on every run; the battle's own entities (HUD,
actors, director) are kept and updated in place. Needs PyYAML and numpy (+ Pillow for --preview).
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import math
import sys
from pathlib import Path

import numpy as np
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stage import StageCamera  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "Oxygen" / "project" / "Assets"
BATTLE = ASSETS / "Scenes" / "Battle.hyscene"
SHRINE = ASSETS / "Scenes" / "ShrineGlade.hyscene"

# The stage camera: BattleScript's defaults (CameraTilt, CameraFieldOfView, CameraFocus, ViewHeight).
CAMERA = StageCamera(tilt_degrees=73.0, field_of_view=30.0, focus=(0.0, 5.5, 0.0), view_height=10.0)
UI_LIFT = 1.0  # metres from the view plane towards the camera (Overlay sprites ignore depth anyway)

# Asset handles (from the files' .hymeta / first line).
MIKO_TEXTURE = 16527506524077974322
MIKO_ANIMATIONS = 9503405222623168017
FX_STRIKES_TEXTURE = None  # read from Battle/FX_Strikes.png.hymeta
FX_STRIKES_ANIMATIONS = None

# Where the feet are in a cell (a fraction of its height from the bottom) and the sheet's pivot (its .hymeta).
# An Upright sprite stands on its quad's bottom edge, which the pivot moves back by pivot x height; stand_offset
# moves the entity so the feet are on the floor at its ground point (BattleScript HeroFoot / StandOffset).
FOOT_HERO, PIVOT_HERO = 15.0 / 96.0, 0.5
FOOT_SLIME, PIVOT_SLIME = 4.0 / 32.0, 0.125
FOOT_WISP, PIVOT_WISP = 4.0 / 32.0, 0.12
HERO_SIZE = 2.4          # metres: the cell's height (her figure is a bit over half of it)
ENEMY_SCALE = 0.62       # the old 2D sizes times k times this: slimes and wisps about a metre tall
ENEMY_HEAD = 1.15        # metres above the floor where an enemy's HUD row starts


def stand_offset(foot: float, pivot: float, size: float):
    """From where the feet stand to the entity's position (BattleScript::StandOffset)."""
    return np.array([0.0, pivot * size, 0.0]) - CAMERA.up * (foot * size)


def shadow_entity(tag: str, position, shadow: dict):
    """A contact shadow of its own, so it stays on the floor while its sprite is lowered or floats."""
    return {"Entity": uuid_for(tag), "TagComponent": {"Tag": tag},
            "TransformComponent": {"Translation": vec(position), "Rotation": [0, 0, 0], "Scale": [1, 1, 1]},
            "Shadow2DComponent": shadow}


def read_handle(path: Path) -> int:
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("Handle:"):
            return int(line.split(":", 1)[1])
    raise ValueError(f"{path}: no Handle")


def uuid_for(name: str) -> int:
    digest = hashlib.sha1(("battle-arena:" + name).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "little") & 0x7FFFFFFFFFFFFFFF


def vec(values):
    return [round(float(v), 4) for v in values]


class FlowList(list):
    pass


def flow_representer(dumper, data):
    return dumper.represent_sequence("tag:yaml.org,2002:seq", data, flow_style=True)


yaml.add_representer(FlowList, flow_representer, Dumper=yaml.SafeDumper)


def flowify(node):
    """Short lists of numbers as [x, y, z], like Hydro writes them."""
    if isinstance(node, dict):
        return {k: flowify(v) for k, v in node.items()}
    if isinstance(node, list):
        if node and all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in node) and len(node) <= 4:
            return FlowList(node)
        return [flowify(v) for v in node]
    return node


def tag_of(entity) -> str:
    return entity.get("TagComponent", {}).get("Tag", "")


def transform(entity):
    return entity.setdefault("TransformComponent", {"Translation": [0, 0, 0], "Rotation": [0, 0, 0], "Scale": [1, 1, 1]})


def shrine_templates():
    data = yaml.safe_load(SHRINE.read_text(encoding="utf-8"))
    by_tag = {}
    for entity in data["Entities"]:
        by_tag.setdefault(tag_of(entity), []).append(entity)
    return data, by_tag


def from_template(template, tag: str, position, scale=None, color=None, flip=None, rotation=None):
    entity = copy.deepcopy(template)
    entity["Entity"] = uuid_for(tag)
    entity["TagComponent"] = {"Tag": tag}
    entity.pop("RelationshipComponent", None)  # the glade's folders are not the arena's
    for collider in ("BoxCollider2DComponent", "BoxColliderComponent", "MeshColliderComponent"):
        entity.pop(collider, None)  # nothing walks in the battle
    t = transform(entity)
    t["Translation"] = vec(position)
    if scale is not None:
        t["Scale"] = vec(scale)
    if rotation is not None:
        t["Rotation"] = vec(rotation)
    sprite = entity.get("SpriteRendererComponent")
    if sprite is not None:
        if color is not None:
            sprite["Color"] = vec(color)
        if flip is not None:
            sprite["FlipX"] = flip
    return entity


def mesh_entity(tag: str, shape: str, position, size, material: int, cast_shadows: bool = True, tiling=(1.0, 1.0), tint=(1, 1, 1, 1)):
    return {
        "Entity": uuid_for(tag),
        "TagComponent": {"Tag": tag},
        "TransformComponent": {"Translation": vec(position), "Rotation": [0, 0, 0], "Scale": [1, 1, 1]},
        "MeshComponent": {
            "Shape": shape,
            "Size": vec(size),
            "CastShadows": cast_shadows,
            "Materials": [{"MaterialHandle": material, "Inline": {
                "Tint": vec(tint), "Tiling": vec(tiling), "EmissionColor": [0, 0, 0], "EmissionIntensity": 1,
                "AlphaCutoff": 0, "Filter": "Nearest"}}],
        },
    }


def with_prefix(templates, prefix: str):
    """Every shrine entity whose tag is `prefix` or starts with `prefix` + a space."""
    return [e for tag, group in templates.items() if tag == prefix or tag.startswith(prefix + " ") for e in group]


# The Shrine Glade's ground is a terrain now (scripts/terrain/build_shrine_glade.py): the ground entities these
# materials came from are gone, so they are read from the material files instead.
GROUND_MATERIALS = {
    "Ground Grass": "Materials/Ground/Grass.hymat",
    "Valley Floor": "Materials/Ground/GrassShade.hymat",
    "Ground": "Materials/Ground/Dirt.hymat",
    "Plaza Flagstones": "Materials/Ground/Flagstones.hymat",
}


def material_of(templates, tag: str) -> int:
    found = with_prefix(templates, tag)
    meshes = [e for e in found if "MeshComponent" in e and e["MeshComponent"].get("Materials")]
    if meshes:
        return meshes[0]["MeshComponent"]["Materials"][0]["MaterialHandle"]
    path = ROOT / "Oxygen" / "project" / "Assets" / GROUND_MATERIALS[tag]
    return int(path.read_text().split("\n", 1)[0].split()[1])


def texture_of(templates, tag: str, index: int = 0) -> int:
    return templates[tag][index]["SpriteRendererComponent"]["TextureHandle"]


def sprite_template(templates, tag: str, texture_file: str | None = None):
    """The first entity with this tag (and, when given, a texture whose file ends with texture_file)."""
    for entity in with_prefix(templates, tag):
        if texture_file is None:
            return entity
        sprite = entity.get("SpriteRendererComponent", {})
        if HANDLES.get(sprite.get("TextureHandle"), "").endswith(texture_file):
            return entity
    raise KeyError(f"no '{tag}' with {texture_file}")


HANDLES: dict[int, str] = {}


def index_handles():
    for path in ASSETS.rglob("*"):
        if path.suffix in (".hymeta", ".hymat", ".hyanim", ".hyrender", ".hyprefab") and path.is_file():
            try:
                handle = read_handle(path)
            except (ValueError, UnicodeDecodeError):
                continue
            HANDLES[handle] = str(path.with_suffix("") if path.suffix == ".hymeta" else path)


def build_arena(templates, shrine_data):
    """The stage around the actors. World metres, +Y away from the camera, the clearing's centre at (0, 0)."""
    entities = []
    add = entities.append

    grass = material_of(templates, "Ground Grass")
    grass_shade = material_of(templates, "Valley Floor")
    dirt = material_of(templates, "Ground")
    flagstones = material_of(templates, "Plaza Flagstones")
    sunset = material_of(templates, "Backdrop Sunset")

    # Floor: earth underneath, grass over it, the clearing (mossy earth) and the path to the torii.
    # A box stands on its position (its bottom face there), like the glade's Ground: top at z = 0.
    add(mesh_entity("Arena Ground", "Box", (0, 12, -0.5), (90, 70, 0.5), dirt))
    add(mesh_entity("Arena Grass Shade", "Plane", (0, 12, 0.002), (90, 70, 0), grass_shade))
    add(mesh_entity("Arena Grass", "Plane", (0, 2, 0.004), (30, 22, 0), grass))
    add(mesh_entity("Arena Clearing", "Plane", (-0.4, -0.25, 0.008), (11.6, 4.6, 0), dirt, tint=(0.9, 0.88, 0.82, 1)))
    add(mesh_entity("Arena Path", "Plane", (0, 8.6, 0.012), (3.2, 10.4, 0), flagstones))
    add(mesh_entity("Arena Path Pad", "Plane", (0, 3.4, 0.012), (5.5, 1.4, 0), flagstones))

    # The torii at the end of the path, lanterns either side of it and of the clearing.
    torii = with_prefix(templates, "Torii")[0]
    add(from_template(torii, "Arena Torii", (0, 13.5, 0)))
    lantern = templates["Stone Lantern Plaza"][0] if "Stone Lantern Plaza" in templates else next(v[0] for k, v in templates.items() if k.startswith("Stone Lantern"))
    light = templates["Light Lantern Plaza"][0] if "Light Lantern Plaza" in templates else next(v[0] for k, v in templates.items() if k.startswith("Light Lantern"))
    for name, x, y in (("Path Left", -2.7, 11.6), ("Path Right", 2.7, 11.6), ("Clearing Left", -9.0, 2.6), ("Clearing Right", 9.2, 2.2)):
        add(from_template(lantern, f"Arena Stone Lantern {name}", (x, y, 0)))
        lamp = from_template(light, f"Arena Light Lantern {name}", (x, y, 0))
        lamp["Light2DComponent"]["Height"] = 1.33
        add(lamp)

    # Landmarks behind: the sacred tree on the left, the blossom tree on the right.
    add(from_template(templates["Sacred Tree"][0], "Arena Sacred Tree", (-8.5, 14.5, 0), (11.0, 11.0, 1)))
    add(from_template(with_prefix(templates, "Tree Blossom")[0], "Arena Blossom Tree", (8.8, 16.5, 0)))

    # Oaks framing the clearing (the glade's edge trees, OakSunA / OakSunB), darker further back.
    oak = {name: sprite_template(templates, "Tree Edge", name) for name in ("OakSunA.png", "OakSunB.png")}
    oaks = [
        ("Left Front", "OakSunB.png", -13.5, -5.5, 10.6, True, 0.86),
        ("Left Middle", "OakSunA.png", -15.5, 1.5, 11.5, False, 0.8),
        ("Left Back", "OakSunA.png", -14.0, 8.5, 11.0, True, 0.72),
        ("Left Far", "OakSunB.png", -19.0, 13.0, 12.0, False, 0.62),
        ("Right Front", "OakSunA.png", 13.8, -5.0, 10.6, False, 0.86),
        ("Right Middle", "OakSunA.png", 16.0, 2.0, 11.5, True, 0.8),
        ("Right Back", "OakSunB.png", 14.5, 9.0, 11.0, False, 0.72),
        ("Right Far", "OakSunA.png", 19.5, 13.5, 12.0, True, 0.62),
    ]
    for name, image, x, y, size, flip, shade in oaks:
        template = oak[image]
        aspect = template["TransformComponent"]["Scale"][0] / template["TransformComponent"]["Scale"][1]
        # never mirrored: the oaks have the sun painted on their upper left
        add(from_template(template, f"Arena Tree {name}", (x, y, 0), (size * aspect, size, 1), (shade, shade, shade * 0.97, 1), False))

    # Rows of pines behind the landmarks, darker and bluer with distance (the fog does the rest).
    pines = with_prefix(templates, "Tree Back")
    row_index = 0
    for row, (y, shade, height) in enumerate(((21.0, 0.5, 13.0), (26.0, 0.42, 15.0), (31.0, 0.36, 17.0))):
        count = 12 + row * 2
        span = 56 + row * 10
        for i in range(count):
            template = pines[(row_index * 7) % len(pines)]
            row_index += 1
            x = -span / 2 + span * (i + 0.5) / count + (2.1 if (i + row) % 2 else -1.4)
            if abs(x) < 2.5 and row == 0:
                continue  # keep the torii's silhouette clear
            aspect = template["TransformComponent"]["Scale"][0] / template["TransformComponent"]["Scale"][1]
            h = height * (0.85 + 0.3 * ((i * 37 + row * 11) % 10) / 10)
            add(from_template(template, f"Arena Pine {row} {i}", (x, y + ((i * 13) % 5) * 0.6, 0), (h * aspect, h, 1),
                              (shade, shade * 0.93, shade * 1.05, 1), False))  # firs are never mirrored: their sun is painted on

    add(mesh_entity("Arena Backdrop Sunset", "Box", (0, 40, -9), (110, 0.2, 30), sunset, cast_shadows=False))

    # Undergrowth along the clearing's back and sides (not in front: the actors stand there).
    def decor(tag, image, x, y, height, flip=False, shade=1.0):
        template = sprite_template(templates, image[0], image[1])
        s = template["TransformComponent"]["Scale"]
        aspect = s[0] / s[1]
        flip = flip and not image[1].startswith(("BushDense", "Fir", "OakSun", "Lavender", "FlowerBush", "Wildflowers", "Fern", "GrassTuft", "FlowerClump"))  # painted sun: never mirrored
        add(from_template(template, tag, (x, y, 0), (height * aspect, height, 1), (shade, shade, shade, 1), flip))

    lavender = ("Decor Plaza", "Lavender01.png")
    wildflowers = ("Decor Plaza", "Wildflowers01.png")
    moss_bush = ("Decor Hill", "BushDense01.png")
    round_bush = ("Decor Hill", "BushDense09.png")
    fern = ("Decor Shrine", "Fern01.png")
    fern_wide = ("Decor Shrine", "Fern02.png")
    stone = ("Rock", "StoneBlock.png")
    back_row = [
        (lavender, -6.6, 4.2, 1.3, False), (moss_bush, -4.3, 4.9, 1.6, True), (wildflowers, -2.6, 4.4, 0.8, False),
        (wildflowers, 2.7, 4.5, 0.8, True), (round_bush, 4.6, 5.0, 1.8, False), (lavender, 6.8, 4.0, 1.3, True),
        (fern_wide, -11.0, 5.5, 1.5, False), (fern, 11.2, 5.8, 1.6, True), (moss_bush, -7.8, 7.5, 1.9, False),
        (round_bush, 7.6, 7.8, 2.0, True), (lavender, -1.9, 10.4, 1.0, False), (lavender, 1.9, 10.5, 1.0, True),
    ]
    for i, (image, x, y, h, flip) in enumerate(back_row):
        decor(f"Arena Decor Back {i}", image, x, y, h, flip)
    sides = [
        (lavender, -11.2, -1.5, 1.4, True), (fern_wide, -10.6, -4.8, 1.4, False), (stone, -9.6, -2.9, 1.0, False),
        (lavender, 11.0, -1.0, 1.4, False), (fern, 10.4, -4.4, 1.6, True), (stone, 9.9, -2.4, 0.9, True),
        (moss_bush, -12.4, 3.0, 1.8, False), (moss_bush, 12.6, 3.4, 1.8, True),
    ]
    for i, (image, x, y, h, flip) in enumerate(sides):
        decor(f"Arena Decor Side {i}", image, x, y, h, flip)

    # The clearing's edges softened with low grass, ferns and flowers (the glade's path edges).
    edge_images = [("Glade Edge", "Fern03.png"), ("Glade Edge", "Wildflowers02.png"), ("Glade Root", "GrassTuft01.png"),
                   ("Glade Edge", "GrassTuft02.png")]
    spots = [(x, 2.05) for x in (-5.6, -4.1, -2.7, -1.4, 0.9, 2.2, 3.6, 4.9)]
    spots += [(-6.25, y) for y in (1.2, -0.3, -1.8)] + [(5.45, y) for y in (1.3, -0.2, -1.7)]
    spots += [(x, -2.6) for x in (-5.2, -2.9, 3.9)]
    for i, (x, y) in enumerate(spots):
        decor(f"Arena Edge {i}", edge_images[i % len(edge_images)], x, y, 0.55 + 0.15 * (i % 3), i % 2 == 0)

    # A big soft bush in the bottom-left corner, between the camera and the clearing (the glade's Canopy Front).
    canopy = [(("Canopy Front", "BushDense09.png"), -6.4, -3.3, 2.2, False)]
    for i, (image, x, y, h, flip) in enumerate(canopy):
        decor(f"Arena Canopy {i}", image, x, y, h, flip, 0.82)

    # Petals on the floor (flat).
    petals = [e for k, v in templates.items() if k.startswith("Glade Petals") for e in v]
    for i, (x, y, r) in enumerate(((-6.5, 2.6, 0.4), (5.8, 3.0, -0.3), (-3.0, -4.6, 0.9), (7.5, -3.8, 0.2), (-8.2, -0.8, -0.6), (0.8, 5.6, 0.1))):
        template = petals[i % len(petals)]
        entity = from_template(template, f"Arena Petals {i}", (x, y, 0.016), rotation=(0, 0, r))
        add(entity)

    # Drifting motes in the warm light (fireflies / pollen): lit, glowing, wandering.
    add({
        "Entity": uuid_for("Arena Motes"),
        "TagComponent": {"Tag": "Arena Motes"},
        "TransformComponent": {"Translation": [0, 3, 1.4], "Rotation": [0, 0, 0], "Scale": [1, 1, 1]},
        "ParticleEmitter2DComponent": {
            "PlayOnStart": True, "Looping": True, "Duration": 4, "Rate": 5, "BurstCount": 14, "MaxParticles": 48, "Seed": 0,
            "Shape": "Box", "ShapeSize": [26, 14], "Lifetime": [4, 7], "Speed": [0.05, 0.3], "Direction": 90, "Spread": 180,
            "StartSize": [0.05, 0.11], "StartRotation": [0, 0], "AngularVelocity": [0, 0], "EndSizeScale": 0.6,
            "StartColor": [1, 0.86, 0.5, 0.9], "EndColor": [1, 0.75, 0.4, 0], "Gravity": [0, 0], "Drag": 0.6, "Turbulence": 0.6,
            "TextureHandle": 0, "SpriteIndex": 0, "Blend": "Additive", "Lighting": "Lit", "Space": "Ground", "SortOrder": 0,
            "EmissionIntensity": 3, "LightRadius": 0, "LightIntensity": 1, "MaxLights": 0, "DestroyWhenFinished": False,
        },
    })

    # The sun: the glade's (low, golden, from behind on the right).
    sun = copy.deepcopy(with_prefix(templates, "Sun")[0])
    sun["Entity"] = uuid_for("Arena Sun")
    sun["TagComponent"] = {"Tag": "Arena Sun"}
    sun["TransformComponent"]["Translation"] = [6, 6, 8]
    add(sun)
    return entities


def update_battle(battle, templates):
    """The battle's own entities: actors stand on the floor, the HUD goes onto the view plane, effects get theirs."""
    k = CAMERA.k
    kept = []
    # Scenes from before the shadows had entities of their own stood the sprites at their ground point.
    stood_on_feet = any(tag_of(e) == "Hero Miko Shadow" for e in battle["Entities"])
    old_shadows = {}
    for entity in battle["Entities"]:
        tag = tag_of(entity)
        if tag.startswith("Arena ") or tag in ("Battle Background", "Ambient Light", "Warm Light") or tag in BATTLE_FOLDERS:
            continue  # rebuilt below / replaced by the 3D stage
        if tag.endswith(" Shadow") and "Shadow2DComponent" in entity:
            old_shadows[tag[: -len(" Shadow")]] = entity["Shadow2DComponent"]
            continue  # rebuilt with their actors
        kept.append(entity)
    shadows = []

    def layout_from_old(entity):
        t = entity["TransformComponent"]
        return (t["Translation"][0], t["Translation"][1])

    for entity in kept:
        tag = tag_of(entity)
        t = entity.get("TransformComponent")
        sprite = entity.get("SpriteRendererComponent")
        if tag == "Camera":
            camera = entity["CameraComponent"]
            camera.update({"Primary": True, "Projection": "Perspective", "PerspectiveFOV": CAMERA.field_of_view,
                           "PerspectiveNear": 0.5, "PerspectiveFar": round(CAMERA.distance + 200, 2),
                           "OrthographicSize": CAMERA.view_height, "OrthographicNear": 0.1, "OrthographicFar": round(CAMERA.distance + 200, 2)})
            t["Translation"] = vec(CAMERA.position)
            t["Rotation"] = vec(CAMERA.camera_rotation())
            continue
        if tag == "Battle Director":
            scripts = entity["NativeScriptComponent"]["Scripts"]
            for script in scripts:
                if script.get("Name") == "BattleScript":
                    properties = script.setdefault("Properties", {})
                    properties.update({"CameraTilt": CAMERA.tilt_degrees, "CameraFieldOfView": CAMERA.field_of_view,
                                       "CameraFocus": list(CAMERA.focus), "ViewHeight": CAMERA.view_height})
            continue
        if t is None:
            continue

        # Positions are read in layout units the first time (the old 2D battle) and from the world afterwards:
        # actors from where they stand on the floor, everything else from the view plane.
        actor = tag == "Hero Miko" or (tag.startswith("Enemy ") and (tag.endswith(" Slime") or tag.endswith(" Wisp")))
        if actor:
            foot, pivot = (FOOT_HERO, PIVOT_HERO) if tag == "Hero Miko" else (FOOT_WISP, PIVOT_WISP) if tag.endswith(" Wisp") else (FOOT_SLIME, PIVOT_SLIME)
        if battle.get("_from_2d"):
            layout = layout_from_old(entity)
        elif sprite is not None and sprite.get("Overlay"):
            layout = CAMERA.plane_layout_of(t["Translation"])
        elif actor and stood_on_feet:
            feet = np.array(t["Translation"], float) - stand_offset(foot, pivot, t["Scale"][1])
            layout = CAMERA.layout_of((feet[0], feet[1], CAMERA.ground_z))
        else:
            layout = CAMERA.layout_of((t["Translation"][0], t["Translation"][1], CAMERA.ground_z))

        if tag == "Hero Miko":
            scale = HERO_SIZE
            ground = CAMERA.ground_point(layout)
            t["Translation"] = vec(ground + stand_offset(foot, pivot, scale))
            t["Scale"] = vec((scale, scale, 1))
            sprite.update({"TextureHandle": MIKO_TEXTURE, "Sprite": "Miko_64", "SpriteIndex": 64, "Facing": "Upright", "SortOrder": 30})
            sprite.pop("Overlay", None)
            entity["SpriteAnimatorComponent"] = {"AnimationsHandle": MIKO_ANIMATIONS, "StartClip": "BattleIdle", "PlayOnStart": True, "Speed": 1}
            entity.pop("Shadow2DComponent", None)
            shadows.append(shadow_entity("Hero Miko Shadow", ground, {"Offset": [0, 0], "Size": vec((1.15, 0.36)), "Strength": 0.55}))
        elif tag.startswith("Enemy ") and (tag.endswith(" Slime") or tag.endswith(" Wisp")):
            wisp = tag.endswith(" Wisp")
            if battle.get("_from_2d"):
                scale = t["Scale"][0] * k * ENEMY_SCALE
            else:
                scale = t["Scale"][0]
            ground = CAMERA.ground_point(layout)
            t["Translation"] = vec(ground + stand_offset(foot, pivot, scale))
            t["Scale"] = vec((scale, scale, 1))
            sprite["Facing"] = "Upright"
            sprite.pop("Overlay", None)
            shadow = entity.pop("Shadow2DComponent", None) or old_shadows.get(tag) or {"Strength": 0.0}
            shadow["Offset"] = [0, 0]
            shadow["Size"] = vec((1.0 if wisp else 1.15, 0.32))
            shadows.append(shadow_entity(tag + " Shadow", ground, shadow))
        elif tag.startswith("Enemy ") and tag.endswith(" Light"):
            ground = CAMERA.ground_point(layout)
            t["Translation"] = vec(ground)
            entity["Light2DComponent"]["Height"] = 1.0
            entity["Light2DComponent"]["Radius"] = 3.0
        elif tag == "FX Slash" or tag == "FX Strike":
            entity["TagComponent"]["Tag"] = "FX Strike"
            t["Translation"] = vec((0, 0, -20))
            t["Scale"] = [2.6, 2.6, 1]
            sprite.update({"Color": [1, 1, 1, 0], "TextureHandle": FX_STRIKES_TEXTURE, "Sprite": "Punch_0", "SpriteIndex": 0,
                           "Facing": "Billboard", "AlphaMode": "Translucent", "CastShadows": False,
                           "EmissionColor": [1, 1, 1], "EmissionIntensity": 2.5, "SortOrder": 60})
            sprite.pop("Overlay", None)
            entity["SpriteAnimatorComponent"] = {"AnimationsHandle": FX_STRIKES_ANIMATIONS, "StartClip": "Punch", "PlayOnStart": False, "Speed": 1}
            entity["Light2DComponent"].update({"Radius": 4.0, "Height": 0.0, "Intensity": 0})
        elif tag == "FX Heal":
            t["Translation"] = vec((0, 0, -20))
            t["Scale"] = vec((4 * k, 4 * k, 1)) if battle.get("_from_2d") else t["Scale"]
            sprite.update({"Facing": "Billboard", "AlphaMode": "Translucent", "CastShadows": False,
                           "EmissionColor": [1, 1, 1], "EmissionIntensity": 1.5})
            sprite.pop("Overlay", None)
            if "Light2DComponent" in entity:
                entity["Light2DComponent"].update({"Radius": 3.0, "Height": 0.0})
        elif sprite is not None:
            # HUD: stays where it was on screen, drawn last (Overlay), facing the camera.
            if battle.get("_from_2d"):
                t["Scale"] = vec((t["Scale"][0] * k, t["Scale"][1] * k, 1))
            t["Translation"] = vec(CAMERA.screen_point(layout, UI_LIFT))
            sprite["Facing"] = "Billboard"
            sprite["Overlay"] = True
            sprite["CastShadows"] = False
    kept += shadows
    place_enemy_huds(kept)
    return kept


def place_enemy_huds(entities):
    """Each enemy's HP bar, shield and weakness icons as one row just above its head (kept as laid out)."""
    by_tag = {tag_of(e): e for e in entities}
    for index in range(3):
        body = by_tag.get(f"Enemy {index} Slime Shadow")  # on the floor under the feet
        if body is None:
            continue
        feet = body["TransformComponent"]["Translation"]
        head = CAMERA.layout_of((feet[0], feet[1], CAMERA.ground_z + ENEMY_HEAD))
        # The old layout, relative to the HP bar: shield 1.9 left and 0.25 up, weakness icons from 0.9 left, 0.55 up.
        offsets = {"HP Back": (0.3, 0.45), "HP Fill": (0.3, 0.45), "Shield": (-1.6, 0.7), "Shield Digit": (-1.6, 0.77),
                   "Weak 0": (-0.6, 1.0), "Weak 1": (0.05, 1.0), "Weak 2": (0.7, 1.0)}
        for part, (dx, dy) in offsets.items():
            hud = by_tag.get(f"Enemy {index} {part}")
            if hud is not None:
                hud["TransformComponent"]["Translation"] = vec(CAMERA.screen_point((head[0] + dx, head[1] + dy), UI_LIFT))


# The folders the entities go in, by tag (first match wins). Lantern units are made from their stone lantern.
ARENA_GROUPS = [
    ("Arena Floor", ("Arena Ground", "Arena Grass", "Arena Grass Shade", "Arena Clearing", "Arena Path", "Arena Path Pad", "Arena Petals")),
    ("Arena Landmarks", ("Arena Torii", "Arena Sacred Tree", "Arena Blossom Tree")),
    ("Arena Trees", ("Arena Tree",)),
    ("Arena Pines", ("Arena Pine",)),
    ("Arena Undergrowth", ("Arena Decor", "Arena Edge", "Arena Canopy")),
    ("Arena Sky", ("Arena Backdrop Sunset", "Arena Sun", "Arena Motes")),
]
BATTLE_GROUPS = [
    ("Party", ("=Hero Miko", "=Hero Miko Shadow")),
    ("Party HUD", ("Hero Miko HP", "Hero Miko MP", "Hero Miko Name", "Hero Miko Pip", "Party Panel")),
    ("Enemy Slot 0", ("Enemy 0",)),
    ("Enemy Slot 1", ("Enemy 1",)),
    ("Enemy Slot 2", ("Enemy 2",)),
    ("HUD", ("Order", "Message", "Target Cursor", "Turn Marker", "Command", "Boost", "Popup", "Battle Overlay")),
    ("FX", ("FX",)),
]
BATTLE_FOLDERS = {name for name, _ in BATTLE_GROUPS}


def matches(tag: str, prefixes) -> bool:
    """`tag` is one of the prefixes or starts with one and a space ("=Name": exactly that tag)."""
    return any(tag == p[1:] if p.startswith("=") else (tag == p or tag.startswith(p + " ")) for p in prefixes)


def folder_entity(tag: str, position, parent=None):
    entity = {"Entity": uuid_for(tag), "TagComponent": {"Tag": tag}}
    if parent is not None:
        entity["RelationshipComponent"] = {"Parent": parent}
    entity["TransformComponent"] = {"Translation": vec(position), "Rotation": [0, 0, 0], "Scale": [1, 1, 1]}
    return entity


def with_parent(entity, parent, origin=(0, 0, 0)):
    """The entity under `parent` (whose world position is `origin`), its Parent right after its tag like Hydro writes it."""
    out = {}
    for key, value in entity.items():
        if key == "RelationshipComponent":
            continue
        out[key] = value
        if key == "TagComponent" and parent is not None:
            out["RelationshipComponent"] = {"Parent": parent}
    t = out.get("TransformComponent")
    if t is not None and any(origin):
        t["Translation"] = vec(np.array(t["Translation"], float) - np.array(origin, float))
    return out


def group(entities):
    """The scene's hierarchy, in depth-first order (each folder followed by its children)."""
    out = []
    taken = set()

    def take(predicate):
        found = [e for e in entities if id(e) not in taken and predicate(tag_of(e))]
        taken.update(id(e) for e in found)
        return found

    roots = take(lambda tag: tag in ("Camera", "Battle Director"))
    out += [with_parent(e, None) for e in roots]

    stage = folder_entity("Arena Stage", (0, 0, 0))
    out.append(stage)
    for name, prefixes in ARENA_GROUPS[:1]:
        folder = folder_entity(name, (0, 0, 0), stage["Entity"])
        out.append(folder)
        out += [with_parent(e, folder["Entity"]) for e in take(lambda tag: matches(tag, prefixes))]
    # Each stone lantern and its light as one unit, at the lantern.
    lanterns = folder_entity("Arena Lanterns", (0, 0, 0), stage["Entity"])
    out.append(lanterns)
    for stone in [e for e in entities if tag_of(e).startswith("Arena Stone Lantern ")]:
        name = tag_of(stone)[len("Arena Stone Lantern "):]
        position = stone["TransformComponent"]["Translation"]
        unit = folder_entity(f"Arena Lantern {name}", position, lanterns["Entity"])
        out.append(unit)
        parts = take(lambda tag: tag in (f"Arena Stone Lantern {name}", f"Arena Light Lantern {name}"))
        out += [with_parent(e, unit["Entity"], position) for e in parts]
    for name, prefixes in ARENA_GROUPS[1:]:
        folder = folder_entity(name, (0, 0, 0), stage["Entity"])
        out.append(folder)
        out += [with_parent(e, folder["Entity"]) for e in take(lambda tag: matches(tag, prefixes))]

    for name, prefixes in BATTLE_GROUPS:
        folder = folder_entity(name, (0, 0, 0))
        out.append(folder)
        out += [with_parent(e, folder["Entity"]) for e in take(lambda tag: matches(tag, prefixes))]
    rest = [e for e in entities if id(e) not in taken]
    out += [with_parent(e, None) for e in rest]  # anything new stays a root
    return out


def main() -> int:
    global FX_STRIKES_TEXTURE, FX_STRIKES_ANIMATIONS
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preview", type=Path, help="also render an offline preview (preview.py)")
    args = parser.parse_args()

    index_handles()
    FX_STRIKES_TEXTURE = read_handle(ASSETS / "Battle" / "FX_Strikes.png.hymeta")
    FX_STRIKES_ANIMATIONS = read_handle(ASSETS / "Battle" / "FX_Strikes.hyanim")

    battle = yaml.safe_load(BATTLE.read_text(encoding="utf-8"))
    shrine_data, templates = shrine_templates()
    # The first run converts the old 2D battle (an orthographic camera at the origin, sprites lying flat).
    camera = next(e for e in battle["Entities"] if tag_of(e) == "Camera")
    battle["_from_2d"] = camera["CameraComponent"].get("Projection") == "Orthographic"

    entities = update_battle(battle, templates)
    arena = build_arena(templates, shrine_data)
    from_2d = battle.pop("_from_2d")

    out = {"Handle": battle["Handle"], "Scene": battle.get("Scene", "Battle")}
    for key in ("RenderSettings", "Look", "Environment"):
        if key in shrine_data:
            out[key] = copy.deepcopy(shrine_data[key])
    out["Entities"] = group(arena + entities)

    body = yaml.safe_dump(flowify(out), sort_keys=False, width=200, allow_unicode=True)
    # Hydro reads the handle from the first line of a scene file.
    BATTLE.write_bytes((body).replace("\n", "\r\n").encode("utf-8"))
    print(f"Battle.hyscene: {len(arena)} arena entities, {len(entities)} battle entities ({'converted from 2D' if from_2d else 'updated'})")

    if args.preview:
        import preview
        preview.render(BATTLE, args.preview, CAMERA)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
