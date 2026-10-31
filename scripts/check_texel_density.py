"""Texel density check for Hydro scenes (the world density contract in docs/SpriteSizes.md).

Every surface in a field scene is drawn at one density: 32 texels per metre (one texture pixel = 1/32 m),
within a tolerance (default +-20 %, so 25.6..38.4). This script measures what a scene actually draws and lists
everything outside it, per texture, so mismatched assets are found before they are seen in a screenshot.

What it measures:
  - Sprites (SpriteRendererComponent): the sprite's pixel height divided by its world height. The renderer draws
    a sprite as a 1 x 1 m quad times the transform's scale; PixelsPerUnit does NOT change the drawn size (it is
    only used by the inspector's "Set Native Size" button). So density = sprite pixels / transform scale.
  - Mesh shapes with a material (MeshComponent Shape Box/Plane/Stairs...): texture width times the material's
    Tiling (repeats per metre).
  - glTF models (.glb): the UV-area-weighted density of each material, from the model's positions and UVs
    (texture size from the image embedded in the .glb or next to it in <model>.textures/).

Overlay sprites (UI) and sprites without a texture are skipped.

Usage (Python 3.9+, needs PyYAML; numpy optional for the .glb part):
  python scripts/check_texel_density.py Oxygen/project/Assets/Scenes/FarmVillage.hyscene
  python scripts/check_texel_density.py <scene> --target 32 --tolerance 0.2 --assets Oxygen/project/Assets
  python scripts/check_texel_density.py <scene> --strict      (exit code 1 when anything is outside)
"""

import argparse
import json
import os
import statistics
import struct
import sys
from collections import defaultdict

try:
	import yaml
except ImportError:
	sys.exit("check_texel_density: needs PyYAML (pip install pyyaml)")


def PngSize(path):
	try:
		with open(path, "rb") as file:
			header = file.read(24)
		if header[:8] != b"\x89PNG\r\n\x1a\n":
			return None
		return struct.unpack(">II", header[16:24])
	except OSError:
		return None


def LoadYaml(path):
	try:
		with open(path, encoding="utf-8") as file:
			return yaml.safe_load(file)
	except (OSError, yaml.YAMLError):
		return None


def IndexAssets(assetsFolder):
	"""Handle -> (path, data) for every .hymeta and every .hymat in the asset folder."""
	textures = {}
	materials = {}
	for root, _, files in os.walk(assetsFolder):
		for name in files:
			path = os.path.join(root, name)
			if name.endswith(".hymeta"):
				data = LoadYaml(path)
				if data and "Handle" in data and data.get("Type") == "Texture2D":
					textures[data["Handle"]] = (path[: -len(".hymeta")], data)
			elif name.endswith(".hymat"):
				data = LoadYaml(path)
				if data and "Handle" in data:
					materials[data["Handle"]] = (path, data)
	return textures, materials


def SpritePixelHeight(texturePath, meta, spriteName):
	size = PngSize(texturePath)
	if size is None:
		return None
	importConfig = meta.get("TextureImport") or {}
	if importConfig.get("SpriteMode") == "Multiple":
		for sprite in importConfig.get("Sprites") or []:
			if sprite.get("Name") == spriteName:
				return sprite["Rect"][3]
		cell = (importConfig.get("Slicing") or {}).get("CellSize")
		if cell:
			return cell[1]
	return size[1]


def GlbMaterialDensity(glbPath):
	"""Material name -> texels per metre for a .glb, UV-area weighted. Empty when it can't be read."""
	try:
		import numpy as np
	except ImportError:
		return {}
	try:
		with open(glbPath, "rb") as file:
			blob = file.read()
	except OSError:
		return {}
	if blob[:4] != b"glTF":
		return {}
	offset = 12
	document = None
	binary = b""
	while offset < len(blob):
		length, kind = struct.unpack("<II", blob[offset : offset + 8])
		chunk = blob[offset + 8 : offset + 8 + length]
		if kind == 0x4E4F534A:
			document = json.loads(chunk)
		elif kind == 0x004E4942:
			binary = chunk
		offset += 8 + length
	if document is None:
		return {}

	componentTypes = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
	componentCounts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

	def Accessor(index):
		accessor = document["accessors"][index]
		view = document["bufferViews"][accessor["bufferView"]]
		dtype = np.dtype(componentTypes[accessor["componentType"]])
		count = accessor["count"]
		width = componentCounts[accessor["type"]]
		start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
		stride = view.get("byteStride", 0) or dtype.itemsize * width
		raw = np.frombuffer(binary, dtype=np.uint8, count=stride * (count - 1) + dtype.itemsize * width, offset=start)
		rows = np.lib.stride_tricks.as_strided(raw, shape=(count, dtype.itemsize * width), strides=(stride, 1))
		return np.ascontiguousarray(rows).view(dtype).reshape(count, width).astype(np.float64)

	texturesFolder = glbPath[: -len(".glb")] + ".textures"

	def ImageSize(materialIndex):
		material = document["materials"][materialIndex]
		info = (material.get("pbrMetallicRoughness") or {}).get("baseColorTexture")
		if info is None:
			return None
		image = document["images"][document["textures"][info["index"]]["source"]]
		if "bufferView" in image:
			view = document["bufferViews"][image["bufferView"]]
			data = binary[view.get("byteOffset", 0) : view.get("byteOffset", 0) + 24]
			if data[:8] == b"\x89PNG\r\n\x1a\n":
				return struct.unpack(">II", data[16:24])
			return None
		if "uri" in image:
			return PngSize(os.path.join(os.path.dirname(glbPath), image["uri"]))
		return None

	totals = defaultdict(lambda: [0.0, 0.0])
	for mesh in document.get("meshes", []):
		for primitive in mesh.get("primitives", []):
			attributes = primitive.get("attributes", {})
			if "POSITION" not in attributes or "TEXCOORD_0" not in attributes or "material" not in primitive:
				continue
			size = ImageSize(primitive["material"])
			if size is None:
				continue
			positions = Accessor(attributes["POSITION"])
			uvs = Accessor(attributes["TEXCOORD_0"])
			if "indices" in primitive:
				triangles = Accessor(primitive["indices"]).astype(np.int64).reshape(-1, 3)
			else:
				triangles = np.arange(len(positions)).reshape(-1, 3)
			p0, p1, p2 = positions[triangles[:, 0]], positions[triangles[:, 1]], positions[triangles[:, 2]]
			worldArea = 0.5 * np.linalg.norm(np.cross(p1 - p0, p2 - p0), axis=1)
			u0, u1, u2 = uvs[triangles[:, 0]], uvs[triangles[:, 1]], uvs[triangles[:, 2]]
			a, b = u1 - u0, u2 - u0
			uvArea = 0.5 * np.abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0])
			name = document["materials"][primitive["material"]].get("name", str(primitive["material"]))
			totals[name][0] += float((uvArea * size[0] * size[1]).sum())
			totals[name][1] += float(worldArea.sum())
	return {name: (texels / area) ** 0.5 for name, (texels, area) in totals.items() if area > 1e-9}


