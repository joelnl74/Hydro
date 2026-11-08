"""Rebuild art_incoming/style_refs/palette_48.png from the approved art.

The 48-colour palette that PixelLab's "Limit colors" and the snapping step use was lost. This rebuilds it from art that
is already approved, so new art lands in the same colour space:

  - Textures/Palettes/ShrineGlade_Palette.png (the shrine's 96 colours: greens, earth, Miko's red)
  - the medieval kit's textures (Models/MedievalVillage/Source/MV_Kit/Textures)
  - Miko's field sheet and the OakSun trees
  - the Japanese city kit's textures (blue-grey tiles, cedar, plaster, granite, indigo)

Every source gets the same total weight (so a big texture can't drown Miko's few colours), colours are clustered in
OKLab with k-means (k = 48), and the darkest and lightest colour of every source are kept as fixed centres so no
ramp loses its ends. The result is sorted into hue families, dark to light, one row each, for reading as ramps.

  python scripts/build_palette_48.py --out art_incoming/style_refs/palette_48.png --sources <png or folder> ...
Needs Pillow and numpy.
"""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image


def SrgbToOklab(rgb):
	c = rgb / 255.0
	c = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
	l = 0.4122214708 * c[:, 0] + 0.5363325363 * c[:, 1] + 0.0514459929 * c[:, 2]
	m = 0.2119034982 * c[:, 0] + 0.6806995451 * c[:, 1] + 0.1073969566 * c[:, 2]
	s = 0.0883024619 * c[:, 0] + 0.2817188376 * c[:, 1] + 0.6299787005 * c[:, 2]
	l, m, s = np.cbrt(l), np.cbrt(m), np.cbrt(s)
	return np.stack([
		0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
		1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
		0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s], axis=1)


def SourceColours(path):
	"""Unique opaque colours of one image and their pixel counts."""
	image = np.asarray(Image.open(path).convert("RGBA")).reshape(-1, 4)
	image = image[image[:, 3] > 127][:, :3]
	if len(image) == 0:
		return None, None
	colours, counts = np.unique(image, axis=0, return_counts=True)
	return colours.astype(np.float64), counts.astype(np.float64)


def Main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--out", required=True)
	parser.add_argument("--sources", nargs="+", required=True, help="png files or folders of png files")
	parser.add_argument("--accents", nargs="*", default=[], help="images whose most saturated colours must survive (Miko)")
	parser.add_argument("--exclude", nargs="*", default=[], help="file name parts to skip (emissive masks)")
	parser.add_argument("--colors", type=int, default=48)
	parser.add_argument("--seed", type=int, default=3)
	args = parser.parse_args()

	files = []
	for source in args.sources:
		p = Path(source)
		files += sorted(p.glob("*.png")) if p.is_dir() else [p]
	files = [f for f in files if any(x in f.name for x in args.exclude) == False]
	allColours, allWeights = [], []
	for f in files:
		colours, counts = SourceColours(f)
		if colours is None:
			continue
		allColours.append(colours)
		allWeights.append(np.sqrt(counts) / np.sqrt(counts).sum())   # every source weighs the same in total
	colours = np.concatenate(allColours)
	weights = np.concatenate(allWeights)
	lab = SrgbToOklab(colours)
	keep = lab[:, 0] > 0.17          # StyleGuide: no pure black in base art; the darkest shade is a coloured dark
	colours, weights, lab = colours[keep], weights[keep], lab[keep]
	W = np.array([1.0, 2.0, 2.0])    # hue counts double: ramps of different hues must not merge into greys

	# fixed centres: the ends of every ramp. Per hue family (neutrals + eight 45-degree hue sectors) with real weight,
	# the darkest and the lightest colour that carry at least a little weight (stray pixels don't count).
	chroma = np.hypot(lab[:, 1], lab[:, 2])
	hue = (np.degrees(np.arctan2(lab[:, 2], lab[:, 1])) + 360) % 360
	fam = np.where(chroma < 0.03, -1, (hue // 45).astype(int))
	fixedLab = []
	for f in sorted(set(fam)):
		members = np.where(fam == f)[0]
		if weights[members].sum() < 0.02 * weights.sum():
			continue
		heavy = members[weights[members] >= np.percentile(weights[members], 40)]
		for i in (heavy[np.argmin(lab[heavy, 0])], heavy[np.argmax(lab[heavy, 0])]):
			if all(np.linalg.norm(lab[i] - x) > 0.05 for x in fixedLab):
				fixedLab.append(lab[i])
	for a in args.accents:   # accents: a character's saturated colours (Miko's red) are kept as they are
		ac, cnt = SourceColours(a)
		al = SrgbToOklab(ac)
		ch = np.hypot(al[:, 1], al[:, 2])
		for i in np.argsort(-ch * np.sqrt(cnt))[:4]:
			if al[i, 0] > 0.17 and all(np.linalg.norm(al[i] - x) > 0.05 for x in fixedLab):
				fixedLab.append(al[i])
	fixedLab = np.array(fixedLab)
	free = args.colors - len(fixedLab)
	assert free > 8, "too many fixed ramp ends (%d)" % len(fixedLab)

	rng = np.random.default_rng(args.seed)
	centres = lab[rng.choice(len(lab), free, replace=False, p=weights / weights.sum())]
	for _ in range(60):
		allCentres = np.concatenate([fixedLab, centres])
		d = ((((lab[:, None, :] - allCentres[None, :, :]) * W) ** 2)).sum(axis=2)
		nearest = d.argmin(axis=1)
		for i in range(free):
			members = nearest == len(fixedLab) + i
			if members.any():
				w = weights[members]
				centres[i] = (lab[members] * w[:, None]).sum(axis=0) / w.sum()
	# each free centre becomes the real source colour closest to it (no invented in-between colours)
	allCentres = np.concatenate([fixedLab, centres])
	palette = []
	for c in allCentres:
		palette.append(colours[(((lab - c) * W) ** 2).sum(axis=1).argmin()])
	palette = np.unique(np.array(palette).astype(np.uint8), axis=0)

	# sort: neutrals first, then by hue; each family dark to light
	pl = SrgbToOklab(palette.astype(np.float64))
	chroma = np.hypot(pl[:, 1], pl[:, 2])
	hue = (np.degrees(np.arctan2(pl[:, 2], pl[:, 1])) + 360) % 360
	family = np.where(chroma < 0.03, -1, (hue // 45).astype(int))
	order = np.lexsort((pl[:, 0], family))
	palette = palette[order]
	families = family[order]

	Image.fromarray(palette[None, :, :]).save(args.out)
	rows = [palette[families == f] for f in sorted(set(families))]
	swatch = Image.new("RGB", (max(len(r) for r in rows) * 24, len(rows) * 24), (40, 40, 40))
	for y, row in enumerate(rows):
		for x, c in enumerate(row):
			swatch.paste(tuple(int(v) for v in c), (x * 24, y * 24, x * 24 + 22, y * 24 + 22))
	swatchPath = Path(args.out).with_name(Path(args.out).stem + "_swatches.png")
	swatch.save(swatchPath)
	print(f"{len(files)} sources, {len(fixedLab)} fixed ramp ends -> {len(palette)} colours: {args.out}, {swatchPath}")


if __name__ == "__main__":
	Main()
