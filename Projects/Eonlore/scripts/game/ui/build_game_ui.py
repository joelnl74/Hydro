"""Build the game's runtime UI into the test bed scenes (Battle, Shrine Glade, Farm Village and its inn) on the 1920 x 1080
canvas (decision D5), in the game's own look: ink ribbons with bronze fittings, a vermilion cursor, washi paper for
dialogue (scripts/game/ui/make_ui_art.py draws the art).

    python scripts/game/ui/build_game_ui.py [battle] [field] [prefab]

"prefab" writes the field UI as Prefabs/UI/FieldUI.hyprefab, which FieldDirectorScript instantiates in any scene
that has no field UI baked in.

Close the scenes in the editor first. Each run removes what an earlier run (or the older UI it replaces) put there,
by tag, with every descendant, and appends the UI again, so it can be run as often as needed; entity IDs come from
the tags, so they stay the same between runs. Everything else in the scenes is left alone. The entity names are the
ones BattleScript, FieldDirectorScript (its views) and DialogueScript look up: rename them in both places.

Check the result offline with scripts/game/ui/preview_ui.py.

Needs PyYAML.
"""

import hashlib
import os
import re
import sys

import yaml

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ASSETS = os.path.join(ROOT, "Project", "Assets")
SCENES = os.path.join(ASSETS, "Scenes")


def handle_of(relative):
    with open(os.path.join(ASSETS, relative + ".hymeta"), encoding="utf-8") as file:
        return int(re.search(r"Handle:\s*(\d+)", file.read()).group(1))


FONT_BODY = handle_of("Fonts/EBGaramond30.fnt")
FONT_SMALL = handle_of("Fonts/EBGaramond22.fnt")
FONT_TITLE = handle_of("Fonts/Cinzel34.fnt")
FONT_LABEL = handle_of("Fonts/Cinzel20.fnt")
ART = {name: handle_of("Sprites/UI/%s.png" % name) for name in
       ("Ribbon", "RibbonFocus", "Frame", "Paper", "Banner", "Divider", "Plate", "Cursor", "Marker", "Shield", "Chip", "Fade", "Pip",
        "Band", "QuestMark")}
BORDERS = {"Ribbon": [20, 10, 34, 10], "RibbonFocus": [20, 10, 34, 10], "Frame": [22, 22, 22, 22], "Paper": [22, 22, 22, 22],
           "Banner": [26, 12, 26, 12], "Divider": [14, 0, 14, 0], "Plate": [16, 8, 16, 8]}
BATTLE_ICONS = handle_of("Sprites/UI/BattleIcons.png")

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


def image(art=None, color=(1, 1, 1, 1), sprite=0, texture=None, flip_x=False, nine=True, sprite_name=None):
    i = {"SpriteIndex": sprite, "Color": list(color), "NineSlice": BORDERS.get(art, [0, 0, 0, 0]) if (art and nine) else [0, 0, 0, 0], "FlipX": flip_x, "FlipY": False}
    handle = texture if texture is not None else (ART[art] if art else None)
    if handle:
        # A sheet's sprites are saved by name as well, as the editor saves them (the name wins when the sheet changes).
        named = {"Sprite": sprite_name} if sprite_name else {}
        i = {"TextureHandle": handle, **named, **i}
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


# One status block per place at the front of the party (PartyState::MaxFighters), with sample names and numbers; the
# scripts fill them from the party and hide the empty ones.
PARTY = [("Miko", "90 / 90", "32 / 32"), ("Orin", "82 / 82", "20 / 20"), ("Lila", "76 / 76", "40 / 40"), ("Juno", "88 / 88", "24 / 24")]


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


ORDER_CHIPS = 8  # BattleScript's OrderChips: a round of four heroes and three enemies, plus one
COMMAND_X = 30  # the lists' left edge from the middle, in pixels: they end left of the party's status column


