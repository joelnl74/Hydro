"""The delivered Japanese trees and shrub sheets (Nov 2, PixelLab Create image Pro, 64 px/m) -> game sprites:
black outline -> a darker shade of the colour it borders, then
  out/<name>.png        halved to 32 px/m (the size the prompts asked for: a 3.6 m pine)
  out/<name>_Large.png  the full image read at 32 px/m (twice the size: a 7 m temple pine, a 7.5 m bamboo clump)
  out/JP_Shrub_*.png    the shrub sheets cut into their nine plants, halved.

    python scripts/game/japanese_city/art/cut_trees.py art_incoming/jp_city/trees_r1/_raw art_incoming/jp_city/trees_r1
Then install_jp_trees.py puts them in the game's sprite folders."""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
from scipy import ndimage
from fix_sheet import selective_outline, reduce2

RAW = sys.argv[1] if len(sys.argv) > 1 else "raw"
TREES = ["JP_Tree_BlackPine_A", "JP_Tree_BlackPine_B", "JP_Tree_BlackPine_Small", "JP_Tree_Maple_Red_A", "JP_Tree_Maple_Red_B",
         "JP_Tree_Maple_Orange", "JP_Tree_Bamboo_Clump", "JP_Tree_Bamboo_Clump_B", "JP_Tree_Persimmon",
         "JP_Tree_Sugi_A", "JP_Tree_Sugi_B"]
# round 2 (Nov 2): the young trees came as 2 x 2 sheets of four variants each
GRIDS = {"JP_Sheet_Maple_Young": ("JP_Tree_Maple_Young", "ABCD"), "JP_Sheet_BlackPine_Young": ("JP_Tree_BlackPine_Young", "ABCD"),
         "JP_Sheet_Bamboo_Young": ("JP_Tree_Bamboo_Young", "ABCD"), "JP_Sheet_Bamboo_Young_V2": ("JP_Tree_Bamboo_Young", "EFGH")}
SHRUBS = ["Azalea_A", "Azalea_B", "Camellia", "Hedge", "Sasa", "Nandina", "Fern", "MossStone", "Mondo"]
SHEETS = {"JP_Sheet_Shrubs": "", "JP_Sheet_Shrubs_V2": ""}
SHEETS["JP_Sheet_Shrubs_V2"] = "_V2"                # the second delivery's plants as variants
OUT = sys.argv[2] if len(sys.argv) > 2 else "out"
os.makedirs(OUT, exist_ok=True)
info = {}


def crop_even(img, m):
    """The object's box made even and bottom-aligned, so the 2x2 blocks don't split its foot."""
    ys, xs = np.where(m)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    h = (y1 - y0 + 1) // 2 * 2
    w = (x1 - x0 + 1) // 2 * 2
    y0 = y1 - h
    x0 = x0 - (w - (x1 - x0)) // 2
    pad = np.zeros((img.shape[0] + 4, img.shape[1] + 4) + img.shape[2:], img.dtype)
    pad[2:-2, 2:-2] = img
    return pad[y0 + 2:y1 + 2, x0 + 2:x0 + w + 2]


def save(name, arr):
    Image.fromarray(arr).save(os.path.join(OUT, name + ".png"))
    info[name] = dict(px=[arr.shape[1], arr.shape[0]], metres=[round(arr.shape[1] / 32, 2), round(arr.shape[0] / 32, 2)])


for name in TREES:
    a = np.asarray(Image.open(os.path.join(RAW, name + ".png")).convert("RGBA"))
    b, ol = selective_outline(a)
    m = b[..., 3] > 127
    full, olc = crop_even(b, m), crop_even(ol.astype(np.uint8), m).astype(bool)
    full[full[..., 3] <= 127] = 0
    save(name + "_Large", full)
    half = reduce2(full, olc)
    nz = np.where(half[..., 3] > 0)
    save(name, half[:nz[0].max() + 1, nz[1].min():nz[1].max() + 1])

def cells(b, n):
    """The sheet's objects grouped by the cell of an n x n grid their middle falls in (stray leaves join their tree)."""
    al = b[..., 3] > 127
    lab, count = ndimage.label(ndimage.binary_dilation(al, iterations=2))
    cw, ch = b.shape[1] / n, b.shape[0] / n
    out = {}
    for i in range(1, count + 1):
        mm = (lab == i) & al
        if mm.sum() < 12:
            continue
        ys, xs = np.where(mm)
        c = int(xs.mean() // cw) + n * int(ys.mean() // ch)
        out.setdefault(c, np.zeros_like(al))
        out[c] |= mm
    return out


for sheet, (base, letters) in GRIDS.items():
    path = os.path.join(RAW, sheet + ".png")
    if os.path.exists(path) is False:
        continue
    a = np.asarray(Image.open(path).convert("RGBA"))
    b, ol = selective_outline(a)
    for c, mm in sorted(cells(b, 2).items()):
        name = base + "_" + letters[c]
        full = crop_even(np.where(mm[..., None], b, 0).astype(np.uint8), mm)
        olc = crop_even((ol & mm).astype(np.uint8), mm).astype(bool)
        save(name + "_Large", full)
        half = reduce2(full, olc)
        nz = np.where(half[..., 3] > 0)
        save(name, half[:nz[0].max() + 1, nz[1].min():nz[1].max() + 1])

for sheet, suffix in SHEETS.items():
    a = np.asarray(Image.open(os.path.join(RAW, sheet + ".png")).convert("RGBA"))
    b, ol = selective_outline(a)
    al = b[..., 3] > 127
    lab, n = ndimage.label(ndimage.binary_dilation(al, iterations=2))
    cw, ch = a.shape[1] / 3, a.shape[0] / 3
    cells = {}
    for i in range(1, n + 1):
        mm = (lab == i) & al
        if mm.sum() < 12:
            continue
        ys, xs = np.where(mm)
        c = int(xs.mean() // cw) + 3 * int(ys.mean() // ch)
        cells.setdefault(c, np.zeros_like(al))
        cells[c] |= mm
    for c, plant in enumerate(SHRUBS):
        mm = cells[c]
        full = crop_even(np.where(mm[..., None], b, 0).astype(np.uint8), mm)
        olc = crop_even((ol & mm).astype(np.uint8), mm).astype(bool)
        half = reduce2(full, olc)
        nz = np.where(half[..., 3] > 0)
        save("JP_Shrub_" + plant + suffix, half[:nz[0].max() + 1, nz[1].min():nz[1].max() + 1])

# mirrored copies for install_jp_trees.py (which only copies files: no Pillow in Blender's Python)
os.makedirs(os.path.join(OUT, "_mirrored"), exist_ok=True)
for name in ("JP_Tree_Maple_Red_B", "JP_Tree_Persimmon", "JP_Tree_Maple_Orange", "JP_Tree_Bamboo_Clump_Large"):
    Image.open(os.path.join(OUT, name + ".png")).transpose(Image.FLIP_LEFT_RIGHT).save(os.path.join(OUT, "_mirrored", name + ".png"))
json.dump(info, open(os.path.join(OUT, "sizes.json"), "w"), indent=1)
for k, v in info.items():
    print("%-30s %-10s %s m" % (k, v["px"], v["metres"]))
