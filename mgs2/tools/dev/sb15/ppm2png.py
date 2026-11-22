# ppm2png.py <in.ppm> <out.png> [w h] [crop x0 y0 x1 y1]: container-only views of gsref images (game data: never sent or published)
import sys
from PIL import Image
im = Image.open(sys.argv[1])
a = sys.argv[3:]
if len(a) >= 6: im = im.crop(tuple(int(v) for v in a[2:6]))
if len(a) >= 2: im = im.resize((int(a[0]), int(a[1])), Image.LANCZOS)
im.save(sys.argv[2])
