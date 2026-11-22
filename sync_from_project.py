#!/usr/bin/env python3
"""sync_from_project.py [SRC] [DST]   (stdlib only; run with python3 -I)

Copies our own MGS2 port work from the project folder (default /mnt/project-files/mgs2/) into this
repository's mgs2/ folder, leaving out everything taken from the game or from other people's projects:

  - disc files, game binaries and modules (disc/, _stage_x/, *.83, *.irx, *.img, *.ipk);
  - code generated from the game's code and hand translations of game functions (iopgen/, native_w00a.*, native_elf.*);
  - reverse-engineering notes (re/) and the other files the private rules name;
  - copies of public projects (third_party/) and our changed copy of PS2RC's Media Engine bring-up (hle_me_mgs2.*, ps2rc_exc.h),
    which stay under the PS2RC License in the project folder;
  - pictures, captures and any file that holds a NUL byte (binary data);
  - Python caches.

Private rules: a JSON file kept outside the copied folder (default /mnt/project-files/.mgs2psp_private.json, or $MGS2PSP_PRIVATE)
adds paths to leave out and line rules for the copies of text files. Without it the script stops. After copying it searches the
whole repository for the rules' guard words and stops with an error if any is left.

It only adds or replaces files; it lists repository files that no longer exist in SRC, so a person decides whether to remove them.
"""
import os, sys, re, json, shutil, fnmatch

SRC = sys.argv[1] if len(sys.argv) > 1 else '/mnt/project-files/mgs2/'
REPO = os.path.dirname(os.path.abspath(__file__))
DST = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, 'mgs2')
PRIVATE = os.environ.get('MGS2PSP_PRIVATE', '/mnt/project-files/.mgs2psp_private.json')

SKIP_DIRS_AT_ROOT = {'disc', '_stage_x', 'iopgen', 're', 'third_party'}
SKIP_NAMES = ['native_w00a.c', 'native_w00a.h', 'native_elf.c', 'native_elf.h', 'hle_me_mgs2.c', 'hle_me_mgs2.h', 'ps2rc_exc.h',
              'gen_s_*', 'gen_elf*', '*.pyc', '*.83', '*.irx', '*.img', '*.ipk',
              '*.ring', '*.png', '*.bmp', '*.ppm', '*.jpg', '*.jpeg', '*.mp4', '*.avi', '*.wav', '*.vag', '*.at3', '*.pmf',
              '*.PBP', '*.pbp', '*.prx', '*.elf', '*.o', '*.a', '*.bin', '*.pak', '*.dat', '*.DAT', '*.iso', '*.gcda', '*.gcno']
MAX_BYTES = 50 * 1024 * 1024

if not os.path.isfile(PRIVATE):
    sys.exit('the private rules file %s is missing: nothing copied' % PRIVATE)
RULES = json.load(open(PRIVATE))
SKIP_PATHS = set(RULES.get('skip_paths', []))
DROPS = [re.compile(p) for p in RULES.get('drop_lines', [])]
SUBS = [(re.compile(p), r) for p, r in RULES.get('subs', [])]
GUARD = re.compile(RULES['guard'])


def skipped(rel, name):
    parts = rel.split('/')
    if parts[0] in SKIP_DIRS_AT_ROOT or '__pycache__' in parts:
        return True
    if any(rel == p or rel.startswith(p + '/') for p in SKIP_PATHS):
        return True
    return any(fnmatch.fnmatchcase(name, pat) for pat in SKIP_NAMES)


def clean(data):
    """The repository's copy of a text file: the private line rules applied."""
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        text = data.decode('latin-1')
        enc = 'latin-1'
    else:
        enc = 'utf-8'
    out = []
    for line in text.splitlines(keepends=True):
        if any(d.search(line) for d in DROPS):
            continue
        for p, r in SUBS:
            line = p.sub(r, line)
        out.append(line)
    return ''.join(out).encode(enc)


def main():
    copied = same = 0
    left_out = []
    seen = set()
    for root, dirs, files in os.walk(SRC):
        dirs.sort()
        for name in sorted(files):
            src = os.path.join(root, name)
            rel = os.path.relpath(src, SRC).replace(os.sep, '/')
            if skipped(rel, name):
                continue
            if os.path.getsize(src) > MAX_BYTES:
                left_out.append(rel)
                continue
            with open(src, 'rb') as f:
                data = f.read()
            if b'\0' in data[:1 << 20]:
                left_out.append(rel)
                continue
            data = clean(data)
            seen.add(rel)
            dst = os.path.join(DST, rel)
            if os.path.exists(dst):
                with open(dst, 'rb') as f:
                    if f.read() == data:
                        same += 1
                        continue
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, 'wb') as f:
                f.write(data)
            shutil.copystat(src, dst)
            copied += 1
    gone = []
    for root, dirs, files in os.walk(DST):
        for name in files:
            rel = os.path.relpath(os.path.join(root, name), DST).replace(os.sep, '/')
            if rel not in seen:
                gone.append(rel)
    print('copied %d, unchanged %d, left out as binary or too large %d' % (copied, same, len(left_out)))
    for r in left_out:
        print('  left out: ' + r)
    for r in sorted(gone):
        print('  in the repository but not in the project folder: ' + r)
    # keep the private paths out of `git add` too (local only, never pushed)
    info = os.path.join(REPO, '.git', 'info')
    if os.path.isdir(info):
        with open(os.path.join(info, 'exclude'), 'w') as f:
            f.write(''.join('mgs2/%s\n' % p for p in sorted(SKIP_PATHS)))
    hits = []
    for root, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d != '.git']
        for name in files:
            path = os.path.join(root, name)
            with open(path, 'rb') as f:
                data = f.read()
            if b'\0' in data[:1 << 20]:
                continue
            for n, line in enumerate(data.decode('utf-8', 'replace').splitlines(), 1):
                if GUARD.search(line):
                    hits.append('%s:%d' % (os.path.relpath(path, REPO), n))
    if hits:
        print('GUARD: %d lines still name what the private rules keep out:' % len(hits))
        for h in hits[:50]:
            print('  ' + h)
        sys.exit(1)
    print('guard: clean')


main()
