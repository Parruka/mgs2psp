#!/bin/sh
# gl45 batch 3: tools/g45_size.py for the named variants' deck objects (and ELF ones when missing), results in out/sizew.txt
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; cd /home/claude/g45
for v in "$@"; do
  for u in elf w00a; do
    [ -s out/size_${v}_$u.json ] && [ out/size_${v}_$u.json -nt psp/${v}_${u}_g.o ] && continue
    python3 -I tools/g45_size.py psp/${v}_${u}_g.o out/map_${v}_$u.json out/keys.json out/bc_rain.json 1200 out/size_${v}_$u.json >> out/sizew.txt 2>&1
  done
done
echo "sizew $* done" >> out/sizew.txt
