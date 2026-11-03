"""Delivered prop sheets -> kit-style sprites: black outline -> darker shade of the colour it borders (selective
outline), then a 2x pixel-art reduction (each 2x2 block takes its most common colour, outline pixels win ties so the
silhouette survives)."""
import sys, numpy as np
from PIL import Image
from scipy import ndimage

def selective_outline(a, dark=0.55):
    a = a.copy(); al = a[..., 3] > 127
    lum = a[..., :3].astype(float).mean(axis=2)
    black = al & (lum < 30)
    inner = al & ~black
    # nearest interior colour for each black pixel
    idx = ndimage.distance_transform_edt(~inner, return_distances=False, return_indices=True)
    src = a[idx[0], idx[1], :3].astype(float)
    a[black, :3] = np.clip(src[black] * dark, 0, 255).astype(np.uint8)
    return a, black

def reduce2(a, outline):
    H, W = a.shape[0] // 2, a.shape[1] // 2
    out = np.zeros((H, W, 4), np.uint8)
    for y in range(H):
        for x in range(W):
            blk = a[2*y:2*y+2, 2*x:2*x+2].reshape(-1, 4); ol = outline[2*y:2*y+2, 2*x:2*x+2].reshape(-1)
            op = blk[:, 3] > 127
            if op.sum() < 2:
                continue
            cand = blk[op]; w = 1 + ol[op].astype(int)        # outline pixels weigh double
            keys = [tuple(c) for c in cand]
            score = {}
            for k, ww in zip(keys, w):
                score[k] = score.get(k, 0) + ww
            out[y, x] = max(score, key=score.get)
    return out

if __name__ == "__main__":
    a = np.asarray(Image.open(sys.argv[1]).convert("RGBA"))
    b, ol = selective_outline(a)
    Image.fromarray(b).save(sys.argv[2] + "_outline.png")
    Image.fromarray(reduce2(b, ol)).save(sys.argv[2] + "_half.png")
