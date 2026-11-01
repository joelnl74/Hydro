# Renders the JP_Kit preview cameras (run headless on JP_Kit.blend): blender -b JP_Kit.blend --python render_jp_preview.py -- CAM ...
import bpy, sys
from pathlib import Path
SRC = Path(bpy.data.filepath).parent
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
cams = argv or [o.name for o in bpy.data.objects if o.type == 'CAMERA']
sc = bpy.context.scene
sc.render.image_settings.file_format = 'JPEG'; sc.render.image_settings.quality = 88
(SRC / "_renders").mkdir(exist_ok=True)
out = []
for c in cams:
    sc.camera = bpy.data.objects[c]
    sc.render.filepath = str(SRC / "_renders" / (c + ".jpg"))
    bpy.ops.render.render(write_still=True)
    out.append(c)
print("RESULT::" + repr(out))
