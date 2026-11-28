#!/usr/bin/env python3
"""Copy gl43's source tree and the builds' own scripts from the container to the project folder's mgs2/main/ (2026-11-28).
Writes a manifest (dst path, md5) so the copy can be read back and checked."""
import os, sys, shutil, hashlib, fnmatch
DST = '/mnt/project-files/mgs2/main'
MAN = sys.argv[1]
out = []
def md5(p):
    return hashlib.md5(open(p, 'rb').read()).hexdigest()
def cp(src, rel):
    dst = os.path.join(DST, rel)
    data = open(src, 'rb').read()
    if b'\0' in data[:1 << 20]:
        print('  binary, left: ' + src); return
    if os.path.exists(dst) and open(dst, 'rb').read() == data:
        out.append((rel, hashlib.md5(data).hexdigest(), 'same')); return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(src, dst); shutil.copystat(src, dst)
    out.append((rel, hashlib.md5(data).hexdigest(), 'copied'))
# A. gl43's final tree (not gen41/: generated from the game's VU code)
T = '/home/claude/t43'
for root, dirs, files in os.walk(T):
    dirs[:] = sorted(d for d in dirs if d not in ('gen41', '__pycache__'))
    for f in sorted(files):
        p = os.path.join(root, f); cp(p, os.path.relpath(p, T))
# B. the main folder's tools, scripts, cfg
M = '/home/claude/main'
for d in ('tools', 'scripts', 'cfg'):
    for root, dirs, files in os.walk(os.path.join(M, d)):
        dirs[:] = sorted(x for x in dirs if x != '__pycache__')
        for f in sorted(files):
            p = os.path.join(root, f)
            if os.path.getsize(p) < 1 << 20 and not f.endswith(('.pyc', '.o')):
                cp(p, os.path.relpath(p, M))
# C. each build's own scripts
EXT = ('.sh', '.py', '.cfg', '.diff', '.md', '.ld')
for b in ('gl38', 'gl39', 'gl40', 'gl41', 'gl42', 'gl43'):
    B = os.path.join(M, 'build_' + b)
    for f in sorted(os.listdir(B)):
        p = os.path.join(B, f)
        if os.path.isfile(p) and f.endswith(EXT) and os.path.getsize(p) < 300000:
            cp(p, 'attic/%s/build_scripts/%s' % (b, f))
    for sub in ('tools', 'cfg', 'cfg_emu'):
        S = os.path.join(B, sub)
        if os.path.isdir(S):
            for f in sorted(os.listdir(S)):
                p = os.path.join(S, f)
                if os.path.isfile(p) and os.path.getsize(p) < 300000 and not f.endswith(('.pyc', '.o', '.bin')) and not fnmatch.fnmatch(f, 'gen_*'):
                    cp(p, 'attic/%s/build_scripts/%s/%s' % (b, sub, f))
with open(MAN, 'w') as f:
    for rel, h, st in out:
        f.write('%s  %s  %s\n' % (h, st, rel))
print('files', len(out), 'copied', sum(1 for o in out if o[2] == 'copied'), 'same', sum(1 for o in out if o[2] == 'same'))
