"""Build the game's runtime UI into the test bed scenes (Battle, Shrine Glade, Farm Village and its inn) on the 1920 x 1080
canvas (decision D5), in the game's own look: ink ribbons with bronze fittings, a vermilion cursor, washi paper for
dialogue (scripts/ui/make_ui_art.py draws the art).

    python scripts/ui/build_game_ui.py [battle] [field]

Close the scenes in the editor first. Each run removes what an earlier run (or the older UI it replaces) put there,
by tag, with every descendant, and appends the UI again, so it can be run as often as needed; entity IDs come from
the tags, so they stay the same between runs. Everything else in the scenes is left alone. The entity names are the
ones BattleScript, MvpDirectorScript and DialogueScript look up: rename them in both places.

Check the result offline with scripts/ui/preview_ui.py.

Needs PyYAML.
"""

import hashlib
import os
import re
import sys

import yaml

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS = os.path.join(ROOT, "Oxygen", "project", "Assets")
SCENES = os.path.join(ASSETS, "Scenes")


def handle_of(relative):
    with open(os.path.join(ASSETS, relative + ".hymeta"), encoding="utf-8") as file:
        return int(re.search(r"Handle:\s*(\d+)", file.read()).group(1))


FONT_BODY = handle_of("Fonts/EBGaramond30.fnt")
FONT_SMALL = handle_of("Fonts/EBGaramond22.fnt")
FONT_TITLE = handle_of("Fonts/Cinzel34.fnt")
FONT_LABEL = handle_of("Fonts/Cinzel20.fnt")
ART = {name: handle_of("UI/%s.png" % name) for name in
       ("Ribbon", "RibbonFocus", "Frame", "Paper", "Banner", "Divider", "Plate", "Cursor", "Marker", "Shield", "Chip", "Fade", "Pip",
        "Band", "QuestMark")}
BORDERS = {"Ribbon": [20, 10, 34, 10], "RibbonFocus": [20, 10, 34, 10], "Frame": [22, 22, 22, 22], "Paper": [22, 22, 22, 22],
           "Banner": [26, 12, 26, 12], "Divider": [14, 0, 14, 0], "Plate": [16, 8, 16, 8]}
BATTLE_ICONS = handle_of("DioramaRPG/UI/BattleIcons.png")

CREAM = [0.95, 0.91, 0.83, 1]
DIM = [0.74, 0.71, 0.64, 1]
GOLD = [1.0, 0.84, 0.5, 1]
BLUE = [0.64, 0.8, 1.0, 1]
INK_TEXT = [0.2, 0.15, 0.1, 1]
SHADOW = [0, 0, 0, 0.8]
SAGE = [0.5, 0.72, 0.4, 1]
MANA = [0.38, 0.6, 0.9, 1]


# ---- Entities ---------------------------------------------------------------------------------------------------

class Builder:
    """Collects entities (dicts in the scene's YAML shape) with IDs made from their tags, so reruns keep them."""

    def __init__(self, scene_name):
        self.scene = scene_name
        self.entities = []

    def uid(self, tag):
        digest = hashlib.sha1(("%s/%s" % (self.scene, tag)).encode()).digest()
        return int.from_bytes(digest[:8], "little") & ((1 << 63) - 1)

    def add(self, tag, parent, **components):
        entity = {"Entity": self.uid(tag), "TagComponent": {"Tag": tag}}
        if parent is not None:
            entity["RelationshipComponent"] = {"Parent": parent}
        entity.update(components)
        self.entities.append(entity)
        return entity["Entity"]


def canvas(sort, visible=True, captures=False, world=None):
    c = {"ReferenceResolution": [1920, 1080], "ScaleMode": "ScaleWithScreen", "SortOrder": sort, "Visible": visible, "BlocksGameplayInput": captures}
    if world is not None:
        c["Space"] = "World"
        c["WorldSize"] = world[0]
        c["WorldPivot"] = world[1]
    return c


