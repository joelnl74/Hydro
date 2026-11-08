"""Writes Project/Assets/Scenes/TempleDistrict.hyscene: the temple district above the city, reached from the
merchant street's west end (the city slice's "Exit TempleDistrict") and laid out after the Nov 2 reference (a stone
stair between castle-stone walls up to a vermilion gate, the main hall and a five-storey pagoda beyond it, black
pines and red maples, a water basin and low plaster walls in the foreground):

  - Forecourt (z 0): flagstones, low tile-capped plaster walls along the front, the water basin (chozuya) at the front
    left, stone lanterns; the lane from the city comes in from the east ("Exit JapaneseCitySlice", "Arrive ...").
  - Temple stair: one wide granite flight between castle-stone cheek walls up to the gate terrace (z 4.5), stepped
    planted terraces either side (z 2.2), stone lanterns at its foot and head.
  - Gate terrace (z 4.5): the vermilion temple gate (JP_Temple_Gate_A) with its lanterns, the vermilion fence along
    the terrace edge, the stone-lantern path to the hall steps; the five-storey pagoda on the left.
  - Hall terrace (z 6.0): the main hall (JP_Temple_Hall_A) with its purple curtain, a vermilion fence along its front
    edge and tile-capped plaster walls round its sides and back (Nov 2: fenced all round).
  - Beyond: the pagoda (left) and the bell tower (right) on the gate terrace inside plaster walls, the wooded hill
    rising behind the hall in three mossy-walled steps, the storehouse and the monks' quarters among the trees.

Look (Nov 2): the temple's own render profiles (RenderProfiles/Temple: lower, cooler ambient than the city's, so the
sun's side reads), the sun low from the left so walls, posts and trees throw their shadows across the terraces and
towards the camera; mossy temple grass with ragged grass edges over an earth band at every seam, grass creeping onto
the path sides and darker / lighter grass patches against the tiling (art_incoming/jp_city/temple_r1, made by
gen_temple_tex.py from the field grass).

Townsfolk: pilgrims going up to pray (routes, NpcRouteScript), a monk sweeping the stair, a monk at the gate who
talks ("Npc TempleMonk"). The camera fades what stands in front of Miko (OccluderFadeScript; meshes to 0.15, Nov 3: at
0.3 the gate's roof still lay grey over the komainu behind it).

Trees and shrubs: the delivered Japanese set (Nov 2, PixelLab: black pines, red and orange maples, bamboo, nine garden
shrubs), installed into Sprites/Trees and Sprites/Plants by install_jp_trees.py; the field set stands in for any that
are missing. Each tree comes at two sizes: the prompts' (a 3.6 m pine) and "_Large", the full image read at 32 px/m (a
7 m pine), for the landmark pines and the hill's forest.

Nov 3 (towards a 9): wind in the bamboo and maples (sway groups, SWAY), falling red, orange and bamboo leaves and the
basin's trickle (the "Life" group), the vignette in the three temple profiles (VIGNETTE), the user's komainu pair at
the hall steps, and a small kit torii before the wayside shrine.

Rewrites the scene on every run (don't edit it in the editor). Uses build_city_slice's Slice (its kit, materials,
helpers). Close the scene in Oxygen first. Check: Oxygen --scene TempleDistrict --play --frames 150 --capture out.png

    python scripts/game/japanese_city/build_temple_district.py [--profile Day|Dusk|Night] [--start lane|stair|gate|hall]
"""

import argparse
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_city_slice as sl  # noqa: E402
import look  # noqa: E402  (scripts/game/look.py; build_city_slice puts scripts/game on the path)  (Slice, its materials and helpers; sets kit.KEY, reset below)

kit, city = sl.kit, sl.city
kit.KEY = "JP_Temple:"
ASSETS, REPO = kit.ASSETS, kit.REPO
SCENE = os.path.join(ASSETS, "Scenes", "TempleDistrict.hyscene")
TREES_R1 = os.path.join(REPO, "art_incoming", "jp_city", "trees_r1")
TEMPLE_R1 = os.path.join(REPO, "art_incoming", "jp_city", "temple_r1")   # gen_temple_tex.py's ground textures (Nov 2)
TEMPLE_PROFILES = os.path.join(ASSETS, "RenderProfiles", "Temple")

ZF, ZS, ZG, ZH = 0.0, 2.2, 4.5, 6.0     # forecourt, the planted side terraces, the gate terrace, the hall terrace
STAIR_W, CHEEK = 5.0, 1.0
HALF, GAP = STAIR_W / 2, STAIR_W / 2 + CHEEK
Y_STAIR0, Y_STAIR1 = 6.0, 15.0          # the temple stair climbs from the forecourt to the gate terrace
Y_GATE = 15.6                           # the gate's front (its passage runs to y 18)
Y_HALL_STEPS0, Y_HALL_STEPS1 = 23.5, 27.0
Y_HALL = 31.0                           # the hall podium's front (its own steps reach 2.4 m in front)
# a stone lantern's light sits low in its firebox (0.95 m, not the glass's 1.15), strong and reaching 4.6 m: a warm
# pool about 3 m across on the ground round it at dusk instead of a faint glow on the lantern only (Nov 2; at 1.15 and
# 4.2 m the pool lifted the paving only 13 % under the dusk sun, now also lowered from 1.15 to 1.0)
STONE_LIGHT_Z = 0.95
# the user's composition (Nov 2, 09:20, made in the editor and kept here): the hand-washing pavilion at the left of the
# stair foot facing the court (as at most shrines: you wash before you climb), the omikuji and ema racks mirrored on
# the right of the stair foot, the tea bench in the west corner by the young maple
X_TEMIZUYA, Y_TEMIZUYA = -8.43, 0.45    # the pavilion's front (it faces -Y, the court)
X_OMIKUJI, Y_OMIKUJI = 4.75, 3.15
Y_BURNER = 21.0                         # the incense burner on the approach between the gate and the hall steps
X_PRECINCT = 30.25                      # the plaster walls closing the gate terrace's sides
X_HALL_WALL = 17.75                     # the plaster walls along the hall terrace's sides (its back wall at y 45.6)
ZB = (8.5, 11.5, 14.5)                  # the wooded hill behind the hall: three mossy-walled steps
YB = (46.0, 60.0, 76.0, 150.0)

# the trees: kind -> (delivered sprite, stand-in, tint for the stand-in, base scale; a placement's scale multiplies it)
TREE = {
    "pine": ("JP_Tree_BlackPine_A", "PineTall", None, 1.3),                 # 4.7 m
    "pine_b": ("JP_Tree_BlackPine_B", "PineTall", None, 1.3),
    "pine_large": ("JP_Tree_BlackPine_A_Large", "PineTall", None, 1.0),     # 7.2 m: the landmark pines
    "pine_small": ("JP_Tree_BlackPine_Small", "PineTall", None, 1.15),      # 3.4 m
    "maple": ("JP_Tree_Maple_Red_A", "LindenAutumnA", (1.0, 0.5, 0.42, 1.0), 1.25),
    "maple_b": ("JP_Tree_Maple_Red_B", "LindenAutumnA", (1.0, 0.5, 0.42, 1.0), 1.25),
    "maple_large": ("JP_Tree_Maple_Red_A_Large", "LindenAutumnA", (1.0, 0.5, 0.42, 1.0), 0.95),
    "maple_small": ("JP_Tree_Maple_Orange", "LindenAutumnC", (1.0, 0.52, 0.42, 1.0), 1.0),
    "bamboo": ("JP_Tree_Bamboo_Clump_Large", "FirTallB", (0.8, 0.95, 0.7, 1.0), 0.85),   # 6.3 m
    "bamboo_b": ("JP_Tree_Bamboo_Clump_B_Large", "FirTallB", (0.8, 0.95, 0.7, 1.0), 0.85),
    # round 2 (Nov 2): four variants each ({v}); the young bamboo at its full size (its thin culms vanish halved)
    "maple_young": ("JP_Tree_Maple_Young_{v}", "LindenAutumnC", (1.0, 0.52, 0.42, 1.0), 1.15),     # 1.85 m
    "pine_young": ("JP_Tree_BlackPine_Young_{v}", "PineTall", None, 1.2),                         # 1.85 m
    "bamboo_young": ("JP_Tree_Bamboo_Young_{v}_Large", "FirTallB", (0.8, 0.95, 0.7, 1.0), 0.9),   # 2.9 m
    "sugi": ("JP_Tree_Sugi_{s}_Large", "FirTallC", None, 1.0),                                   # 7.8 m
}
VARIANTS = {"maple_young": "ABCD", "pine_young": "ABCD", "bamboo_young": "ABCDEFGH"}
SHRUB_VARIANT = 0.4     # how often a shrub uses the second delivery's version (_V2)
# wind (Nov 3): the trees of each kind stand under a group per terrace that sways them (FoliageSwayScript), the tall
# ones less: kind -> (group, degrees, seconds of one sway, gust); pines are stiff and stay still
SWAY = {"bamboo": ("Swaying Bamboo", 1.6, 4.2, 0.6), "maple_young": ("Swaying Young Maples", 2.4, 3.2, 0.5),
        "maple": ("Swaying Maples", 1.0, 5.0, 0.5)}