def build_battle(director):
    b = Builder("Battle")
    root = b.add("Battle UI Canvas", director, UICanvasComponent=canvas(0))

    # The turn order, top left: a bronze rule with a diamond chip per turn (the actor's sprite inside).
    b.add("Turn Order Rule", root, UIRectComponent=rect((0, 1), None, (0, 0.5), (36, -82), (760, 12)), UIImageComponent=image("Divider"))
    order = b.add("Turn Order", root, UIRectComponent=rect((0, 1), None, (0, 1), (40, -36), (760, 92)),
                  UILayoutGroupComponent=group("Horizontal", 14, fit=False, stretch=False, align="Start"))
    for c in range(ORDER_CHIPS):
        chip = b.add("Order Chip %d" % c, order, UIRectComponent=rect(size=(92, 92) if c == 0 else (76, 76)), UIImageComponent=image("Chip", nine=False))
        b.add("Order Chip %d Actor" % c, chip, UIRectComponent=fill((14, 14, 14, 14)), UIImageComponent=image(None, (1, 1, 1, 0)))
    b.add("Turn Order Label", root, UIRectComponent=rect((0, 1), None, (0, 1), (150, -112), (300, 30)),
          UITextComponent=text("Next turn", FONT_LABEL, DIM))

    # The party, top right.
    party = b.add("Party Status", root, UIRectComponent=rect((1, 1), None, (1, 1), (-36, -24), (440, 10)), UILayoutGroupComponent=group(spacing=10))
    for s, (hero, hp, mp) in enumerate(PARTY):
        status_block(b, "Status %d" % s, party, hero, hp, mp)

    # The skill / message banner, top middle.
    banner = b.add("Battle Message Panel", root, UIRectComponent=rect((0.5, 1), None, (0.5, 1), (0, -168), (600, 72)), UIImageComponent=image("Banner"))
    b.add("Battle Message Text", banner, UIRectComponent=fill((40, 0, 40, 0)), UITextComponent=text("Flame Kick", FONT_BODY, CREAM, "Center"))

    # The command list, right of the middle, and the target list in its place; left of the four status blocks.
    commands = b.add("Command Panel", root, UIRectComponent=rect((0.5, 0), None, (0, 1), (COMMAND_X, 560), (440, 10)), UILayoutGroupComponent=group(spacing=4))
    header = b.add("Command Header", commands, UIRectComponent=rect(size=(440, 46)), UIImageComponent=image("Plate"))
    b.add("Command Hero", header, UIRectComponent=fill((60, 0, 20, 0)), UITextComponent=text("Miko", FONT_LABEL, GOLD))
    b.add("Command Boost", header, UIRectComponent=fill((60, 0, 24, 0)), UITextComponent=text("E: boost", FONT_SMALL, CREAM, "Right"))
    names = [("Attack", "Attack", ""), ("Abilities", "Abilities", ""), ("Items", "Items", "x2"), ("Guard", "Guard", ""), ("Run", "Run", "")]
    for c, (event, name, cost) in enumerate(names):
        ribbon_row(b, "Command %d" % c, commands, 440, 56, name, cost, event)
    helpbar = b.add("Command Help Bar", commands, UIRectComponent=rect(size=(440, 48)), UIImageComponent=image("Fade", nine=False))
    b.add("Command Help", helpbar, UIRectComponent=fill((24, 0, 12, 0)), UITextComponent=text("Strike one foe.", FONT_SMALL, CREAM))

    targets = b.add("Target Panel", root, UIRectComponent=rect((0.5, 0), None, (0, 1), (COMMAND_X, 560), (440, 10)), UILayoutGroupComponent=group(spacing=4))
    theader = b.add("Target Header", targets, UIRectComponent=rect(size=(440, 46)), UIImageComponent=image("Plate"))
    b.add("Target Command", theader, UIRectComponent=fill((60, 0, 20, 0)), UITextComponent=text("Attack", FONT_LABEL, GOLD))
    b.add("Target Hint", theader, UIRectComponent=fill((60, 0, 24, 0)), UITextComponent=text("Esc: back", FONT_SMALL, CREAM, "Right"))
    for e in range(3):
        ribbon_row(b, "Target %d" % e, targets, 440, 56, "Slime " + "ABC"[e], "HP 40", "Target")

    # The sub-menu (Abilities: the hero's 8 ability slots; Items: the usable items), also in the commands' place.
    sub = b.add("Sub Panel", root, UIRectComponent=rect((0.5, 0), None, (0, 1), (COMMAND_X, 560), (440, 10)), UILayoutGroupComponent=group(spacing=4))
    sheader = b.add("Sub Header", sub, UIRectComponent=rect(size=(440, 46)), UIImageComponent=image("Plate"))
    b.add("Sub Title", sheader, UIRectComponent=fill((60, 0, 20, 0)), UITextComponent=text("Abilities", FONT_LABEL, GOLD))
    b.add("Sub Hint", sheader, UIRectComponent=fill((60, 0, 24, 0)), UITextComponent=text("Esc: back", FONT_SMALL, CREAM, "Right"))
    abilities = [("Flame Kick", "5 MP"), ("Holy Palm", "6 MP"), ("Radiant Kick", "8 MP"), ("Spirit Lance", "10 MP")]
    for r in range(8):
        name, cost = abilities[r] if r < len(abilities) else ("-", "")
        ribbon_row(b, "Sub Command %d" % r, sub, 440, 48, name, cost, "Sub", interactable=r < len(abilities))
    subhelp = b.add("Sub Help Bar", sub, UIRectComponent=rect(size=(440, 48)), UIImageComponent=image("Fade", nine=False))
    b.add("Sub Help", subhelp, UIRectComponent=fill((24, 0, 12, 0)), UITextComponent=text("Fire damage to one foe.", FONT_SMALL, CREAM))

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
    "Field HUD Canvas", "Field Menu Canvas", "Inventory Canvas", "Dialogue Canvas", "Fade Canvas", "Prompt Canvas", "Quest Canvas",
    "UI", "UI Prompt Panel", "UI Prompt Label",
}