def rect(anchor_min=(0.5, 0.5), anchor_max=None, pivot=(0.5, 0.5), position=(0, 0), size=(100, 100), visible=True):
    return {"AnchorMin": list(anchor_min), "AnchorMax": list(anchor_max if anchor_max is not None else anchor_min), "Pivot": list(pivot),
            "AnchoredPosition": list(position), "SizeDelta": list(size), "Visible": visible}


def fill(inset=(0, 0, 0, 0), visible=True):
    """A rect that fills its parent less (left, bottom, right, top) canvas pixels."""
    l, b, r, t = inset
    return rect((0, 0), (1, 1), (0.5, 0.5), ((l - r) / 2, (b - t) / 2), (-(l + r), -(b + t)), visible)


def image(art=None, color=(1, 1, 1, 1), sprite=0, texture=None, flip_x=False, nine=True):
    i = {"SpriteIndex": sprite, "Color": list(color), "NineSlice": BORDERS.get(art, [0, 0, 0, 0]) if (art and nine) else [0, 0, 0, 0], "FlipX": flip_x, "FlipY": False}
    handle = texture if texture is not None else (ART[art] if art else None)
    if handle:
        i = {"TextureHandle": handle, **i}
    return i


def text(value, font=FONT_BODY, color=CREAM, align="Left", valign="Middle", wrap=False, shadow=True, spacing=1.0):
    t = {"Text": value, "FontHandle": font, "Color": list(color), "Scale": 1, "AlignH": align, "AlignV": valign, "Wrap": wrap,
         "LineSpacing": spacing, "VisibleCharacters": -1}
    if shadow:
        t["ShadowColor"] = SHADOW
        t["ShadowOffset"] = [2, -2]
    return t


def button(event, interactable=True):
    return {"Interactable": interactable, "Event": event, "NormalTint": [0.84, 0.84, 0.84, 1], "FocusedTint": [1, 1, 1, 1],
            "PressedTint": [0.86, 0.82, 0.74, 1], "DisabledTint": [0.55, 0.55, 0.55, 0.7], "Navigation": "Auto"}


def group(direction="Vertical", spacing=4, padding=(0, 0, 0, 0), fit=True, stretch=True, align="Start"):
    return {"Direction": direction, "Spacing": spacing, "Padding": list(padding), "ChildAlignment": align, "StretchCrossAxis": stretch, "FitContent": fit}


def transform():
    return {"Translation": [0, 0, 0], "Rotation": [0, 0, 0], "Scale": [1, 1, 1]}


def ribbon_row(b, tag, parent, width, height, name, detail="", event="", name_font=FONT_BODY, interactable=True):
    """A menu row: an ink ribbon (a warm one while focused), the cursor in a strip on its left, a name and a detail."""
    row = b.add(tag, parent, UIRectComponent=rect(size=(width, height)), UIButtonComponent=button(event, interactable))
    b.add(tag + " Ribbon", row, UIRectComponent=fill((44, 0, 0, 0)), UIImageComponent=image("Ribbon"),
          UIFocusVisibilityComponent={"Show": "Unfocused"})
    b.add(tag + " Glow", row, UIRectComponent=fill((44, 0, 0, 0)), UIImageComponent=image("RibbonFocus"),
          UIFocusVisibilityComponent={"Show": "Focused"})
    b.add(tag + " Cursor", row, UIRectComponent=rect((0, 0.5), None, (0, 0.5), (0, 0), (44, 44)), UIImageComponent=image("Cursor"),
          UIFocusVisibilityComponent={"Show": "Focused"})
    b.add(tag + " Name", row, UIRectComponent=fill((66, 0, 56, 0)), UITextComponent=text(name, name_font))
    b.add(tag + " " + ("Cost" if "Command" in tag else ("HP" if "Target" in tag else "Count")), row,
          UIRectComponent=fill((66, 0, 56, 0)), UITextComponent=text(detail, FONT_SMALL, BLUE, "Right"))
    return row


