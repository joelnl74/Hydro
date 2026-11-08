"""Snap finished pixel art (textures, decals, sprites) to a fixed palette, e.g. art_incoming/style_refs/palette_48.png.

Unlike scripts/snap_pixel_texture.py (which detects blocks, re-tiles and quantises each image to its own colours), this
changes nothing but the colours: every opaque pixel becomes the nearest palette colour in OKLab (hue weighted double so a
shade never jumps to a grey of another hue), alpha becomes binary (>= 128 opaque), size, seams and layout stay.

Guard: two different colours of one image that would land on the same palette colour are a ramp losing a step. The
script reports how many distinct colours each image had and has after; with --keep-ramps a lost step is put back as the
next-nearest palette colour that is still free in that image, so a 5-shade ramp stays 5 shades.

  python scripts/snap_to_palette.py --palette art_incoming/style_refs/palette_48.png --out <folder> <png or folder> ...
Needs Pillow and numpy.
"""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

W = np.array([1.0, 2.0, 2.0])


def SrgbToOklab(rgb):
	c = np.asarray(rgb, dtype=np.float64) / 255.0
	c = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
	l = 0.4122214708 * c[..., 0] + 0.5363325363 * c[..., 1] + 0.0514459929 * c[..., 2]
	m = 0.2119034982 * c[..., 0] + 0.6806995451 * c[..., 1] + 0.1073969566 * c[..., 2]
	s = 0.0883024619 * c[..., 0] + 0.2817188376 * c[..., 1] + 0.6299787005 * c[..., 2]
	l, m, s = np.cbrt(l), np.cbrt(m), np.cbrt(s)
	return np.stack([
		0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
		1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
		0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s], axis=-1)


def Snap(path, palette, paletteLab, keepRamps):
	image = np.asarray(Image.open(path).convert("RGBA")).copy()
	opaque = image[..., 3] >= 128
	colours, inverse = np.unique(image[..., :3][opaque], axis=0, return_inverse=True)
	lab = SrgbToOklab(colours)
	d = ((((lab[:, None, :] - paletteLab[None, :, :]) * W) ** 2)).sum(axis=2)
	choice = d.argmin(axis=1)
	if keepRamps:
		# most-used colours claim their nearest palette colour first; a later colour whose nearest is taken by a
		# clearly different colour (a separate ramp step) gets its next-nearest free one, if that is still close
		counts = np.bincount(inverse.ravel(), minlength=len(colours))
		taken = {}
		for i in np.argsort(-counts):
			ranked = np.argsort(d[i])
			pick = ranked[0]
			if pick in taken and np.linalg.norm((lab[i] - lab[taken[pick]]) * W) > 0.012:
				for alt in ranked[1:4]:
					if alt not in taken and d[i, alt] < 6.0 * d[i, ranked[0]] + 0.004:
						pick = alt
						break
			choice[i] = pick
			taken.setdefault(pick, i)
	out = image.copy()
	snapped = palette[choice][inverse.ravel()]
	out[..., :3][opaque] = snapped
	out[..., 3] = np.where(opaque, 255, 0)
	out[..., :3][opaque == False] = 0
	shift = np.sqrt(((((lab - paletteLab[choice]) * W) ** 2)).sum(axis=1))
	return Image.fromarray(out), len(colours), len(set(choice.tolist())), float(np.average(shift, weights=np.bincount(inverse.ravel())))


def Main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("inputs", nargs="+")
	parser.add_argument("--palette", required=True)
	parser.add_argument("--out", required=True)
	parser.add_argument("--keep-ramps", action="store_true")
	args = parser.parse_args()
	palette = np.asarray(Image.open(args.palette).convert("RGB")).reshape(-1, 3)
	palette = np.unique(palette, axis=0)
	paletteLab = SrgbToOklab(palette)
	files = []
	for i in args.inputs:
		p = Path(i)
		files += sorted(p.glob("*.png")) if p.is_dir() else [p]
	Path(args.out).mkdir(parents=True, exist_ok=True)
	for f in files:
		image, before, after, shift = Snap(f, palette, paletteLab, args.keep_ramps)
		image.save(Path(args.out) / f.name)
		print(f"{f.name:34s} {before:3d} -> {after:2d} colours, mean shift {shift:.3f}")


if __name__ == "__main__":
	Main()
