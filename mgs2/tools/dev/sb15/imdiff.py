# imdiff.py <a.ppm> <b.ppm> [y1]: how far two gsref displays differ in the picture area (rows 0..y1-1): mean abs level, % pixels > 8, > 16, > 32
import sys
import numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]), dtype=np.int16); b = np.asarray(Image.open(sys.argv[2]), dtype=np.int16)
y1 = int(sys.argv[3]) if len(sys.argv) > 3 else a.shape[0]
d = np.abs(a[:y1] - b[:y1]).max(axis=2)
n = d.size
print('mean %.2f  >8 %.2f%%  >16 %.2f%%  >32 %.2f%%  max %d' % (np.abs(a[:y1] - b[:y1]).mean(), 100.0 * (d > 8).sum() / n, 100.0 * (d > 16).sum() / n, 100.0 * (d > 32).sum() / n, d.max()))