def status_block(b, tag, parent, hero="Miko", hp="90 / 90", mp="32 / 32"):
    """A hero's status, frameless over a soft ink fade: name, boost points, HP and MP with thin bars."""
    block = b.add(tag, parent, UIRectComponent=rect(size=(440, 176)))
    b.add(tag + " Back", block, UIRectComponent=fill(), UIImageComponent=image("Fade", (1, 1, 1, 0.9), flip_x=True, nine=False))
    b.add(tag + " Name", block, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -6), (-48, 46)), UITextComponent=text(hero, FONT_TITLE))
    pips = b.add(tag + " Pips", block, UIRectComponent=rect((1, 1), None, (1, 1), (-24, -20), (150, 20)),
                 UILayoutGroupComponent=group("Horizontal", 6, fit=False, stretch=False, align="End"))
    for p in range(5):
        b.add("%s Pip %d" % (tag, p), pips, UIRectComponent=rect(size=(18, 18)), UIImageComponent=image("Pip", (1, 1, 1, 0.25)))
    for row, (label, value, colour, y) in enumerate((("HP", hp, SAGE, -58), ("MP", mp, MANA, -112))):
        b.add("%s %s Label" % (tag, label), block, UIRectComponent=rect((0, 1), None, (0, 1), (26, y), (70, 40)),
              UITextComponent=text(label, FONT_LABEL, GOLD))
        b.add("%s %s" % (tag, label), block, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, y), (-48, 40)),
              UITextComponent=text(value, FONT_BODY, CREAM, "Right"))
        back = b.add("%s %s Back" % (tag, label), block, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, y - 40), (-48, 6)),
                     UIImageComponent=image(None, (0, 0, 0, 0.6)))
        b.add("%s %s Fill" % (tag, label), back, UIRectComponent=rect((0, 0), (1, 1), (0, 0.5), (0, 0), (0, 0)),
              UIImageComponent=image(None, colour))
    b.add(tag + " Rule", block, UIRectComponent=rect((0, 0), (1, 0), (0.5, 0), (0, 2), (-16, 12)), UIImageComponent=image("Divider", (1, 1, 1, 0.8)))
    return block


# ---- Battle -----------------------------------------------------------------------------------------------------

BATTLE_REMOVE = {
    # what this builder makes
    "Battle UI Canvas", "Enemy 0 Info", "Enemy 1 Info", "Enemy 2 Info",
    # the sprite HUD it replaces (and their folders)
    "Party HUD", "Party Panel", "Order Panel",
} | {"Order Chip %d" % c for c in range(6)} \
  | {"Hero Miko %s" % n for n in ("HP Back", "HP Fill", "MP Back", "MP Fill", "HP Digit 0", "HP Digit 1", "HP Digit 2", "Name",
                                  "Pip 0", "Pip 1", "Pip 2", "Pip 3", "Pip 4")} \
  | {"Enemy %d %s" % (e, n) for e in range(3) for n in ("HP Back", "HP Fill", "Shield", "Shield Digit", "Weak 0", "Weak 1", "Weak 2")}