ITEMS = [("Potion", "x3"), ("Hi-Potion", "x1"), ("Ether", "x2"), ("Herb Tea", "x2"), ("Iron Sword", "x1"), ("Herb Bundle", "x4"),
         ("Phoenix Feather", "x1"), ("Crystal Shard", "x2"), ("Leather Boots", "x1"), ("Antidote", "x2"), ("Ancient Key", "x1"),
         ("Copper Ore", "x5"), ("Old Map", "x1"), ("Shrine Charm", "x1"), ("Lantern Oil", "x3"), ("Rope", "x1"),
         ("Smoked Fish", "x2"), ("Spring Water", "x3"), ("Pine Resin", "x2"), ("Fox Mask", "x1")]


def build_field(scene_name, director, b=None):
    b = b or Builder(scene_name)

    # The interaction prompt over the player's head: a World canvas FieldDirectorScript (InteractPromptView) moves and shows.
    prompt = b.add("Prompt Canvas", director, TransformComponent=transform(), UICanvasComponent=canvas(5, world=([220, 52], [0.5, 0])))
    b.add("Prompt Plate", prompt, UIRectComponent=fill(), UIImageComponent=image("Plate"))
    b.add("Prompt Text", prompt, UIRectComponent=fill((16, 0, 16, 0)), UITextComponent=text("E  Talk", FONT_SMALL, CREAM, "Center"))

    # Messages: a banner at the top.
    hud = b.add("Field HUD Canvas", director, UICanvasComponent=canvas(20))
    panel = b.add("Field Message Panel", hud, UIRectComponent=rect((0.5, 1), None, (0.5, 1), (0, -110), (760, 104)), UIImageComponent=image("Banner"))
    b.add("Field Message Text", panel, UIRectComponent=fill((48, 8, 48, 8)), UITextComponent=text("Found a Potion!", FONT_BODY, CREAM, "Center", wrap=True))

    build_field_menu(b, director)

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


MENU_TABS = ["Journal", "Inventory", "Character", "Abilities", "Equipment", "Party", "System"]  # FieldMenuView::Tab's order
PAGE_X, PAGE_WIDTH = 392, 1000


