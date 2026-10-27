import bpy,sys
from pathlib import Path
SG=Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\ShrineGlade")
out=[]
for f in ["Foliage/OakLargeA.png","Foliage/OakLargeB.png","Foliage/OakMediumA.png","Foliage/PineA.png","Foliage/PineTall.png","Foliage/BushA.png","Foliage/BushB.png","Foliage/RoundBush.png","Foliage/Fern.png","Foliage/FernWide.png","Foliage/FlowersA.png","Foliage/GrassA.png","Foliage/WildflowerPatch.png","Foliage/MossBush.png","Textures/Grass.png","Textures/Dirt.png","Textures/Flagstones.png","Props/Fence.png","Foliage/Oak1.png","Foliage/Pine1.png"]:
    p=SG/f
    im=bpy.data.images.load(str(p)) if p.exists() else None
    out.append(f+" "+(str(list(im.size)) if im else "missing"))
print("RESULT::"+"|".join(out))
