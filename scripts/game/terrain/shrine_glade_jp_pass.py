"""Shrine Glade, Japanese pass (Nov 2, 2026): brings Miko's starting scene in line with the Japanese city and temple.

The Shrine Glade was dressed from the medieval farm village's library before the Japanese kit existed, so its frame
mixed a Japanese shrine with a European garden. This pass edits the saved scene and its terrain in place (the scene is
hand-dressed; build_shrine_glade.py only re-seats it on the terrain), and is safe to run again: it only touches
entities that still use the old assets.

  - Trees: the pointed firs behind the glade become Japanese cedars (sugi); the oaks along the edges become black
    pines with a red maple in every third place; the tall narrow oak becomes a cedar. Heights are kept; widths follow
    the new sprite's proportions, tints and flips are kept.
  - Shrubs: lavender, the round yellow-green bushes and the generic shrub cards become the delivered Japanese
    shrubs (nandina, azalea, camellia, bamboo grass, fern, mondo grass), at most 1.6 times their drawn size; the big
    framing plants along the frame's bottom edge become young bamboo and maples at their old height.
  - Fences: the brown picket-fence cards become the kit's 3D bamboo fence (JP_Prop_FenceBamboo_2m_A), one section per
    card, scaled to the card's width and turned along the fence line, with the model's collider.
  - Light: the two stone lanterns' lights sit lower in the firebox (1.0 m instead of 1.33) and a little stronger, so
    they pool on the paving, as in the temple district.
  - Terrain decor (ShrineGlade.hyterrain): the same shrub swap in the scattered decor lists.
  - Shrine dressing (Nov 2, 22:50; 23:15): a pair of nobori at the top of the stairs; a slatted offering
    box over the shrine model's plain one, an altar inside the door, a bell rope (suzu) and two glowing paper lanterns
    under the eaves (the lanterns lit: the frame's hero light);
    the two Western banners on stands become nobori on bamboo poles; the notice board ("Sign Shrine") becomes the
    Japanese notice board; a hand-washing pavilion (temizuya, dark tiles) on the lower plaza by the torii; a row of jizo and moss
    stones in front of the graves. New entities sit under "Japanese Dressing" with stable ids: never added twice,
    and the pass owns them (a run puts them back as written here; move them in this script, not in the editor).
  - Consistency (Nov 2, late): the shrine hall and the torii were older, flatter models than the kit pieces beside
    them; they become the kit's JP_Shrine_Haiden_A (same footprint, so the colliders still fit) and
    JP_Shrine_Torii_A (a myojin torii). The props on the hall move to the new hall's veranda, beam and eave.
  - Clutter (Nov 2, late): the pair of nobori at the top of the stairs is retired (it crowded the door with the
    elder, the quest marker and the hall's props, and hid the stone lanterns); RETIRED entities are removed.
  - Life (Nov 2, late): fireflies (green glowing motes drifting low around the trees and bushes on every level) and
    cherry petals falling from the weeping cherry beside the hall, as particle emitters (ParticleEmitter2DComponent).
  - Leaves and foxes (Nov 2, 23:55, the user's sprites): falling leaves from the user's leaf sprites
    (Sprites/Effects/FX_Leaves_Green, a 4 x 4 sheet, and FX_Leaf_Pink, at 12 px): green ones from the sacred tree
    and the right-hand edge trees, pink ones from the weeping cherry, and the pink leaf tinted red under the young
    maples by the torii. A pair of stone fox statues (kitsune, Inari's messengers; JP_Shrine_Kitsune_A) guards the
    approach just inside the torii, where the komainu would stand.
  - Statues (Nov 3, 00:15, the user's sprites): a pair of stone komainu (JP_Shrine_Komainu_A/B, sprites; the
    models never read) at the foot of the upper stairs, guarding the hall's approach without crowding its door; a
    sacred boulder bound with a shimenawa (iwakura, JP_Shrine_Iwakura_A) on the lower plaza opposite the temizuya;
    the old jizo sprite (Jizo.png, the "Stone Jizo" by the upper fence and one in the jizo row) becomes the red-capped
    JP_Shrine_Jizo_A. The weeping cherry drops the user's sakura leaves (FX_Leaves_Sakura, an 8 x 8 sheet).
  - Nov 3, 00:50: a calico shrine cat (the user's PixelLab cat, Characters/NPC/Cat, 38.4 px/m) wanders slowly to and fro
    in front of the jizo row on the shrine terrace, stopping now and then, and at every other stop lies down to sleep
    (NpcWanderScript with WorkStartClip / WorkClip / WorkEndClip: LieDown, Sleep, GetUp; no collider, and not an
    "Npc", so it is never talked to).
  - Nov 3, 00:40: the Old Monk ("Npc Elder") moves from beside the hall's door to the sacred tree's roots (the door
    and its quest marker stop crowding each other, and he stands by the tree his errand is about); the young bamboo
    and maples along the frame's bottom ("Front Canopy") sway (FoliageSwayScript on the group); lighting: the four
    stone lanterns lit alike (two had kept the old, higher light), a warm glow in the sanctuary behind the open door,
    each firefly cloud lights the ground under it a little, and the dusk profile's bloom picks up the lanterns and
    the fireflies (Dusk.hyrender: threshold 1 -> 0.85, intensity 0.6 -> 0.8).
  - Nov 3, 01:10: a vignette in the three time-of-day profiles (the engine's new ScenePostProcess::Vignette*: day 0.25,
    dusk 0.5, night 0.6), darker frame edges in the Game view only (the Editor view leaves it out).

    python scripts/game/terrain/shrine_glade_jp_pass.py [--dry-run]

Close the scene in the editor first. Backups go to bin-int/shrine_glade_backup/.
"""