def build_field_menu(b, director):
    """The field menu (Tab; the Menu plan, Octopath-style; FieldMenuView, Game/src/Scripts/Field/FieldMenuView.h): over a
    dimmed scene, the tab ribbons in a column on the left (the selected one marked), the tab's page in the middle, the
    party's status on the right and a help bar with the keys at the bottom. Pages are "Page <Tab>" (the Journal page is
    QuestsView, the Inventory page InventoryView, ...); the menu opens on the Journal."""
    menu = b.add("Field Menu Canvas", director, UICanvasComponent=canvas(10, visible=False, captures=True))
    b.add("Menu Dim", menu, UIRectComponent=fill(), UIImageComponent=image(None, (0.03, 0.04, 0.06, 0.62)))
    b.add("Menu Title", menu, UIRectComponent=rect((0, 1), None, (0, 1), (72, -40), (480, 60)), UITextComponent=text("Journal", FONT_TITLE, CREAM))
    b.add("Menu Title Rule", menu, UIRectComponent=rect((0, 1), None, (0, 1), (56, -100), (560, 12)), UIImageComponent=image("Divider"))

    # The tabs: ribbon rows like the battle's commands; the script shows the selected one's mark and gilds its name.
    tabs = b.add("Menu Tabs", menu, UIRectComponent=rect((0, 1), None, (0, 1), (56, -130), (300, 10)), UILayoutGroupComponent=group(spacing=6))
    for t, name in enumerate(MENU_TABS):
        row = ribbon_row(b, "Tab %d" % t, tabs, 300, 60, name, "", "Tab", name_font=FONT_LABEL)
        # The selected tab while the focus is in its page: a dimmer cursor where the focused one sits (shown by the script).
        b.add("Tab %d Mark" % t, row, UIRectComponent=rect((0, 0.5), None, (0, 0.5), (0, 0), (44, 44), visible=False),
              UIImageComponent=image("Cursor", (1, 1, 1, 0.55)))

    # The Inventory page: the list (scrolling) in a frame, and the selected item's details beside it.
    page = b.add("Page Inventory", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800), visible=False))
    frame = b.add("Inventory Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, 0), (560, 800)), UIImageComponent=image("Frame"))
    head = b.add("Inventory Header", frame, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -26), (-52, 44)), UIImageComponent=image("Plate"))
    b.add("Inventory Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Items", FONT_LABEL, GOLD))
    b.add("Inventory Header Count", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Owned", FONT_SMALL, DIM, "Right"))
    track = b.add("Item Scroll Track", frame, UIRectComponent=rect((1, 0), (1, 1), (1, 0.5), (-22, -18), (6, -136)), UIImageComponent=image(None, (1, 1, 1, 0.12)))
    thumb = b.add("Item Scroll Thumb", track, UIRectComponent=fill(), UIImageComponent=image(None, (0.86, 0.7, 0.42, 0.9)))
    view = b.add("Item List View", frame, UIRectComponent=rect((0, 0), (1, 1), (0.5, 0.5), (-10, -18), (-76, -136)),
                 UIScrollViewComponent={"ScrollPosition": 0, "FollowFocus": True, "Scrollbar": thumb})
    content = b.add("Item List", view, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, 0), (0, 100)), UILayoutGroupComponent=group(spacing=4))
    for i, (name, count) in enumerate(ITEMS):
        ribbon_row(b, "Item %d" % i, content, 480, 56, name, count, "UseItem")

    details = b.add("Item Detail Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (588, 0), (412, 320)), UIImageComponent=image("Frame"))
    b.add("Item Detail Name", details, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -28), (-72, 48)), UITextComponent=text("Potion", FONT_TITLE, GOLD))
    b.add("Item Detail Rule", details, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -80), (-60, 12)), UIImageComponent=image("Divider", (1, 1, 1, 0.7)))
    b.add("Item Detail Text", details, UIRectComponent=fill((36, 30, 36, 100)),
          UITextComponent=text("Restores 25 HP to every hero.", FONT_BODY, CREAM, "Left", "Top", wrap=True))

    build_quests_page(b, menu)
    build_character_page(b, menu)
    build_abilities_page(b, menu)
    build_equipment_page(b, menu)
    build_party_page(b, menu)
    build_system_page(b, menu)

    party = b.add("Menu Party", menu, UIRectComponent=rect((1, 1), None, (1, 1), (-40, -130), (440, 10)), UILayoutGroupComponent=group(spacing=10))
    for s, (hero, hp, mp) in enumerate(PARTY):
        status_block(b, "Menu Status %d" % s, party, hero, hp, mp)
    # The party's money (aster, the world's currency), above the help bar on the right.
    purse = b.add("Menu Money", menu, UIRectComponent=rect((1, 0), None, (1, 0), (-40, 104), (440, 52)), UIImageComponent=image("Fade", (1, 1, 1, 0.9), flip_x=True, nine=False))
    b.add("Menu Money Label", purse, UIRectComponent=fill((26, 0, 24, 0)), UITextComponent=text("Aster", FONT_LABEL, GOLD))
    b.add("Menu Money Amount", purse, UIRectComponent=fill((26, 0, 24, 0)), UITextComponent=text("50", FONT_BODY, CREAM, "Right"))

    help_bar = b.add("Help Bar", menu, UIRectComponent=rect((0, 0), (1, 0), (0.5, 0), (0, 0), (0, 84)), UIImageComponent=image(None, (0.02, 0.02, 0.03, 0.94)))
    b.add("Help Bar Rule", help_bar, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, 0), (0, 2)), UIImageComponent=image(None, (0.78, 0.63, 0.38, 0.6)))
    b.add("Help Text", help_bar, UIRectComponent=fill((72, 0, 72, 0)), UITextComponent=text("Select a category.", FONT_BODY, CREAM))
    b.add("Help Keys", help_bar, UIRectComponent=fill((72, 0, 72, 0)), UITextComponent=text("Enter  Open        Esc / Tab  Close", FONT_SMALL, DIM, "Right"))


STAT_NAMES = ["Max HP", "Max MP", "Attack", "Magic", "Speed"]
EQUIP_NAMES = ["Weapon", "Off-hand", "Head", "Body", "Accessory", "Accessory"]
MIKO_FIELD = handle_of("Characters/Player/Miko/Miko_Field.png")


def page_text(b, tag, parent, position, size, value, font=FONT_BODY, color=CREAM, align="Left", wrap=False):
    """A text placed from its parent's top-left corner."""
    return b.add(tag, parent, UIRectComponent=rect((0, 1), None, (0, 1), position, size), UITextComponent=text(value, font, color, align, "Top" if wrap else "Middle", wrap=wrap))


