# Renders the MV_Kit preview cameras to Source/MV_Kit/preview_*.png (run headless on MV_Kit.blend).
import bpy, sys
from pathlib import Path
SRC = Path(bpy.data.filepath).parent
scene = bpy.context.scene
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
pct = int(argv[0]) if argv else 100
scene.render.resolution_percentage = pct
out = []
for cam, fname in (("PV_GameCam", "preview_game.png"), ("PV_ThreeQuarterCam", "preview_34.png"), ("PV_PrefabsCam", "preview_prefabs.png")):
    if cam in bpy.data.objects:
        scene.camera = bpy.data.objects[cam]
        scene.render.filepath = str(SRC / fname)
        bpy.ops.render.render(write_still=True)
        out.append(fname)
print("RESULT::" + repr(out))