# the vignette per time of day (Nov 3, as the shrine glade's): intensity, radius, softness
VIGNETTE = {"Day": (0.3, 0.45, 0.6), "Dusk": (0.5, 0.42, 0.6), "Night": (0.6, 0.38, 0.6)}
EFFECTS = os.path.join(ASSETS, "Sprites", "Effects")   # the user's leaf sprites (Nov 2 / 3)
# entities the user moved in Oxygen whose place the builder draws at random (Nov 3, 11:19): tag -> (x, y) of its
# Translation; main() applies them after the build and says if one is not found (the tags count up, so an edit above
# them can shift the numbers: check the diff, bin-int/claude_td_semdiff.py)
USER_PLACED = {"Decal 576": (-17.1824379, 18.9640255)}     # a leaf decal under the young maple left of the gate
RED, ORANGE = (1.0, 0.5, 0.4), (1.0, 0.64, 0.36)       # the pink leaf tinted to the maples' reds


LOOK = {}      # build() fills it: the decal materials tree() and planted() use, and the random generator


def temple_material(name, w_px, h_px, cutoff=0.5, tint=(1, 1, 1)):
    """A material on a temple_r1 image (copied to Textures/JapaneseCity): one image over w_px / 32 by h_px / 32 metres
    of a box face; cutoff 0 for a ground sheet, 0.5 for a ragged decal."""
    src = os.path.join(TEMPLE_R1, name + ".png")
    png = os.path.join(city.TEX_DIR, name + ".png")
    os.makedirs(city.TEX_DIR, exist_ok=True)
    shutil.copy2(src, png)
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Repeat\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % city.stable_handle("tex:" + name))
    tex = kit.handle_of(png + ".hymeta")
    mat = os.path.join(city.MAT_DIR, name + ".hymat")
    h = kit.handle_of(mat) if os.path.exists(mat) else city.stable_handle("mat:" + name)
    city.write_material(mat, "Handle: %d\nMaterial:\n  Type: Standard\n  AlbedoHandle: %d\n  Tint: [%g, %g, %g, 1]\n  Tiling: [%g, %g]\n"
                "  EmissionColor: [0, 0, 0]\n  EmissionIntensity: 1\n  AlphaCutoff: %g\n  Filter: Nearest\n"
                % (h, tex, tint[0], tint[1], tint[2], 32.0 / w_px, 32.0 / h_px, cutoff))
    return h


def temple_profiles(city_handles):
    """RenderProfiles/Temple/<Day|Dusk|Night>.hyrender: the city's profiles with a lower, cooler ambient (the city's
    lifted every face the sun missed, so the temple looked evenly lit) and the fog a little further out."""
    os.makedirs(TEMPLE_PROFILES, exist_ok=True)
    tweaks = {
        "Day": dict(SkyAmbient="[0.5, 0.52, 0.56]", GroundAmbient="[0.18, 0.17, 0.15]"),
        "Dusk": dict(SkyAmbient="[0.23, 0.19, 0.22]", GroundAmbient="[0.08, 0.06, 0.05]", FogColor="[0.7, 0.46, 0.38]",
                     FogStart="40", FogEnd="230", BloomIntensity="0.95", BloomThreshold="0.85"),
        "Night": dict(),
    }
    handles = {}
    for k, vals in tweaks.items():
        src = open(os.path.join(sl.CITY_PROFILES, k + ".hyrender")).read().splitlines()
        dst = os.path.join(TEMPLE_PROFILES, k + ".hyrender")
        h = kit.handle_of(dst) if os.path.exists(dst) else city.stable_handle("profile:Temple" + k)
        vals = dict(vals, **dict(zip(("VignetteIntensity", "VignetteRadius", "VignetteSoftness"),
                                     ("%g" % v for v in VIGNETTE[k]))))
        out = []
        for line in src:
            key = line.strip().split(":")[0]
            if line.startswith("Handle:"):
                line = "Handle: %d" % h
            elif key in vals:
                line = line[: line.index(key)] + key + ": " + vals[key]
            out.append(line)
        for key in ("VignetteIntensity", "VignetteRadius", "VignetteSoftness"):    # the PostProcess map is the last
            if any(line.strip().startswith(key + ":") for line in out) is False:
                out.append("    %s: %s" % (key, vals[key]))
        with open(dst, "w", newline="\r\n") as f:
            f.write(look.apply("\n".join(out) + "\n", k))      # the shared vignette and grade (Nov 3)
        handles[k] = h
    return handles


def sprite_card(c, parent, png, x, y, z, color=None, scale=1.0, sort=0, flip=False, tag=None, shadow=0.0):
    """An upright card for any sprite PNG with a .hymeta (32 px/m), its foot at (x, y, z), tinted by `color`."""
    if os.path.exists(png) is False or os.path.exists(png + ".hymeta") is False:
        c.d.missing.append(os.path.basename(png))
        return
    pw, ph = kit.png_size(png)
    sx, sy = pw / 32.0 * scale, ph / 32.0 * scale
    tag = tag or c._n(os.path.splitext(os.path.basename(png))[0].replace("JP_", ""))
    col = color or (1, 1, 1, 1)
    body = ("    SpriteRendererComponent:\n      Color: [%g, %g, %g, %g]\n      TilingFactor: 1\n      SortOrder: %d\n"
            "      TextureHandle: %d\n      SpriteIndex: 0\n      FlipX: %s\n      FlipY: false\n      Facing: Upright\n"
            % (col[0], col[1], col[2], col[3], sort, kit.handle_of(png + ".hymeta"), "true" if flip else "false"))
    if shadow:
        body += ("    Shadow2DComponent:\n      Offset: [0, -0.02]\n      Size: [%g, %g]\n      Strength: %g\n"
                 % (round(sx * 0.6, 2), round(min(1.2, sx * 0.3), 2), shadow))
    c.w.entity("card:" + tag, tag, parent=parent, t=(x, y + sy / 2, z), s=(sx, sy, 1), body=body)


def tree(c, parent, kind, x, y, z, scale=1.0, flip=False, ground=None):
    """A tree: the delivered Japanese one (copied to Sprites/Trees with a .hymeta) if it is there, else the stand-in.
    Maples drop a scatter of red leaves round their foot."""
    name, stand_in, tint, base = TREE[kind]
    if "{v}" in name or "{s}" in name:
        name = name.replace("{v}", LOOK["rnd"].choice(VARIANTS.get(kind, "A"))).replace("{s}", LOOK["rnd"].choice("AB"))
    scale *= base
    if kind.startswith("maple") and kind != "maple_small" and "leaves" in LOOK:
        rnd = LOOK["rnd"]
        for k in range(2):
            c.decal(parent, LOOK["leaves"], x + rnd.uniform(-1.6, 1.6), y + rnd.uniform(-1.4, 0.6), 4.0, 4.0,
                    (z if ground is None else ground) + 0.04 + k * 0.004, rnd.uniform(0, 360))
    card_parent = sway_group(c, parent, kind)
    png = os.path.join(sl.TREES, name + ".png")
    if os.path.exists(png + ".hymeta"):
        sprite_card(c, card_parent, png, x, y, z, scale=scale, flip=flip, shadow=0.5)
    else:
        sprite_card(c, card_parent, os.path.join(sl.TREES, stand_in + ".png"), x, y, z, color=tint, scale=scale / base, flip=flip,
                    shadow=0.5)


def sway_group(c, parent, kind):
    """The group under `parent` that sways trees of `kind` (made the first time; only the cards go in it, the leaf
    decals at their feet stay put), or `parent` for a tree that doesn't sway."""
    cls = "bamboo" if kind.startswith("bamboo") else kind if kind in SWAY else "maple" if kind.startswith("maple") else None
    if cls is None or "sway" not in LOOK:
        return parent
    key = "sway:%s:%s" % (parent, cls)
    if key not in LOOK["sway"]:
        name, degrees, period, gust = SWAY[cls]
        c.w.entity(key, name, parent=parent, body=(
            "    NativeScriptComponent:\n      Scripts:\n        - Name: FoliageSwayScript\n          Properties:\n"
            "            Degrees: %g\n            Period: %g\n            Gust: %g\n" % (degrees, period, gust)))
        LOOK["sway"][key] = True
    return key


