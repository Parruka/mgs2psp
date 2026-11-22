#!/bin/sh
# Unpack Ffm's PAL stage.dat (SLES-50383) with Joy-Division/tools-mgs dat-extract_enc (zlib licence)
# and the JoyDict PAL name dictionary. Output goes to $OUT (default /home/claude/mgs2/stage, ~1.4 GB).
set -e
W=${W:-/home/claude/mgs2/work}; OUT=${OUT:-/home/claude/mgs2/stage}
DAT=${DAT:-/mnt/project-files/mgs2/disc/stage.dat}
mkdir -p "$W" "$OUT"; cd "$W"
[ -d tools-mgs ] || git clone -q --depth 1 https://github.com/Joy-Division/tools-mgs.git
[ -d JoyDict ] || git clone -q --depth 1 https://github.com/Joy-Division/JoyDict.git
mkdir -p bin
gcc -O2 -o bin/dat-extract_enc tools-mgs/strcode/strcode.c tools-mgs/stage-dictionary.c tools-mgs/dat-extract_enc.c -Itools-mgs -lz
cp JoyDict/mgs2/stage.dat/mgs2.pal.txt bin/common-mgs2.txt
cd "$OUT" && "$W/bin/dat-extract_enc" "$DAT" > "$W/extract.log" 2>&1
echo "done: $(ls -d "$OUT"/*/ | wc -l) stages"
