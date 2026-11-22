# gcxfind.py: each chara command of the deck's script (w00a scenerio.gcx) that names one of these character-table hashes (06 + 3 bytes LE)
import sys
d = open('/home/claude/mgs2/stage/w00a/cache/scenerio.gcx', 'rb').read()
H = {0x16cb: 'rain', 0x1a6dd: 'line_sph', 0x68f6cb: 'rain_gas_pers', 0x4a777d: 'wave2', 0xbe0863: 'd_splash_floor', 0x264c7d: 'chim_smo',
     0xa8560d: 'bonbori_yw', 0xd2bd87: 'wind_man'}
for h, n in H.items():
    pat = bytes([6, h & 255, (h >> 8) & 255, h >> 16]); i = 0; k = 0
    while True:
        i = d.find(pat, i)
        if i < 0: break
        k += 1
        print('%-15s %06x at %05x: before %s | %s' % (n, h, i, d[i - 10:i].hex(' '), d[i:i + 48].hex(' ')))
        i += 1
    if not k: print('%-15s %06x: none' % (n, h))
