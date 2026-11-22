me9: the x86 twin that runs the picture late, as a second CPU (the Media Engine) would (task #130; results in mgs2/me_picture_study.md
section 3).  2026-11-02.  Container tree /home/claude/me9 (ephemeral): boot/xlive_chk.c = sb9's boot/xlive_chk.c (md5 fe1f7de9) +
me9.diff; xd/ = sb9's x1 objects symlinked + that file (mk.sh).  Game data stays in the container: the logs carry digests and counts, the
replay files (ger/) whole frames, never copied here.

What me9.diff adds to boot/xlive_chk.c (the comment at its top has the full text):
- MGS2_XLIVE_DEFER=1 / 2: each transfer is queued; the translator, the frame work and the thinning run at the end of the step the
  transfer came in (1) or of the step after it (2).  The runtime's own walk (what the game reads back) stays at the transfer.
- MGS2_XLIVE_SNAP=2: at each transfer's start, what its DMA chain reads is copied into a shadow RAM, and the late run reads that
  (M.mem = the shadow); the thinning reads a whole-RAM copy.  "me9 snap" lines count what the game changed before the late run.
- "me9 vb a-b" lines and "me9 totals": the game's time a step, where the first and last transfer start, the late work's time, the
  queue's depth; "me9 snap totals": bytes a step with transfers, DMA tags, by place.

Scripts (run from /home/claude/me9):
- mk.sh: the build.  chkd.sh <i|d|c> <tag> [env]: one run (gl1_x86.sh's settings or sb9's codec_x86.sh; xd/out_<tag>.log).
- runs2.sh .. runs9.sh: the runs in the order made (tags: dd1 deck 1 step no copy; ds2 deck 1 step copy; d2s2t deck 2 steps copy with
  the texture check; d2n deck 2 steps no copy; is2 / id1 inside 1 step copy / no copy; cs2 codec route 1 step copy; dntex / ntex /
  stex the texture check normally and on the title; ing / isg, jng / jdg, dng / dsg runs that save replay frames; runs7 (TK with GX)
  cannot run: xlive_chk.c refuses MGS2_XLIVE_TK with MGS2_XLIVE_GX, so runs9 times the texture path in GB mode (tkn, tkd)).
- sumrun.sh <tag> <reference log>: state hashes, frames (picdiff.py), the copy's totals.
- picdiff.py <a> <b>: differing frames as ranges.  picsets.py <no copy> <copy> <reference>: overlap of the differing frames.
  picstretch.py <no copy> <copy> <reference> <edges...>: frames differing only without the copy, by stretch.  listdiff.py <a> <b>:
  frames whose GE list differs.  gerdiff.py <a.ger> <b.ger>: what differs in one frame (header, batches, vertices, textures, data).
  texsum.py: the texture check's lines summed (its totals double-count; use the route line).
References: sb9's x1 runs out_d2on.log (deck), out_i2on.log (inside), out_c2on.log (codec route).