def build_quests_page(b, menu):
    """The Journal page (QuestsView, Game/src/Scripts/Field/QuestsView.h): the run's quests on the left (unfinished
    first, newest first; "Done" on finished ones), the focused one's title, state, objective and steps on the right.
    The first tab, so it is the page shown when the menu first opens."""
    page = b.add("Page Journal", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800)))
    frame = b.add("Quest Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, 0), (420, 800)), UIImageComponent=image("Frame"))
    head = b.add("Quest Header", frame, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -26), (-52, 44)), UIImageComponent=image("Plate"))
    b.add("Quest Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Journal", FONT_LABEL, GOLD))
    b.add("Quest Header Count", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("1 active", FONT_SMALL, DIM, "Right"))
    rows = b.add("Quest List", frame, UIRectComponent=rect((0, 1), None, (0, 1), (14, -96), (370, 10)), UILayoutGroupComponent=group(spacing=4))
    sample = [("The Sleeping Tree", ""), ("Lost Lantern", "Done")]
    for q in range(10):
        name, state = sample[q] if q < len(sample) else ("Quest", "")
        row = ribbon_row(b, "Quest %d" % q, rows, 370, 56, name, state, "Quest")
        if q >= len(sample):
            next(e for e in b.entities if e["Entity"] == row)["UIRectComponent"]["Visible"] = False  # shown by QuestsView

    detail = b.add("Quest Detail Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (440, 0), (560, 800)), UIImageComponent=image("Frame"))
    page_text(b, "Quest Detail Title", detail, (36, -28), (488, 48), "The Sleeping Tree", FONT_TITLE, GOLD)
    page_text(b, "Quest Detail State", detail, (38, -78), (488, 30), "In progress", FONT_SMALL, DIM)
    b.add("Quest Detail Rule", detail, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -112), (-60, 12)), UIImageComponent=image("Divider", (1, 1, 1, 0.7)))
    page_text(b, "Quest Detail Objective Label", detail, (36, -136), (488, 30), "Objective", FONT_LABEL, GOLD)
    page_text(b, "Quest Detail Objective", detail, (36, -172), (488, 120), "Ask the elder about the tree.", FONT_BODY, CREAM, wrap=True)
    page_text(b, "Quest Detail Steps Label", detail, (36, -310), (488, 30), "Steps", FONT_LABEL, GOLD)
    steps = b.add("Quest Steps", detail, UIRectComponent=rect((0, 1), None, (0, 1), (36, -350), (488, 10)), UILayoutGroupComponent=group(spacing=6))
    for i in range(8):
        b.add("Quest Step %d" % i, steps, UIRectComponent=rect(size=(488, 48), visible=i < 2),
              UITextComponent=text(["Find the shrine maiden.", "Ask the elder about the tree."][i] if i < 2 else "", FONT_SMALL, DIM if i == 0 else CREAM, "Left", "Top", wrap=True))