def particle_body(**kw):
    """A ParticleEmitter2DComponent (EntitySerializer's keys, as the shrine glade pass writes them); kw overrides the
    defaults below."""
    p = dict(PlayOnStart="true", Looping="true", Duration=4, Rate=1.0, BurstCount=0, MaxParticles=8, Seed=0, Shape="Box",
             ShapeSize=(4, 2), Lifetime=(3.5, 6.5), Speed=(0.05, 0.3), Direction=90, Spread=180, StartSize=(0.15, 0.24),
             StartRotation=(0, 0), AngularVelocity=(0, 0), EndSizeScale=0.5, StartColor=(1, 1, 1, 1), EndColor=(1, 1, 1, 0),
             Gravity=(0, 0), Drag=0.5, Turbulence=0.9, TextureHandle=None, SpriteIndex=0, Blend="Alpha", Lighting="Lit",
             Space="View", SortOrder=2, EmissionIntensity=0, LightRadius=0, LightIntensity=1, MaxLights=0,
             DestroyWhenFinished="false")
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


def life(c):
    """Falling leaves and the basin's trickle (Nov 3): clouds facing the camera, low and in front of each tree's card
    (inside a canopy the card hides them), each leaf turning as it falls; the red and orange maple leaves are the
    user's pink leaf tinted, the bamboo's the green leaf sheet (4 x 4)."""
    L = c.group("Life")

    def handle(name):
        meta = os.path.join(EFFECTS, name + ".png.hymeta")
        return kit.handle_of(meta) if os.path.exists(meta) else None

    pink, green = handle("FX_Leaf_Pink"), handle("FX_Leaves_Green")

    def leaves(tag, x, y, z, size, rate, most, texture, index=0, tint=(1.0, 1.0, 1.0)):
        if texture is None:
            c.d.missing.append(tag)
            return
        c.w.entity("life:" + tag, tag, parent=L, t=(x, y, z), body=particle_body(
            Rate=rate, MaxParticles=most, ShapeSize=size, Lifetime=(4.0, 7.0), Speed=(0.25, 0.5), Direction=265,
            Spread=25, StartSize=(0.32, 0.375), StartRotation=(0, 360), AngularVelocity=(-120, 120), EndSizeScale=1.0,
            StartColor=tint + (1.0,), EndColor=tint + (0.0,), Gravity=(-0.06, -0.2), Drag=0.4, Turbulence=0.9,
            TextureHandle=texture, SpriteIndex=index))
    # tag, the cloud (1.5 m in front of the tree's foot, about 3 m up), its size, per second, at most, colour
    for (tag, x, y, z, size, rate, most, tint) in (
            ("Leaves Forecourt Maple", 16.5, -0.5, ZF + 3.0, (2.6, 0.6), 0.5, 6, RED),
            ("Leaves Terrace Maple", 8.0, 7.0, ZS + 3.2, (3.0, 0.6), 0.6, 7, RED),
            ("Leaves Gate Maple", 10.5, 19.0, ZG + 3.2, (3.0, 0.6), 0.6, 7, ORANGE),
            ("Leaves Gate Maple West", -27.0, 18.0, ZG + 3.0, (2.6, 0.6), 0.4, 5, RED),
            ("Leaves Gate Maple East", 27.0, 17.5, ZG + 3.0, (2.6, 0.6), 0.4, 5, ORANGE),
            ("Leaves Hall Maple", 13.5, 31.5, ZH + 4.0, (3.5, 0.8), 0.6, 7, RED)):
        leaves(tag, x, y, z, size, rate, most, pink, tint=tint)
    leaves("Leaves Bamboo Grove", -25.0, 37.5, ZG + 3.5, (5.0, 1.0), 0.6, 7, green, 1)
    leaves("Leaves Bamboo Grove 2", -23.0, 40.0, ZG + 3.0, (3.0, 0.8), 0.5, 6, green, 6)
    # the basin's trickle: drops from the bamboo spout's mouth (the kit's temizuya: (0.35, 1.13, 0.98) from its
    # origin) into the water 0.2 m below, pale and quick
    c.w.entity("life:Basin Trickle", "Basin Trickle", parent=L, t=(X_TEMIZUYA + 0.35, Y_TEMIZUYA + 1.13, ZF + 0.96),
               body=particle_body(Rate=9.0, MaxParticles=4, ShapeSize=(0.03, 0.02), Lifetime=(0.18, 0.24), Speed=(1.0, 1.2),
                                  Direction=270, Spread=4, StartSize=(0.035, 0.05), EndSizeScale=0.8,
                                  StartColor=(0.82, 0.92, 1.0, 0.9), EndColor=(0.82, 0.92, 1.0, 0.5), Gravity=(0, -1.5),
                                  Drag=0.0, Turbulence=0.0, EmissionIntensity=0.4, SortOrder=3))


def inari_torii(c, parent):
    """A small vermilion torii (the kit's shrine torii at half size: 2.3 m) before the wayside shrine in the forecourt's
    west corner (Nov 3), as the small Inari shrines in temple grounds have; no collider (the posts are thin)."""
    glb = os.path.join(city.MODELS, "JP_Shrine_Torii_A.glb")
    if os.path.exists(glb) is False:
        c.d.missing.append("JP_Shrine_Torii_A")
        return
    c.w.entity("model:Inari Torii", "Inari Torii", parent=parent, t=(-19.2, 1.75, ZF), s=(0.5, 0.5, 0.5),
               body=kit.mesh_body(kit.ensure_mesh_meta(glb), False))


def shrub(c, parent, kinds, x, y, z, rnd):
    """One of the delivered garden shrubs (Sprites/Plants/JP_Shrub_<kind>[_V2]), picked from `kinds`."""
    name = "JP_Shrub_" + rnd.choice(kinds) + ("_V2" if rnd.random() < SHRUB_VARIANT else "")
    c.plant(parent, name, x, y, z)


def planted(c, parent, grass, x0, x1, y0, y1, z, mats, rnd, edges="f", plants=True):
    """Grass on a terrace or a court: a grass sheet over the earth cap, kept back 0.8 m from the stone (`edges`: f
    front, b back, l left, r right). Each seam gets one ragged grass edge strip (its tufts reach 0.3 m onto the earth,
    its inner edge is ragged too so the texture's phase change hides), earth patches, tufts, ferns and small bushes;
    darker and lighter grass patches break up the sheet's tiling (Nov 2)."""
    inset = 0.8
    gx0, gx1 = x0 + (inset if "l" in edges else 0.0), x1 - (inset if "r" in edges else 0.0)
    gy0, gy1 = y0 + (inset if "f" in edges else 0.0), y1 - (inset if "b" in edges else 0.0)
    c.box(parent, grass, gx0, gx1, gy0, gy1, z, z + 0.02, tag=c._n("Grass"), shadows=False, collider=False)
    moss, dirt = mats
    seams = []
    if "f" in edges:
        seams.append(((gx0, gy0), (gx1, gy0), 0.0))
    if "b" in edges:
        seams.append(((gx0, gy1), (gx1, gy1), 180.0))
    if "l" in edges:
        seams.append(((gx0, gy0), (gx0, gy1), -90.0))
    if "r" in edges:
        seams.append(((gx1, gy0), (gx1, gy1), 90.0))
    out = {0.0: (0.0, -1.0), 180.0: (0.0, 1.0), -90.0: (-1.0, 0.0), 90.0: (1.0, 0.0)}
    edge = LOOK.get("edge")
    for ((ax, ay), (bx, by), rot) in seams:
        length = max(abs(bx - ax), abs(by - ay))
        if edge:
            ox, oy = out[rot]
            c.decal(parent, edge, (ax + bx) / 2 + ox * 0.1, (ay + by) / 2 + oy * 0.1, length, 1.0, z + 0.025, rot)
        n = max(1, int(length / 3.6))
        for k in range(n):
            f = (k + 0.5) / n
            x, y = ax + (bx - ax) * f, ay + (by - ay) * f
            if edge is None:
                c.decal(parent, moss, x, y, 4.0, 1.0, z + 0.025, rot)
            elif rnd.random() < 0.5:
                ox, oy = out[rot]
                c.plant(parent, "GrassTuft%02d" % rnd.randint(1, 24), x + rnd.uniform(-1.5, 1.5) + ox * 0.45,
                        y + oy * 0.45 + rnd.uniform(-0.1, 0.1), z)
            if rnd.random() < 0.45:
                # one image per box (the decal images are stored rolled half an image, see gen_temple_tex.py)
                c.decal(parent, LOOK.get("dirt", dirt), x + rnd.uniform(-1.0, 1.0), y + rnd.uniform(-0.4, 0.4), 2.25, 2.25, z + 0.03,
                        rnd.uniform(0, 360))
            if plants and rnd.random() < 0.6:
                shrub(c, parent, ("Fern", "Sasa", "Mondo", "MossStone", "Fern"), x + rnd.uniform(-1.2, 1.2),
                      y + (0.25 if rot in (0.0, 90.0, -90.0) else -0.25), z, rnd)
    patches = LOOK.get("patches")
    if patches:
        area = (gx1 - gx0) * (gy1 - gy0)
        size = 4.0                                    # exactly one image (128 px at 32 px/m): see gen_temple_tex.py
        for k in range(min(40, int(area / 28.0))):
            if gx1 - gx0 < size or gy1 - gy0 < size:
                break
            px, py = rnd.uniform(gx0 + size / 2, gx1 - size / 2), rnd.uniform(gy0 + size / 2, gy1 - size / 2)
            c.decal(parent, rnd.choice(patches), px, py, size, size, z + 0.022 + (k % 4) * 0.001, rnd.uniform(0, 360))