def Main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("scene")
	parser.add_argument("--assets", help="the project's Assets folder (default: the scene's ../)")
	parser.add_argument("--target", type=float, default=32.0, help="texels per metre (default 32)")
	parser.add_argument("--tolerance", type=float, default=0.2, help="allowed relative deviation (default 0.2)")
	parser.add_argument("--all", action="store_true", help="list everything, not only what is outside")
	parser.add_argument("--strict", action="store_true", help="exit code 1 when anything is outside")
	args = parser.parse_args()

	assetsFolder = args.assets or os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(args.scene)), ".."))
	scene = LoadYaml(args.scene)
	if scene is None:
		sys.exit(f"check_texel_density: can't read {args.scene}")
	textures, materials = IndexAssets(assetsFolder)

	sprites = defaultdict(list)       # texture name -> densities
	shapes = defaultdict(list)        # material name -> densities
	models = set()
	for entity in scene.get("Entities") or []:
		transform = entity.get("TransformComponent") or {}
		scale = transform.get("Scale", [1, 1, 1])
		tag = (entity.get("TagComponent") or {}).get("Tag", "?")

		sprite = entity.get("SpriteRendererComponent")
		if sprite and sprite.get("Overlay") is not True and sprite.get("TextureHandle", 0) in textures:
			path, meta = textures[sprite["TextureHandle"]]
			pixels = SpritePixelHeight(path, meta, sprite.get("Sprite"))
			if pixels and scale[1]:
				sprites[os.path.basename(path)].append((pixels / abs(scale[1]), tag))

		mesh = entity.get("MeshComponent")
		if mesh:
			if mesh.get("ModelHandle"):
				# A model: measured from its .glb below (its slots only override materials).
				models.add(mesh["ModelHandle"])
				continue
			for slot in mesh.get("Materials") or []:
				# A slot uses its material asset when it names one, else its inline values (MeshMaterialSlot).
				material = materials.get(slot.get("MaterialHandle", 0))
				data = (material[1].get("Material") or material[1]) if material else (slot.get("Inline") or {})
				albedo = data.get("AlbedoHandle")
				tiling = data.get("Tiling") or [1, 1]
				if albedo in textures:
					size = PngSize(textures[albedo][0])
					if size:
						name = os.path.basename(material[0]) if material else f"inline {os.path.basename(textures[albedo][0])}"
						shapes[name].append((size[0] * tiling[0], tag))

	low = args.target * (1.0 - args.tolerance)
	high = args.target * (1.0 + args.tolerance)
	outside = 0

	def Report(title, groups):
		nonlocal outside
		print(f"\n{title}")
		rows = []
		for name, values in groups.items():
			densities = [value for value, _ in values]
			median = statistics.median(densities)
			bad = [(value, tag) for value, tag in values if value < low or value > high]
			rows.append((median, name, densities, bad))
		for median, name, densities, bad in sorted(rows):
			if bad or args.all:
				mark = "  " if not bad else "!!"
				print(f" {mark} {median:6.1f}  ({min(densities):5.1f}-{max(densities):5.1f})  x{len(densities):<4d} {name}"
					+ (f"   e.g. {bad[0][1]}" if bad else ""))
			outside += len(bad)

	print(f"Target {args.target:g} texels per metre, allowed {low:.1f}..{high:.1f}")
	Report("Sprites (pixels / world height)", sprites)
	Report("Mesh shapes (texture width x tiling)", shapes)

	glbs = {}
	for root, _, files in os.walk(assetsFolder):
		for name in files:
			if name.endswith(".glb.hymeta"):
				data = LoadYaml(os.path.join(root, name))
				if data and data.get("Handle") in models:
					glbs[data["Handle"]] = os.path.join(root, name[: -len(".hymeta")])
	if glbs:
		print("\nModels (UV-area weighted, per material)")
		for handle, path in sorted(glbs.items(), key=lambda item: item[1]):
			for material, density in sorted(GlbMaterialDensity(path).items()):
				bad = density < low or density > high
				if bad or args.all:
					print(f" {'!!' if bad else '  '} {density:6.1f}  {os.path.basename(path)}  {material}")
				outside += 1 if bad else 0

	print(f"\n{outside} item(s) outside {low:.1f}..{high:.1f}")
	if args.strict and outside > 0:
		sys.exit(1)


if __name__ == "__main__":
	Main()
