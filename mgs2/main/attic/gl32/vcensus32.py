#!/usr/bin/env python3
"""vcensus32.py PREFIX [--kb 256,512,...] [--win a:b,...]   (gl32 plan step 2; run with python3 -I)
Reads the twin's ship-mesh census (boot/nat_x86.c nc_group, MGS2_NAT_CENSUS=PREFIX): PREFIX.keys (one line a key: the three streams'
addresses and quadwords and the header's vertex count) and PREFIX.draws (uint32: 0xffffffff, vblank at a picture's first group, then a
word a drawn group: key index | 1<<24 DF0 entry | 1<<25 new key | 1<<28 A changed | 1<<29 B changed | 1<<30 C changed, against the same
key's draw before).  Prints per stage visit: pictures, draws, keys, vertices, bytes at the GE's format (16 B a vertex + 4 B a strip + 2 B
an index, each key once), how often a stream's bytes differ from the key's last draw, the keys by life (first to last vblank), the bytes
a picture draws, and a simulation of the cache gl32 builds (slots by key, a miss when the key is new or a stream changed; the arena
dropped whole when full) at several sizes.  Every figure is a count from the x86 twin [C]."""
import sys, numpy as np
a = sys.argv[1:]; pre = a[0]; KB = [256, 384, 512, 768, 1024, 4096]; WIN = None; i = 1
while i < len(a):
    if a[i] == '--kb': KB = [int(x) for x in a[i + 1].split(',')]; i += 2
    elif a[i] == '--win': WIN = [tuple(int(y) for y in x.split(':')) for x in a[i + 1].split(',')]; i += 2
    else: i += 1
K = []
for ln in open(pre + '.keys'):
    if ln.startswith('#'): continue
    f = ln.split(); K.append([int(f[0])] + [int(x, 16) for x in f[1:4]] + [int(x) for x in f[4:18]] + [int(f[18], 16)] + [int(x) for x in f[19:23]])
