"""Add the Shrine Glade's opening quest to Oxygen/project/Assets/Scenes/ShrineGlade.hyscene (docs/TestBed.md, "Shrine
Glade quest"): the "Quest Director" (ShrineQuestScript, Oxygen/src/Scripts/QuestScripts.h) under "Directors", its
cutscene letterbox ("Cinematic Canvas") and the marker over the Old Monk ("Quest Marker Canvas"), and three dormant
slimes on the lower path ("Slime Grounds 1" to "3" under "Characters" / "Slimes Lower Path") that the quest wakes.

    python scripts/quests/build_shrine_quest.py

Run scripts/ui/build_game_ui.py field first (it builds the field UI the quest uses: the dialogue box and the
quest tracker), close the scene in the editor, then run this. Like the UI builder it removes what an earlier run
added (by tag, with every descendant) and appends it again with IDs made from the tags, so it can be run as often
as needed; everything else in the scene is left alone. The Sacred Tree's "Sacred Tree Awake" and "Sacred Tree
Glow" layers and the monk ("Npc Elder") are the scene's own and must be there.

Needs PyYAML.
"""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "ui"))
import build_game_ui as ui  # noqa: E402  (the helpers, fonts and art handles)

SCENE = "ShrineGlade.hyscene"
REMOVE = {"Quest Director", "Slimes Lower Path"}

# Where the slimes wait on the lower path (z 0, between the plaza's stairs and the road to the village).
SLIMES = [(-4.8, -6.4), (5.0, -8.8), (-3.4, -9.6)]


def anim_handle(relative):
    with open(os.path.join(ui.ASSETS, relative), encoding="utf-8") as file:
        return int(re.search(r"^Handle:\s*(\d+)", file.read(), re.M).group(1))


SLIME_TEXTURE = ui.handle_of("DioramaRPG/Enemies/Slime.png")
SLIME_ANIMATIONS = anim_handle("DioramaRPG/Enemies/Slime.hyanim")


def entity_id(tag):
    """The ID of the scene's entity with this tag."""
    text = open(os.path.join(ui.SCENES, SCENE), encoding="utf-8").read()
    match = re.search(r"- Entity: (\d+)\r?\n\s+TagComponent:\r?\n\s+Tag: %s\r?\n" % re.escape(tag), text)
    if match is None:
        sys.exit("%s has no entity tagged %r" % (SCENE, tag))
    return int(match.group(1))


def slime(b, tag, parent, x, y):
    return b.add(tag, parent,
                 TransformComponent={"Translation": [x, y, 0], "Rotation": [0, 0, 0], "Scale": [1.4, 1.4, 1]},
                 SpriteRendererComponent={"Color": [1, 1, 1, 1], "TilingFactor": 1, "SortOrder": 0, "TextureHandle": SLIME_TEXTURE,
                                          "Sprite": "Slime_0", "SpriteIndex": 0, "FlipX": False, "FlipY": False, "Facing": "Upright"},
                 SpriteAnimatorComponent={"AnimationsHandle": SLIME_ANIMATIONS, "StartClip": "Bounce", "PlayOnStart": True, "Speed": 1},
                 Shadow2DComponent={"Offset": [0, 0.05], "Size": [0.75, 0.24], "Strength": 0.45},
                 Rigidbody2DComponent={"Type": "Dynamic", "FixedRotation": True, "IsBullet": False, "GravityScale": 0,
                                       "LinearDamping": 10, "AngularDamping": 0.05},
                 BoxCollider2DComponent={"Offset": [0, 0.2], "Size": [0.6, 0.3], "Density": 1, "Friction": 0.4, "Restitution": 0, "IsSensor": False},
                 BoxColliderComponent={"Offset": [0, 0.2, 0.8], "Size": [0.6, 0.3, 1.6], "Layer": "World", "IsTrigger": False},
                 NativeScriptComponent={"Scripts": [{"Name": "EnemyChaseScript", "Properties": {
                     "AggroRange": 4.0, "ChaseSpeed": 2.0, "Dormant": True, "LeashRange": 8.0, "MaxFromHome": 8.0, "ReturnSpeed": 1.8,
                     "TouchRange": 0.7, "WanderRadius": 1.5, "WanderSpeed": 0.7}}]})


def build(directors, characters):
    b = ui.Builder("ShrineGlade")
    director = b.add("Quest Director", directors, TransformComponent=ui.transform(),
                     NativeScriptComponent={"Scripts": [{"Name": "ShrineQuestScript"}]})

    # The cutscene's letterbox: black bars that slide in from the top and bottom (ShrineQuestScript moves them).
    # It captures input while shown, so the player stands still and can't open the menu.
    cinematic = b.add("Cinematic Canvas", director, UICanvasComponent=ui.canvas(25, visible=False, captures=True))
    b.add("Letterbox Top", cinematic, UIRectComponent=ui.rect((0, 1), (1, 1), (0.5, 1), (0, 140), (0, 140)),
          UIImageComponent=ui.image(None, (0, 0, 0, 1)))
    b.add("Letterbox Bottom", cinematic, UIRectComponent=ui.rect((0, 0), (1, 0), (0.5, 0), (0, -140), (0, 140)),
          UIImageComponent=ui.image(None, (0, 0, 0, 1)))

    # The marker over the monk: a World canvas the script moves over his head and tints.
    marker = b.add("Quest Marker Canvas", director, TransformComponent=ui.transform(),
                   UICanvasComponent=ui.canvas(4, visible=False, world=([56, 70], [0.5, 0])))
    b.add("Quest Marker", marker, UIRectComponent=ui.fill(), UIImageComponent=ui.image("QuestMark", (1, 0.42, 0.3, 1), nine=False))

    folder = b.add("Slimes Lower Path", characters, TransformComponent=ui.transform())
    for i, (x, y) in enumerate(SLIMES):
        slime(b, "Slime Grounds %d" % (i + 1), folder, x, y)
    return b


def main():
    characters = entity_id("Characters")
    ui.rebuild(SCENE, REMOVE, lambda directors: build(directors, characters), director_tags=("Directors",))


if __name__ == "__main__":
    main()