def build_battle(director):
    b = Builder("Battle")
    root = b.add("Battle UI Canvas", director, UICanvasComponent=canvas(0))

    # The turn order, top left: a bronze rule with a diamond chip per turn (the actor's sprite inside).
    b.add("Turn Order Rule", root, UIRectComponent=rect((0, 1), None, (0, 0.5), (36, -82), (760, 12)), UIImageComponent=image("Divider"))
    order = b.add("Turn Order", root, UIRectComponent=rect((0, 1), None, (0, 1), (40, -36), (760, 92)),
                  UILayoutGroupComponent=group("Horizontal", 14, fit=False, stretch=False, align="Start"))
    for c in range(6):
        chip = b.add("Order Chip %d" % c, order, UIRectComponent=rect(size=(92, 92) if c == 0 else (76, 76)), UIImageComponent=image("Chip", nine=False))
        b.add("Order Chip %d Actor" % c, chip, UIRectComponent=fill((14, 14, 14, 14)), UIImageComponent=image(None, (1, 1, 1, 0)))
    b.add("Turn Order Label", root, UIRectComponent=rect((0, 1), None, (0, 1), (150, -112), (300, 30)),
          UITextComponent=text("Next turn", FONT_LABEL, DIM))

    # The party, top right.
    party = b.add("Party Status", root, UIRectComponent=rect((1, 1), None, (1, 1), (-36, -24), (440, 10)), UILayoutGroupComponent=group(spacing=10))
    status_block(b, "Status Miko", party)

    # The skill / message banner, top middle.
    banner = b.add("Battle Message Panel", root, UIRectComponent=rect((0.5, 1), None, (0.5, 1), (0, -168), (600, 72)), UIImageComponent=image("Banner"))
    b.add("Battle Message Text", banner, UIRectComponent=fill((40, 0, 40, 0)), UITextComponent=text("Flame Kick", FONT_BODY, CREAM, "Center"))

    # The command list, right of the middle, and the target list in its place.
    commands = b.add("Command Panel", root, UIRectComponent=rect((0.5, 0), None, (0, 1), (150, 560), (440, 10)), UILayoutGroupComponent=group(spacing=4))
    header = b.add("Command Header", commands, UIRectComponent=rect(size=(440, 46)), UIImageComponent=image("Plate"))
    b.add("Command Hero", header, UIRectComponent=fill((60, 0, 20, 0)), UITextComponent=text("Miko", FONT_LABEL, GOLD))
    b.add("Command Boost", header, UIRectComponent=fill((60, 0, 24, 0)), UITextComponent=text("E: boost", FONT_SMALL, CREAM, "Right"))
    names = [("Attack", "Attack", ""), ("SkillA", "Flame Kick", "5 MP"), ("SkillB", "Holy Palm", "6 MP"), ("Guard", "Guard", ""),
             ("Potion", "Potion", "x3"), ("Run", "Run", "")]
    for c, (event, name, cost) in enumerate(names):
        ribbon_row(b, "Command %d" % c, commands, 440, 56, name, cost, event)
    helpbar = b.add("Command Help Bar", commands, UIRectComponent=rect(size=(440, 48)), UIImageComponent=image("Fade", nine=False))
    b.add("Command Help", helpbar, UIRectComponent=fill((24, 0, 12, 0)), UITextComponent=text("Strike one foe.", FONT_SMALL, CREAM))

    targets = b.add("Target Panel", root, UIRectComponent=rect((0.5, 0), None, (0, 1), (150, 560), (440, 10)), UILayoutGroupComponent=group(spacing=4))
    theader = b.add("Target Header", targets, UIRectComponent=rect(size=(440, 46)), UIImageComponent=image("Plate"))
    b.add("Target Command", theader, UIRectComponent=fill((60, 0, 20, 0)), UITextComponent=text("Attack", FONT_LABEL, GOLD))
    b.add("Target Hint", theader, UIRectComponent=fill((60, 0, 24, 0)), UITextComponent=text("Esc: back", FONT_SMALL, CREAM, "Right"))
    for e in range(3):
        ribbon_row(b, "Target %d" % e, targets, 440, 56, "Slime " + "ABC"[e], "HP 40", "Target")

    # Each enemy's shield and weaknesses, pinned under its feet (World canvases, placed by BattleScript).
    for e in range(3):
        info = b.add("Enemy %d Info" % e, director, TransformComponent=transform(), UICanvasComponent=canvas(-1, world=([300, 84], [0.5, 1])))
        shield = b.add("Enemy %d Shield" % e, info, UIRectComponent=rect((0, 0.5), None, (0, 0.5), (8, 0), (54, 62)), UIImageComponent=image("Shield", nine=False))
        b.add("Enemy %d Shield Count" % e, shield, UIRectComponent=fill((0, 6, 0, 0)), UITextComponent=text("3", FONT_TITLE, CREAM, "Center"))
        b.add("Enemy %d Weak Label" % e, info, UIRectComponent=rect((0, 1), None, (0, 1), (72, -4), (220, 28)),
              UITextComponent=text("WEAK", FONT_LABEL, GOLD))
        icons = b.add("Enemy %d Weak Icons" % e, info, UIRectComponent=rect((0, 0), None, (0, 0), (72, 14), (220, 34)),
                      UILayoutGroupComponent=group("Horizontal", 4, fit=False, stretch=False))
        for w in range(3):
            b.add("Enemy %d Weak %d" % (e, w), icons, UIRectComponent=rect(size=(34, 34)), UIImageComponent=image(None, texture=BATTLE_ICONS, sprite=5))
        back = b.add("Enemy %d HP Back" % e, info, UIRectComponent=rect((0, 0), None, (0, 0), (72, 4), (124, 5)), UIImageComponent=image(None, (0, 0, 0, 0.6)))
        b.add("Enemy %d HP Fill" % e, back, UIRectComponent=rect((0, 0), (1, 1), (0, 0.5), (0, 0), (0, 0)), UIImageComponent=image(None, [0.86, 0.36, 0.28, 1]))
    return b