def build_character_page(b, menu):
    """The Character page (CharacterView, Game/src/Scripts/Field/CharacterView.h): the hero's portrait, name, level and
    experience, its stats with what equipment adds, and what it wears."""
    page = b.add("Page Character", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800), visible=False))
    panel = b.add("Character Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, 0), (PAGE_WIDTH, 800)), UIImageComponent=image("Frame"))
    portrait = b.add("Character Portrait Frame", panel, UIRectComponent=rect((0, 1), None, (0, 1), (40, -36), (232, 222)), UIImageComponent=image("Frame"))
    b.add("Character Portrait", portrait, UIRectComponent=fill((20, 16, 20, 16)), UIImageComponent=image(None, texture=MIKO_FIELD, sprite=0, sprite_name="Miko_0"))
    page_text(b, "Character Name", panel, (304, -44), (640, 52), "Miko", FONT_TITLE, GOLD)
    page_text(b, "Character Level", panel, (306, -102), (300, 40), "Level 1", FONT_BODY)
    page_text(b, "Character Place", panel, (606, -102), (330, 40), "Line-up   1 / 2", FONT_SMALL, DIM, "Right")
    page_text(b, "Character Experience", panel, (306, -150), (300, 34), "Experience 0", FONT_SMALL, DIM)
    page_text(b, "Character Next", panel, (606, -150), (330, 34), "Next level: 30", FONT_SMALL, DIM, "Right")
    back = b.add("Character Experience Back", panel, UIRectComponent=rect((0, 1), None, (0, 1), (306, -196), (630, 8)), UIImageComponent=image(None, (0, 0, 0, 0.6)))
    b.add("Character Experience Fill", back, UIRectComponent=rect((0, 0), (1, 1), (0, 0.5), (0, 0), (0, 0)), UIImageComponent=image(None, GOLD))
    b.add("Character Rule", panel, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -284), (-60, 12)), UIImageComponent=image("Divider", (1, 1, 1, 0.7)))

    for column, (title, names, x) in enumerate((("Stats", STAT_NAMES, 40), ("Equipment", EQUIP_NAMES, 516))):
        tag = "Character " + ("Stat" if column == 0 else "Equip")
        head = b.add(tag + " Header", panel, UIRectComponent=rect((0, 1), None, (0, 1), (x, -310), (444, 44)), UIImageComponent=image("Plate"))
        b.add(tag + " Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text(title, FONT_LABEL, GOLD))
        group_entity = b.add(tag + " List", panel, UIRectComponent=rect((0, 1), None, (0, 1), (x, -366), (444, 10)), UILayoutGroupComponent=group(spacing=2))
        for i, name in enumerate(names):
            row = b.add("%s %d" % (tag, i), group_entity, UIRectComponent=rect(size=(444, 54)))
            b.add("%s %d Back" % (tag, i), row, UIRectComponent=fill(), UIImageComponent=image(None, (1, 1, 1, 0.045 if i % 2 == 0 else 0.0)))
            b.add("%s %d Name" % (tag, i), row, UIRectComponent=fill((22, 0, 22, 0)), UITextComponent=text(name, FONT_LABEL, GOLD))
            value = ["90", "32", "10", "8", "9"][i] if column == 0 else "-"
            b.add("%s %d Value" % (tag, i), row, UIRectComponent=fill((22, 0, 22, 0)), UITextComponent=text(value, FONT_BODY, CREAM, "Right"))


def list_panel(b, tag, parent, position, size, title, count, rows, row_tag, row_width, row_height, event, names):
    """A framed list for the menu's pages: a header plate (title, count) and a fitted group of ribbon rows; rows past
    `names` are hidden (the page's view shows them)."""
    frame = b.add(tag, parent, UIRectComponent=rect((0, 1), None, (0, 1), position, size), UIImageComponent=image("Frame"))
    head = b.add(tag + " Header", frame, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -26), (-52, 44)), UIImageComponent=image("Plate"))
    b.add(tag + " Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text(title, FONT_LABEL, GOLD))
    b.add(tag + " Header Count", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text(count, FONT_SMALL, DIM, "Right"))
    group_entity = b.add(tag + " List", frame, UIRectComponent=rect((0, 1), None, (0, 1), (14, -96), (row_width + 10, 10)), UILayoutGroupComponent=group(spacing=4))
    for i in range(rows):
        name, detail = names[i] if i < len(names) else ("", "")
        row = ribbon_row(b, "%s %d" % (row_tag, i), group_entity, row_width, row_height, name, detail, event)
        if i >= len(names):
            next(e for e in b.entities if e["Entity"] == row)["UIRectComponent"]["Visible"] = False
    return frame


def build_abilities_page(b, menu):
    """The Abilities page (AbilitiesView, Game/src/Scripts/Field/AbilitiesView.h): the battle's 8 ability slots on the
    left, the abilities the hero knows on the right (the first row empties a slot), the focused ability's details
    below. Enter on a slot moves to the known list; Enter there puts that ability in the slot (swapping)."""
    page = b.add("Page Abilities", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800), visible=False))
    slots = [("Flame Kick", "5 MP"), ("Holy Palm", "6 MP")] + [("-", "")] * 6
    list_panel(b, "Ability Slot Panel", page, (0, 0), (480, 600), "Battle slots", "2 / 8", 8, "Ability Slot", 420, 56, "AbilitySlot", slots)
    known = [("Empty the slot", ""), ("Flame Kick", "Slot 1"), ("Holy Palm", "Slot 2")]
    list_panel(b, "Ability Known Panel", page, (500, 0), (500, 600), "Known", "2", 10, "Known Ability", 440, 46, "KnownAbility", known)
    detail = b.add("Ability Detail Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, -620), (PAGE_WIDTH, 180)), UIImageComponent=image("Frame"))
    page_text(b, "Ability Detail Name", detail, (36, -26), (600, 46), "Flame Kick", FONT_TITLE, GOLD)
    page_text(b, "Ability Detail Info", detail, (600, -30), (364, 40), "Fire  5 MP", FONT_SMALL, DIM, "Right")
    page_text(b, "Ability Detail Text", detail, (36, -84), (928, 70), "A kick wreathed in fire.", FONT_BODY, CREAM, wrap=True)


def build_equipment_page(b, menu):
    """The Equipment page (EquipmentView, Game/src/Scripts/Field/EquipmentView.h): what the hero wears per slot on the
    left, the stats below it (now, and with the focused choice), the bag's items that fit the chosen slot on the
    right (the first row takes off what is worn)."""
    page = b.add("Page Equipment", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800), visible=False))
    worn = [("-", name) for name in EQUIP_NAMES]
    list_panel(b, "Gear Slot Panel", page, (0, 0), (480, 456), "Equipped", "", 6, "Gear Slot", 420, 56, "GearSlot", worn)
    choices = [("Take off", ""), ("Tekko", "x1"), ("Iron Sword", "x1")]
    list_panel(b, "Gear Choice Panel", page, (500, 0), (500, 600), "Bag", "Weapon", 10, "Gear Choice", 440, 46, "GearChoice", choices)

    stats = b.add("Gear Stats Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, -476), (480, 324)), UIImageComponent=image("Frame"))
    head = b.add("Gear Stats Header", stats, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -26), (-52, 44)), UIImageComponent=image("Plate"))
    b.add("Gear Stats Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Stats", FONT_LABEL, GOLD))
    rows = b.add("Gear Stat List", stats, UIRectComponent=rect((0, 1), None, (0, 1), (24, -84), (432, 10)), UILayoutGroupComponent=group(spacing=0))
    for i, name in enumerate(STAT_NAMES):
        row = b.add("Gear Stat %d" % i, rows, UIRectComponent=rect(size=(432, 42)))
        b.add("Gear Stat %d Name" % i, row, UIRectComponent=fill((12, 0, 12, 0)), UITextComponent=text(name, FONT_LABEL, GOLD))
        b.add("Gear Stat %d Now" % i, row, UIRectComponent=rect((1, 0.5), None, (1, 0.5), (-150, 0), (90, 40)),
              UITextComponent=text(["90", "32", "10", "8", "9"][i], FONT_BODY, CREAM, "Right"))
        b.add("Gear Stat %d Arrow" % i, row, UIRectComponent=rect((1, 0.5), None, (1, 0.5), (-108, 0), (36, 40)),
              UITextComponent=text("\u00bb", FONT_BODY, DIM, "Center"))
        b.add("Gear Stat %d New" % i, row, UIRectComponent=rect((1, 0.5), None, (1, 0.5), (-12, 0), (90, 40)),
              UITextComponent=text(["90", "32", "14", "8", "9"][i], FONT_BODY, CREAM, "Right"))