K = np.array(K, dtype=np.int64)
# columns: 0 id, 1-3 s, 4-6 n, 7 nv, 8 first, 9 last, 10 draws, 11 pics, 12 cA, 13 cB, 14 cC, 15 cMp, 16 cM, 17 cD, 18 mscal, 19 df0, 20 nst, 21 ni, 22 cS
nk = len(K); nv = K[:, 7]; gb = 16 * nv + 4 * K[:, 20] + 2 * K[:, 21]; gb64 = (gb + 63) & ~63
slot = (16 * nv + 4 * (nv // 3) + 2 * (nv + 2 * (nv // 3)) + 63) & ~63   # a slot for the worst strips of nv vertices, on 64-byte lines
d = np.fromfile(pre + '.draws', dtype='<u4')
mk = np.nonzero(d == 0xffffffff)[0]; pvb = d[mk + 1].astype(np.int64)
isd = np.ones(len(d), bool); isd[mk] = False; isd[mk + 1] = False
pic_of = np.cumsum(d == 0xffffffff) - 1          # the picture each word belongs to
w = d[isd]; pic = pic_of[isd]; kid = (w & 0xffffff).astype(np.int64); df0 = (w >> 24 & 1).astype(bool); new = (w >> 25 & 1).astype(bool)
cA = (w >> 28 & 1).astype(bool); cB = (w >> 29 & 1).astype(bool); cC = (w >> 30 & 1).astype(bool); vb = pvb[pic]
print('%s: %d keys, %d pictures (vb %d..%d), %d groups drawn; DF0-entry draws %d (%.2f%%), other entries %d' % (
    pre, nk, len(mk), pvb.min(), pvb.max(), len(w), df0.sum(), 100.0 * df0.sum() / len(w), (~df0).sum()))
ms = sorted(set(K[:, 18] & 0xffff)); print('MSCAL entries of the keys (first draw): ' + ', '.join(
    'pc %#x = P04+%#x: %d keys' % (m, m * 8, (K[:, 18] & 0xffff == m).sum()) for m in ms))
if WIN is None: WIN = [(0, 3555), (3556, 20136), (20143, 23244), (23261, 23663), (23670, 29052), (29070, 31559), (31661, 37799), (24001, 31400), (29490, 31400), (29600, 30500), (30900, 31300), (0, 37800)]
def sim(kk, ch, cap, sl):
    """the cache at cap bytes: a slot a key (sl bytes each), a miss when the key has no slot or ch (a stream changed since its last
    draw, or its first draw); full: everything dropped; returns hits, misses, drops, the most bytes in use, converted-again-by-drop"""
    have = np.zeros(nk, bool); used = 0; hit = miss = drops = most = 0; inside = []
    kk = kk.tolist(); ch = ch.tolist(); sll = sl.tolist()
    for k, c in zip(kk, ch):
        if have[k] and not c: hit += 1; continue
        miss += 1
        if not have[k]:
            s = sll[k]
            if s > cap: continue
            if used + s > cap:
                have[inside] = False; inside = []; used = 0; drops += 1
            have[k] = True; inside.append(k); used += s
            if used > most: most = used
    return hit, miss, drops, most
for (v0, v1) in WIN:
    m = (vb >= v0) & (vb <= v1)
    if not m.any(): print('vb %d-%d: no ship-mesh group drawn' % (v0, v1)); continue
    kk = kid[m]; pp = pic[m]; u = np.unique(kk); npic = len(np.unique(pp)); f = df0[m]
    ud = np.unique(kk[f])
    print('\nvb %d-%d: %d pictures, %d groups drawn (%.0f a picture), %d vertices (%.0f a picture, %.1f a group); DF0 entry: %d groups, %d vertices' % (
        v0, v1, npic, m.sum(), m.sum() / npic, nv[kk].sum(), nv[kk].sum() / npic, nv[kk].mean(), f.sum(), nv[kk[f]].sum()))
    print('  keys drawn %d (DF0 %d): %d vertices, %.0f KB at the GE format (%.0f KB in 64-byte slots, %.0f KB in worst-strip slots); DF0 keys %.0f KB / %.0f KB' % (
        len(u), len(ud), nv[u].sum(), gb[u].sum() / 1024, gb64[u].sum() / 1024, slot[u].sum() / 1024, gb64[ud].sum() / 1024, slot[ud].sum() / 1024))
    print('  a stream differs from the same key\'s draw before: A (positions) %d draws of %d keys, B (colours, flags) %d of %d, C (UVs) %d of %d; any %d (%.3f%% of the draws); first draws of a key %d' % (
        cA[m].sum(), len(np.unique(kk[cA[m]])), cB[m].sum(), len(np.unique(kk[cB[m]])), cC[m].sum(), len(np.unique(kk[cC[m]])),
        (cA[m] | cB[m] | cC[m]).sum(), 100.0 * (cA[m] | cB[m] | cC[m]).sum() / m.sum(), new[m].sum()))
    # bytes a picture draws (each key once a picture)
    o = np.lexsort((kk, pp)); k2 = kk[o]; p2 = pp[o]; first = np.ones(len(k2), bool); first[1:] = (k2[1:] != k2[:-1]) | (p2[1:] != p2[:-1])
    per = np.bincount(p2[first] - p2.min(), weights=gb64[k2[first]].astype(float)); per = per[per > 0]
    rep = len(k2) - first.sum()
    print('  bytes a picture draws (each key once, 64-byte slots): mean %.0f KB, most %.0f KB; a key drawn again in the same picture: %d draws (%.2f%%)' % (
        per.mean() / 1024, per.max() / 1024, rep, 100.0 * rep / len(k2)))
    # the cache: DF0 groups only (the converter the cache serves)
    kd = kk[f]; chd = (cA[m] | cB[m] | cC[m] | new[m])[f]
    hb = np.zeros(nk, bool)
    for cap in KB:
        h, mi, dr, mo = sim(kd, chd, cap * 1024, slot)
        print('  cache %5d KB (worst-strip slots, dropped whole when full): hits %d (%.2f%%), misses %d, drops %d, most in use %.0f KB' % (
            cap, h, 100.0 * h / max(1, len(kd)), mi, dr, mo / 1024))
# keys by life and by change
life = K[:, 9] - K[:, 8]
print('\nkeys by life (last - first vblank): one picture or less (<= 2) %d, <= 100 %d, <= 1000 %d, longer %d' % (
    (life <= 2).sum(), ((life > 2) & (life <= 100)).sum(), ((life > 100) & (life <= 1000)).sum(), (life > 1000).sum()))
print('keys whose stream ever changed: A %d, B %d, C %d; material pointer %d, material bytes %d, descriptor words %d, strips (count or indices) %d' % (
    (K[:, 12] > 0).sum(), (K[:, 13] > 0).sum(), (K[:, 14] > 0).sum(), (K[:, 15] > 0).sum(), (K[:, 16] > 0).sum(), (K[:, 17] > 0).sum(), (K[:, 22] > 0).sum()))
t = np.argsort(-K[:, 13])[:12]
print('keys with most B changes (id s0 s1 s2 nv first last draws cA cB cC):')
for j in t:
    if K[j, 13] == 0: break
    print('   %6d %08x %08x %08x nv %3d vb %5d-%5d draws %6d cA %d cB %d cC %d' % (K[j, 0], K[j, 1], K[j, 2], K[j, 3], K[j, 7], K[j, 8], K[j, 9], K[j, 10], K[j, 12], K[j, 13], K[j, 14]))
t = np.argsort(-K[:, 12])[:8]
print('keys with most A changes:')
for j in t:
    if K[j, 12] == 0: break
    print('   %6d %08x %08x %08x nv %3d vb %5d-%5d draws %6d cA %d cB %d cC %d' % (K[j, 0], K[j, 1], K[j, 2], K[j, 3], K[j, 7], K[j, 8], K[j, 9], K[j, 10], K[j, 12], K[j, 13], K[j, 14]))
