#!/bin/sh
# reuse_chk.sh (gl31): the picture objects rl31.sh takes from gl29's stage without compiling them (gl23's objects, made from
# /home/claude/main/ge): each one's source preprocessed (its build_gl20/u2 line with -E -P) from main and from /home/claude/t31 must be
# the same text, so the tree's changed headers (gelist.h, lw_lanes.h, xlate.h, xlme.h, xme.h, natp04.h, natsnap.h) do not reach them.
M=/home/claude/main; T=/home/claude/t31; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; r=0
for f in xlive gsfront texconv texcache gebatch vu1conv gsup2 me_rt fxthin; do
  c=$(head -1 $M/build_gl20/u2/cc_ge_$f.o.log | sed "s/^cmd: //"); s=$(echo "$c" | grep -o "/home/claude/main/ge/[a-z0-9_]*\.c")
  [ -n "$s" ] || { echo "$f: no source in its line"; r=1; continue; }
  a=$(sh -c "$(echo "$c" | sed "s# -c $s -o ge_$f.o# -E -P $s#")" 2>/dev/null | md5sum | cut -c1-8)
  t=$(echo "$s" | sed "s#^/home/claude/main/#$T/#")
  b=$(sh -c "$(echo "$c" | sed "s# -c $s -o ge_$f.o# -E -P $t#")" 2>/dev/null | md5sum | cut -c1-8)
  [ "$a" = "$b" ] && echo "ge_$f.o: preprocessed text equal ($a)" || { echo "ge_$f.o: DIFFERS (main $a, t31 $b)"; r=1; }
done
exit $r
