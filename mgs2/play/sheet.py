"""sheet.py <tag> <from> <n> <every> <count> [cols]: render run/cap_<tag>.bin with gsref and tile the last vblank of each capture window
into run/sheet_<tag>.png (container only: game images, personal use).  The capture is deleted after rendering (disk)."""
import os, subprocess, sys
from PIL import Image, ImageDraw
R = '/home/claude/play/run'; G = '/home/claude/ge_run6/ref/gsref'
tag = sys.argv[1]; fr, n, ev, cnt = map(int, sys.argv[2:6]); cols = int(sys.argv[6]) if len(sys.argv) > 6 else 4
od = '%s/r_%s' % (R, tag); os.makedirs(od, exist_ok=True); cap = '%s/cap_%s.bin' % (R, tag)
if not os.path.exists('%s/vb%d_disp1.ppm' % (od, fr + n - 1)):
    subprocess.run([G, cap, od, '-q'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
ims = []
for k in range(cnt):
    vb = fr + k * ev + n - 1; p = '%s/vb%d_disp1.ppm' % (od, vb)
    if os.path.exists(p): ims.append((vb, Image.open(p).convert('RGB')))
for f in os.listdir(od):
    if '_fb' in f or '_disp2' in f: os.remove(os.path.join(od, f))
if not ims: print('no frames'); sys.exit(1)
w, h = 320, 224; rows = (len(ims) + cols - 1) // cols
sh = Image.new('RGB', (cols * w, rows * (h + 14)), (40, 40, 40)); d = ImageDraw.Draw(sh)
for i, (vb, im) in enumerate(ims):
    x, y = (i % cols) * w, (i // cols) * (h + 14)
    sh.paste(im.resize((w, h)), (x, y + 14)); d.text((x + 4, y + 1), '%s vb %d' % (tag, vb), fill=(255, 255, 0))
sh.save('%s/sheet_%s.png' % (R, tag)); print('%s/sheet_%s.png: %d frames' % (R, tag, len(ims)))
if os.path.exists(cap): os.remove(cap)
