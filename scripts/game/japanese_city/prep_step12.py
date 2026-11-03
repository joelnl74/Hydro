"""Prepares the step 1 / 2 deliverables (art_incoming/jp_city/step1, step2) for the city slice.

Decals are drawn on Hydro Box shapes, whose top face takes its UVs from the box's *local* corners (centred on the
box, MeshFactory::AddQuad). So every decal image is rolled by half its size here: then a box exactly the decal's
size shows the image centred and whole. Wrap is Repeat, so strips stay seamless.

    python prep_step12.py <deliverables_dir> <setts_clean.png> <out_dir>
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

SRC, CLEAN, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
S1, S2 = os.path.join(SRC, "Step 1"), os.path.join(SRC, "Step 2")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(11)


def load(path):
    return np.array(Image.open(path).convert("RGBA"))


def save(name, a, roll=True):
    a = a.copy()
    a[a[..., 3] < 128] = 0
    a[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
    if roll:
        a = np.roll(a, (a.shape[0] // 2, a.shape[1] // 2), (0, 1))
    Image.fromarray(a.astype(np.uint8), "RGBA").save(os.path.join(OUT, name + ".png"))


def tnoise(n, cells, seed):
    r = np.random.default_rng(seed)
    g = r.random((cells, cells))
    xs = np.arange(n) * cells / n
    i0 = np.floor(xs).astype(int); f = xs - i0; f = f * f * (3 - 2 * f); i1 = (i0 + 1) % cells
    a = g[np.ix_(i0, i0)]; b = g[np.ix_(i0, i1)]; c = g[np.ix_(i1, i0)]; d = g[np.ix_(i1, i1)]
    fy = f[:, None]; fx = f[None, :]
    return a * (1 - fy) * (1 - fx) + b * (1 - fy) * fx + c * fy * (1 - fx) + d * fy * fx


# ---- worn setts: three patches, whole stones only, each fading out towards the patch edge
worn = load(os.path.join(S1, "JP_Paving_Setts_Worn.png"))
lum = worn[..., :3].astype(int).sum(2)
joint = lum <= np.percentile(lum, 9)
labels, count = ndimage.label(~joint)
n = worn.shape[0]
yy, xx = np.mgrid[0:n, 0:n]
for k in range(3):
    centre = np.array([n / 2, n / 2]) + rng.normal(0, 8, 2)
    r = np.sqrt(((yy - centre[0]) / (n * 0.42)) ** 2 + ((xx - centre[1]) / (n * 0.46)) ** 2)
    field = (1 - r) + 0.35 * (tnoise(n, 5, 30 + k) - 0.5)
    keep = np.zeros(count + 1, bool)
    for lab in range(1, count + 1):
        m = labels == lab
        keep[lab] = field[m].mean() > 0.18
    mask = keep[labels]
    grown = ndimage.binary_dilation(mask) & joint          # the joints around kept stones come along
    a = worn.copy()
    a[..., 3] = np.where(mask | grown, 255, 0)
    save("JP_Decal_SettsWorn_%d" % (k + 1), a)

# ---- wall grime: key out the baked-in transparency checker (neutral light greys) at the top
g = load(os.path.join(S1, "JP_Decal_WallGrime_0002.png")).astype(int)
neutral = (np.abs(g[..., 0] - g[..., 2]) < 14) & (np.abs(g[..., 0] - g[..., 1]) < 14) & (g[..., :3].sum(2) > 300)
g[neutral, 3] = 0
# whatever is left above the band's ragged top in a column is speckle: clear rows above the first solid run
for x in range(g.shape[1]):
    col = g[:, x, 3] > 0
    run = 0
    for y in range(g.shape[0]):
        run = run + 1 if col[y] else 0
        if run >= 3:
            g[: y - 2, x, 3] = 0
            break
save("JP_Decal_WallGrime", g.astype(np.uint8))

# ---- moss edge: as delivered
save("JP_Decal_MossEdge", load(os.path.join(S1, "JP_Decal_MossEdge.png")))


# ---- leaves and petals: thinned out to about a third, whole leaves only
def thin(path, name, keep_frac, seed):
    a = load(path)
    m = a[..., 3] > 127
    lab, c = ndimage.label(m)
    r = np.random.default_rng(seed)
    keep = np.concatenate([[False], r.random(c) < keep_frac])
    a[~keep[lab]] = 0
    save(name, a)


thin(os.path.join(S1, "JP_Decal_MapleLeaves_0002.png"), "JP_Decal_MapleLeaves", 0.3, 5)
thin(os.path.join(S1, "JP_Decal_SakuraLeaves.png"), "JP_Decal_Petals", 0.35, 6)

# ---- dirt patch and puddle: the delivered ones need a redo (see the report); the code bases stand in
BASES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bases")
for name in ("JP_Decal_DirtPatch", "JP_Decal_Puddle"):
    save(name, load(os.path.join(BASES, name + ".png")))


# ---- backdrops: halved with nearest-neighbour (chunkier pixels), cropped to their content, alpha cut
def backdrop(src, name, crop=True):
    im = Image.open(os.path.join(S2, src)).convert("RGBA")
    im = im.resize((im.width // 2, im.height // 2), Image.NEAREST)
    a = np.array(im)
    if crop:
        rows = np.where((a[..., 3] > 127).any(1))[0]
        a = a[: rows.max() + 1]
    a[..., 3] = np.where(a[..., 3] > 127, 255, 0)
    a = np.roll(a, a.shape[1] // 2, 1)       # box faces take local (centred) UVs: see the top of this file
    Image.fromarray(a, "RGBA").save(os.path.join(OUT, name + ".png"))
    return a.shape


shapes = {
    "JP_Backdrop_Sky_Dusk": backdrop("JP_Backdrop_Sky_Dusk_2.png", "JP_Backdrop_Sky_Dusk", crop=False),
    "JP_Backdrop_Far": backdrop("JP_Backdrop_Far.png", "JP_Backdrop_Far"),
    "JP_Backdrop_Mid": backdrop("JP_Backdrop_Mid.png", "JP_Backdrop_Mid"),
}
# ---- foreground branches: halved, cropped to the branch
for src, name in (("JP_FG_CherryBranch_1.png", "JP_FG_CherryBranch"), ("JP_FG_MapleBranch_2.png", "JP_FG_MapleBranch")):
    im = Image.open(os.path.join(S2, src)).convert("RGBA")
    im = im.resize((im.width // 2, im.height // 2), Image.NEAREST)
    a = np.array(im)
    ys, xs = np.where(a[..., 3] > 127)
    a = a[: ys.max() + 1, : xs.max() + 1]
    a[..., 3] = np.where(a[..., 3] > 127, 255, 0)
    Image.fromarray(a, "RGBA").save(os.path.join(OUT, name + ".png"))
    shapes[name] = a.shape
print(shapes)
