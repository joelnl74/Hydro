#!/usr/bin/env python3
"""Compare two view captures (PNG) pixel by pixel.

Usage:
    python scripts/compare_captures.py before.png after.png [--out diff.png] [--tolerance N]

Prints the number of pixels that differ by more than the tolerance (0..255 per channel, default 0),
the largest channel difference and where it is. With --out, writes a diff image: unchanged pixels
dimmed to grey, changed pixels red with brightness by how much they changed.
Exit code: 0 when the images match, 1 when they differ, 2 on an error (missing file, other size).

Captures come from the editor's Capture menu (F12 / Shift+F12) and are saved in
<project>/.hydro/captures. Needs Pillow: pip install pillow
"""

import argparse
import sys

try:
    from PIL import Image, ImageChops
except ImportError:
    print("compare_captures.py needs Pillow: pip install pillow", file=sys.stderr)
    sys.exit(2)


def load(path):
    try:
        return Image.open(path).convert("RGB")
    except OSError as error:
        print(f"Can't open {path}: {error}", file=sys.stderr)
        sys.exit(2)


def main():
    parser = argparse.ArgumentParser(description="Compare two view captures.")
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--out", help="write a diff image here")
    parser.add_argument("--tolerance", type=int, default=0, help="largest channel difference still counted as equal (0-255)")
    args = parser.parse_args()

    before = load(args.before)
    after = load(args.after)
    if before.size != after.size:
        print(f"Different sizes: {before.size[0]} x {before.size[1]} and {after.size[0]} x {after.size[1]}")
        sys.exit(2)

    width, height = before.size
    difference = ImageChops.difference(before, after)
    # Largest channel difference per pixel.
    channels = difference.split()
    largest = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
    values = largest.tobytes()  # one byte per pixel (mode L), row by row

    changed = 0
    worst = 0
    worst_at = (0, 0)
    for index, value in enumerate(values):
        if value > args.tolerance:
            changed += 1
        if value > worst:
            worst = value
            worst_at = (index % width, index // width)

    total = width * height
    print(f"{width} x {height}: {changed} of {total} pixels differ ({100.0 * changed / total:.3f} %), "
          f"largest difference {worst}" + (f" at {worst_at}" if worst > 0 else ""))

    if args.out:
        grey = before.convert("L").point(lambda v: v // 3).convert("RGB")
        mask = largest.point(lambda v: 255 if v > args.tolerance else 0)
        red = Image.merge("RGB", (largest.point(lambda v: 0 if v == 0 else min(255, 96 + v * 2)),
                                  Image.new("L", (width, height), 0), Image.new("L", (width, height), 0)))
        Image.composite(red, grey, mask).save(args.out)
        print(f"Diff image: {args.out}")

    sys.exit(0 if changed == 0 else 1)


if __name__ == "__main__":
    main()