# ---- Field ------------------------------------------------------------------------------------------------------

FIELD_REMOVE = {
    "Field HUD Canvas", "Inventory Canvas", "Dialogue Canvas", "Fade Canvas", "Prompt Canvas", "Quest Canvas",
    "UI", "UI Prompt Panel", "UI Prompt Label",
}

ITEMS = [("Potion", "x3"), ("Hi-Potion", "x1"), ("Ether", "x2"), ("Herb Tea", "x2"), ("Iron Sword", "x1"), ("Herb Bundle", "x4"),
         ("Phoenix Feather", "x1"), ("Crystal Shard", "x2"), ("Leather Boots", "x1"), ("Antidote", "x2"), ("Ancient Key", "x1"),
         ("Copper Ore", "x5"), ("Old Map", "x1"), ("Shrine Charm", "x1"), ("Lantern Oil", "x3"), ("Rope", "x1"),
         ("Smoked Fish", "x2"), ("Spring Water", "x3"), ("Pine Resin", "x2"), ("Fox Mask", "x1")]


def build_field(scene_name, director):
    b = Builder(scene_name)

    # The interaction prompt over the player's head: a World canvas MvpDirectorScript moves and shows.
    prompt = b.add("Prompt Canvas", director, TransformComponent=transform(), UICanvasComponent=canvas(5, world=([220, 52], [0.5, 0])))
    b.add("Prompt Plate", prompt, UIRectComponent=fill(), UIImageComponent=image("Plate"))
    b.add("Prompt Text", prompt, UIRectComponent=fill((16, 0, 16, 0)), UITextComponent=text("E  Talk", FONT_SMALL, CREAM, "Center"))

    # Messages: a banner at the top.
    hud = b.add("Field HUD Canvas", director, UICanvasComponent=canvas(20))
    panel = b.add("Field Message Panel", hud, UIRectComponent=rect((0.5, 1), None, (0.5, 1), (0, -110), (760, 104)), UIImageComponent=image("Banner"))
    b.add("Field Message Text", panel, UIRectComponent=fill((48, 8, 48, 8)), UITextComponent=text("Found a Potion!", FONT_BODY, CREAM, "Center", wrap=True))

    # The inventory: a title, the list (scrolling) in a frame, the item's details, the party, a help bar.
    inv = b.add("Inventory Canvas", director, UICanvasComponent=canvas(10, visible=False, captures=True))
    b.add("Inventory Dim", inv, UIRectComponent=fill(), UIImageComponent=image(None, (0.03, 0.04, 0.06, 0.55)))
    b.add("Inventory Title", inv, UIRectComponent=rect((0, 1), None, (0, 1), (72, -40), (480, 60)), UITextComponent=text("Inventory", FONT_TITLE, CREAM))
    b.add("Inventory Title Rule", inv, UIRectComponent=rect((0, 1), None, (0, 1), (56, -100), (560, 12)), UIImageComponent=image("Divider"))

    frame = b.add("Inventory Panel", inv, UIRectComponent=rect((0, 1), None, (0, 1), (56, -130), (760, 800)), UIImageComponent=image("Frame"))
    head = b.add("Inventory Header", frame, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -26), (-52, 44)), UIImageComponent=image("Plate"))
    b.add("Inventory Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Items", FONT_LABEL, GOLD))
    b.add("Inventory Header Count", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Owned", FONT_SMALL, DIM, "Right"))
    track = b.add("Item Scroll Track", frame, UIRectComponent=rect((1, 0), (1, 1), (1, 0.5), (-22, -18), (6, -136)), UIImageComponent=image(None, (1, 1, 1, 0.12)))
    thumb = b.add("Item Scroll Thumb", track, UIRectComponent=fill(), UIImageComponent=image(None, (0.86, 0.7, 0.42, 0.9)))
    view = b.add("Item List View", frame, UIRectComponent=rect((0, 0), (1, 1), (0.5, 0.5), (-10, -18), (-76, -136)),
                 UIScrollViewComponent={"ScrollPosition": 0, "FollowFocus": True, "Scrollbar": thumb})
    content = b.add("Item List", view, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, 0), (0, 100)), UILayoutGroupComponent=group(spacing=4))
    for i, (name, count) in enumerate(ITEMS):
        ribbon_row(b, "Item %d" % i, content, 640, 56, name, count, "UseItem")

    details = b.add("Item Detail Panel", inv, UIRectComponent=rect((0, 1), None, (0, 1), (852, -130), (560, 300)), UIImageComponent=image("Frame"))
    b.add("Item Detail Name", details, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -28), (-72, 48)), UITextComponent=text("Potion", FONT_TITLE, GOLD))
    b.add("Item Detail Rule", details, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -80), (-60, 12)), UIImageComponent=image("Divider", (1, 1, 1, 0.7)))
    b.add("Item Detail Text", details, UIRectComponent=fill((36, 30, 36, 100)),
          UITextComponent=text("Restores 25 HP to every hero.", FONT_BODY, CREAM, "Left", "Top", wrap=True))

    party = b.add("Menu Party", inv, UIRectComponent=rect((1, 1), None, (1, 1), (-40, -130), (440, 10)), UILayoutGroupComponent=group(spacing=10))
    status_block(b, "Menu Status Miko", party)

    help_bar = b.add("Help Bar", inv, UIRectComponent=rect((0, 0), (1, 0), (0.5, 0), (0, 0), (0, 84)), UIImageComponent=image(None, (0.02, 0.02, 0.03, 0.94)))
    b.add("Help Bar Rule", help_bar, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, 0), (0, 2)), UIImageComponent=image(None, (0.78, 0.63, 0.38, 0.6)))
    b.add("Help Text", help_bar, UIRectComponent=fill((72, 0, 72, 0)), UITextComponent=text("Choose an item.", FONT_BODY, CREAM))
    b.add("Help Keys", help_bar, UIRectComponent=fill((72, 0, 72, 0)), UITextComponent=text("Enter  Use        Esc / Tab  Close", FONT_SMALL, DIM, "Right"))

    # Dialogue: washi paper at the bottom, the speaker's name in its corner, a vermilion "next" marker.
    dlg = b.add("Dialogue Canvas", director, UICanvasComponent=canvas(30, visible=False, captures=True),
                NativeScriptComponent={"Scripts": [{"Name": "DialogueScript", "Properties": {"CharactersPerSecond": 40}}]})
    box = b.add("Dialogue Box", dlg, UIRectComponent=rect((0.5, 0), None, (0.5, 0), (0, 48), (1280, 232)), UIImageComponent=image("Paper"))
    b.add("Dialogue Name", box, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -18), (-96, 46)), UITextComponent=text("Elder", FONT_TITLE, [0.55, 0.2, 0.14, 1], shadow=False))
    b.add("Dialogue Text", box, UIRectComponent=fill((56, 30, 72, 72)), UITextComponent=text("...", FONT_BODY, INK_TEXT, "Left", "Top", wrap=True, shadow=False, spacing=1.15))
    b.add("Dialogue Next", box, UIRectComponent=rect((1, 0), None, (1, 0), (-28, 22), (28, 28), visible=False), UIImageComponent=image("Marker", nine=False))

    build_quest_canvas(b, director)

    # The fade to black over everything.
    fade = b.add("Fade Canvas", director, UICanvasComponent=canvas(100))
    b.add("Fade", fade, UIRectComponent=fill(visible=False), UIImageComponent=image(None, (0, 0, 0, 0)))
    return b


