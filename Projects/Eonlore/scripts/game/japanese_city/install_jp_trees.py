"""Installs the delivered Japanese trees and shrubs (art_incoming/jp_city/trees_r1, made by art/cut_trees.py) into the
game's sprite folders, Nov 2:

  - JP_Tree_*  -> Sprites/Trees, JP_Shrub_* -> Sprites/Plants, each with a .hymeta (32 px/m; the handle is kept).
  - The European stand-ins that only the Japanese scenes use are replaced in place, keeping their handles, so the
    city, the countryside and the temple all get the Japanese trees: PineTall -> black pine, LindenAutumnA/C/E/F ->
    maples, LindenAutumnB/D (the countryside's persimmons) -> persimmon, BambooA-C -> the delivered bamboo at full size.
    The firs, oaks and the weeping cherry stay: the farm village, Shrine Glade and the battle arena use them too.
  - The stand-ins' old images are backed up once to art_incoming/jp_city/_backup/stand_in_trees_2026-11-02.

A scene stores each tree card's size when it is built, so rebuild the scenes that use them afterwards
(build_city_slice.py, build_temple_district.py, build_countryside.py, build_city_scene.py).

    python scripts/game/japanese_city/install_jp_trees.py
"""

import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_city_scene as city  # noqa: E402  (stable_handle)

REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
ASSETS = os.path.join(REPO, "Project", "Assets")
SRC = os.path.join(REPO, "art_incoming", "jp_city", "trees_r1")
BACKUP = os.path.join(REPO, "art_incoming", "jp_city", "_backup", "stand_in_trees_2026-11-02")
TREES = os.path.join(ASSETS, "Sprites", "Trees")
PLANTS = os.path.join(ASSETS, "Sprites", "Plants")

# stand-in -> (delivered sprite, mirrored)
REPLACE = {
    "PineTall": ("JP_Tree_BlackPine_A", False),
    "LindenAutumnA": ("JP_Tree_Maple_Red_A", False),
    "LindenAutumnB": ("JP_Tree_Persimmon", False),
    "LindenAutumnC": ("JP_Tree_Maple_Red_B", True),
    "LindenAutumnD": ("JP_Tree_Persimmon", True),
    "LindenAutumnE": ("JP_Tree_Maple_Orange", False),
    "LindenAutumnF": ("JP_Tree_Maple_Orange", True),
    "BambooA": ("JP_Tree_Bamboo_Clump_Large", False),
    "BambooB": ("JP_Tree_Bamboo_Clump_B_Large", False),
    "BambooC": ("JP_Tree_Bamboo_Clump_Large", True),
}


def meta(png, name):
    if os.path.exists(png + ".hymeta") is False:
        with open(png + ".hymeta", "w", newline="\r\n") as f:
            f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
                    "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % city.stable_handle("sprite:" + name))


def main():
    names = sorted(f[:-4] for f in os.listdir(SRC) if f.endswith(".png"))
    for name in names:
        folder = PLANTS if name.startswith("JP_Shrub_") else TREES
        png = os.path.join(folder, name + ".png")
        shutil.copy2(os.path.join(SRC, name + ".png"), png)
        meta(png, name)
    os.makedirs(BACKUP, exist_ok=True)
    for old, (new, mirrored) in REPLACE.items():
        png = os.path.join(TREES, old + ".png")
        keep = os.path.join(BACKUP, old + ".png")
        if os.path.exists(keep) is False and os.path.exists(png):
            shutil.copy2(png, keep)
        # mirrored ones come ready-made from cut_trees.py (trees_r1/_mirrored): this script only copies files
        shutil.copy2(os.path.join(SRC, "_mirrored" if mirrored else "", new + ".png"), png)
    print("installed %d sprites, replaced %d stand-ins (%s)" % (len(names), len(REPLACE), ", ".join(sorted(REPLACE))))


if __name__ == "__main__":
    main()
