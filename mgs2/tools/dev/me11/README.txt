me11: the late-run harness of the ME hand-off design (task #142, mgs2/me_handoff_design.md sections 1 and 3).  Saved 2026-11-03 from the
container tree /home/claude/me11 (ephemeral).  boot/xlive_chk.c = sb9's boot/xlive_chk.c (md5 fe1f7de9) + me11.diff (md5 of the result
5c06fe51; checked 2026-11-03 by patching sb9's file).  me11.diff holds me9's and me10's additions too (me9: tools/dev/me9/README.txt).
Build: as me9 (xd/ = sb9's x1 objects symlinked + this file); xdbg/ the same with the census switches.  Settings the design's runs used:
MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=0 MGS2_XLIVE_SNAPP=1 MGS2_XLIVE_SNAPR=12c..142,1ed (the design's
copy: packets, the data they point to, texture sources in the two rewritten ranges), MGS2_XLIVE_SNAPDBG=1 (the census by 16-byte piece).
runs1.sh, runs2.sh: the runs of the design; chkd.sh: hash and picture checks against sb9's x1 runs; tools/: picdiff3.py (pictures),
patch_*.py (the "me11 rg / hist / block / dbg" line readers).  Logs and replay files stay in the container (game data).