import argparse
import math
import os
import re
import shutil
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(HERE, ".."))
import look  # noqa: E402  (scripts/game/look.py)
ASSETS = os.path.join(REPO, "Oxygen", "project", "Assets")
SCENE = os.path.join(ASSETS, "Scenes", "ShrineGlade.hyscene")
TERRAIN = os.path.join(ASSETS, "Terrains", "ShrineGlade.hyterrain")
FENCE_MODEL = "JP_Prop_FenceBamboo_2m_A"

TREES = {   # old sprite -> new sprites (cycled per entity)
    "FirTallA": ["JP_Tree_Sugi_A_Large", "JP_Tree_Sugi_B_Large"],
    "FirTallB": ["JP_Tree_Sugi_B_Large", "JP_Tree_Sugi_A_Large"],
    "FirTallC": ["JP_Tree_Sugi_A_Large"],
    "FirTallD": ["JP_Tree_Sugi_B_Large"],
    "OakSunA": ["JP_Tree_BlackPine_A_Large", "JP_Tree_BlackPine_B_Large", "JP_Tree_Maple_Red_A_Large"],
    "OakSunB": ["JP_Tree_BlackPine_B_Large", "JP_Tree_Maple_Red_B_Large", "JP_Tree_BlackPine_A_Large"],
    "OakSunC": ["JP_Tree_Sugi_A_Large"],
}
SHRUBS = {  # old sprite (prefix) -> new sprites (cycled)
    "Lavender": ["JP_Shrub_Nandina", "JP_Shrub_Azalea_A", "JP_Shrub_Fern_V2", "JP_Shrub_Nandina_V2", "JP_Shrub_Azalea_A_V2"],
    "BushDense": ["JP_Shrub_Azalea_B", "JP_Shrub_Camellia", "JP_Shrub_Sasa", "JP_Shrub_Camellia_V2", "JP_Shrub_Azalea_B_V2"],
    "BushSmall": ["JP_Shrub_Sasa_V2", "JP_Shrub_Mondo", "JP_Shrub_Fern"],
    "ShrubLarge": ["JP_Shrub_Camellia_V2", "JP_Shrub_Hedge"],
    "ShrubVertical": ["JP_Shrub_Nandina"],
    "ShrubFernHeavy": ["JP_Shrub_Fern"],
    "ShrubLow": ["JP_Shrub_Sasa"],
    "PlantSmallBush": ["JP_Shrub_Mondo_V2"],
}
SHRUB_MAX_ZOOM = 1.6      # a shrub card is drawn at most this many times its pixel size (32 px per metre)
# the big framing plants along the bottom of the frame ("Canopy Front"): young bamboo and maples, drawn at their own
# height (the shrubs, blown up to 2.3 m, turned into blocks of fat pixels there)
CANOPY = ["JP_Tree_Bamboo_Young_A_Large", "JP_Tree_Maple_Young_B_Large", "JP_Tree_Bamboo_Young_D_Large",
          "JP_Tree_Bamboo_Young_F_Large", "JP_Tree_Maple_Young_D_Large", "JP_Tree_Bamboo_Young_C_Large"]
LANTERN_TAGS = ("Light Lantern Plaza", "Light Lantern Shrine")
PROPS_R1 = os.path.join(REPO, "art_incoming", "jp_city", "props_r1")
JP_SPRITES = os.path.join(ASSETS, "Sprites", "Props", "JapaneseCity")
SHRINE = (0.0, 7.9, 3.0)        # the shrine model's origin; its platform top is 0.45 up, its front edge 2.5 forward
# the kit's hall and torii replace the old models (old file -> kit prefab), on the same entities
MODEL_SWAPS = {"ShrineHall.glb": "JP_Shrine_Haiden_A.glb", "ToriiGate.glb": "JP_Shrine_Torii_A.glb"}
# where JP_Shrine_Haiden_A takes its props (its info in the kit's summary): deck top, the offering box's front,
# the bell's hanging point, the eave lanterns' hanging points, the sanctuary behind the open middle bay
HALL = dict(deck_z=0.52, box_front_y=-2.33, bell=(0.0, -1.72, 2.64), lantern=(2.05, -2.7, 3.074), door_y=-1.35)
EAVE_LANTERN, EAVE_CORD = "JP_Prop_LanternHanging_B", 0.7   # the long-corded lantern: below the eave, in view
EFFECTS = os.path.join(ASSETS, "Sprites", "Effects")
# the monk's spot by the sacred tree; moved only while he still stands where he was placed (or at 00:40's first try)
ELDER_FROM, ELDER_TO = ((1.71964, 4.12041), (-4.9, 5.35)), (-5.5, 4.9, 3.0)
CAT = dict(texture=707953810633971444, animations=1464375335230931911, player=4271470499060943565)   # Characters/NPC/Cat
SWAY = ("Front Canopy", "    NativeScriptComponent:\n      Scripts:\n        - Name: FoliageSwayScript\n          Properties:\n"
        "            Degrees: 2.2\n            Period: 3.4\n            Gust: 0.5\n")
