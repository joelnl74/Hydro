"""Add the countryside's part of "The Guardian's Duty" to Oxygen/project/Assets/Scenes/JapaneseCountryside.hyscene
(docs/TestBed.md, "Countryside quest"): the "Countryside Quest" (CountrysideQuestScript,
Game/src/Scripts/Countryside/CountrysideQuestScript.h) with the leyline's points ("Leyline 0" to "Leyline N", from the
shrine road's gate down the road and west along the paddies to the tree) and the marker over Orin; the ancient tree
("Ley Tree"); Orin Kenshi waiting there ("Npc Orin", the stand-in sheet of build_orin_standin.py) with the spirits
circling the tree ("Ley Wisp 1" to "3"); and where the player arrives from the Shrine Glade ("Arrive ShrineGlade").

    python scripts/quests/build_countryside_quest.py

Run scripts/ui/build_game_ui.py field first (it builds the field UI the quest uses: the dialogue box, the quest
tracker, the menu), close the scene in the editor, then run this. Like the UI builder it removes what an earlier run
added (by tag, with every descendant) and appends it again with IDs made from the tags, so it can be run as often as
needed; everything else in the scene is left alone (build_countryside.py, which writes the rest, needs art that isn't
in the repository, so the scene is edited in place).

Needs PyYAML.
"""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "ui"))
import build_game_ui as ui  # noqa: E402  (the helpers, fonts and art handles)

SCENE = "JapaneseCountryside.hyscene"
REMOVE = {"Countryside Quest", "Arrive ShrineGlade"}

TREE = (-30.0, -28.0)        # the ancient tree's foot, west of the southern paddies
ORIN = (-26.6, -30.6)        # in front of it, facing the road
WISPS = [(-32.0, -30.2), (-28.6, -26.4), (-24.8, -28.4)]
# The leyline, from the shrine road's gate down the road, then west below the paddies to the tree (z 0: the ground).
LINE = [(0.0, 42.0), (0.0, 36.0), (0.0, 30.0), (0.0, 24.0), (0.0, 18.0), (0.0, 12.0), (0.0, 6.0), (0.0, 0.0),
        (0.0, -6.0), (0.0, -12.0), (0.0, -18.0), (0.0, -24.0), (-1.6, -29.4), (-7.0, -30.6), (-13.0, -30.8),
        (-19.0, -30.4), (-24.0, -29.6), (-28.0, -28.6)]
ARRIVE_FROM_SHRINE = (0.0, 43.0)


def handle(relative):
    with open(os.path.join(ui.ASSETS, relative), encoding="utf-8") as file:
        return int(re.search(r"^Handle:\s*(\d+)", file.read(), re.M).group(1))


def transform(x, y, z=0.0, scale=(1, 1, 1)):
    return {"Translation": [x, y, z], "Rotation": [0, 0, 0], "Scale": list(scale)}


def sprite(texture, name=None, index=0, sort=0):
    s = {"Color": [1, 1, 1, 1], "TilingFactor": 1, "SortOrder": sort, "TextureHandle": texture}
    if name is not None:
        s["Sprite"] = name
    s.update({"SpriteIndex": index, "FlipX": False, "FlipY": False, "Facing": "Upright"})
    return s


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

    # The ancient tree: a 10 m card (the sprite stands on its foot), its trunk solid.
    b.add("Ley Tree", quest, TransformComponent=transform(TREE[0], TREE[1], 0.0, (10, 10, 1)),
          SpriteRendererComponent=sprite(tree_texture),
          BoxColliderComponent={"Offset": [0, 0, 1], "Size": [2.0, 1.0, 2.0], "Layer": "World", "IsTrigger": False})

    # Orin, like the scenes' NPCs (2.5 x 2.34375: the cast's scale), facing the road.
    b.add("Npc Orin", quest, TransformComponent=transform(ORIN[0], ORIN[1], 0.0, (2.5, 2.34375, 1)),
          SpriteRendererComponent=sprite(orin_texture, "IdleRight_0", 4, 60),
          SpriteAnimatorComponent={"AnimationsHandle": orin_animations, "StartClip": "IdleRight", "PlayOnStart": True, "Speed": 1},
          Shadow2DComponent={"Offset": [0, -0.04], "Size": [0.95, 0.58], "Strength": 0.6},
          BoxCollider2DComponent={"Offset": [0, 0.1], "Size": [0.5, 0.2], "Density": 1, "Friction": 0.4, "Restitution": 0, "IsSensor": False},
          BoxColliderComponent={"Offset": [0, 0.1, 0.8], "Size": [0.5, 0.2, 1.6], "Layer": "Npc", "IsTrigger": False})

    # The spirits circling the tree (CountrysideQuestScript moves them; no script of their own: they don't chase).
    for i, (x, y) in enumerate(WISPS):
        b.add("Ley Wisp %d" % (i + 1), quest, TransformComponent=transform(x, y, 0.8, (1.6, 1.6, 1)),
              SpriteRendererComponent=sprite(wisp_texture, None, 0, 40),
              SpriteAnimatorComponent={"AnimationsHandle": wisp_animations, "StartClip": "Float", "PlayOnStart": True, "Speed": 1},
              Shadow2DComponent={"Offset": [0, -0.5], "Size": [0.7, 0.3], "Strength": 0.35})

    # The marker over Orin after a lost battle (a World canvas the script moves over his head).
    marker = b.add("Ley Marker Canvas", quest, TransformComponent=ui.transform(),
                   UICanvasComponent=ui.canvas(4, visible=False, world=([56, 70], [0.5, 0])))
    b.add("Ley Marker", marker, UIRectComponent=ui.fill(), UIImageComponent=ui.image("QuestMark", (1, 0.84, 0.42, 1), nine=False))

    # Where the player stands when the Shrine Glade pushes this scene (FieldDirectorScript::PlaceArrival).
    b.add("Arrive ShrineGlade", None, TransformComponent=transform(*ARRIVE_FROM_SHRINE))
    return b


def main():
    ui.rebuild(SCENE, REMOVE, lambda director: build(), director_tags=("Field Director",))


if __name__ == "__main__":
    main()
