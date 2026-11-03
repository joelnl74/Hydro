"""Add the countryside's quests to Oxygen/project/Assets/Scenes/JapaneseCountryside.hyscene (docs/TestBed.md,
"Countryside quest" and "Japanese countryside"):

  - "The Guardian's Duty" (CountrysideQuestScript, Game/src/Scripts/Countryside/CountrysideQuestScript.h): the
    leyline's points ("Leyline 0" to "Leyline N", from the shrine stairs down the road, west along the bund path into
    the cedar forest, to the ancient tree), the tree ("Ley Tree"), Orin Kenshi waiting there ("Npc Orin", the stand-in
    sheet of build_orin_standin.py) with the spirits circling it ("Ley Wisp 1" to "3") and the marker shown over him
    after a lost battle.
  - The way down into the leyline that opens once he joins: the rift in front of the tree's roots ("Ley Rift", and
    its glowing veins "Ley Rift Glow", both flat on the ground and hidden until then) and its trigger area ("Leyline
    Gate", a box trigger; walking into it: "the way is sealed", Along the Leyline's second stage).
  - The side quests' givers' markers ("Quest Marker <Giver>", World canvases QuestGiverScript moves over the NPCs the
    scene builder placed: Jokai and Sanzo at the camp, Genji and Okiku at the farm, the fox shrine's voice) and the script ("Side Quests").
  - Where the player arrives from the Shrine Glade ("Arrive ShrineGlade", on the landing halfway up the mountain).

    python scripts/game/quests/build_countryside_quest.py

scripts/game/japanese_city/build_countryside.py runs it after writing the scene (and the field UI, which the quest uses:
the dialogue box, the quest tracker, the menu). On its own: close the scene in the editor, then run it. Like the UI
builder it removes what an earlier run added (by tag, with every descendant) and appends it again with IDs made from
the tags, so it can be run as often as needed. Heights come from the scene builder's ground_z.

Needs PyYAML.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "ui"))
sys.path.insert(0, os.path.join(HERE, "..", "japanese_city"))
import build_game_ui as ui  # noqa: E402  (the helpers, fonts and art handles)
import build_countryside as land  # noqa: E402  (the layout and its ground heights)

SCENE = "JapaneseCountryside.hyscene"
REMOVE = {"Countryside Quest", "Arrive ShrineGlade", "Side Quests"}
GIVERS = ("Jokai", "Sanzo", "Genji", "Okiku", "Inari")   # Quests.hytable's side quests in this scene (Inari: the fox shrine)

TREE = land.TREE                                       # the ancient tree's foot, in its clearing in the cedar forest
ORIN = (TREE[0] + 4.0, TREE[1] - 3.6)                  # in front of it, facing the path he came up
WISPS = [(TREE[0] - 2.6, TREE[1] - 2.4), (TREE[0] + 1.6, TREE[1] - 1.2), (TREE[0] + 5.2, TREE[1] - 0.8)]
RIFT = (TREE[0], TREE[1] - 3.4)                        # the way down, among the roots in front of the trunk
# The leyline: from the landing down the long stairs and the road, west along the bund path between the paddies, up into
# the forest and across the clearing to the rift (the points avoid the ramps' edges).
LINE = [(0.0, 119.5), (0.0, 113.0), (0.0, 106.0), (0.0, 99.0), (0.0, 92.0), (0.0, 86.5), (0.0, 81.0), (0.0, 76.5), (-0.8, 73.0), (-0.6, 67.0), (0.4, 61.5), (0.6, 57.0),
        (-5.0, 56.0), (-11.0, 56.1), (-17.0, 56.0), (-23.0, 55.9), (-29.0, 56.0), (-35.0, 56.0), (-40.0, 56.0),
        (-44.5, 56.2), (-48.0, 57.2), (-51.0, 58.0), RIFT]
ARRIVE_FROM_SHRINE = (0.0, 119.5)                     # on the landing, below the last flight


def handle(relative):
    with open(os.path.join(ui.ASSETS, relative), encoding="utf-8") as file:
        return int(re.search(r"^Handle:\s*(\d+)", file.read(), re.M).group(1))


def ground(x, y, lift=0.0):
    return [x, y, round(land.ground_z(x, y) + lift, 3)]


def transform(x, y, lift=0.0, scale=(1, 1, 1)):
    return {"Translation": ground(x, y, lift), "Rotation": [0, 0, 0], "Scale": list(scale)}


def sprite(texture, name=None, index=0, sort=0, facing="Upright", color=(1, 1, 1, 1)):
    s = {"Color": list(color), "TilingFactor": 1, "SortOrder": sort, "TextureHandle": texture}
    if name is not None:
        s["Sprite"] = name
    s.update({"SpriteIndex": index, "FlipX": False, "FlipY": False, "Facing": facing})
    return s


def marker(b, tag, parent, color, image_tag=None):
    """A World canvas with the quest mark (a script moves it over someone's head and shows it)."""
    canvas = b.add(tag, parent, TransformComponent=ui.transform(), UICanvasComponent=ui.canvas(4, visible=False, world=([56, 70], [0.5, 0])))
    b.add(image_tag or tag + " Image", canvas, UIRectComponent=ui.fill(), UIImageComponent=ui.image("QuestMark", color, nine=False))
    return canvas


def build():
    b = ui.Builder("JapaneseCountryside")
    tree_texture = ui.handle_of("Sprites/Trees/ancient_tree.png")
    orin_texture = handle("Characters/Player/Orin/Orin_Field.png.hymeta")
    orin_animations = handle("Characters/Player/Orin/Orin_Field.hyanim")
    wisp_texture = handle("Characters/Enemy/Wisp/Wisp.png.hymeta")
    wisp_animations = handle("Characters/Enemy/Wisp/Wisp.hyanim")

    quest = b.add("Countryside Quest", None, TransformComponent=transform(0, 0),
                  NativeScriptComponent={"Scripts": [{"Name": "CountrysideQuestScript"}]})
    line = b.add("Leyline", quest, TransformComponent=transform(0, 0))
    for i, (x, y) in enumerate(LINE):
        b.add("Leyline %d" % i, line, TransformComponent=transform(x, y))

    # The ancient tree: a 10 m card (the sprite stands on its foot), its trunk solid (the box is in the card's
    # scaled space: 2.4 x 1.2 x 2 m).
    b.add("Ley Tree", quest, TransformComponent=transform(TREE[0], TREE[1], 0.0, (10, 10, 1)),
          SpriteRendererComponent=sprite(tree_texture),
          BoxColliderComponent={"Offset": [0, 0, 1], "Size": [0.24, 0.12, 2.0], "Layer": "World", "IsTrigger": False})

    # Orin, like the scenes' NPCs (2.5 x 2.34375: the cast's scale), facing the path.
    b.add("Npc Orin", quest, TransformComponent=transform(ORIN[0], ORIN[1], 0.0, (2.5, 2.34375, 1)),
          SpriteRendererComponent=sprite(orin_texture, "IdleRight_0", 4, 60),
          SpriteAnimatorComponent={"AnimationsHandle": orin_animations, "StartClip": "IdleRight", "PlayOnStart": True, "Speed": 1},
          Shadow2DComponent={"Offset": [0, -0.04], "Size": [0.95, 0.58], "Strength": 0.6},
          BoxCollider2DComponent={"Offset": [0, 0.1], "Size": [0.5, 0.2], "Density": 1, "Friction": 0.4, "Restitution": 0, "IsSensor": False},
          BoxColliderComponent={"Offset": [0, 0.04, 0.8], "Size": [0.32, 0.08, 1.6], "Layer": "Npc", "IsTrigger": False})

    # The spirits circling the tree (CountrysideQuestScript moves them; no script of their own: they don't chase).
    for i, (x, y) in enumerate(WISPS):
        b.add("Ley Wisp %d" % (i + 1), quest, TransformComponent=transform(x, y, 0.8, (1.6, 1.6, 1)),
              SpriteRendererComponent=sprite(wisp_texture, None, 0, 40),
              SpriteAnimatorComponent={"AnimationsHandle": wisp_animations, "StartClip": "Float", "PlayOnStart": True, "Speed": 1},
              Shadow2DComponent={"Offset": [0, -0.5], "Size": [0.7, 0.3], "Strength": 0.35})

    # The way down: hidden (transparent) until the ground opens; the glow is drawn emissive over the rift.
    rift_texture = ui.handle_of("Sprites/Props/LeyRift.png")
    glow_texture = ui.handle_of("Sprites/Props/LeyRiftGlow.png")
    b.add("Ley Rift", quest, TransformComponent=transform(RIFT[0], RIFT[1], 0.03, (3.5, 2.5, 1)),
          SpriteRendererComponent=sprite(rift_texture, None, 0, -2, "Flat", (1, 1, 1, 0)))
    glow = sprite(glow_texture, None, 0, -1, "Flat", (1, 1, 1, 0))
    glow.update({"EmissionColor": [0.4, 0.95, 1.0], "EmissionIntensity": 2.5})
    b.add("Ley Rift Glow", quest, TransformComponent=transform(RIFT[0], RIFT[1], 0.05, (3.5, 2.5, 1)), SpriteRendererComponent=glow)
    b.add("Leyline Gate", quest, TransformComponent=transform(RIFT[0], RIFT[1]),
          BoxColliderComponent={"Offset": [0, 0, 1], "Size": [3.6, 2.8, 2], "Layer": "Trigger", "IsTrigger": True})

    # The marker over Orin after a lost battle.
    marker(b, "Ley Marker Canvas", quest, (1, 0.84, 0.42, 1), "Ley Marker")

    # Where the player stands when the Shrine Glade pushes this scene (FieldDirectorScript::PlaceArrival).
    b.add("Arrive ShrineGlade", None, TransformComponent=transform(*ARRIVE_FROM_SHRINE))

    # The side quests: the givers' markers and the script that offers their quests.
    side = b.add("Side Quests", None, TransformComponent=transform(0, 0), NativeScriptComponent={"Scripts": [{"Name": "QuestGiverScript"}]})
    for giver in GIVERS:
        marker(b, "Quest Marker " + giver, side, (1, 0.42, 0.3, 1))
    return b


def main():
    ui.rebuild(SCENE, REMOVE, lambda director: build(), director_tags=("Field Director",))


if __name__ == "__main__":
    main()
