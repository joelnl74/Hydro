"""Build the Farm Village inn's interiors: the common room (Assets/Scenes/FarmVillageInn.hyscene) and the guest rooms
upstairs (FarmVillageInnUpstairs.hyscene). Placeholder rooms of built-in boxes, cylinders and stairs with the medieval
kit's textures, until the inn interior kit is modelled in Blender (see the wiki's Interiors page, docs/TestBed.md,
"Interiors", and art_incoming/Inn-Interior-Art-Prompt.md).

    python scripts/interiors/build_inn.py

How interiors work (MvpScripts.h, ShrineScripts.h InteriorScript):
  - "Door Inn" in the Farm Village opens FarmVillageInn (a door's scene is "<this scene><Name>", else "<Name>"):
    the village fades out and the inn is pushed on top, so the village waits exactly as it was left.
  - "Door Out" (at the room's open front) pops back to the village, at the inn's door.
  - Upstairs is a scene of its own on top of the common room: the stairs on the common room's west wall end on a
    landing with "Door Upstairs" (it pushes FarmVillageInnUpstairs); upstairs, "Door Out" at the stair head pops
    back down, so the player is on the landing again. The beds are upstairs, in the rooms the inn rents ("Rest Bed").
  - The rooms are built at the characters' scale (1 m = 32 texels, like the field), with the camera-facing (south)
    walls left out and nothing around them, so they float in black. The camera comes in closer than the field's.
  - Light: no sun. The windows' point lights ("Light Window *") take the time of day of the village the player came
    from; the hearth, candles and lamps ("Light Fire *") flicker. Keys 1 / 2 / 3 switch day, evening, night.

Writes the rooms' materials (Assets/Interiors/Materials, the kit's textures at 32 texels per metre) and render
profiles (Assets/Interiors/RenderProfiles) when they are missing, then the scenes, then the field UI into each
(scripts/ui/build_game_ui.py: inventory, dialogue, prompt, fade). Every handle and entity ID comes from a name, so
reruns give the same files. Close the scenes in the editor first. Needs PyYAML.

Coordinates: X east (right on screen), Y north (up the screen, away from the camera), Z up; each floor is z = 0 in
its own scene and the rooms span x -5..5, y -3.5..3.5. The common room is 3.0 m high and its stairs climb 3.0 m;
upstairs, the same flight is drawn 3.0 m down through the stairwell. Built-in shapes stand on their entity's
position; a Stairs shape rises towards +Y.
"""

import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "ui"))
import build_game_ui as ui  # noqa: E402  (the scene dumper and the field UI)

ASSETS = ui.ASSETS
SCENE_NAME = "FarmVillageInn"
SCENE_FILE = SCENE_NAME + ".hyscene"
UPSTAIRS_NAME = "FarmVillageInnUpstairs"
UPSTAIRS_FILE = UPSTAIRS_NAME + ".hyscene"


def handle(name):
    """A stable 63-bit handle from a name (materials, profiles, the scene)."""
    digest = hashlib.sha1(("Interiors/%s" % name).encode()).digest()
    return int.from_bytes(digest[:8], "little") & ((1 << 63) - 1)


def write_crlf(path, text, overwrite=True):
    if os.path.exists(path) and overwrite is False:
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as file:
        file.write(text.replace("\n", "\r\n"))
    return True


# ---- Materials: the kit's textures (MV_House_A / MV_House_B imports), 32 texels per metre on the built-in shapes ----
# Tiling = 32 / the texture's size in pixels (the shapes' UVs are in metres). The textures are the kit's import
# products (<model>.textures/), so the handles stay as long as the models aren't re-imported from scratch.

TEX_PLANKS = 8303934797510901812    # MV_House_B.textures/5_MV_Wood_Planks.png, 64 x 64
TEX_PLASTER = 9626923311203257854   # MV_House_A.textures/8_MV_Plaster_Warm.png, 64 x 64
TEX_BEAM = 4106423450497729754      # MV_House_A.textures/0_MV_Wood_DarkBeam.png, 16 x 64 (grain along v)
TEX_CHIMNEY = 1733481833523172452   # MV_House_A.textures/1_MV_Stone_Chimney.png, 64 x 64
TEX_STONE_CUT = 13246013011659548719  # MV_House_A.textures/2_MV_Stone_Cut.png, 64 x 64
TEX_DOOR = 5416523222750301245      # MV_House_A.textures/4_MV_Wood_Door.png, 32 x 64 (one door, 1 x 2 m)
TEX_CLOTH = 17119333170710613626    # MV_House_B.textures/6_MV_Cloth_WarmNeutral.png, 32 x 32
WINDOW_GLASS = 8169443999239105143  # MV_House_A_MAT_Window_Glass.hymat (lit glass, emissive)

MATERIALS = {
    # name: (albedo, tiling, tint)
    "Inn_FloorPlanks": (TEX_PLANKS, (0.5, 0.5), (0.86, 0.8, 0.74, 1)),
    "Inn_WallPlaster": (TEX_PLASTER, (0.5, 0.5), (0.96, 0.92, 0.86, 1)),
    "Inn_Timber": (TEX_BEAM, (2.0, 0.5), (1, 1, 1, 1)),
    "Inn_Furniture": (TEX_PLANKS, (0.5, 0.5), (0.72, 0.6, 0.5, 1)),
    "Inn_HearthStone": (TEX_CHIMNEY, (0.5, 0.5), (0.9, 0.86, 0.82, 1)),
    "Inn_StoneCut": (TEX_STONE_CUT, (0.5, 0.5), (0.85, 0.82, 0.78, 1)),
    "Inn_Door": (TEX_DOOR, (1.0, 0.5), (1, 1, 1, 1)),
    "Inn_Cloth": (TEX_CLOTH, (1.0, 1.0), (0.78, 0.36, 0.28, 1)),   # a madder-red rug and blanket
    "Inn_Linen": (TEX_CLOTH, (1.0, 1.0), (1, 1, 1, 1)),
}