PROFILE_DIR = os.path.join(ASSETS, "RenderProfiles", "ShrineGlade")
PROFILE_EDITS = {   # per time of day: replacements in its PostProcess map, and its vignette (Nov 3; appended once)
    "Dusk": ((("BloomThreshold: 1\n", "BloomThreshold: 0.85\n"), ("BloomIntensity: 0.600000024\n", "BloomIntensity: 0.8\n")),
             (0.5, 0.42, 0.6)),
    "Day": ((), (0.3, 0.45, 0.6)),
    "Night": ((), (0.6, 0.38, 0.6)),
}
RETIRED = ("komainu0", "komainu1", "nobori-shrine0", "nobori-shrine1")     # dressing keys removed from the scene


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])


def handles():
    """handle -> asset path (relative to Assets, without .hymeta), for the sprite and model folders."""
    out = {}
    for sub in ("Sprites", os.path.join("Models", "JapaneseCity"), os.path.join("Models", "Architecture")):
        for root, _, files in os.walk(os.path.join(ASSETS, sub)):
            for name in files:
                if name.endswith(".hymeta") is False:
                    continue
                p = os.path.join(root, name)
                with open(p, encoding="utf-8", errors="ignore") as f:
                    m = re.search(r"Handle: (\d+)", f.read(200))
                if m:
                    out[int(m.group(1))] = os.path.relpath(p, ASSETS)[:-7].replace("\\", "/")
    return out


def entities(text):
    """The scene split into (header, [entity blocks]); each block starts with '\\n  - Entity: '."""
    parts = re.split(r"(?=\n  - Entity: )", text)
    return parts[0], parts[1:]


def field(block, name):
    m = re.search(r"%s: \[([^\]]*)\]" % name, block)
    return [float(v) for v in m.group(1).split(",")] if m else None


def set_field(block, name, values):
    return re.sub(r"(%s: )\[[^\]]*\]" % name, lambda m: m.group(1) + "[%s]" % ", ".join("%.6g" % v for v in values), block, count=1)


def stable_id(key):
    import hashlib
    return int(hashlib.sha1(("glade-jp:" + key).encode()).hexdigest()[:16], 16) & (2 ** 63 - 1)


def install_sprite(name):
    """A props_r1 sprite (already at 32 px/m) copied into Sprites/Props/JapaneseCity with a meta, if it isn't there."""
    png = os.path.join(JP_SPRITES, name + ".png")
    if os.path.exists(png) is False:
        shutil.copy2(os.path.join(PROPS_R1, name + ".png"), png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % stable_id("sprite:" + name))
    return png


def card_body(handle, sx, flip=False, sort=1, shadow=0.45):
    b = ("    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: %d\n      TextureHandle: %d\n"
         "      SpriteIndex: 0\n      FlipX: %s\n      FlipY: false\n      Facing: Upright\n") % (sort, handle, "true" if flip else "false")
    if shadow:
        b += ("    Shadow2DComponent:\n      Offset: [0, -0.02]\n      Size: [%g, %g]\n      Strength: %g\n"
              % (round(sx * 0.85, 2), round(min(0.5, sx * 0.35), 2), shadow))
    return b


def model_body(handle, collider=True):
    b = ("    MeshComponent:\n      ModelHandle: %d\n      Shape: Box\n      Size: [1, 1, 1]\n      CastShadows: true\n"
         "      Materials:\n        []\n") % handle
    if collider:
        b += "    MeshColliderComponent:\n      Layer: World\n"
    return b


def particle_body(**kw):
    """A ParticleEmitter2DComponent (EntitySerializer's keys); kw overrides the defaults below (a firefly cloud)."""
    p = dict(PlayOnStart="true", Looping="true", Duration=4, Rate=1.0, BurstCount=0, MaxParticles=8, Seed=0, Shape="Box",
             ShapeSize=(4, 2), Lifetime=(3.5, 6.5), Speed=(0.05, 0.3), Direction=90, Spread=180, StartSize=(0.15, 0.24),
             StartRotation=(0, 0), AngularVelocity=(0, 0), EndSizeScale=0.5, StartColor=(0.72, 1.0, 0.38, 0.95),
             EndColor=(0.45, 0.9, 0.3, 0.0), Gravity=(0, 0), Drag=0.5, Turbulence=0.9, TextureHandle=None, SpriteIndex=0, Blend="Additive",
             Lighting="Lit", Space="Ground", SortOrder=2, EmissionIntensity=5, LightRadius=0, LightIntensity=1,
             MaxLights=0, DestroyWhenFinished="false")
    p.update(kw)
    out = "    ParticleEmitter2DComponent:\n"
    for k, v in p.items():
        if v is None:
            continue
        if isinstance(v, (tuple, list)):
            v = "[%s]" % ", ".join("%g" % x for x in v)
        elif isinstance(v, float):
            v = "%g" % v
        out += "      %s: %s\n" % (k, v)
    return out