def build_quest_canvas(b, director):
    """The quests (QuestTrackerScript, Game/src/Scripts/QuestScripts.h): the tracked quest top right, frameless over
    a soft ink fade like the party status, and the "New Quest" / "Quest Complete" banner across the middle, between
    two bronze rules that open out from the centre (Octopath Traveler's story banners: the rules, not the look)."""
    quest = b.add("Quest Canvas", director, UICanvasComponent=canvas(22),
                  NativeScriptComponent={"Scripts": [{"Name": "QuestTrackerScript", "Properties": {"BannerHoldTime": 2.4, "BannerOpenTime": 0.5}}]})
    tracker = b.add("Quest Tracker", quest, UIRectComponent=rect((1, 1), None, (1, 1), (-28, -28), (560, 170), visible=False))
    b.add("Quest Tracker Back", tracker, UIRectComponent=fill(), UIImageComponent=image("Fade", (1, 1, 1, 0.92), flip_x=True, nine=False))
    b.add("Quest Tracker Label", tracker, UIRectComponent=rect((1, 1), None, (1, 1), (-28, -10), (400, 30)),
          UITextComponent=text("Quest", FONT_LABEL, GOLD, "Right"))
    b.add("Quest Tracker Title", tracker, UIRectComponent=rect((1, 1), None, (1, 1), (-28, -40), (520, 44)),
          UITextComponent=text("Clearing the Grounds", FONT_BODY, CREAM, "Right"))
    b.add("Quest Tracker Rule", tracker, UIRectComponent=rect((1, 1), None, (1, 1), (-20, -88), (440, 12)), UIImageComponent=image("Divider", (1, 1, 1, 0.8)))
    b.add("Quest Tracker Objective", tracker, UIRectComponent=rect((1, 1), None, (1, 1), (-28, -102), (520, 32)),
          UITextComponent=text("Drive the slimes from the lower path", FONT_SMALL, CREAM, "Right"))
    b.add("Quest Tracker Count", tracker, UIRectComponent=rect((1, 1), None, (1, 1), (-28, -134), (300, 30)),
          UITextComponent=text("0 / 3", FONT_SMALL, BLUE, "Right"))

    banner = b.add("Quest Banner", quest, UIRectComponent=rect((0.5, 0.5), None, (0.5, 0.5), (0, 150), (1920, 240), visible=False))
    b.add("Quest Banner Band", banner, UIRectComponent=rect(size=(1500, 176)), UIImageComponent=image("Band", (1, 1, 1, 0.95), nine=False))
    b.add("Quest Banner Rule Top", banner, UIRectComponent=rect(position=(0, 66), size=(880, 12)), UIImageComponent=image("Divider"))
    b.add("Quest Banner Header", banner, UIRectComponent=rect(position=(0, 36), size=(900, 34)), UITextComponent=text("New Quest", FONT_LABEL, GOLD, "Center"))
    b.add("Quest Banner Title", banner, UIRectComponent=rect(position=(0, -8), size=(1300, 56)),
          UITextComponent=text("Clearing the Grounds", FONT_TITLE, CREAM, "Center"))
    b.add("Quest Banner Rule Bottom", banner, UIRectComponent=rect(position=(0, -54), size=(880, 12)), UIImageComponent=image("Divider"))


