# Terrain look spike (TR0)

The reference for the terraced terrain ([Terrain plan](https://github.com/joelnl74/Hydro/wiki/Terrain-Plan), TR0): the mesher, the pixel splat and the edges in Python, rendered in Blender at the Farm Village's game camera. The C++ `TerrainMesher` and `terrain.frag` (TR1 to TR3) follow this code: same data, same hashes, same rules.

| File | What |
| --- | --- |
| `terrain_mesher.py` | Data (corner levels, cell ramps, cell wall materials), canonical edge crossings, regions and bands (saddles, nesting, stacked walls), walls with lean and noise, lip, fringe, cap stones, ramp runs and their side walls, chunk-safe hashing |
| `splat.py` | The pixel splat (weights + clustered edge noise, one layer per art pixel), the fringe strip texture |
| `test_patch.py` | The 20 x 20 m test patch with one of everything risky, the layers (the Farm Village's textures) and the presets |
| `build_patch.py` | Builds mesh and textures per preset into `out/<preset>/` |
| `preview_blender.py` | Renders `out/<preset>/{game,close,west}.png` |
| `test_terrain.py` | Checks the rules: golden hashes, saddles, two-level drops, coverage, shared crossings, chunk seams, ramp validation, splat guarantee |

```
python scripts/terrain/test_terrain.py
python scripts/terrain/build_patch.py [bare conservative stronger tr1]
blender --background --python scripts/terrain/preview_blender.py -- [preset ...] [--views game,close,west]
```

`build_patch.py` and the tests need numpy and Pillow. `preview_blender.py` runs inside Blender (or with the pip `bpy` module); a render takes about a minute per view on the CPU.

## Presets

Tuning starts conservative and goes up one effect at a time (plan, TR0):

| Preset | Jitter | Lean | Wall noise | Lip | Fringe | Caps | Edge roughness |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `bare` | 0.3 | 0 | 0 | 0 | 0 | off | 0.6 |
| `conservative` | 0.3 | 0 | 0 | 2 px | 4 px | coping | 0.6 |
| `stronger` | 0.38 | 2 px/m | 1.5 px | 3 px | 7 px | coping | 0.9 |
| **`tr1`** (approved) | 0.38 x variation | 1 px/m | 1.5 px x variation | 3 px | 7 px | coping, stone only | 0.9 |

`tr1` is the approved TR1 default (October 30): stronger path breakup, walls calmer through each wall layer's **Variation** (stone 0.65, earth banks 0.57), which scales the outline jitter and wall noise. An edge takes the calmer of its two cells' variations, so it stays a function of the edge and the data (seam-safe). Cap stones are a coping row: flat stones every 0.28 m of outline, aligned with the wall, small size and position jitter, 3 degrees of rotation, 8 % missing; only on stone walls.

## Rules found here (now in the plan)

- **Jitter is a smooth field plus a little per-edge noise**, both keyed by the edge ID. Pure per-edge jitter made outlines zigzag every 0.5 m.
- **Wall noise only ever pushes a course outwards** (lean plus a random 0..noise per course), so a wall never undercuts and the tops still cover each cell exactly once.
- **A ramp's surface comes only from its run's two ends**: the levels stored on its side corners belong to the neighbouring ground. Storing in-between levels there grew small steps beside the ramp.
- **The lip's front face is the wall's material, 20 % darker** (the plan's open point).