def material_file(albedo, tiling, tint, name):
    return (
        "Handle: %d\n"
        "Material:\n"
        "  Type: Standard\n"
        "  AlbedoHandle: %d\n"
        "  Tint: [%g, %g, %g, %g]\n"
        "  Tiling: [%g, %g]\n"
        "  EmissionColor: [0, 0, 0]\n"
        "  EmissionIntensity: 1\n"
        "  AlphaCutoff: 0\n"
        "  Filter: Nearest\n" % ((handle(name), albedo) + tuple(tint) + tuple(tiling)))


# ---- Render profiles: dim rooms the fire carries; black around (the project's clear colour), no fog ---------------

PROFILES = {
    # name: (sky ambient, ground ambient, exposure, bloom threshold, bloom intensity)
    "Day": ((0.42, 0.38, 0.33), (0.2, 0.16, 0.12), 0.1, 1.0, 0.45),
    "Evening": ((0.26, 0.2, 0.16), (0.1, 0.07, 0.05), 0.3, 0.95, 0.6),
    "Night": ((0.12, 0.11, 0.15), (0.04, 0.03, 0.03), 0.5, 0.9, 0.8),
}


def profile_file(name, sky, ground, exposure, threshold, intensity):
    return (
        "Handle: %d\n"
        "RenderProfile:\n"
        "  Environment:\n"
        "    SkyAmbient: [%g, %g, %g]\n"
        "    GroundAmbient: [%g, %g, %g]\n"
        "    Fog: false\n"
        "    FogColor: [0, 0, 0]\n"
        "    FogStart: 40\n"
        "    FogEnd: 80\n"
        "  PostProcess:\n"
        "    Exposure: %g\n"
        "    Bloom: true\n"
        "    BloomThreshold: %g\n"
        "    BloomKnee: 0.5\n"
        "    BloomIntensity: %g\n"
        "    BloomRadius: 1\n" % ((handle("Profile" + name),) + sky + ground + (exposure, threshold, intensity)))


# ---- Entity helpers ---------------------------------------------------------------------------------------------

def transform(position=(0, 0, 0), rotation=(0, 0, 0), scale=(1, 1, 1)):
    return {"Translation": list(position), "Rotation": list(rotation), "Scale": list(scale)}


def inline(tint=(1, 1, 1, 1), emission=(0, 0, 0), intensity=1):
    return {"Tint": list(tint), "Tiling": [1, 1], "EmissionColor": list(emission), "EmissionIntensity": intensity,
            "AlphaCutoff": 0, "Filter": "Nearest"}


def shape(kind, size, material=None, shadows=True, emission=None, tint=(1, 1, 1, 1)):
    if material is None:
        slot = {"Inline": inline(tint, emission or (0, 0, 0), 4 if emission else 1)}
    else:
        slot = {"MaterialHandle": material, "Inline": inline()}
    return {"Shape": kind, "Size": list(size), "CastShadows": shadows, "Materials": [slot]}


def model(relative):
    return {"ModelHandle": ui.handle_of(relative), "Shape": "Box", "Size": [1, 1, 1], "CastShadows": True, "Materials": []}


SOLID = {"Layer": "World"}


def point_light(color, intensity, radius, height):
    return {"Type": "Point", "Color": list(color), "Intensity": intensity, "Radius": radius, "Height": height}


def box_collider(size, offset=(0, 0, 0), layer="World", trigger=False):
    return {"Offset": list(offset), "Size": list(size), "Layer": layer, "IsTrigger": trigger}


def villager(texture, sprite, animations, clip="IdleDown"):
    """A villager sprite (Characters/Villagers/<Name>.png, 96 x 72 cells at 32 px/m), standing with its feet on its position."""
    return {
        "SpriteRendererComponent": {"Color": [1, 1, 1, 1], "TilingFactor": 1, "SortOrder": 0, "TextureHandle": texture,
                                    "Sprite": sprite, "SpriteIndex": 0, "FlipX": False, "FlipY": False, "Facing": "Upright"},
        "SpriteAnimatorComponent": {"AnimationsHandle": animations, "StartClip": clip, "PlayOnStart": True, "Speed": 1},
        "Shadow2DComponent": {"Offset": [0, 0.03], "Size": [0.42, 0.11], "Strength": 0.5},
        "BoxColliderComponent": box_collider((0.32, 0.08, 1.6), (0, 0.04, 0.8), "Npc"),
    }


BARMAID_ANIMATIONS = 2472910742119809806
GATHERER_ANIMATIONS = 1444298772692147167
BARMAID_TEXTURE = 6156810907980390543
GATHERER_TEXTURE = 13537106793910617173
MIKO_TEXTURE = 5407845820862445765
MIKO_ANIMATIONS = 4798671707330513403


HALF_PI = 1.5707963


