"""Clean PixelLab prop sprites for the field (docs/StyleGuide.md, "Pixel rules").

For each sprite: binary alpha; the art moved so a standing prop ends on the bottom row and a hanging prop starts on the
top row (canvas size unchanged); the dark outline PixelLab adds even in Lineless mode is lifted (environment sprites are
lineless: under 20 % of the silhouette's edge pixels may be darker than luma 45); optionally snapped to a palette.

Outline lift: an edge pixel darker than luma 45 takes the colour of the nearest inside pixel that is not part of the
outline, darkened one step on the shadow side (lower right) and kept as is on the lit side (upper left). Objects whose
material is dark (black iron, lacquer) keep their dark edge because their inside is dark too.

  python scripts/japanese_city/clean_prop_sprites.py --map map.json --src <folder> --out <folder> [--palette palette_48.png]
map.json: {"source.png": ["NewName", "top" | "bottom"], ...}
Needs Pillow and numpy.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path[0:0] = [str(Path(__file__).resolve().parent), str(Path(__file__).resolve().parent.parent)]


def Luma(a):
	return 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]


def EdgeMask(opaque):
	p = np.pad(opaque, 1)
	inner = p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
	return opaque & (inner == False)


def LiftOutline(a, threshold=45.0, passes=2):
	a = a.astype(np.float64)
	h, w = a.shape[:2]
	opaque = a[..., 3] > 0
	for _ in range(passes):
		edge = EdgeMask(opaque)
		dark = edge & (Luma(a) < threshold)
		source = opaque & (dark == False)
		ys, xs = np.where(dark)
		new = a.copy()
		for y, x in zip(ys, xs):
			best = None
			for r in (1, 2):
				for dy in range(-r, r + 1):
					for dx in range(-r, r + 1):
						yy, xx = y + dy, x + dx
						if 0 <= yy < h and 0 <= xx < w and source[yy, xx]:
							d = abs(dy) + abs(dx)
							if best is None or d < best[0]:
								best = (d, a[yy, xx, :3], dy, dx)
				if best is not None:
					break
			if best is None:
				continue
			colour = best[1].copy()
			lit = best[2] > 0 or best[3] > 0      # the inside lies down/right of this pixel: it is an upper-left edge
			if lit == False:
				colour *= 0.78
			new[y, x, :3] = colour
		a = new
	return np.clip(a, 0, 255).astype(np.uint8)


def DarkEdgePercent(a):
	opaque = a[..., 3] > 0
	edge = EdgeMask(opaque)
	return float((Luma(a.astype(np.float64))[edge] < 45).mean() * 100) if edge.any() else 0.0


def Main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--map", required=True)
	parser.add_argument("--src", required=True)
	parser.add_argument("--out", required=True)
	parser.add_argument("--palette")
	args = parser.parse_args()
	mapping = json.loads(Path(args.map).read_text(encoding="utf-8"))
	out = Path(args.out)
	out.mkdir(parents=True, exist_ok=True)
	snap = None
	if args.palette:
		from snap_to_palette import Snap, SrgbToOklab
		palette = np.unique(np.asarray(Image.open(args.palette).convert("RGB")).reshape(-1, 3), axis=0)
		snap = (Snap, palette, SrgbToOklab(palette))
	for source, (name, anchor) in mapping.items():
		a = np.asarray(Image.open(Path(args.src) / source).convert("RGBA")).copy()
		a[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
		rows = np.where(a[..., 3].any(axis=1))[0]
		shift = -rows[0] if anchor == "top" else a.shape[0] - 1 - rows[-1]
		a = np.roll(a, shift, axis=0)
		before = DarkEdgePercent(a)
		a = LiftOutline(a)
		target = out / (name + ".png")
		Image.fromarray(a).save(target)
		if snap:
			image = snap[0](target, snap[1], snap[2], True)[0]
			image.save(target)
			a = np.asarray(image)
		colours = len(np.unique(a[a[..., 3] > 0][:, :3], axis=0))
		print(f"{name:28s} {a.shape[1]}x{a.shape[0]} moved {shift:+d}, dark edge {before:3.0f}% -> {DarkEdgePercent(a):3.0f}%, {colours} colours")


if __name__ == "__main__":
	Main()