def entity(eid, tag, parent, t, body, s=(1, 1, 1), rz=0.0):
    rel = "    RelationshipComponent:\n      Parent: %d\n" % parent if parent else ""
    return ("\n  - Entity: %d\n    TagComponent:\n      Tag: %s\n%s    TransformComponent:\n      Translation: [%s]\n"
            "      Rotation: [0, 0, %.6g]\n      Scale: [%s]\n%s") % (
        eid, tag, rel, ", ".join("%.6g" % v for v in t), rz, ", ".join("%.6g" % v for v in s), body)


def dress(blocks, H, by_name):
    """The shrine's dressing (see the module doc). Returns (blocks, count)."""
    def handle(file_name):
        return by_name[file_name][0]

    def png(name, folder=JP_SPRITES):
        p = os.path.join(folder, name + ".png")
        with open(p + ".hymeta", encoding="utf-8") as f:              # this folder's file (names repeat elsewhere)
            return int(re.search(r"Handle: (\d+)", f.read()).group(1)), png_size(p)

    index = {int(re.search(r"Entity: (\d+)", b).group(1)): i for i, b in enumerate(blocks)}
    added = []
    updated = []
    group = stable_id("group")
    if group not in index:
        added.append(entity(group, "Japanese Dressing", None, (0, 0, 0), ""))

    def add(key, tag, t, body, s=(1, 1, 1), rz=0.0):
        eid = stable_id(key)
        text = entity(eid, tag, group, t, body, s, rz)
        if eid not in index:
            added.append(text)
        elif blocks[index[eid]].rstrip("\n") != text.rstrip("\n"):         # the pass owns these: placed as written here
            blocks[index[eid]] = text + ("\n" if blocks[index[eid]].endswith("\n") else "")
            updated.append(tag)

    def card(key, tag, name, x, y, z, scale=1.0, flip=False, hanging=False, shadow=0.45, sort=1, folder=JP_SPRITES):
        h, (w, hh) = png(name, folder)
        sx, sy = w / 32.0 * scale, hh / 32.0 * scale
        add(key, tag, (x, y, z - sy if hanging else z), card_body(h, sx, flip, sort, 0 if hanging else shadow), (sx, sy, 1))

    def model(key, tag, name, x, y, z, rz=0.0, collider=True):
        add(key, tag, (x, y, z), model_body(handle(name + ".glb"), collider), rz=rz)

    sx_, sy_, sz_ = SHRINE
    deck = sz_ + HALL["deck_z"]
    # 1. the hall's props (the kit hall, Nov 2, late): the slatted offering box on the veranda's front edge, the altar
    # in the sanctuary behind the open middle bay, the bell rope from its cross beam, a lit paper lantern under each
    # corner of the front eave (the frame's hero light)
    model("offering-box", "Shrine Offering Box", "JP_Prop_SaisenBako_A", 0.0, sy_ + HALL["box_front_y"], deck)
    model("altar", "Shrine Altar", "JP_Prop_ShrineAltar_A", 0.0, sy_ + HALL["door_y"] + 0.42, deck, collider=False)
    bx, by, bz = HALL["bell"]
    model("suzu", "Shrine Bell Rope", "JP_Prop_SuzuRope_A", bx, sy_ + by, sz_ + bz, collider=False)
    lx, ly, lz = HALL["lantern"]
    for k, x in enumerate((-lx, lx)):
        model("eave-lantern%d" % k, "Shrine Eave Lantern %d" % (k + 1), EAVE_LANTERN, x, sy_ + ly, sz_ + lz,
              collider=False)
        add("eave-light%d" % k, "Light Lantern Eave %d" % (k + 1), (x, sy_ + ly, deck),
            "    Light2DComponent:\n      Type: Point\n      Color: [1, 0.72, 0.42]\n      Intensity: 1.7\n      Radius: 4.5\n"
            "      Height: %.3g\n" % (sz_ + lz - EAVE_CORD - 0.29 - deck))
    # a warm glow in the sanctuary behind the open middle bay, so the door reads as lit from inside
    add("sanctuary-light", "Light Lantern Sanctuary", (0.0, sy_ + HALL["door_y"] + 0.55, deck),
        "    Light2DComponent:\n      Type: Point\n      Color: [1, 0.68, 0.36]\n      Intensity: 1.4\n      Radius: 2.2\n"
        "      Height: 1.1\n")
    # 6. life: fireflies low around the trees and bushes of every level (ground-plane clouds at 0.6 to 1.6 m), and
    # cherry petals falling from the weeping cherry right of the hall (a cloud facing the camera, drifting down)
    flies = [  # key, x, y, ground z, height, cloud w x d, per second, at most
        ("sacred-roots", -6.3, 7.4, 3.0, 0.9, (5.0, 2.6), 1.4, 10), ("sacred-crown", -9.6, 10.6, 3.0, 1.6, (4.0, 3.0), 0.9, 7),
        ("cherry-bushes", 8.6, 9.8, 3.0, 1.0, (5.0, 2.6), 1.2, 9), ("upper-left", -10.4, 4.0, 3.0, 1.1, (3.2, 3.4), 0.8, 6),
        ("upper-right", 10.4, 4.2, 3.0, 1.1, (3.2, 3.4), 0.8, 6), ("terrace-left", -7.2, 0.4, 1.0, 0.8, (3.6, 1.6), 0.8, 6),
        ("terrace-right", 7.4, 0.2, 1.0, 0.8, (3.6, 1.6), 0.8, 6), ("plaza-left", -7.4, -4.4, 0.0, 0.9, (5.0, 1.6), 1.0, 8),
        ("plaza-right", 7.8, -5.0, 0.0, 0.9, (5.0, 1.6), 1.0, 8), ("gate-left", -5.6, -9.8, 0.0, 0.8, (3.0, 2.0), 0.8, 6),
        ("gate-right", 5.6, -10.0, 0.0, 0.8, (3.0, 2.0), 0.8, 6),
    ]
    for key, x, y, z, hgt, size, rate, most in flies:
        add("fireflies-" + key, "Fireflies " + key.replace("-", " ").title(), (x, y, z + hgt),
            particle_body(ShapeSize=size, Rate=rate * 1.6, MaxParticles=int(most * 1.6), LightRadius=1.4, LightIntensity=0.6,
                          MaxLights=1))
    add("petals-cherry", "Petals Weeping Cherry", (4.6, 8.4, sz_ + 3.0),
        particle_body(Rate=2.0, MaxParticles=20, ShapeSize=(5.0, 0.8), Lifetime=(5.0, 8.0), Speed=(0.3, 0.6),
                      Direction=262, Spread=22, StartSize=(0.16, 0.22), StartRotation=(0, 360), AngularVelocity=(-160, 160),
                      EndSizeScale=0.9, StartColor=(1.0, 0.86, 0.9, 1.0), EndColor=(1.0, 0.9, 0.93, 0.0), Gravity=(-0.1, -0.15),
                      Drag=0.3, Turbulence=0.7, Blend="Alpha", Space="View", EmissionIntensity=0.6))
    # (pale pink, nearly white: pink petals vanished against the pink canopy; the cloud sits in front of the tree's
    # card, 1.5 m nearer than its foot, and low: placed inside the canopy, the card hid them)
    # falling leaves (the user's leaf sprites, Nov 2, 23:55): clouds facing the camera, low and in front of each
    # tree's card (inside a canopy the card hides them), each leaf turning as it falls
    green, _ = png("FX_Leaves_Green", EFFECTS)
    pink, _ = png("FX_Leaf_Pink", EFFECTS)

    def leaves(key, tag, x, y, z, size, rate, most, texture, index=0, tint=(1.0, 1.0, 1.0)):
        add(key, tag, (x, y, z), particle_body(
            Rate=rate, MaxParticles=most, ShapeSize=size, Lifetime=(4.0, 7.0), Speed=(0.25, 0.5), Direction=265,
            Spread=25, StartSize=(0.32, 0.375), StartRotation=(0, 360), AngularVelocity=(-120, 120), EndSizeScale=1.0,
            StartColor=tint + (1.0,), EndColor=tint + (0.0,), Gravity=(-0.06, -0.2), Drag=0.4, Turbulence=0.9,
            TextureHandle=texture, SpriteIndex=index, Blend="Alpha", Space="View", EmissionIntensity=0))
    leaves("leaves-sacred0", "Leaves Sacred Tree", -7.6, 9.9, sz_ + 3.4, (5.0, 1.0), 0.7, 8, green, 1)
    leaves("leaves-sacred1", "Leaves Sacred Tree 2", -5.0, 9.6, sz_ + 3.0, (3.0, 0.8), 0.6, 7, green, 6)
    leaves("leaves-edge", "Leaves Edge Trees", 11.2, 5.5, sz_ + 3.2, (3.0, 1.0), 0.6, 7, green, 11)
    sakura, _ = png("FX_Leaves_Sakura", EFFECTS)          # the user's sakura leaves (an 8 x 8 sheet, Nov 3)
    leaves("leaves-cherry", "Leaves Weeping Cherry", 5.6, 8.6, sz_ + 3.2, (4.5, 0.8), 0.8, 8, sakura, 2)
    leaves("leaves-cherry2", "Leaves Weeping Cherry 2", 4.4, 8.9, sz_ + 3.6, (3.5, 0.8), 0.8, 8, sakura, 21)
    for k, x in enumerate((-5.6, 5.6)):                   # red maple leaves: the pink leaf tinted red
        leaves("leaves-maple%d" % k, "Leaves Maple %d" % (k + 1), x, -10.0, 2.2, (2.6, 0.6), 0.5, 6, pink,
               tint=(1.0, 0.5, 0.4))
    # 8. the shrine cat: wanders to and fro in front of the jizo row, stopping now and then; at every other stop it
    # lies down, sleeps curled up and gets up again (it starts the scene asleep)
    add("cat", "Shrine Cat", (5.4, 4.95, sz_),
        ("    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TilingFactor: 1\n      SortOrder: 0\n      TextureHandle: %d\n"
         "      Sprite: IdleLeft_0\n      SpriteIndex: 0\n      FlipX: false\n      FlipY: false\n      Facing: Upright\n"
         "    SpriteAnimatorComponent:\n      AnimationsHandle: %d\n      StartClip: IdleLeft\n      PlayOnStart: true\n      Speed: 1\n"
         "    Shadow2DComponent:\n      Offset: [0, -0.02]\n      Size: [0.55, 0.22]\n      Strength: 0.5\n"
         "    NativeScriptComponent:\n      Scripts:\n        - Name: NpcWanderScript\n          Properties:\n"
         "            PathLength: 3.2\n            Axis: X\n            Speed: 0.55\n            PauseMin: 4\n            PauseMax: 14\n"
         "            StopChance: 0.5\n            WorkClip: Sleep\n            WorkEvery: 2\n            WorkFacesCamera: false\n"
         "            WorkStartClip: LieDown\n            WorkEndClip: GetUp\n            StartDelay: 1.5\n            Loop: false\n"
         "            Player: %d\n            Clearance: 0.6\n            Patience: 4\n") % (CAT["texture"], CAT["animations"], CAT["player"]),
        (1.25, 1.171875, 1))       # 48 x 45 cuts at 38.4 px/m, as the cast

    # 7. statues (sprites from props_r1, at 32 px/m unless scaled), each with a small collider at its foot
    def statue(key, tag, name, x, y, z, scale=1.0, flip=False, col=(0.7, 0.6)):
        install_sprite(name)
        h, (w, hh) = png(name)
        sx, sy = w / 32.0 * scale, hh / 32.0 * scale
        add(key, tag, (x, y, z), card_body(h, sx, flip=flip, sort=1, shadow=0.5)
            + "    BoxCollider2DComponent:\n      Offset: [0, 0]\n      Size: [%g, %g]\n      Density: 1\n      Friction: 0\n"
              "      Restitution: 0\n      IsSensor: false\n    BoxColliderComponent:\n      Offset: [0, 0, 0.8]\n"
              "      Size: [%g, %g, 1.6]\n      Layer: World\n      IsTrigger: false\n" % (col + col), (sx, sy, 1))
    # a pair of stone foxes (kitsune) on pedestals just inside the torii, either side of the path, facing each other
    for k, x in enumerate((-2.55, 2.55)):
        statue("kitsune%d" % k, "Kitsune Statue %d" % (k + 1), "JP_Shrine_Kitsune_A", x, -10.1, 0.0, flip=x < 0)
    # a pair of komainu at the foot of the upper stairs, looking at each other across the path: the user's two
    # sprites are drawn as a left and a right one (A faces right, B faces left), so neither is flipped; in front of
    # the flower beds beside the stairs (behind them, the right one hid in the red flowers)
    statue("komainu-sprite0", "Komainu Statue 1", "JP_Shrine_Komainu_A", -2.15, 0.75, 1.0, col=(1.1, 0.8))
    statue("komainu-sprite1", "Komainu Statue 2", "JP_Shrine_Komainu_B", 2.15, 0.75, 1.0, col=(1.1, 0.8))
    # a sacred boulder (iwakura) bound with a shimenawa on the lower plaza, opposite the hand-washing pavilion
    statue("iwakura", "Sacred Rock", "JP_Shrine_Iwakura_A", 6.3, -7.5, 0.0, col=(1.6, 1.0))

    # 4. the hand-washing pavilion on the lower plaza, left of the path by the torii, its front to the path
    model("temizuya", "Temizuya", "JP_Temple_Temizuya_B", -4.2, -7.0, 0.0, rz=math.pi / 2)    # dark grey tiles, as the shrine
    # 5. a row of jizo and moss stones in front of the graves
    card("jizo0", "Jizo Row 1", "JP_Field_Jizo_B", 4.5, 5.95, 3.0)
    install_sprite("JP_Shrine_Jizo_A")
    card("jizo1", "Jizo Row 2", "JP_Shrine_Jizo_A", 4.95, 6.0, 3.0, scale=0.55)          # the red-capped jizo (Nov 3)
    card("jizo2", "Jizo Row 3", "JP_Field_Jizo_B", 5.4, 5.95, 3.0, flip=True)
    card("moss0", "Moss Stone Jizo", "JP_Shrub_MossStone", 4.05, 5.75, 3.0, shadow=0.3, folder=os.path.join(ASSETS, "Sprites", "Plants"))
    card("moss1", "Moss Rock Jizo", "MossRockA", 5.85, 5.95, 3.0, shadow=0.3, folder=os.path.join(ASSETS, "Sprites", "Props"))

    # 2 and 3: the banners on stands become nobori on bamboo poles; the notice board the Japanese one
    count = {"banners": 0, "sign": 0}
    for i, b in enumerate(blocks):
        m = re.search(r"TextureHandle: (\d+)", b)
        old = os.path.basename(H.get(int(m.group(1)), "")) if m else ""
        tag = re.search(r"Tag: ([^\n]*)", b).group(1)
        if old == "Banner.png" and tag.startswith("Banner"):
            t = field(b, "Translation")
            body = b[:b.index("    SpriteRendererComponent:")]
            body = set_field(body, "Scale", [1.0, 1.0, 1.0])
            blocks[i] = body + model_body(handle("JP_Prop_BannerPole_A.glb"))
            card("nobori-" + tag, tag + " Cloth", "JP_Banner_Nobori_Cloth_A", t[0] + 0.45, t[1] - 0.08, t[2] + 3.3, hanging=True,
                 sort=2)
            count["banners"] += 1
        elif old == "Jizo.png" and tag == "Stone Jizo":           # the red-capped jizo, at the old one's height
            install_sprite("JP_Shrine_Jizo_A")
            h, (w, hh) = png("JP_Shrine_Jizo_A")
            sy = field(b, "Scale")[1]
            b = re.sub(r"TextureHandle: \d+", "TextureHandle: %d" % h, b, count=1)
            blocks[i] = set_field(b, "Scale", [sy * w / hh, sy, 1.0])
            count["jizo"] = count.get("jizo", 0) + 1
        elif old == "NoticeBoard.png" and tag == "Sign Shrine":
            h, (w, hh) = png("JP_Gate_Noticeboard")
            b = re.sub(r"TextureHandle: \d+", "TextureHandle: %d" % h, b, count=1)
            blocks[i] = set_field(b, "Scale", [w / 32.0, hh / 32.0, 1.0])
            count["sign"] += 1
    for i, b in enumerate(blocks):
        m = re.search(r"ModelHandle: (\d+)", b)
        old = os.path.basename(H.get(int(m.group(1)), "")) if m else ""
        if old in MODEL_SWAPS:
            blocks[i] = re.sub(r"ModelHandle: \d+", "ModelHandle: %d" % handle(MODEL_SWAPS[old]), b, count=1)
            count["models"] = count.get("models", 0) + 1
    gone = {stable_id(k) for k in RETIRED}
    kept = [b for b in blocks if int(re.search(r"Entity: (\d+)", b).group(1)) not in gone]
    count["retired"] = len(blocks) - len(kept)
    count["added"], count["updated"] = len(added), len(updated)
    return kept + added, count


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    H = handles()
    by_name = {}
    for h, p in H.items():
        by_name.setdefault(os.path.basename(p), (h, p))

    def sprite(name):
        h, p = by_name[name + ".png"]
        return h, png_size(os.path.join(ASSETS, p))

    fence = [v for v in by_name.values() if v[1].endswith("/" + FENCE_MODEL + ".glb")]
    assert fence, "the kit's bamboo fence model is missing (build the JP kit first)"
    fence_handle = fence[0][0]

    raw = open(SCENE, "rb").read()
    scene_nl = "\r\n" if b"\r\n" in raw else "\n"                     # keep the file's own line endings
    text = raw.decode("utf-8").replace("\r\n", "\n")
    header, blocks = entities(text)
    count = {"trees": 0, "shrubs": 0, "fences": 0, "lights": 0}
    cycle = {}

    def pick(table_key, options):
        k = cycle.get(table_key, 0)
        cycle[table_key] = k + 1
        return options[k % len(options)]

    # the fence cards, grouped into lines (same parent and height), for their direction
    fences = []
    for i, b in enumerate(blocks):
        m = re.search(r"TextureHandle: (\d+)", b)
        if m and H.get(int(m.group(1)), "").endswith("Props/Fence.png"):
            t = field(b, "Translation")
            parent = re.search(r"Parent: (\d+)", b)
            fences.append((i, parent.group(1) if parent else "", round(t[2], 2), t))
    lines = {}
    for (i, parent, z, t) in fences:
        lines.setdefault((parent, z), []).append((t[0], t[1], i))
    angle = {}
    for pts in lines.values():
        pts.sort()
        for k, (x, y, i) in enumerate(pts):
            a = pts[max(0, k - 1)]
            b = pts[min(len(pts) - 1, k + 1)]
            angle[i] = math.atan2(b[1] - a[1], b[0] - a[0]) if b[0] != a[0] else 0.0

    for i, b in enumerate(blocks):
        tag = re.search(r"Tag: ([^\n]*)", b)
        tag = tag.group(1) if tag else ""
        m = re.search(r"TextureHandle: (\d+)", b)
        old = os.path.basename(H.get(int(m.group(1)), "")) if m else ""
        stem = old[:-4] if old.endswith(".png") else old
        if stem in TREES:
            new = pick(stem, TREES[stem])
            h, (w, hh) = sprite(new)
            s = field(b, "Scale")
            b = re.sub(r"TextureHandle: \d+", "TextureHandle: %d" % h, b, count=1)
            b = set_field(b, "Scale", [s[1] * w / hh, s[1], s[2]])
            count["trees"] += 1
        elif any(stem.startswith(k) for k in SHRUBS):
            key = next(k for k in SHRUBS if stem.startswith(k))
            canopy = tag.startswith("Canopy Front")
            new = pick("canopy", CANOPY) if canopy else pick(key, SHRUBS[key])
            h, (w, hh) = sprite(new)
            s = field(b, "Scale")
            sy = s[1] if canopy else min(s[1], hh / 32.0 * SHRUB_MAX_ZOOM)
            b = re.sub(r"TextureHandle: \d+", "TextureHandle: %d" % h, b, count=1)
            b = set_field(b, "Scale", [sy * w / hh, sy, s[2]])
            count["shrubs"] += 1
        elif old == "Fence.png":
            s = field(b, "Scale")
            body = b[:b.index("    SpriteRendererComponent:")]
            body = set_field(body, "Scale", [s[0] / 2.0, 1.0, 1.0])
            body = set_field(body, "Rotation", [0.0, 0.0, angle.get(i, 0.0)])
            b = body + ("    MeshComponent:\n      ModelHandle: %d\n      Shape: Box\n      Size: [1, 1, 1]\n      CastShadows: true\n"
                        "      Materials:\n        []\n    MeshColliderComponent:\n      Layer: World\n") % fence_handle
            if blocks[i].endswith("\n") is False:
                b = b.rstrip("\n")
            count["fences"] += 1
        elif any(tag == t or tag == t + " 2" for t in LANTERN_TAGS) and "Height: 1.33" in b:
            b = re.sub(r"Height: [\d.]+", "Height: 1", b, count=1)
            b = re.sub(r"Intensity: [\d.]+", "Intensity: 2.5", b, count=1)
            count["lights"] += 1
        blocks[i] = b

    # the Old Monk to the sacred tree's roots; the sway on the front canopy group
    for i, b in enumerate(blocks):
        tag = re.search(r"Tag: ([^\n]*)", b)
        tag = tag.group(1) if tag else ""
        if tag == "Npc Elder":
            t = field(b, "Translation")
            if any(abs(t[0] - f[0]) < 0.01 and abs(t[1] - f[1]) < 0.01 for f in ELDER_FROM):
                blocks[i] = set_field(b, "Translation", list(ELDER_TO))
                count["elder"] = 1
        elif tag == SWAY[0] and "NativeScriptComponent" not in b:
            blocks[i] = b.rstrip("\n") + "\n" + SWAY[1].rstrip("\n") + b[len(b.rstrip("\n")):]
            count["sway"] = 1

    # the terrain's scattered decor: the same shrub swap in its sprite lists
    traw = open(TERRAIN, "rb").read()
    terrain_nl = "\r\n" if b"\r\n" in traw else "\n"
    terrain = traw.decode("utf-8").replace("\r\n", "\n")
    a = terrain.find("  Decor:")
    swapped = 0
    if a >= 0:
        e = terrain.find("\nData:", a)
        e = len(terrain) if e < 0 else e
        section = terrain[a:e]

        def swap(mm):
            nonlocal swapped
            hnd = int(mm.group(0))
            stem = os.path.basename(H.get(hnd, ""))[:-4]
            key = next((k for k in SHRUBS if stem.startswith(k)), None)
            if key is None:
                return mm.group(0)
            swapped += 1
            return str(sprite(pick("decor-" + key, SHRUBS[key]))[0])
        section = re.sub(r"\b\d{15,20}\b", swap, section)
        terrain = terrain[:a] + section + terrain[e:]

    blocks, dressed = dress(blocks, H, by_name)
    print("dressing:", dressed)
    profiles = {}
    for name, (edits, (strength, radius, softness)) in PROFILE_EDITS.items():
        path = os.path.join(PROFILE_DIR, name + ".hyrender")
        raw_profile = open(path, "rb").read().decode("utf-8")
        nl = "\r\n" if "\r\n" in raw_profile else "\n"
        profile = raw_profile.replace("\r\n", "\n")
        tuned = profile
        for old, new in edits:
            tuned = tuned.replace("    " + old, "    " + new)
        for key, value in (("VignetteIntensity", strength), ("VignetteRadius", radius), ("VignetteSoftness", softness)):
            if ("    %s:" % key) in tuned:
                tuned = re.sub(r"(    %s: )[^\n]*" % key, lambda m: m.group(1) + "%g" % value, tuned, count=1)
            else:                                      # the PostProcess map is the file's last
                tuned = tuned.rstrip("\n") + "\n    %s: %g\n" % (key, value)
        tuned = look.apply(tuned, name)                # the shared colour grade (scripts/game/look.py, Nov 3)
        if tuned != profile:
            profiles[path] = (tuned, nl)
    count["profile"] = len(profiles)
    print("trees %(trees)d, shrubs %(shrubs)d, fences %(fences)d, lantern lights %(lights)d" % count,
          "; terrain decor sprites swapped %d" % swapped,
          "; elder moved %d, sway added %d, profile tuned %d" % (count.get("elder", 0), count.get("sway", 0), count["profile"]))
    if args.dry_run:
        return
    stamp = time.strftime("%Y%m%d_%H%M%S")
    backup = os.path.join(REPO, "bin-int", "shrine_glade_backup")
    os.makedirs(backup, exist_ok=True)
    shutil.copy2(SCENE, os.path.join(backup, "ShrineGlade_%s.hyscene" % stamp))
    shutil.copy2(TERRAIN, os.path.join(backup, "ShrineGlade_%s.hyterrain" % stamp))
    with open(SCENE, "w", encoding="utf-8", newline=scene_nl) as f:
        f.write(header + "".join(blocks))
    with open(TERRAIN, "w", encoding="utf-8", newline=terrain_nl) as f:
        f.write(terrain)
    for path, (tuned, nl) in profiles.items():
        shutil.copy2(path, os.path.join(backup, os.path.basename(path).replace(".hyrender", "_%s.hyrender" % stamp)))
        with open(path, "w", encoding="utf-8", newline=nl) as f:
            f.write(tuned)
    print("wrote", os.path.relpath(SCENE, REPO), "and", os.path.relpath(TERRAIN, REPO), "(backups in bin-int/shrine_glade_backup)")


if __name__ == "__main__":
    main()
