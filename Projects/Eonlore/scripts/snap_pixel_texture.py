"""Turn a "fake pixel art" texture (each art pixel drawn as an N x N block, softened, often not tileable) into a clean,
tileable pixel texture for Hydro's 32 texels per metre (docs/SpriteSizes.md).

Steps: find the block size (or --period), box-downsample to one texel per art pixel, optionally scale to a feature size,
make it tile (fold an edge band over the opposite edge through a ragged mask; the middle is untouched),
then quantise to a limited palette without dithering so every texel is a clean colour again.

  python scripts/snap_pixel_texture.py in.png out.png                       (auto period, 24 colours)
  python scripts/snap_pixel_texture.py in.png out.png --period 3 --colors 20 --scale 0.5 --no-tile
  Prints the material Tiling that gives 32 texels per metre for the result (e.g. 0.0938 for a 341 px texture).

Good for organic surfaces (grass, dirt, gravel, moss). For stones, bricks and planks the tiling blend cuts through
shapes: author those tileable at the right size instead, and only use --no-tile here to snap them.
Needs Pillow and numpy.
"""

import argparse

import numpy as np
from PIL import Image


def DetectPeriod(rgb):
	"""The art-pixel block size: the first peak of the autocorrelation of the column edge strength."""
	edges = np.abs(np.diff(rgb.mean(axis=2), axis=1)).mean(axis=0)
	edges = edges - edges.mean()
	correlation = np.correlate(edges, edges, "full")[len(edges) - 1:]
	correlation /= correlation[0] + 1e-9
	for period in range(2, 32):
		if correlation[period] > correlation[period - 1] and correlation[period] >= correlation[period + 1] and correlation[period] > 0.05:
			return period
	return 1


def MakeTileable(rgb, seed, band=0.12):
	"""Overlap tiling: drop a band at the far edge and fold it over the near edge through a ragged mask, once across
	and once down. The new last column was the original's neighbour of the folded band, so the result tiles, and
	nothing changes in the middle of the texture. The result is band x smaller."""
	rng = np.random.default_rng(seed)

	def Fold(image):
		height, width, _ = image.shape
		b = max(2, int(width * band))
		kept = image[:, : width - b].copy()
		x = np.arange(b)[None, :] / b
		noise = np.kron(rng.random((height // 2 + 1, b // 2 + 1)), np.ones((2, 2)))[:height, :b]
		take_far = (x + (noise - 0.5) * 0.6) < 0.5
		kept[:, :b] = np.where(take_far[..., None], image[:, width - b:], image[:, :b])
		return kept

	rgb = Fold(rgb)
	rgb = np.transpose(Fold(np.transpose(rgb, (1, 0, 2))), (1, 0, 2))
	return rgb


def Main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("input")
	parser.add_argument("output")
	parser.add_argument("--period", type=int, default=0, help="art-pixel block size (0 = detect)")
	parser.add_argument("--scale", type=float, default=1.0, help="extra scale after snapping (0.5 = half size)")
	parser.add_argument("--colors", type=int, default=24)
	parser.add_argument("--no-tile", action="store_true")
	parser.add_argument("--seed", type=int, default=7)
	args = parser.parse_args()

	image = Image.open(args.input).convert("RGB")
	rgb = np.asarray(image).astype(np.float32)
	period = args.period or DetectPeriod(rgb)
	width = max(1, round(image.width / period * args.scale))
	height = max(1, round(image.height / period * args.scale))
	small = np.asarray(image.resize((width, height), Image.BOX)).astype(np.float32)
	if args.no_tile is False:
		small = MakeTileable(small, args.seed)
		height, width = small.shape[:2]
	result = Image.fromarray(np.clip(small, 0, 255).astype(np.uint8)).quantize(
		args.colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")
	result.save(args.output)
	print(f"{args.input}: period {period}, {width} x {height} px, {args.colors} colours -> {args.output}; "
		f"Tiling {32.0 / width:.4f} for 32 texels per metre ({width / 32.0:.2f} m repeat)")


if __name__ == "__main__":
	Main()
