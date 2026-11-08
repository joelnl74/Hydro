"""EONLORE Medieval Village kit - hedge textures (Oct 30, 2026).

Placeholder pixel art for MV_Hedge (build_hedge.py) until the PixelLab versions arrive (same file names and sizes;
prompts in the project doc FARM_REGEN_PROMPTS.md). Same rules as textures_hd2d.py: 32 texels per metre, seamless,
lit from the upper left, no outlines, leaf clumps 4-8 px, colours from art_incoming/style_refs/palette_foliage_32.png.

  MV_Hedge_Top.png   64 x 64   2 x 2 m, the clipped top seen from above (lighter, warm olive tips)
  MV_Hedge_Side.png  64 x 64   2 x 2 m, the clipped sides (deeper greens, dark gaps between clumps)
  MV_Hedge_Leaf.png  32 x 16   1 x 0.5 m cut-out card: ragged leaf clumps standing up out of the top (binary alpha)

Run: python textures_hedge.py <outdir>      (numpy + Pillow; imports textures_hd2d.py from this folder)
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from textures_hd2d import hexramp, fbm, paint  # noqa: E402

# dark blue-green shadow -> olive -> warm light tip (palette_foliage_32; no lemon)
HEDGE = hexramp('#0e2424', '#143d33', '#154930', '#19542c', '#316426', '#436c23', '#5b7c22', '#809022', '#9b9737')


def stamp_clumps(idx, seed, layers, count, rx, ry, wrap=True, mask=None):
    """Overlapping leaf clumps, back to front. Each clump is a jagged blob lit on its upper left (+1..+2 steps) and
    shaded on its lower right (-1); later layers sit in front and start one step lighter. In place on idx."""
    h, w = idx.shape
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]
    for layer, base in enumerate(layers):
        for _ in range(count):
            cx, cy = r.uniform(0, w), r.uniform(0, h)
            ax, ay = r.uniform(*rx), r.uniform(*ry)
            dx = xx - cx; dy = yy - cy
            if wrap:
                dx = (dx + w / 2) % w - w / 2; dy = (dy + h / 2) % h - h / 2
            d = (dx / ax) ** 2 + (dy / ay) ** 2
            jag = r.random((h, w)) * 0.45
            blob = d < 1.0 - jag * (d > 0.45)
            if mask is not None:
                blob &= mask
            light = -(dx / ax + dy / ay)                                 # +: upper left
            shade = np.where(light > 0.9, 2, np.where(light > 0.15, 1, np.where(light < -0.9, -1, 0)))
            idx[blob] = base + shade[blob]
    return idx


def leaves(w, h, seed, lo, hi):
    """Hedge foliage: a dark interior with three layers of leaf clumps (4-8 px) on top, plus a few lit leaf tips."""
    r = np.random.default_rng(seed)
    idx = np.full((h, w), float(lo - 1))
    stamp_clumps(idx, seed, layers=(lo, lo + 1, lo + 2), count=w * h // 40, rx=(2.5, 4.0), ry=(2.0, 3.2))
    tips = (r.random((h, w)) > 0.965) & (idx >= lo + 2)
    idx = np.where(tips, idx + 1, idx)
    big = fbm(w, h, [32, 16], seed + 1)                                   # broad light/dark drift across the hedge
    idx += np.round((big - 0.5) * 1.6)
    return np.clip(idx, 0, hi)


def hedge_top(seed=51):
    idx = leaves(64, 64, seed, 4, 7)
    return paint(idx, HEDGE)


def hedge_side(seed=52):
    idx = leaves(64, 64, seed, 3, 7)
    return paint(idx, HEDGE)


def hedge_leaf(seed=53, w=32, h=16):
    """Cut-out card: a ragged top line of clumps; solid below it so it sinks into the hedge body."""
    r = np.random.default_rng(seed)
    idx = leaves(w, h, seed, 4, 7)
    # silhouette: a few rounded bumps of different heights
    col = np.full(w, h - 5.0)
    for _ in range(4):
        c = r.integers(2, w - 2); rad = r.integers(3, 6); top = r.integers(1, 7)
        xs = np.arange(w)
        bump = top + (xs - c) ** 2 / (rad * 1.2)
        col = np.minimum(col, np.where(np.abs(xs - c) <= rad + 1, bump, col))
    col = np.round(col).astype(int)
    yy = np.mgrid[0:h, 0:w][0]
    alpha = yy >= col[None, :]
    # ragged edge: knock out or add single pixels along the silhouette
    edge = (yy == col[None, :])
    noise = r.random((h, w))
    alpha = np.where(edge & (noise < 0.3), False, alpha)
    above = (yy == col[None, :] - 1) & (noise < 0.25)
    alpha |= above
    # the top row of each bump is lit, the lower-right side darker
    top = alpha & ~np.roll(alpha, 1, 0)
    idx = np.where(top, np.maximum(idx, 7), idx)
    idx = np.maximum(idx, 5)
    rgb = paint(idx, HEDGE)
    a = (alpha * 255).astype(np.uint8)
    return np.dstack([rgb, a])


def build():
    return {'MV_Hedge_Top.png': hedge_top(), 'MV_Hedge_Side.png': hedge_side(), 'MV_Hedge_Leaf.png': hedge_leaf()}


def main(out):
    os.makedirs(out, exist_ok=True)
    for k, v in build().items():
        mode = 'RGBA' if v.shape[2] == 4 else 'RGB'
        Image.fromarray(v.astype(np.uint8), mode).save(os.path.join(out, k))
        print('%-22s %3d x %-3d' % (k, v.shape[1], v.shape[0]))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'out_hedge')
