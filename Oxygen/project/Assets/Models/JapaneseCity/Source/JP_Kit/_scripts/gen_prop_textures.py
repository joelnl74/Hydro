"""Base textures for the Japanese city kit's 3D props (evenly lit albedo, 32 texels per metre, seamless).
Placeholders in the kit's pixel style; replaced in place by PixelLab versions under the same names.
  python gen_prop_textures.py <out_dir>
"""
import os
import sys

import numpy as np
from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else "out"
os.makedirs(OUT, exist_ok=True)


def hx(h):
    return np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)], dtype=np.uint8)


def tnoise(n, cells, rng):
    g = rng.random((cells, cells))
    xs = np.arange(n) * cells / n
    i0 = np.floor(xs).astype(int); f = xs - i0; f = f * f * (3 - 2 * f); i1 = (i0 + 1) % cells
    a = g[np.ix_(i0, i0)]; b = g[np.ix_(i0, i1)]; c = g[np.ix_(i1, i0)]; d = g[np.ix_(i1, i1)]
    fy = f[:, None]; fx = f[None, :]
    return a * (1 - fy) * (1 - fx) + b * (1 - fy) * fx + c * fy * (1 - fx) + d * fy * fx


def save(name, img):
    Image.fromarray(img.astype(np.uint8), "RGB").save(os.path.join(OUT, name + ".png"))


def staves(n=64, seed=21):
    """Barrel / bucket / tub staves: vertical boards 6 px wide, warm cedar, lit left edge, dark joint."""
    rng = np.random.default_rng(seed)
    R = [hx("#3b2a20"), hx("#5a3f2c"), hx("#6e4e35"), hx("#82603f"), hx("#9a754d")]
    img = np.zeros((n, n, 3), np.uint8)
    w = 6
    x = 0
    while x < n:
        ww = min(w + (1 if rng.random() < 0.3 else 0), n - x)
        base = 1 + rng.integers(3)
        img[:, x:x + ww] = R[base]
        img[:, x] = R[min(base + 1, 4)]
        img[:, x + ww - 1] = R[0]
        gx = x + 2 + rng.integers(max(1, ww - 3))
        for y in range(n):
            if rng.random() < 0.7:
                img[y, min(gx, x + ww - 2)] = R[max(base - 1, 1)]
        x += ww
    return img


def clay(n=64, seed=22):
    """Ochre clay with soot patches (kamado, small walls of stoves)."""
    rng = np.random.default_rng(seed)
    R = [hx("#4a3426"), hx("#7a5634"), hx("#946a40"), hx("#a97c4c"), hx("#bf9260")]
    v = tnoise(n, 6, rng) * 0.7 + tnoise(n, 16, rng) * 0.3
    idx = np.clip((v * 4.2).astype(int), 1, 4)
    img = np.array(R)[idx]
    soot = tnoise(n, 4, rng) > 0.72
    img[soot] = R[0]
    for _ in range(14):                        # straw flecks
        y, x = rng.integers(n), rng.integers(n)
        img[y, x:x + 2] = R[4]
    return img


def glaze(n=64, seed=23):
    """Dark brown glazed stoneware with a lighter drip band (water jar, sake jars)."""
    rng = np.random.default_rng(seed)
    R = [hx("#22160f"), hx("#3a2517"), hx("#4f3220"), hx("#6a452a"), hx("#8a6a44")]
    v = tnoise(n, 5, rng)
    idx = np.clip((v * 3.4).astype(int), 1, 3)
    img = np.array(R)[idx]
    for x in range(n):                          # drips running down from the top band
        L = 4 + int(tnoise(n, 8, rng)[0, x] * 10)
        img[:L, x] = R[4] if x % 9 else R[3]
    return img


def cloth_red(n=64, seed=24):
    """Muted red felt (tea-house bench mat): soft weave, never vermilion-bright."""
    rng = np.random.default_rng(seed)
    R = [hx("#4a1c1a"), hx("#6b2a24"), hx("#80342b"), hx("#934132")]
    v = tnoise(n, 8, rng)
    idx = np.clip((v * 3.0).astype(int) + 1, 1, 3)
    img = np.array(R)[idx]
    img[::4, :] = np.where(rng.random((n // 4, n, 1)) < 0.5, R[1], img[::4, :])
    return img


def hide(n=32, seed=25):
    """Drum hide: cream skin with a darker rim tone and a few stains."""
    rng = np.random.default_rng(seed)
    R = [hx("#8a7656"), hx("#b39e78"), hx("#c9b58e"), hx("#dccaa2")]
    v = tnoise(n, 4, rng)
    idx = np.clip((v * 3.2).astype(int) + 1, 1, 3)
    img = np.array(R)[idx]
    return img


def flat(hexcol, n=4):
    img = np.zeros((n, n, 3), np.uint8); img[:] = hx(hexcol); return img


if __name__ == "__main__":
    save("JP_Wood_Staves", staves())
    save("JP_Clay_Ochre", clay())
    save("JP_Glaze_Brown", glaze())
    save("JP_Cloth_Red", cloth_red())
    save("JP_Drum_Hide", hide())
    for name, col in (("JP_Mat_Iron", "#2c2e33"), ("JP_Mat_Bronze", "#a07a3c"), ("JP_Mat_Rope", "#8a7352"), ("JP_Mat_Glass", "#7f9a96")):
        save(name, flat(col))
    print("ok")