def trigger(c, parent, tag, x, y, z, size):
    c.w.entity("trigger:" + tag, tag, parent=parent, t=(x, y, z), body=(
        "    BoxColliderComponent:\n      Offset: [0, 0, 1]\n      Size: [%g, %g, 2]\n      Layer: Trigger\n      IsTrigger: true\n"
        % size))


def backdrop(c, parent):
    """The far hills and the dusk sky behind the temple hill (the city slice's painted layers, without its rooftops)."""
    for (name, wp, hp, width, y, z, emission, tint) in (
            ("JP_Backdrop_Far", 1086, 294, 260.0, 140.0, ZB[2] - 12.0, (0.72, 0.68, 0.7), (0.3, 0.3, 0.3)),
            ("JP_Backdrop_Sky_Dusk", 1086, 362, 420.0, 175.0, -30.0, (0.78, 0.72, 0.7), (0.2, 0.2, 0.2))):
        height = width * hp / wp
        mat, _ = sl.step_material(name, 32.0 * width, 32.0 * height, 0.5, emission, tint)
        c.w.entity("backdrop:" + name, "Backdrop " + name.replace("JP_Backdrop_", ""), parent=parent, t=(0, y, z), body=(
            "    MeshComponent:\n      Shape: Box\n      Size: [%g, 0.1, %g]\n      CastShadows: false\n      Materials:\n"
            "        - MaterialHandle: %d\n" % (width, height, mat)))