def build_party_page(b, menu):
    """The Party page (PartyView, Game/src/Scripts/Field/PartyView.h): the line-up (who fights, up to four; empty places
    show "-") on the left, the reserve on the right, the focused member's level, HP and MP and what Enter does below.
    Enter moves a member between the two."""
    page = b.add("Page Party", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800), visible=False))
    lineup = [("Miko", "Lv 1"), ("Orin", "Lv 1"), ("-", ""), ("-", "")]
    list_panel(b, "Party Lineup Panel", page, (0, 0), (480, 400), "Line-up", "2 / 4", 4, "Lineup Member", 420, 56, "LineupMember", lineup)
    reserve = [("Lila", "Lv 1")]
    list_panel(b, "Party Reserve Panel", page, (500, 0), (500, 600), "Reserve", "1", 8, "Reserve Member", 440, 52, "ReserveMember", reserve)
    detail = b.add("Party Detail Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, -620), (PAGE_WIDTH, 180)), UIImageComponent=image("Frame"))
    page_text(b, "Party Detail Name", detail, (36, -26), (420, 46), "Miko", FONT_TITLE, GOLD)
    page_text(b, "Party Detail Info", detail, (456, -30), (508, 40), "Lv 1   HP 90 / 90   MP 32 / 32", FONT_SMALL, DIM, "Right")
    page_text(b, "Party Detail Text", detail, (36, -84), (928, 70), "In the line-up: fights in battle. Enter moves them to the reserve.",
              FONT_BODY, CREAM, wrap=True)


def build_system_page(b, menu):
    """The System page (SystemView, Game/src/Scripts/Field/SystemView.h): save, load, restart and delete the save on
    the left, what the save slot holds on the right, what the focused option does (or asks to confirm) below."""
    page = b.add("Page System", menu, UIRectComponent=rect((0, 1), None, (0, 1), (PAGE_X, -130), (PAGE_WIDTH, 800), visible=False))
    options = [("Save", ""), ("Load", ""), ("Restart", ""), ("Delete save", "")]
    list_panel(b, "System Option Panel", page, (0, 0), (480, 400), "System", "", 4, "System Option", 420, 56, "SystemOption", options)
    slot = b.add("System Slot Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (500, 0), (500, 400)), UIImageComponent=image("Frame"))
    head = b.add("System Slot Header", slot, UIRectComponent=rect((0, 1), (1, 1), (0.5, 1), (0, -26), (-52, 44)), UIImageComponent=image("Plate"))
    b.add("System Slot Header Text", head, UIRectComponent=fill((28, 0, 28, 0)), UITextComponent=text("Save slot", FONT_LABEL, GOLD))
    page_text(b, "System Slot Info", slot, (40, -96), (420, 270), "2026-11-04 11:58\nFarmVillage\nMiko Lv 3, Orin Lv 2\n120 Aster",
              FONT_BODY, CREAM, wrap=True)
    detail = b.add("System Detail Panel", page, UIRectComponent=rect((0, 1), None, (0, 1), (0, -420), (PAGE_WIDTH, 140)), UIImageComponent=image("Frame"))
    page_text(b, "System Detail Text", detail, (36, -30), (928, 80), "Write the party, items, quests and where you stand to the save slot.",
              FONT_BODY, CREAM, wrap=True)


def build_quest_canvas(b, director):
    """The quests (QuestTrackerScript, Game/src/Views/QuestTrackerScript.h): the tracked quest top right, frameless over
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
        stale = removed.intersection(int(number) for number in re.findall(r"\b\d+\b", block))
        if stale:
            sys.exit("%s: a kept entity still refers to removed %d:\n%s" % (scene_file, min(stale), block[:300]))

    # Where the old UI began, so a rerun leaves the file's order (and its diff) alone; the end of the file the first time.
    at = next((i for i, e in enumerate(parsed) if e["Entity"] in removed), len(parsed))
    before = [block for block, e in zip(blocks[:at], parsed[:at]) if e["Entity"] not in removed]
    after = [block for block, e in zip(blocks[at:], parsed[at:]) if e["Entity"] not in removed]
    new = head + "\n".join(before + [added] + after) + ("\n" if ends_newline else "")
    yaml.safe_load(new)  # still a valid file
    if crlf:
        new = new.replace("\n", "\r\n")
    with open(path, "w", newline="", encoding="utf-8") as file:
        file.write(new)
    print("%s: removed %d entities, added %d" % (scene_file, len(removed), len(builder.entities)))


# The field UI as a prefab (Nov 3, 2026): FieldDirectorScript instantiates it under itself in any scene that has no
# field UI of its own (the Japanese city, the temple, any scene a run starts in). The same canvases as build_field;
# the handle is FieldDirectorScript::DefaultFieldUI.
FIELD_UI_PREFAB = os.path.join(ASSETS, "Prefabs", "UI", "FieldUI.hyprefab")
FIELD_UI_HANDLE = 1324405221109369128


def write_field_prefab():
    b = Builder("FieldUI")
    root = b.add("Field UI", None, TransformComponent=transform())
    build_field("FieldUI", root, b)
    text = "Handle: %d\nPrefab: Field UI\nEntities:\n%s\n" % (FIELD_UI_HANDLE, dump_entities(b.entities, "  "))
    yaml.safe_load(text)  # a valid file
    os.makedirs(os.path.dirname(FIELD_UI_PREFAB), exist_ok=True)
    with open(FIELD_UI_PREFAB, "w", newline="", encoding="utf-8") as file:
        file.write(text)
    print("%s: %d entities" % (FIELD_UI_PREFAB, len(b.entities)))


def main():
    which = sys.argv[1:] or ["battle", "field", "prefab"]
    if "prefab" in which:
        write_field_prefab()
    if "battle" in which:
        rebuild("Battle.hyscene", BATTLE_REMOVE, build_battle)
    if "field" in which:
        rebuild("ShrineGlade.hyscene", FIELD_REMOVE, lambda d: build_field("ShrineGlade", d))
        rebuild("FarmVillage.hyscene", FIELD_REMOVE, lambda d: build_field("FarmVillage", d))
        # The Japanese countryside (scripts/game/japanese_city/build_countryside.py; its director is "Field Director").
        rebuild("JapaneseCountryside.hyscene", FIELD_REMOVE, lambda d: build_field("JapaneseCountryside", d), director_tags=("Field Director",))
        # Interiors (scripts/game/interiors/build_inn.py writes the room; the UI is the field's).
        for interior in ("FarmVillageInn", "FarmVillageInnUpstairs"):
            if os.path.exists(os.path.join(SCENES, interior + ".hyscene")):
                rebuild(interior + ".hyscene", FIELD_REMOVE, lambda d, name=interior: build_field(name, d))


if __name__ == "__main__":
    main()