def bed(b, parent, tag, position, m, width=1.1, length=2.1):
    """A bed with its head against the north wall: frame, blanket, pillow, headboard."""
    group = b.add(tag, parent, TransformComponent=transform(position))
    half = length / 2
    b.add(tag + " Frame", group, TransformComponent=transform(),
          MeshComponent=shape("Box", (width, length, 0.42), m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    b.add(tag + " Blanket", group, TransformComponent=transform((0, -0.2, 0.42)),
          MeshComponent=shape("Box", (width - 0.06, length - 0.5, 0.1), m["Inn_Cloth"]))
    b.add(tag + " Pillow", group, TransformComponent=transform((0, half - 0.3, 0.42)),
          MeshComponent=shape("Box", (width - 0.3, 0.4, 0.16), m["Inn_Linen"]))
    b.add(tag + " Head", group, TransformComponent=transform((0, half - 0.05, 0)),
          MeshComponent=shape("Box", (width, 0.1, 0.9), m["Inn_Timber"]))
    return group


def candle_table(b, parent, tag, position, m, size=(0.5, 0.45, 0.6)):
    """A small table with a lit candle on it (the candle's light is placed with the other lights)."""
    group = b.add(tag, parent, TransformComponent=transform(position))
    b.add(tag + " Top", group, TransformComponent=transform(),
          MeshComponent=shape("Box", size, m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    b.add(tag + " Candle", group, TransformComponent=transform((0.05, 0, size[2])),
          MeshComponent=dict(shape("Cylinder", (0.07, 0.07, 0.14), m["Inn_Linen"], False), Segments=6))
    b.add(tag + " Flame", group, TransformComponent=transform((0.05, 0, size[2] + 0.14)),
          MeshComponent=shape("Box", (0.03, 0.03, 0.06), None, False, emission=(1.0, 0.7, 0.35), tint=(1, 0.8, 0.5, 1)))
    return group


def window(b, parent, tag, x, m, sill=1.12, wall_y=3.47):
    """A lit window in the north wall (the glass is emissive) with its sill."""
    b.add(tag, parent, TransformComponent=transform((x, wall_y, sill + 0.08)),
          MeshComponent=shape("Box", (1.0, 0.06, 1.0), WINDOW_GLASS, False))
    b.add(tag + " Sill", parent, TransformComponent=transform((x, wall_y - 0.07, sill)),
          MeshComponent=shape("Box", (1.2, 0.2, 0.08), m["Inn_Timber"]))


def shell(b, room, m, height, posts_north, posts_side):
    """Floor-less shell: the north, west and east walls (the south one, facing the camera, is left out) with timber
    posts and a head beam and mid rail on the north wall. Walls go 0.3 m below the floor, so they are height + 0.3."""
    b.add("Wall North", room, TransformComponent=transform((0, 3.65, -0.3)),
          MeshComponent=shape("Box", (10.6, 0.3, height + 0.3), m["Inn_WallPlaster"]), MeshColliderComponent=SOLID)
    b.add("Wall West", room, TransformComponent=transform((-5.15, 0, -0.3)),
          MeshComponent=shape("Box", (0.3, 7.6, height + 0.3), m["Inn_WallPlaster"]), MeshColliderComponent=SOLID)
    b.add("Wall East", room, TransformComponent=transform((5.15, 0, -0.3)),
          MeshComponent=shape("Box", (0.3, 7.6, height + 0.3), m["Inn_WallPlaster"]), MeshColliderComponent=SOLID)
    frame = b.add("Timber", room, TransformComponent=transform())
    for i, x in enumerate(posts_north):
        b.add("Post North %d" % i, frame, TransformComponent=transform((x, 3.42, 0)),
              MeshComponent=shape("Box", (0.24, 0.16, height), m["Inn_Timber"]))
    for i, y in enumerate(posts_side):
        b.add("Post West %d" % i, frame, TransformComponent=transform((-4.92, y, 0)),
              MeshComponent=shape("Box", (0.16, 0.24, height), m["Inn_Timber"]))
        b.add("Post East %d" % i, frame, TransformComponent=transform((4.92, y, 0)),
              MeshComponent=shape("Box", (0.16, 0.24, height), m["Inn_Timber"]))
    # Horizontal beams: a box standing up, turned on its side about Y, so the grain runs along it.
    b.add("Beam North", frame, TransformComponent=transform((-5.0, 3.42, height - 0.45), (0, HALF_PI, 0)),
          MeshComponent=shape("Box", (0.22, 0.18, 10.0), m["Inn_Timber"]))
    b.add("Rail North", frame, TransformComponent=transform((-5.0, 3.44, 0.95), (0, HALF_PI, 0)),
          MeshComponent=shape("Box", (0.14, 0.14, 10.0), m["Inn_Timber"]))
    return frame


# ---- The stairs: one flight on the west wall, 14 risers of 0.214 m over 3.4 m (41 degrees), floor to floor 3.0 m.
# Common room: the flight rises north from y -2.1 to a landing at y 1.3..1.8, z 3.0. Upstairs: the same flight is
# drawn 3.0 m down, seen through the stairwell (x -5..-4.05, y -1.0..1.3), and the stair head is at its north end.
STAIR_X = -4.5
STAIR_WIDTH = 0.9
STAIR_RUN = 3.4
STAIR_RISE = 3.0
STAIR_STEPS = 14
STAIR_FOOT_Y = -2.1
STAIR_TOP_Y = STAIR_FOOT_Y + STAIR_RUN          # 1.3
STAIR_PITCH = 0.7230                            # atan(3.0 / 3.4), radians
STAIR_RAIL = 4.534                              # the flight's slope length


def build_room(b, m):
    """The common room. `m` maps material names to handles."""
    root = lambda tag: b.add(tag, None, TransformComponent=transform())  # noqa: E731

    # ---- Shell: floor, three walls (the south one, facing the camera, is left out), timber framing -------------
    room = root("Room")
    b.add("Floor", room, TransformComponent=transform((0, 0, -0.3)),
          MeshComponent=shape("Box", (10, 7, 0.3), m["Inn_FloorPlanks"]), MeshColliderComponent=SOLID)
    shell(b, room, m, 3.0, (-4.85, -1.7, 1.7, 4.85), (3.2, 0.2, -2.8))
    # The cut: a low sill along the open front, with a gap for the doorway, and an unseen wall so nobody walks off.
    b.add("Front Sill West", room, TransformComponent=transform((-3.05, -3.6, -0.3)),
          MeshComponent=shape("Box", (4.5, 0.3, 0.5), m["Inn_Timber"]))
    b.add("Front Sill East", room, TransformComponent=transform((3.05, -3.6, -0.3)),
          MeshComponent=shape("Box", (4.5, 0.3, 0.5), m["Inn_Timber"]))
    b.add("Front Threshold", room, TransformComponent=transform((0, -3.6, -0.3)),
          MeshComponent=shape("Box", (1.6, 0.4, 0.32), m["Inn_StoneCut"]))
    b.add("Front Blocker", room, TransformComponent=transform((0, -3.75, 0)),
          BoxColliderComponent=box_collider((10.6, 0.3, 2.4), (0, 0, 1.2)))

    # ---- The hearth (east wall) ---------------------------------------------------------------------------------
    hearth = b.add("Hearth", room, TransformComponent=transform((4.55, 0.9, 0)))
    b.add("Hearth Breast", hearth, TransformComponent=transform((0.2, 0, 0)),
          MeshComponent=shape("Box", (0.9, 2.2, 3.0), m["Inn_HearthStone"]), MeshColliderComponent=SOLID)
    b.add("Hearth Slab", hearth, TransformComponent=transform((-0.55, 0, 0)),
          MeshComponent=shape("Box", (0.7, 2.4, 0.12), m["Inn_StoneCut"]), MeshColliderComponent=SOLID)
    b.add("Hearth Mouth", hearth, TransformComponent=transform((-0.27, 0, 0.12)),
          MeshComponent=shape("Box", (0.06, 1.2, 0.85), None, False, tint=(0.05, 0.03, 0.02, 1)))
    b.add("Hearth Embers", hearth, TransformComponent=transform((-0.4, 0, 0.12)),
          MeshComponent=shape("Box", (0.3, 0.9, 0.12), None, False, emission=(1.0, 0.42, 0.12), tint=(0.3, 0.1, 0.05, 1)))
    b.add("Hearth Mantel", hearth, TransformComponent=transform((-0.32, 0, 1.25)),
          MeshComponent=shape("Box", (0.3, 2.5, 0.14), m["Inn_Timber"]))
    b.add("Firewood", hearth, TransformComponent=transform((-0.6, -1.5, 0)), MeshComponent=model("MedievalVillage/Models/MV_Crate.glb"),
          MeshColliderComponent=SOLID)

    # ---- The bar (north-west; its west end stops at the stair landing) -----------------------------------------
    bar = b.add("Bar", room, TransformComponent=transform((-2.6, 2.2, 0)))
    b.add("Bar Counter", bar, TransformComponent=transform((0, 0, 0)),
          MeshComponent=shape("Box", (3.2, 0.7, 1.0), m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    b.add("Bar Top", bar, TransformComponent=transform((0, -0.05, 1.0)),
          MeshComponent=shape("Box", (3.4, 0.86, 0.08), m["Inn_Timber"]))
    b.add("Bar Shelf Low", bar, TransformComponent=transform((-0.2, 1.12, 1.4)),
          MeshComponent=shape("Box", (2.6, 0.32, 0.06), m["Inn_Timber"]))
    b.add("Bar Shelf High", bar, TransformComponent=transform((-0.2, 1.12, 1.95)),
          MeshComponent=shape("Box", (2.6, 0.32, 0.06), m["Inn_Timber"]))
    # The barrels stand behind the counter, under the shelves (the stair landing now fills the corner west of it).
    b.add("Bar Barrel 1", bar, TransformComponent=transform((-1.1, 0.85, 0)), MeshComponent=model("MedievalVillage/Models/MV_Barrel.glb"),
          MeshColliderComponent=SOLID)
    b.add("Bar Barrel 2", bar, TransformComponent=transform((-0.45, 0.95, 0), scale=(0.9, 0.9, 0.9)),
          MeshComponent=model("MedievalVillage/Models/MV_Barrel.glb"), MeshColliderComponent=SOLID)
    b.add("Kitchen Door", bar, TransformComponent=transform((2.1, 1.28, 0)),
          MeshComponent=shape("Box", (1.0, 0.1, 2.0), m["Inn_Door"]))

    # ---- The common room: two tables with stools, a rug ---------------------------------------------------------
    hall = b.add("Common Room", room, TransformComponent=transform())
    b.add("Rug", hall, TransformComponent=transform((0.9, -0.5, 0.004)),
          MeshComponent=shape("Plane", (2.8, 1.8, 0), m["Inn_Cloth"], False))
    for name, (x, y) in (("Table 1", (0.4, 0.6)), ("Table 2", (2.4, -1.7))):
        table = b.add(name, hall, TransformComponent=transform((x, y, 0)))
        b.add(name + " Top", table, TransformComponent=transform(),
              MeshComponent=dict(shape("Cylinder", (1.0, 1.0, 0.76), m["Inn_Furniture"]), Segments=12),
              MeshColliderComponent=SOLID)
        for i, (dx, dy) in enumerate(((-0.85, 0.0), (0.85, 0.0), (0.0, 0.8))):
            if name == "Table 1" and i == 1:
                continue  # the patron stands there
            b.add("%s Stool %d" % (name, i), table, TransformComponent=transform((dx, dy, 0)),
                  MeshComponent=dict(shape("Cylinder", (0.42, 0.42, 0.45), m["Inn_Timber"]), Segments=8),
                  MeshColliderComponent=SOLID)

    # ---- The stairs up (west wall, where the bed used to be: the beds are upstairs now) --------------------------
    stairs = b.add("Stairs", room, TransformComponent=transform())
    b.add("Stairs Up", stairs, TransformComponent=transform((STAIR_X, STAIR_FOOT_Y + STAIR_RUN / 2, 0)),
          MeshComponent=dict(shape("Stairs", (STAIR_WIDTH, STAIR_RUN, STAIR_RISE), m["Inn_Furniture"]), Steps=STAIR_STEPS),
          MeshColliderComponent=SOLID)
    b.add("Stair Landing", stairs, TransformComponent=transform((STAIR_X, STAIR_TOP_Y + 0.25, 0)),
          MeshComponent=shape("Box", (STAIR_WIDTH, 0.5, STAIR_RISE), m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    # The banister on the open (east) side: a newel at the foot and at the landing, a rail along the pitch.
    b.add("Stair Newel Foot", stairs, TransformComponent=transform((-3.98, STAIR_FOOT_Y + 0.05, 0)),
          MeshComponent=shape("Box", (0.14, 0.14, 1.05), m["Inn_Timber"]), MeshColliderComponent=SOLID)
    b.add("Stair Rail", stairs, TransformComponent=transform((-3.98, STAIR_FOOT_Y + STAIR_RUN / 2, STAIR_RISE / 2 + 0.95), (STAIR_PITCH, 0, 0)),
          MeshComponent=shape("Box", (0.07, STAIR_RAIL, 0.07), m["Inn_Timber"]))
    b.add("Stair Newel Top", stairs, TransformComponent=transform((-4.12, STAIR_TOP_Y + 0.06, STAIR_RISE)),
          MeshComponent=shape("Box", (0.14, 0.14, 1.0), m["Inn_Timber"]))
    # The stair head: a strip of the upper floor in the corner past the landing (walkable, on a joist) and the doorway
    # onto the floor above at the north wall's line, standing above the wall tops against the black. The doorway is
    # set back 1.6 m from the landing's edge because upright sprites lean back about 1 m at this tilt: nearer, it would
    # cut through Miko. "Door Upstairs" is in reach from the landing.
    b.add("Stair Head Floor", stairs, TransformComponent=transform((-4.525, 2.65, STAIR_RISE - 0.2)),
          MeshComponent=shape("Box", (0.95, 1.7, 0.2), m["Inn_FloorPlanks"]), MeshColliderComponent=SOLID)
    b.add("Stair Head Joist", stairs, TransformComponent=transform((-4.525, 1.88, STAIR_RISE - 0.42)),
          MeshComponent=shape("Box", (0.95, 0.16, 0.22), m["Inn_Timber"]))
    for i, x in enumerate((-4.92, -4.08)):
        b.add("Stair Head Post %d" % i, stairs, TransformComponent=transform((x, 3.4, STAIR_RISE)),
              MeshComponent=shape("Box", (0.12, 0.12, 2.0), m["Inn_Timber"]))
    b.add("Stair Head Lintel", stairs, TransformComponent=transform((STAIR_X, 3.4, STAIR_RISE + 2.0)),
          MeshComponent=shape("Box", (0.96, 0.14, 0.14), m["Inn_Timber"]))
    b.add("Stair Head Opening", stairs, TransformComponent=transform((STAIR_X, 3.44, STAIR_RISE)),
          MeshComponent=shape("Box", (0.72, 0.04, 2.0), None, False, tint=(0.04, 0.03, 0.025, 1)))
    b.add("Stair Sconce", stairs, TransformComponent=transform((-4.93, -1.4, 1.9)),
          MeshComponent=shape("Box", (0.08, 0.12, 0.18), None, False, emission=(1.0, 0.7, 0.35), tint=(1, 0.8, 0.5, 1)))

    # ---- Windows on the north wall and the light through them ---------------------------------------------------
    windows = b.add("Windows", room, TransformComponent=transform())
    for i, x in enumerate((1.0, 3.4)):
        window(b, windows, "Window %d" % i, x, m)

    # ---- Lights ----------------------------------------------------------------------------------------------------
    lights = root("Lights")
    b.add("Light Window 0", lights, TransformComponent=transform((1.0, 2.6, 0)), Light2DComponent=point_light((1, 0.66, 0.38), 1.2, 3.6, 1.8))
    b.add("Light Window 1", lights, TransformComponent=transform((3.4, 2.6, 0)), Light2DComponent=point_light((1, 0.66, 0.38), 1.2, 3.6, 1.8))
    b.add("Light Fire Hearth", lights, TransformComponent=transform((3.6, 0.9, 0)), Light2DComponent=point_light((1, 0.55, 0.24), 2.6, 5.5, 0.7))
    b.add("Light Fire Candle Bar", lights, TransformComponent=transform((-1.9, 2.1, 0)), Light2DComponent=point_light((1, 0.7, 0.4), 0.9, 2.6, 1.3))
    b.add("Light Fire Candle Table", lights, TransformComponent=transform((0.4, 0.6, 0)), Light2DComponent=point_light((1, 0.7, 0.4), 0.8, 2.4, 1.0))
    b.add("Light Fire Sconce Stairs", lights, TransformComponent=transform((-4.6, -1.4, 0)), Light2DComponent=point_light((1, 0.7, 0.4), 0.8, 2.6, 1.9))


def build_upstairs_room(b, m):
    """The guest floor: the stair head and a hall on the west, a passage along the open front, two rooms to rent."""
    root = lambda tag: b.add(tag, None, TransformComponent=transform())  # noqa: E731
    height = 2.8
    room = root("Room")

    # ---- Floor with the stairwell cut out, walls, the open front ------------------------------------------------
    b.add("Floor", room, TransformComponent=transform((0.475, 0, -0.3)),
          MeshComponent=shape("Box", (9.05, 7, 0.3), m["Inn_FloorPlanks"]), MeshColliderComponent=SOLID)
    b.add("Floor Stairwell South", room, TransformComponent=transform((-4.525, -2.25, -0.3)),
          MeshComponent=shape("Box", (0.95, 2.5, 0.3), m["Inn_FloorPlanks"]), MeshColliderComponent=SOLID)
    b.add("Floor Stairwell North", room, TransformComponent=transform((-4.525, 2.4, -0.3)),
          MeshComponent=shape("Box", (0.95, 2.2, 0.3), m["Inn_FloorPlanks"]), MeshColliderComponent=SOLID)
    shell(b, room, m, height, (-4.85, -2.4, 1.3, 4.85), (3.2, -2.8))
    b.add("Front Sill", room, TransformComponent=transform((0, -3.6, -0.3)),
          MeshComponent=shape("Box", (10.6, 0.3, 0.5), m["Inn_Timber"]))
    b.add("Front Blocker", room, TransformComponent=transform((0, -3.75, 0)),
          BoxColliderComponent=box_collider((10.6, 0.3, 2.4), (0, 0, 1.2)))

    # ---- The stairwell: the flight going down (drawn, not walked), a banister round it, a blocker over it --------
    well = b.add("Stairwell", room, TransformComponent=transform())
    b.add("Stairs Down", well, TransformComponent=transform((STAIR_X, STAIR_FOOT_Y + STAIR_RUN / 2, -STAIR_RISE)),
          MeshComponent=dict(shape("Stairs", (STAIR_WIDTH, STAIR_RUN, STAIR_RISE), m["Inn_Furniture"], False), Steps=STAIR_STEPS))
    b.add("Stairwell Blocker", well, TransformComponent=transform((-4.525, 0.1, 0)),
          BoxColliderComponent=box_collider((0.95, 2.2, 2.0), (0, 0, 1.0)))
    b.add("Banister East", well, TransformComponent=transform((-4.0, 0.12, 0.9)),
          MeshComponent=shape("Box", (0.07, 2.3, 0.07), m["Inn_Timber"]))
    b.add("Banister South", well, TransformComponent=transform((-4.5, -1.0, 0.9)),
          MeshComponent=shape("Box", (1.0, 0.07, 0.07), m["Inn_Timber"]))
    for i, (x, y) in enumerate(((-4.0, -1.0), (-4.0, 1.25))):
        b.add("Banister Newel %d" % i, well, TransformComponent=transform((x, y, 0)),
              MeshComponent=shape("Box", (0.12, 0.12, 1.05), m["Inn_Timber"]), MeshColliderComponent=SOLID)
    for i, y in enumerate((-0.55, -0.1, 0.35, 0.8)):
        b.add("Baluster %d" % i, well, TransformComponent=transform((-4.0, y, 0)),
              MeshComponent=shape("Box", (0.05, 0.05, 0.9), m["Inn_Timber"]))

    # ---- Partitions: the hall | room 1 | room 2. Cut at y -2.0 like the outer front wall; the rooms' fronts are a
    # low sill with a threshold (an unseen wall above the sill so the doorway is the way in), the doors stand open.
    parts = b.add("Partitions", room, TransformComponent=transform())
    for name, x in (("Hall", -2.4), ("Rooms", 1.3)):
        b.add("Partition %s" % name, parts, TransformComponent=transform((x, 0.75, -0.3)),
              MeshComponent=shape("Box", (0.16, 5.5, height + 0.3), m["Inn_WallPlaster"]), MeshColliderComponent=SOLID)
        b.add("Partition %s Post" % name, parts, TransformComponent=transform((x, -2.0, 0)),
              MeshComponent=shape("Box", (0.22, 0.22, height), m["Inn_Timber"]))
        b.add("Partition %s Beam" % name, parts, TransformComponent=transform((x, -2.0, height - 0.45), (-HALF_PI, 0, 0)),
              MeshComponent=shape("Box", (0.2, 0.2, 5.5), m["Inn_Timber"]))
    # (name, west x, east x, doorway west x, doorway east x)
    for name, west, east, door_w, door_e in (("Room 1", -2.32, 1.22, -0.95, 0.15), ("Room 2", 1.38, 5.0, 2.7, 3.8)):
        for side, (a, z) in (("West", (west, door_w)), ("East", (door_e, east))):
            length = z - a
            b.add("%s Sill %s" % (name, side), parts, TransformComponent=transform(((a + z) / 2, -2.0, 0)),
                  MeshComponent=shape("Box", (length, 0.16, 0.2), m["Inn_Timber"]),
                  BoxColliderComponent=box_collider((length, 0.16, 2.2), (0, 0, 1.1)))
        b.add("%s Threshold" % name, parts, TransformComponent=transform(((door_w + door_e) / 2, -2.0, 0.004)),
              MeshComponent=shape("Plane", (door_e - door_w, 0.2, 0), m["Inn_StoneCut"], False))
        b.add("%s Door" % name, parts, TransformComponent=transform((door_e + 0.05, -1.5, 0)),
              MeshComponent=shape("Box", (0.06, 0.95, 2.0), m["Inn_Door"]), MeshColliderComponent=SOLID)

    # ---- The hall at the stair head: a linen press and a lamp table ---------------------------------------------
    hall = b.add("Hall", room, TransformComponent=transform())
    b.add("Linen Press", hall, TransformComponent=transform((-3.25, 3.25, 0)),
          MeshComponent=shape("Box", (1.2, 0.45, 1.8), m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    b.add("Linen Press Cloth", hall, TransformComponent=transform((-3.25, 3.0, 0.9)),
          MeshComponent=shape("Box", (0.9, 0.05, 0.6), m["Inn_Linen"]))
    # A wall lamp over the stairwell (a table by the partition would be hidden from the camera, which stays east of it).
    b.add("Hall Lamp", hall, TransformComponent=transform((-4.93, 0.2, 1.6)),
          MeshComponent=shape("Box", (0.08, 0.14, 0.2), None, False, emission=(1.0, 0.7, 0.35), tint=(1, 0.8, 0.5, 1)))
    b.add("Hall Lamp Bracket", hall, TransformComponent=transform((-4.95, 0.2, 1.45)),
          MeshComponent=shape("Box", (0.1, 0.2, 0.08), m["Inn_Timber"]))
    b.add("Passage Runner", hall, TransformComponent=transform((0.0, -2.8, 0.004)),
          MeshComponent=shape("Plane", (8.6, 0.8, 0), m["Inn_Cloth"], False))

    # ---- Room 1: the room the innkeeper gives the shrine's people (the party rests here) -----------------------
    room1 = b.add("Room 1", room, TransformComponent=transform())
    bed(b, room1, "Bed Room 1", (-1.35, 2.4, 0), m)
    candle_table(b, room1, "Room 1 Side Table", (-0.42, 3.15, 0), m)
    b.add("Room 1 Chest", room1, TransformComponent=transform((-1.35, 0.95, 0)),
          MeshComponent=shape("Box", (0.9, 0.5, 0.48), m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    b.add("Room 1 Chest Lid", room1, TransformComponent=transform((-1.35, 0.95, 0.48)),
          MeshComponent=shape("Box", (0.94, 0.54, 0.06), m["Inn_Timber"]))
    b.add("Room 1 Washstand", room1, TransformComponent=transform((0.9, 2.2, 0)),
          MeshComponent=shape("Box", (0.5, 0.5, 0.8), m["Inn_Furniture"]), MeshColliderComponent=SOLID)
    b.add("Room 1 Basin", room1, TransformComponent=transform((0.9, 2.2, 0.8)),
          MeshComponent=dict(shape("Cylinder", (0.4, 0.4, 0.1), m["Inn_StoneCut"]), Segments=10))
    b.add("Room 1 Cloak", room1, TransformComponent=transform((1.17, 0.7, 0.75)),
          MeshComponent=shape("Box", (0.06, 0.4, 1.0), m["Inn_Cloth"]))
    b.add("Room 1 Rug", room1, TransformComponent=transform((-0.3, 0.2, 0.004)),
          MeshComponent=shape("Plane", (1.6, 1.0, 0), m["Inn_Linen"], False))
    window(b, room1, "Window Room 1", 0.35, m)

    # ---- Room 2: another traveller's room, two narrow beds, a pack by the wall ---------------------------------
    room2 = b.add("Room 2", room, TransformComponent=transform())
    bed(b, room2, "Bed Room 2 West", (2.05, 2.45, 0), m, 0.9, 2.0)
    bed(b, room2, "Bed Room 2 East", (4.35, 2.45, 0), m, 0.9, 2.0)
    candle_table(b, room2, "Room 2 Table", (3.2, 3.15, 0), m, (0.55, 0.45, 0.65))
    b.add("Room 2 Crate", room2, TransformComponent=transform((4.45, 0.2, 0), scale=(0.8, 0.8, 0.8)),
          MeshComponent=model("MedievalVillage/Models/MV_Crate.glb"), MeshColliderComponent=SOLID)
    b.add("Room 2 Pack", room2, TransformComponent=transform((3.75, 0.25, 0)),
          MeshComponent=shape("Box", (0.45, 0.3, 0.55), m["Inn_Cloth"]), MeshColliderComponent=SOLID)
    window(b, room2, "Window Room 2", 3.2, m)

    # ---- Lights ----------------------------------------------------------------------------------------------------
    lights = root("Lights")
    b.add("Light Window Room 1", lights, TransformComponent=transform((0.35, 2.6, 0)), Light2DComponent=point_light((1, 0.66, 0.38), 1.1, 3.4, 1.8))
    b.add("Light Window Room 2", lights, TransformComponent=transform((3.2, 2.6, 0)), Light2DComponent=point_light((1, 0.66, 0.38), 1.1, 3.4, 1.8))
    b.add("Light Fire Candle Room 1", lights, TransformComponent=transform((-0.37, 3.15, 0)), Light2DComponent=point_light((1, 0.7, 0.4), 1.0, 2.8, 0.85))
    b.add("Light Fire Candle Room 2", lights, TransformComponent=transform((3.25, 3.15, 0)), Light2DComponent=point_light((1, 0.7, 0.4), 0.9, 2.8, 0.9))
    b.add("Light Fire Lamp Hall", lights, TransformComponent=transform((-4.6, 0.2, 0)), Light2DComponent=point_light((1, 0.7, 0.4), 1.0, 3.2, 1.6))


def player_and_camera(b, position, clip):
    """Copies of the Farm Village's player and camera, the camera closer."""
    b.add("Player", None, TransformComponent=transform(position, scale=(3, 2.25, 3)),
          SpriteRendererComponent={"Color": [1, 1, 1, 1], "TilingFactor": 1, "SortOrder": 96, "TextureHandle": MIKO_TEXTURE,
                                   "Sprite": "Miko_0", "SpriteIndex": 0, "FlipX": False, "FlipY": False, "Facing": "Upright"},
          SpriteAnimatorComponent={"AnimationsHandle": MIKO_ANIMATIONS, "StartClip": clip, "PlayOnStart": True, "Speed": 1},
          Shadow2DComponent={"Offset": [0, 0.05], "Size": [0.85, 0.32], "Strength": 0.55},
          CharacterBodyComponent={"Radius": 0.25, "Height": 1.4, "MaxSlopeDegrees": 50, "StepHeight": 0.35, "StaysOnLedges": True,
                                  "Layer": "Player"},
          NativeScriptComponent={"Scripts": [{"Name": "DioramaWalkScript", "Properties": {"RunMultiplier": 1.7, "m_Speed": 3.0}}]})
    b.add("Camera", None, TransformComponent=transform(position),
          CameraComponent={"Primary": True, "Projection": "Orthographic", "PerspectiveFOV": 45, "PerspectiveNear": 0.01,
                           "PerspectiveFar": 1000, "OrthographicSize": 7, "OrthographicNear": -1, "OrthographicFar": 1},
          NativeScriptComponent={"Scripts": [{"Name": "CameraFollowScript",
                                              "Properties": {"Bounds": [3, 3], "LookAhead": 0.5, "Smoothing": 8}}]})


def directors(b, room_name):
    """The room's framing and light, the field's interaction and UI."""
    group = b.add("Directors", None, TransformComponent=transform())
    b.add("Inn Director", group, TransformComponent=transform(),
          NativeScriptComponent={"Scripts": [{"Name": "InteriorScript", "Properties": {
              "RoomName": room_name, "TiltDegrees": 52, "FieldOfView": 30, "FocusLead": 0.6, "FocusRise": 0.8,
              "BoundsMin": [-1.5, -1.2], "BoundsMax": [1.5, 1.2], "StartPreset": 1}}]})
    b.add("MVP Director", group, TransformComponent=transform(),
          NativeScriptComponent={"Scripts": [{"Name": "MvpDirectorScript", "Properties": {
              "ArriveTime": 0.6, "CloseRange": 0.6, "EyeHeight": 1, "FacingDegrees": 80, "InteractRange": 1.7, "ItemHeight": 0.6,
              "LeaveTime": 0.6, "MaxReachHeight": 1, "MessageTime": 3, "RestTime": 1.2, "SwitchMargin": 0.3}}]})


def build_scene(b, m):
    player_and_camera(b, (0, -2.6, 0), "IdleUp")
    build_room(b, m)

    # ---- People and things to use -------------------------------------------------------------------------------
    people = b.add("Characters", None, TransformComponent=transform())
    # The innkeeper stands behind the bar; the player talks to her across it through "Npc Innkeeper", a point at the
    # counter's front (the line of sight from the player's chest to a figure behind a 1 m counter would hit it).
    b.add("Innkeeper", people, TransformComponent=transform((-2.4, 2.95, 0), scale=(3, 2.25, 1)),
          **villager(BARMAID_TEXTURE, "Barmaid_Down_0", BARMAID_ANIMATIONS))
    b.add("Patron", people, TransformComponent=transform((1.3, 0.6, 0), scale=(3, 2.25, 1)),
          **villager(GATHERER_TEXTURE, "Gatherer_Down_0", GATHERER_ANIMATIONS, "IdleLeft"))
    things = b.add("Interactables", None, TransformComponent=transform())
    b.add("Npc Innkeeper", things, TransformComponent=transform((-2.4, 1.7, 0)))
    b.add("Npc Patron", things, TransformComponent=transform((1.3, 0.42, 0)))
    # At the stair head, in front of its doorway (in reach from the landing): pushes FarmVillageInnUpstairs; its "Door Out" brings the player back here.
    b.add("Door Upstairs", things, TransformComponent=transform((STAIR_X, 2.9, STAIR_RISE + 0.05)))
    b.add("Door Out", things, TransformComponent=transform((0, -3.2, 0)))

    directors(b, "Golden Sheaf Inn")


def build_upstairs(b, m):
    # The player arrives at the stair head (the north end of the stairwell), facing down the passage.
    player_and_camera(b, (STAIR_X, 1.75, 0), "IdleDown")
    build_upstairs_room(b, m)

    things = b.add("Interactables", None, TransformComponent=transform())
    b.add("Rest Bed", things, TransformComponent=transform((-0.45, 1.9, 0)))
    # Back down the stairs: pops to the common room, where the player stands on the landing again.
    b.add("Door Out", things, TransformComponent=transform((STAIR_X, 1.3, 0)))

    directors(b, "Golden Sheaf Inn, upstairs")


HEADER = """Handle: %d
Scene: %s
RenderSettings:
  OverrideProject: true
  Features:
    Depth: Shared
    SpriteSorting: ViewDepth
    Lighting: Forward
    DefaultSpriteAlpha: Cutout
    Meshes: true
    Shadows: Off
    SpriteShadows: AsDrawn
    UprightSprites: Vertical
    HDR: true
    Bloom: true
Look:
  ProfileHandle: %d
  Environment: Profile
  PostProcess: Profile
Environment:
  SkyAmbient: [0.26, 0.2, 0.16]
  GroundAmbient: [0.1, 0.07, 0.05]
  Fog: false
  FogColor: [0, 0, 0]
  FogStart: 40
  FogEnd: 80
Entities:
"""


def write_scene(name, file_name, build, materials, profile):
    b = ui.Builder(name)
    build(b, materials)
    text = HEADER % (handle("Scene" + name), name, profile) + ui.dump_entities(b.entities, "  ") + "\n"
    write_crlf(os.path.join(ui.SCENES, file_name), text)
    print("%s: %d entities" % (file_name, len(b.entities)))
    # The field UI (inventory, dialogue, prompt, fade) under the MVP Director, as in the field scenes.
    ui.rebuild(file_name, ui.FIELD_REMOVE, lambda director: ui.build_field(name, director))


def main():
    # Materials and profiles: written when missing, so tuning them in the editor isn't undone by a rerun.
    materials = {}
    for name, (albedo, tiling, tint) in MATERIALS.items():
        path = os.path.join(ASSETS, "Interiors", "Materials", name + ".hymat")
        if write_crlf(path, material_file(albedo, tiling, tint, name), overwrite=False):
            print("wrote", os.path.relpath(path, ui.ROOT))
        with open(path, encoding="utf-8") as file:
            materials[name] = int(file.read().split("Handle:")[1].split()[0])
    for name, values in PROFILES.items():
        path = os.path.join(ASSETS, "Interiors", "RenderProfiles", name + ".hyrender")
        if write_crlf(path, profile_file(name, *values), overwrite=False):
            print("wrote", os.path.relpath(path, ui.ROOT))
    with open(os.path.join(ASSETS, "Interiors", "RenderProfiles", "Evening.hyrender"), encoding="utf-8") as file:
        evening = int(file.read().split("Handle:")[1].split()[0])

    write_scene(SCENE_NAME, SCENE_FILE, build_scene, materials, evening)
    write_scene(UPSTAIRS_NAME, UPSTAIRS_FILE, build_upstairs, materials, evening)


if __name__ == "__main__":
    main()