def build(c, args):
    d = c.d
    setts = sl.r2_material("JP_Paving_Setts.png", "JP_Paving_Setts")
    flag = sl.r2_material("JP_Paving_Flagstone.png", "JP_Paving_Flagstone")
    ishi = sl.r2_material("JP_Stone_IshigakiSmall.png", "JP_Stone_IshigakiSmall")
    granite = city.ground_material("JP_Stone_Granite")
    riser = sl.r3_material("JP_Stone_StepBlock", sl.RISER_TILING, (0.85, 0.85, 0.85), "JP_Stone_StepBlock_Riser")
    mossy = sl.r3_material("JP_Stone_StepBlock_Mossy", sl.RISER_TILING, (0.85, 0.85, 0.85), "JP_Stone_StepBlock_MossyRiser") \
        if os.path.exists(os.path.join(sl.R3, "JP_Stone_StepBlock_Mossy.png")) else riser
    road = sl.r3_material("JP_Ground_PackedDirt")
    # the temple's own ground (Nov 2): mossy grass, an unrutted earth band at the seams, the hall's gravel on it too
    grass = temple_material("JP_Ground_TempleGrass", 320, 320, cutoff=0)
    earth = sl.r3_material("JP_Ground_PackedDirt_B", (0.25, 0.25), (0.86, 0.82, 0.76), "JP_Ground_TempleEarth")
    gravel = sl.r3_material("JP_Ground_PackedDirt_B", (0.25, 0.25), (1.12, 1.07, 0.98), "JP_Ground_TempleGravel")
    hillstone = sl.r3_material("JP_Stone_ShrineMossy", (0.3, 0.3), (0.82, 0.86, 0.8), "JP_Stone_TempleHill")
    seam_mats = (sl.step_material("JP_Decal_MossEdge", 128, 32)[0], sl.step_material("JP_Decal_DirtPatch", 128, 128)[0])
    rnd = sl.random.Random(11)
    LOOK.clear()
    LOOK.update(rnd=rnd, edge=temple_material("JP_Decal_GrassEdge", 256, 32),
                creep=temple_material("JP_Decal_GrassCreep", 256, 16),
                patches=(temple_material("JP_Decal_GrassPatch_Dark", 128, 128), temple_material("JP_Decal_GrassPatch_Light", 128, 128)),
                leaves=sl.step_material("JP_Decal_MapleLeaves", 128, 128)[0], dirt=temple_material("JP_Decal_DirtPatch_C", 72, 72),
                earth=earth, sway={})

    # ---------------------------------------------------------------- ground and terraces
    G = c.group("Ground")
    c.slab(G, setts, -40, 40, -24, Y_STAIR0, ZF, 2.0)                          # the forecourt and the front strip
    c.box(G, flag, -HALF, HALF, -12.0, Y_STAIR0, ZF, ZF + 0.02, tag="Forecourt Path", shadows=False, collider=False)
    c.slab(G, road, 21.0, 60.0, -5.0, 1.0, ZF + 0.02, 0.5)                     # the lane to the city (east)
    # the planted side terraces beside the stair (stepped castle-stone walls, as in the reference)
    c.terrace(G, ishi, earth, -40, -GAP, 4.0, Y_STAIR1, ZS)
    c.terrace(G, ishi, earth, GAP, 40, 4.0, Y_STAIR1, ZS)
    planted(c, G, grass, -40, -GAP, 4.0, Y_STAIR1, ZS, seam_mats, rnd, edges="frb")
    planted(c, G, grass, GAP, 40, 4.0, Y_STAIR1, ZS, seam_mats, rnd, edges="flb")
    # the gate terrace, its front wall rising behind the side terraces; the hall terrace behind
    # the temple grounds are grass (Nov 2): only the path and the hall's gravel are not; a dirt band along every edge
    c.terrace(G, ishi, earth, -40, -GAP, Y_STAIR1, 46.0, ZG, ZS)
    c.terrace(G, ishi, earth, GAP, 40, Y_STAIR1, 46.0, ZG, ZS)
    c.terrace(G, ishi, earth, -GAP, GAP, Y_STAIR1, 46.0, ZG, ZS)
    c.box(G, flag, -1.6, 1.6, Y_STAIR1, Y_HALL_STEPS0, ZG, ZG + 0.03, tag="Gate Path", shadows=False, collider=False)
    planted(c, G, grass, -40, -1.3, Y_STAIR1 + 0.2, 46.0, ZG, seam_mats, rnd, edges="fr")
    planted(c, G, grass, 1.3, 40, Y_STAIR1 + 0.2, 46.0, ZG, seam_mats, rnd, edges="fl")
    c.terrace(G, ishi, earth, -18.0, 18.0, Y_HALL_STEPS1, 46.0, ZH, ZG)
    planted(c, G, grass, -18.0, -1.7, Y_HALL_STEPS1, 46.0, ZH, seam_mats, rnd, edges="flr")
    planted(c, G, grass, 1.7, 18.0, Y_HALL_STEPS1, 46.0, ZH, seam_mats, rnd, edges="flr")
    c.box(G, gravel, -8.4, 8.4, Y_HALL - 2.9, Y_HALL + 11.8, ZH, ZH + 0.035, tag="Hall Gravel", shadows=False, collider=False)
    c.box(G, flag, -2.0, 2.0, Y_HALL_STEPS1, Y_HALL - 2.9, ZH, ZH + 0.04, tag="Hall Path", shadows=False, collider=False)
    for s_ in (-1, 1):                                                         # grass creeping onto the path sides
        c.decal(G, LOOK["creep"], s_ * 1.5, (Y_STAIR1 + 0.4 + Y_HALL_STEPS0) / 2, Y_HALL_STEPS0 - Y_STAIR1 - 0.6, 0.5, ZG + 0.04,
                -90.0 if s_ > 0 else 90.0)
        c.decal(G, LOOK["creep"], s_ * 1.9, (Y_HALL_STEPS1 + Y_HALL - 2.9) / 2, Y_HALL - 2.9 - Y_HALL_STEPS1, 0.5, ZH + 0.045,
                -90.0 if s_ > 0 else 90.0)
    # the wooded hill behind the hall (Nov 2: the background was a flat lawn): three mossy-walled steps, planted
    base = ZG
    for i in range(3):
        c.terrace(G, hillstone, earth, -70, 70, YB[i], YB[i + 1], ZB[i], base)
        planted(c, G, grass, -70, 70, YB[i], YB[i + 1], ZB[i], seam_mats, rnd, edges="f", plants=False)
        base = ZB[i]
    backdrop(c, G)

    # ---------------------------------------------------------------- the temple stair
    S = c.group("Temple Stair")
    c.stairs(S, granite, 0.0, Y_STAIR0, Y_STAIR1, ZF, ZG, riser=mossy)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_ * HALF, s_ * GAP))
        c.box(S, ishi, x0, x1, Y_STAIR0, Y_STAIR1, ZF, ZG + 0.5, tag=c._n("Cheek Wall"))
        c.box(S, granite, x0, x1, Y_STAIR0, Y_STAIR1, ZG + 0.5, ZG + 0.62, tag=c._n("Cheek Coping"), shadows=False)
        for (lx, ly, lz) in ((s_ * (GAP + 0.8), Y_STAIR0 - 0.8, ZF), (s_ * 3.6, Y_GATE + 0.8, ZG)):
            c.model(S, "Props/JP_Prop_StoneLantern_A", lx, ly, lz)
            d.lantern(S, lx, ly, lz + STONE_LIGHT_Z, 1.7, 4.6)
    # the hall steps (a short flight from the gate terrace)
    c.stairs(S, granite, 0.0, Y_HALL_STEPS0, Y_HALL_STEPS1, ZG, ZH, width=4.0, riser=riser)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_ * 2.0, s_ * 2.6))
        c.box(S, ishi, x0, x1, Y_HALL_STEPS0, Y_HALL_STEPS1, ZG, ZH + 0.35, tag=c._n("Cheek Wall"))

    # ---------------------------------------------------------------- forecourt
    F = c.group("Forecourt")
    # low tile-capped walls round the court (Nov 2: continuous; the only openings are the entrance in the middle of the
    # front and the lane from the city on the east)
    for x in (-20.0, -16.0, -12.0, -8.0, -4.0, 4.0, 8.0, 12.0, 16.0, 20.0):
        c.model(F, "JP_Rich_CompoundWall_4m_A", x, -12.0, ZF)
    for y in (-10.0, -6.0, -2.0, 2.0):
        c.model(F, "JP_Rich_CompoundWall_4m_A", -22.25, y, ZF, 90.0)
    for y in (-10.0, -7.0, 3.0):
        c.model(F, "JP_Rich_CompoundWall_4m_A", 22.25, y, ZF, 90.0)
    # the hand-washing pavilion (temizuya) at the left of the stair foot, a water jar beside it; the bench in the west
    # corner (Nov 2: a 3D pavilion instead of the kakehi card; placed by the user)
    c.building(F, "JP_Temple_Temizuya_A", X_TEMIZUYA, Y_TEMIZUYA, ZF)
    c.model(F, "Props/JP_Prop_WaterJar_A", -10.43, 1.93, ZF)
    c.model(F, "Props/JP_Prop_TeaBench_A", -18.62, -6.61, ZF)
    # where bad fortunes are tied (omikuji rack with its offering box) and the plaque rack beside it, at the right of
    # the stair foot
    c.model(F, "Modules/JP_Temple_OmikujiRack_A", X_OMIKUJI, Y_OMIKUJI, ZF)
    sl.prop_card(c, F, "JP_Temple_EmaRack_A", 8.02, 3.27, ZF, scale=0.8, sort=1)
    sl.prop_card(c, F, "JP_Temple_Hokora_A", -19.2, 2.6, ZF, scale=0.8, sort=1)        # a small wayside shrine
    inari_torii(c, F)
    for (x, y) in ((-6.0, -6.0), (6.0, -6.0), (-6.0, 0.0), (6.0, 0.0)):        # stone lanterns along the path
        c.model(F, "Props/JP_Prop_StoneLantern_A", x, y, ZF)
        d.lantern(F, x, y, ZF + STONE_LIGHT_Z, 1.7, 4.6)
    sl.prop_card(c, F, "JP_Field_Jizo_B", -14.5, 2.6, ZF, sort=1)
    sl.prop_card(c, F, "JP_Gate_Noticeboard", 13.0, 2.6, ZF, sort=1, tag="Sign TempleNotices")
    # the forecourt's trees stand in low stone beds of earth (Nov 2: on the bare paving they looked dropped there)
    earth = LOOK["earth"]
    for (kind, x, y, sc, fl, w, dd) in (("pine_large", -16.5, 0.5, 1.0, False, 3.2, 2.4), ("maple", 16.5, 1.0, 1.0, False, 2.8, 2.2),
                                        ("maple_young", -18.5, -8.5, 1.1, True, 2.4, 2.0)):
        tree(c, F, kind, x, y, sl.tree_bed(c, F, ishi, earth, x, y, ZF, w, dd, rnd=rnd), sc, flip=fl, ground=ZF)
        c.model(F, "Props/JP_Prop_BenchMat_A", x, y - dd / 2 - 0.7, ZF)          # a bench in front of each bed

    # ---------------------------------------------------------------- the planted side terraces
    T = c.group("Side Terraces")
    # a clipped hedge across each side terrace's lawn (Nov 2), just behind its front edge: the delivered hedge sprite
    # end to end, the two deliveries alternating
    for s_ in (-1, 1):
        k = 0
        x = GAP + 1.0
        while x < 39.0:
            name = "JP_Shrub_Hedge" + ("_V2" if k % 2 else "")
            c.plant(T, name, s_ * x, 5.3, ZS)
            x += 1.05
            k += 1
    tree(c, T, "pine_b", -8.5, 9.0, ZS, 1.0, flip=True)
    tree(c, T, "maple", 8.0, 8.5, ZS, 1.1)
    tree(c, T, "maple_young", -15.0, 11.5, ZS, 1.1)
    tree(c, T, "pine_young", -21.0, 10.5, ZS, 1.0)
    tree(c, T, "maple_young", 20.5, 9.5, ZS, 1.0, flip=True)
    tree(c, T, "pine_small", 14.0, 11.0, ZS, 1.0, flip=True)

    # ---------------------------------------------------------------- gate terrace
    P = c.group("Gate Terrace")
    gate = c.building(P, "JP_Temple_Gate_A", 0.0, Y_GATE, ZG)
    beam = gate.get("beam_z", 3.7) if gate else 3.7
    lp = os.path.join(sl.PROPS_R1, "JP_Gate_LanternPair.png")
    if os.path.exists(lp):
        lh = kit.png_size(lp)[1] / 32.0
        sl.prop_card(c, P, "JP_Gate_LanternPair", 0.0, Y_GATE + 0.15, ZG + beam - 0.2 - lh, sort=3, shadow=0)
        d.lantern(P, 0.0, Y_GATE - 0.1, ZG + beam - 1.0, 1.3, 4.5)
        # a low warm pool on the paving under the lantern pair and through the passage (the gate's hero light)
        d.lantern(P, 0.0, Y_GATE + 0.6, ZG + 1.1, 2.4, 5.4, kind="Lantern Pool")
    for s_ in (-1, 1):                                                         # the vermilion fence along the edge
        for k in range(7):
            c.model(P, "Modules/JP_Fence_Vermilion_4m", s_ * (4.6 + 4.0 * k), Y_STAIR1 + 0.5, ZG)
        for y in range(18, 46, 4):                                             # the precinct's plaster side walls
            c.model(P, "JP_Rich_CompoundWall_4m_A", s_ * X_PRECINCT, float(y), ZG, 90.0)
        for y in (19.5, 22.5):                                                 # stone lanterns along the path
            c.model(P, "Props/JP_Prop_StoneLantern_A", s_ * 2.6, y, ZG)
            d.lantern(P, s_ * 2.6, y, ZG + STONE_LIGHT_Z, 1.7, 4.6)
    # the pagoda (left) and the bell tower (right), clear of the hall terrace's walls (x +-18)
    c.model(P, "Props/JP_Prop_IncenseBurner_A", 0.0, Y_BURNER, ZG)                # the incense burner (jokoro)
    d.lantern(P, 0.0, Y_BURNER, ZG + 1.0, 0.5, 2.0, kind="Lantern Embers")
    c.building(P, "JP_Temple_Pagoda_A", -23.5, 30.0, ZG)       # three tiers on a 9.6 m podium (rebuilt Nov 2)
    c.building(P, "JP_Temple_BellTower_A", 23.0, 27.0, ZG)
    for (x, y) in ((-20.0, 24.5), (-27.0, 24.5), (20.4, 24.4), (25.6, 24.4)):   # stone lanterns in front of both
        c.model(P, "Props/JP_Prop_StoneLantern_A", x, y, ZG)
        d.lantern(P, x, y, ZG + STONE_LIGHT_Z, 1.7, 4.6)
    tree(c, P, "pine", -10.5, 21.0, ZG, 1.0)
    tree(c, P, "maple_b", 10.5, 20.5, ZG, 1.0, flip=True)
    tree(c, P, "maple_young", 16.5, 22.0, ZG, 1.15)
    for s_ in (-1, 1):                                       # young pines either side of the hall steps
        tree(c, P, "pine_young", s_ * 4.3, 24.4, ZG, 1.0, flip=s_ > 0)
        # the guardian lion-dogs (komainu) facing each other across the foot of the hall steps: the user's pair (Nov 3,
        # as at the shrine glade), drawn as a left one facing right (A) and a right one facing left (B), so neither is
        # flipped; at 0.8 (1.45 m, the old sprite's height); the old JP_Temple_Komainu_A stands in if they are missing
        pair = os.path.join(kit.SPRITES, "JP_Shrine_Komainu_%s.png" % ("A" if s_ < 0 else "B"))
        if os.path.exists(pair + ".hymeta"):
            sprite_card(c, P, pair, s_ * 3.25, 23.2, ZG, scale=0.8, sort=1, shadow=0.5, tag="Komainu %s" % ("A" if s_ < 0 else "B"))
        elif s_ < 0:
            sl.prop_card(c, P, "JP_Temple_Komainu_A", s_ * 3.25, 23.2, ZG, scale=0.8, sort=1, shadow=0.5)
        else:
            sprite_card(c, P, os.path.join(kit.SPRITES, "JP_Temple_Komainu_A.png"), s_ * 3.25, 23.2, ZG, scale=0.8, flip=True,
                        sort=1, shadow=0.5)
    # the sacred pine (shinboku) with its rope and paper streamers, a stone bed and a bench before it (drawn 1:1)
    # (Nov 3, 11:19: the user moved the pine 2 m back, behind its bed, onto the terrace; the bench back 0.24 m and the
    # small pine on the left 1.2 m back; ported here)
    sl.tree_bed(c, P, ishi, LOOK["earth"], -7.6, 25.4, ZG, 3.2, 2.2, rnd=rnd)
    sl.prop_card(c, P, "JP_Temple_SacredPine_A", -7.722, 27.442, ZG + 0.113, scale=1.0, sort=0, shadow=0.5)
    c.model(P, "Props/JP_Prop_BenchMat_A", -7.6, 23.838, ZG)
    tree(c, P, "pine_small", -16.5, 22.719, ZG, 1.0, flip=True)
    for (x, y, sc) in ((-27.5, 39.0, 1.2), (-25.0, 42.5, 1.1), (-28.5, 44.0, 1.25), (-21.0, 43.5, 1.0)):   # bamboo grove
        tree(c, P, "bamboo" if sc > 1.1 else "bamboo_b", x, y, ZG, sc * 0.85, flip=sc < 1.1)
    for (x, y) in ((-23.6, 40.6), (-29.2, 41.2), (-26.2, 45.2), (-22.8, 45.6), (-29.6, 37.4)):   # young bamboo round it
        tree(c, P, "bamboo_young", x, y, ZG, 1.0, flip=x < -26)
    for (kind, x, y, sc, fl) in (("maple", -27.0, 19.5, 1.1, False), ("pine", -20.0, 38.5, 1.2, True), ("maple", 27.0, 19.0, 1.1, True),
                                 ("pine", 26.5, 36.0, 1.25, False), ("maple_young", 21.0, 39.5, 1.15, True), ("pine_small", 28.0, 42.5, 1.0, False),
                                 ("maple_young", -17.5, 18.0, 1.1, False), ("maple_young", 18.5, 33.5, 1.1, True)):
        tree(c, P, kind, x, y, ZG, sc, flip=fl)
    for (x, y) in ((-22.0, 19.0), (22.5, 19.5), (-29.0, 31.0), (19.5, 34.5), (-20.0, 36.4), (27.5, 31.0)):
        shrub(c, P, ("Azalea_B", "Hedge", "Camellia"), x, y, ZG, rnd)
        shrub(c, P, ("Azalea_A", "Sasa", "Nandina"), x + 1.1, y - 0.4, ZG, rnd)

    # ---------------------------------------------------------------- hall terrace
    H = c.group("Hall Terrace")
    hall = c.building(H, "JP_Temple_Hall_A", 0.0, Y_HALL, ZH)
    if hall:
        head = hall.get("head_z", 3.62)
        sl.prop_card(c, H, "JP_Noren_Temple_Long_A", 0.0, Y_HALL + hall.get("wall_y", 1.3) - 0.08, ZH + head - 0.02, sort=1,
                     shadow=0, hanging=True)
        for x in (-4.4, -2.2, 2.2, 4.4):                                       # the lattice doors' glow
            d.window(H, x, Y_HALL + 0.9, ZH + 2.0, 3.0)
        d.lantern(H, 0.0, Y_HALL + 0.6, ZH + 2.6, 1.4, 4.0)
        # the hall's hero light: a warm pool on the steps and the veranda, the doors' glow spilling out either side,
        # and the two hanging bronze lanterns (the kit hangs them at x +-4.4, 3.2 to 3.7 m)
        d.lantern(H, 0.0, Y_HALL - 0.9, ZH + 1.7, 2.3, 5.8, kind="Lantern Pool")
        for x in (-3.3, 3.3):
            d.lantern(H, x, Y_HALL + 0.4, ZH + 1.9, 0.9, 3.8, kind="Lantern Glow")
        for x in (-4.4, 4.4):
            d.lantern(H, x, Y_HALL - 0.05, ZH + 3.45, 1.0, 3.6)
    for s_ in (-1, 1):
        c.model(H, "Props/JP_Prop_StoneLantern_A", s_ * 3.4, Y_HALL - 2.6, ZH)
        d.lantern(H, s_ * 3.4, Y_HALL - 2.6, ZH + STONE_LIGHT_Z, 1.7, 4.6)
        # fenced all round (Nov 2): the vermilion fence along the front edge, plaster walls along the sides and back
        for k in range(4):
            c.model(H, "Modules/JP_Fence_Vermilion_4m", s_ * min(4.6 + 4.0 * k, 15.75), Y_HALL_STEPS1 + 0.35, ZH)
        for y in (29.0, 33.0, 37.0, 41.0, 43.4):
            c.model(H, "JP_Rich_CompoundWall_4m_A", s_ * X_HALL_WALL, y, ZH, 90.0)
        for y in (30.5, 34.5, 38.5):                                           # a row of stone lanterns along each wall
            c.model(H, "Props/JP_Prop_StoneLantern_A", s_ * (X_HALL_WALL - 1.6), y, ZH)
            d.lantern(H, s_ * (X_HALL_WALL - 1.6), y, ZH + STONE_LIGHT_Z, 1.5, 4.2)
        for (x, y) in ((13.0, 32.6), (14.2, 36.6), (12.8, 43.5), (9.6, 44.2)):  # clipped shrubs
            shrub(c, H, ("Azalea_A", "Azalea_B", "Hedge"), s_ * x, y, ZH, rnd)
    for x in range(-16, 17, 4):
        c.model(H, "JP_Rich_CompoundWall_4m_A", float(x), 45.6, ZH)
    for (kind, x, y, sc, fl) in (("pine_large", -14.0, 34.0, 1.0, False), ("maple_large", 13.5, 33.0, 0.95, True),
                                 ("pine_large", 9.0, 44.3, 1.05, True), ("pine_b", -6.0, 44.4, 1.1, False), ("maple", 15.0, 42.8, 1.0, False)):
        tree(c, H, kind, x, y, ZH, sc, flip=fl)
    for (kind, x, y, sc, fl) in (("maple_young", -11.0, 30.5, 1.2, False), ("pine_young", 11.5, 29.5, 1.3, True),
                                 ("maple_young", -12.5, 40.0, 1.2, True), ("pine_young", 12.0, 41.0, 1.25, False)):
        tree(c, H, kind, x, y, ZH, sc, flip=fl)
    for (x, y) in ((-9.6, 29.0), (-10.2, 33.5), (9.8, 33.0), (10.4, 37.5), (-13.0, 39.5), (11.5, 40.5), (-4.6, 28.6), (4.8, 28.4)):
        shrub(c, H, ("Azalea_A", "Azalea_B", "Camellia", "Nandina"), x, y, ZH, rnd)
        shrub(c, H, ("Sasa", "Fern", "Mondo"), x + 0.9, y - 0.3, ZH, rnd)
    for (x, y) in ((-6.0, 19.0), (6.2, 19.4), (-10.6, 22.8), (7.8, 24.0), (-13.0, 18.0), (13.0, 17.6)):
        shrub(c, P, ("Azalea_A", "Azalea_B", "Nandina", "Camellia"), x, y, ZG, rnd)
        shrub(c, P, ("Sasa", "MossStone", "Fern"), x - 0.9, y - 0.3, ZG, rnd)

    # ---------------------------------------------------------------- the wooded hill behind (Nov 2: fill the background)
    K = c.group("Hill Behind")
    c.building(K, "JP_Rich_Storehouse_A", -23.0, YB[0] + 2.0, ZB[0])           # the temple's storehouse
    c.building(K, "JP_Rich_Residence_A", 25.0, YB[0] + 1.5, ZB[0])             # the monks' quarters
    hill = sl.random.Random(23)
    # the delivered trees at their full size (7 m) and the Japanese cedars
    near = ("JP_Tree_BlackPine_A_Large", "JP_Tree_BlackPine_B_Large", "JP_Tree_BlackPine_A_Large", "JP_Tree_Maple_Red_A_Large",
            "JP_Tree_Maple_Red_B_Large", "JP_Tree_Maple_Orange_Large", "JP_Tree_Bamboo_Clump_Large", "JP_Tree_Bamboo_Clump_B_Large",
            "JP_Tree_Sugi_A_Large", "JP_Tree_Sugi_B_Large", "JP_Tree_Sugi_A_Large", "JP_Tree_Sugi_B_Large")   # the cedars (round 2)
    for (i, rows, spacing, dim) in ((0, (YB[0] + 1.2, YB[0] + 5.5, YB[0] + 10.0), 3.4, 1.0), (1, (YB[1] + 1.5, YB[1] + 7.0, YB[1] + 12.5), 4.0, 0.86),
                                    (2, (YB[2] + 2.0, YB[2] + 9.0), 4.6, 0.74)):
        for (r, y) in enumerate(rows):
            x = -50.0 + hill.uniform(0, spacing)
            while x < 50.0:
                yy = y + hill.uniform(-1.2, 1.2)
                if i == 0 and (abs(x + 23.0) < 4.5 or abs(x - 25.0) < 6.5):   # keep the two buildings clear
                    x += spacing
                    continue
                name = hill.choice(near)
                if os.path.exists(os.path.join(sl.TREES, name + ".png.hymeta")) is False:
                    name = "FirTallB"
                sc = hill.uniform(0.85, 1.1) if name.startswith("JP_") else hill.uniform(0.95, 1.3)
                sprite_card(c, K, os.path.join(sl.TREES, name + ".png"), x, yy, ZB[i], color=(dim, dim, dim, 1.0),
                            scale=sc, flip=hill.random() < 0.5, shadow=0.4 if i == 0 else 0.0)
                x += spacing * hill.uniform(0.75, 1.25)

    # ---------------------------------------------------------------- people
    W = c.group("Townsfolk")
    c.townsperson(W, "Monk", -3.2, Y_GATE + 1.2, ZG, "IdleDown", row="TempleMonk")
    c.npc(W, "Townsman", -11.2, -2.0, ZF, "IdleRight")
    c.npc(W, "Townswoman", -10.0, -2.1, ZF, "IdleLeft")
    sweep = dict(wait=(4.0, 9.0), facing="Down", action="Work")
    ys = [Y_STAIR1 - 1.5, (Y_STAIR0 + Y_STAIR1) / 2, Y_STAIR0 + 1.2]
    stair = c.route(W, "StairSweep", [dict(sweep, at=(1.8, y, ZF + (y - Y_STAIR0) / (Y_STAIR1 - Y_STAIR0) * (ZG - ZF))) for y in ys])
    c.route_walker(W, "TempleMonk", stair, "PingPong", 0.5, delay=1.0)
    court = c.route(W, "CourtSweep", [dict(sweep, at=(-4.0, -3.5, ZF)), dict(sweep, at=(3.0, -4.5, ZF), chance=0.7),
                                      dict(sweep, at=(8.0, -2.0, ZF))])
    c.route_walker(W, "TempleMonk", court, "PingPong", 0.5, start=1, delay=3.0)
    # pilgrims: in from the lane, wash at the basin, up the stair, through the gate, waft incense at the burner (one
    # on each side of it), pray at the hall, and back
    def pilgrim(side, dx):
        return [(19.0, -2.0 + dx * 0.5, ZF),
                dict(at=(X_TEMIZUYA - 0.6 + dx, Y_TEMIZUYA - 0.6, ZF), wait=(3.0, 5.0), facing="Up"),     # washing hands
                (-1.0 + dx, Y_STAIR0 - 0.6, ZF), (-1.0 + dx, Y_STAIR1, ZG), (side * 1.3, Y_GATE + 3.0, ZG),
                dict(at=(side * 1.05, Y_BURNER, ZG), wait=(3.0, 6.0), facing="Left" if side > 0 else "Right"),
                (side * 1.3, Y_BURNER + 1.5, ZG), (-0.8 + dx, Y_HALL_STEPS0 - 0.2, ZG), (-0.8 + dx, Y_HALL_STEPS1 + 0.2, ZH),
                dict(at=(-0.8 + dx, Y_HALL - 3.0, ZH), wait=(6.0, 10.0), facing="Up")]  # praying at the hall
    walk1 = c.route(W, "PilgrimA", pilgrim(-1, 0.0))
    c.route_walker(W, "Townswoman", walk1, "PingPong", 0.9, start=0, delay=2.0)
    walk2 = c.route(W, "PilgrimB", pilgrim(1, 1.2))
    c.route_walker(W, "OldWoman", walk2, "PingPong", 0.5, start=5, delay=6.0)
    walk3 = c.route(W, "Visitor", [(19.0, -1.0, ZF), dict(at=(12.6, 1.6, ZF), wait=(4.0, 7.0), facing="Up"),   # the notices
                                   dict(at=(X_OMIKUJI - 0.4, Y_OMIKUJI - 0.7, ZF), wait=(4.0, 7.0), facing="Up", chance=0.8),  # ties a slip
                                   dict(at=(-17.8, -5.9, ZF), wait=(5.0, 9.0), facing="Down", chance=0.7),     # the bench
                                   (2.0, -9.0, ZF)])
    c.route_walker(W, "Merchant", walk3, "Loop", 0.9, start=1, delay=4.0)

    # ---------------------------------------------------------------- invisible walls, the way back to the city
    B = c.group("Blockers")
    c.blocker(B, -22.0, 25.0, -12.6, -11.6)                  # the front (the low walls and the gap between them)
    c.blocker(B, -23.0, -22.0, -12.6, 4.0)                   # the west side
    c.blocker(B, 25.0, 26.0, -6.0, 4.0)                      # past the lane's end (the exit is just before it)
    c.blocker(B, 21.0, 25.0, 1.2, 4.0)
    c.blocker(B, 21.0, 25.0, -6.6, -5.6)
    # the gate terrace: the fence along its front edge, the plaster walls at its sides (the pagoda and the bell tower
    # are inside them now, Nov 2)
    c.blocker(B, -X_PRECINCT - 0.4, -2.6, Y_STAIR1 + 0.3, Y_STAIR1 + 0.7, ZG)
    c.blocker(B, 2.6, X_PRECINCT + 0.4, Y_STAIR1 + 0.3, Y_STAIR1 + 0.7, ZG)
    c.blocker(B, -X_PRECINCT - 0.4, -X_PRECINCT + 0.3, Y_STAIR1, 46.0, ZG)
    c.blocker(B, X_PRECINCT - 0.3, X_PRECINCT + 0.4, Y_STAIR1, 46.0, ZG)
    c.blocker(B, -18.0, -2.0, Y_HALL_STEPS1 - 0.6, Y_HALL_STEPS1, ZG)       # the hall terrace's front wall
    c.blocker(B, 2.0, 18.0, Y_HALL_STEPS1 - 0.6, Y_HALL_STEPS1, ZG)
    c.blocker(B, -18.0, -2.4, Y_HALL_STEPS1 + 0.15, Y_HALL_STEPS1 + 0.55, ZH)   # its fence
    c.blocker(B, 2.4, 18.0, Y_HALL_STEPS1 + 0.15, Y_HALL_STEPS1 + 0.55, ZH)
    c.blocker(B, -18.0, 18.0, Y_HALL + 11.0, Y_HALL + 12.0, ZH)             # behind the hall
    c.blocker(B, -18.6, -17.6, Y_HALL_STEPS1, Y_HALL + 12.0, ZH)
    c.blocker(B, 17.6, 18.6, Y_HALL_STEPS1, Y_HALL + 12.0, ZH)
    trigger(c, B, "Exit JapaneseCitySlice", 23.6, -2.2, ZF, (1.2, 6.4))
    c.w.entity("arrive:city", "Arrive JapaneseCitySlice", parent=B, t=(20.5, -2.2, ZF))

    # ---------------------------------------------------------------- camera zones
    Zg = c.group("Camera Zones")
    c.zone(Zg, "Forecourt", 0.0, -3.0, (48.0, 18.0), tilt=75, fov=30, scale=0.9, lead=1.5, rise=2.4,
           pan_min=(-14.0, -2.0), pan_max=(14.0, 2.0), blend=1.4)
    c.zone(Zg, "Stair", 0.0, 10.5, (8.0, 11.0), priority=1, tilt=72, fov=30, scale=0.9, lead=3.5, rise=3.0,
           pan_min=(-0.5, -4.0), pan_max=(0.5, 3.0), blend=1.4)
    c.zone(Zg, "Gate", 0.0, 27.0, (34.0, 24.0), tilt=73, fov=30, scale=0.95, lead=2.0, rise=3.4,
           pan_min=(-6.0, -8.0), pan_max=(6.0, 2.0), blend=1.4)
    # between the gate and the hall steps (Nov 3): steeper (52 degrees from straight down, 73 elsewhere) and looking
    # 6 m ahead, so the hall and its steps fill the frame and the gate she has just walked through sinks to the
    # frame's bottom instead of fading across the komainu (tried 62 / 55 / 50 with leads 4.5 to 7: rv_td25_*.png)
    c.zone(Zg, "Hall Approach", 0.0, 22.6, (14.0, 8.0), priority=2, tilt=52, fov=30, scale=0.95, lead=6.0, rise=3.2,
           pan_min=(-3.0, -1.0), pan_max=(3.0, 1.0), blend=1.2)
    life(c)