# ---- Scene files ------------------------------------------------------------------------------------------------

class SceneDumper(yaml.SafeDumper):
    """Block maps, flow lists of plain values ([0.5, 1]): how the editor writes scenes. No anchors / aliases."""

    def ignore_aliases(self, data):
        return True


def _represent_list(dumper, value):
    flow = all(isinstance(item, (int, float, str, bool)) for item in value)
    return dumper.represent_sequence("tag:yaml.org,2002:seq", value, flow_style=flow)


SceneDumper.add_representer(list, _represent_list)


def dump_entities(entities, indent):
    text = yaml.dump(entities, Dumper=SceneDumper, sort_keys=False, default_flow_style=False, width=1000, allow_unicode=True)
    return "\n".join((indent + line) if line else line for line in text.rstrip("\n").split("\n"))


def rebuild(scene_file, remove_tags, make, director_tags=("Battle Director", "MVP Director")):
    path = os.path.join(SCENES, scene_file)
    raw = open(path, newline="", encoding="utf-8").read()
    crlf = "\r\n" in raw
    text = raw.replace("\r\n", "\n")
    ends_newline = text.endswith("\n")
    match = re.search(r"^( *)- Entity: ", text, re.M)
    indent = match.group(1)
    head, body = text[: match.start()], text[match.start():]
    blocks = re.split(r"\n(?=%s- Entity: )" % indent, body.rstrip("\n"))
    parsed = [yaml.safe_load(block[len(indent):].replace("\n" + indent, "\n"))[0] for block in blocks]

    # Remove the tags and every descendant.
    removed = {e["Entity"] for e in parsed if e["TagComponent"]["Tag"] in remove_tags}
    changed = True
    while changed:
        changed = False
        for e in parsed:
            parent = (e.get("RelationshipComponent") or {}).get("Parent")
            if parent in removed and e["Entity"] not in removed:
                removed.add(e["Entity"])
                changed = True
    kept = [(block, e) for block, e in zip(blocks, parsed) if e["Entity"] not in removed]
    director = next(e["Entity"] for _, e in kept if e["TagComponent"]["Tag"] in director_tags)
    builder = make(director)
    added = dump_entities(builder.entities, indent)
    for block, _ in kept:
        for gone in removed:
            if re.search(r"\b%d\b" % gone, block):
                sys.exit("%s: a kept entity still refers to removed %d:\n%s" % (scene_file, gone, block[:300]))

    new = head + "\n".join(block for block, _ in kept) + "\n" + added + ("\n" if ends_newline else "")
    yaml.safe_load(new)  # still a valid file
    if crlf:
        new = new.replace("\n", "\r\n")
    with open(path, "w", newline="", encoding="utf-8") as file:
        file.write(new)
    print("%s: removed %d entities, added %d" % (scene_file, len(removed), len(builder.entities)))


def main():
    which = sys.argv[1:] or ["battle", "field"]
    if "battle" in which:
        rebuild("Battle.hyscene", BATTLE_REMOVE, build_battle)
    if "field" in which:
        rebuild("ShrineGlade.hyscene", FIELD_REMOVE, lambda d: build_field("ShrineGlade", d))
        rebuild("FarmVillage.hyscene", FIELD_REMOVE, lambda d: build_field("FarmVillage", d))
        # Interiors (scripts/interiors/build_inn.py writes the room; the UI is the field's).
        for interior in ("FarmVillageInn", "FarmVillageInnUpstairs"):
            if os.path.exists(os.path.join(SCENES, interior + ".hyscene")):
                rebuild(interior + ".hyscene", FIELD_REMOVE, lambda d, name=interior: build_field(name, d))


if __name__ == "__main__":
    main()