def place_user_edits(lines):
    """Moves the USER_PLACED entities to where the user put them (x and y of the Translation; z as built)."""
    for tag, (x, y) in USER_PLACED.items():
        try:
            i = lines.index("      Tag: %s" % tag)
        except ValueError:
            print("user edit not applied (no entity tagged %r)" % tag)
            continue
        j = next(k for k in range(i, len(lines)) if lines[k].startswith("      Translation: "))
        z = lines[j].split("[", 1)[1].rstrip("]").split(",")[2].strip()
        lines[j] = "      Translation: [%s, %s, %s]" % ("%g" % x, "%g" % y, z)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", default="Dusk", choices=["Day", "Dusk", "Night"])
    ap.add_argument("--start", default="lane", choices=["lane", "stair", "gate", "hall"])
    ap.add_argument("--start-pos", default="", help="x,y,z: start the player here instead (overrides --start)")
    args = ap.parse_args()
    summary = json.load(open(kit.SUMMARY))
    character_body = kit.character_body
    kit.character_body = lambda *a, **k: character_body(*a, **k).replace("      Strength: 0.6\n", "      Strength: 0.75\n")
    profiles = temple_profiles(sl.city_profiles())
    scene_handle = kit.handle_of(SCENE) if os.path.exists(SCENE) else kit.stable_id("scene")
    c = sl.Slice(summary)
    w = c.w
    starts = {"lane": (20.5, -2.2, ZF), "stair": (0.0, Y_STAIR0 - 1.0, ZF), "gate": (0.0, Y_GATE + 3.4, ZG + 0.05),
              "hall": (0.0, Y_HALL - 3.0, ZH + 0.05)}
    start = tuple(float(v) for v in args.start_pos.split(",")) if args.start_pos else starts[args.start]
    w.entity("Player", "Player", t=start, s=(2.5, 2.34375, 3), body=kit.character_body(kit.MIKO_TEXTURE, kit.MIKO_ANIM, "IdleUp", "Miko_0", True))
    w.entity("Camera", "Camera", t=(start[0], start[1] - 14.0, start[2] + 10.0), r=(0.977, 0, 0), body=(
        "    CameraComponent:\n      Primary: true\n      Projection: Perspective\n      PerspectiveFOV: 30\n      PerspectiveNear: 0.5\n"
        "      PerspectiveFar: 300\n      OrthographicSize: 13\n      OrthographicNear: -1\n      OrthographicFar: 1\n      FocusDistance: 0\n"
        "    NativeScriptComponent:\n      Scripts:\n        - Name: CameraFollowScript\n          Properties:\n"
        "            Bounds: [60, 60]\n            LookAhead: 0.5\n            Smoothing: 6\n"
        "        - Name: OccluderFadeScript\n          Properties:\n            Target: %d\n            FadedOpacity: 0.15\n"
        "            SpriteFadedOpacity: 0.12\n            FadeSpeed: 4\n            Radius: 0.3\n            HeadHeight: 1.5\n"
        "            MaxSize: 30\n" % kit.stable_id("Player")))
    w.entity("Director", "Temple Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldSceneScript\n          Properties:\n"
        "            SceneName: TempleDistrict\n            TiltDegrees: 68\n            FieldOfView: 30\n"
        "            FocusLead: 2\n            FocusRise: 2\n            BoundsMin: [-20, -8]\n            BoundsMax: [20, 40]\n"
        "            YawDegrees: 0\n            ViewScale: 1\n"
        "            DuskSunColor: [1, 0.7, 0.45]\n            DuskSunIntensity: 1.0\n            DuskLanterns: 1.7\n            DuskWindows: 1.4\n"
        "            NightLanterns: 1.8\n            NightWindows: 1.4\n"
        "            StartPreset: %d\n            DayProfile: %d\n            DuskProfile: %d\n            NightProfile: %d\n")
        % (("Day", "Dusk", "Night").index(args.profile), profiles["Day"], profiles["Dusk"], profiles["Night"]))
    w.entity("FieldDirector", "Field Director", body=(
        "    NativeScriptComponent:\n      Scripts:\n        - Name: FieldDirectorScript\n          Properties:\n"
        "            InteractRange: 1.7\n            MaxReachHeight: 1\n            FacingDegrees: 80\n            CloseRange: 0.6\n"
        "            SwitchMargin: 0.3\n            EyeHeight: 1\n            ItemHeight: 0.6\n            MessageTime: 3\n"
        "            LeaveTime: 0.6\n            ArriveTime: 0.6\n            RestTime: 1.2\n"))
    w.entity("Lighting", "Lighting")
    # the sun low from the left and a little behind the camera (Nov 2): it travels (0.78, 0.26, -0.57), 35 degrees up,
    # so posts, walls and trees throw their shadows sideways across the terraces where the camera sees them, and the
    # faces towards the camera are in half light (from behind the camera everything was lit and looked flat)
    w.entity("Sun", "Sun", parent="Lighting", t=(-12, -6, 12), r=(0.9643, 0, -1.2497), body=(
        "    DirectionalLightComponent:\n      Color: [1, 0.82, 0.64]\n      Intensity: 1.0\n      IsMainLight: true\n      CastShadows: true\n"
        "      ShadowStrength: 0.9\n      ShadowDistance: 55\n      ShadowBias: 1\n"))
    build(c, args)
    place_user_edits(w.lines)
    header = (
        "Handle: %d\nScene: TempleDistrict\nRenderSettings:\n  OverrideProject: true\n  Features:\n    Depth: Shared\n"
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
