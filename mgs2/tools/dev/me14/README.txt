me14: the ME picture's build order step 2, the picture code made ME-safe (task #149; mgs2/me_handoff_design.md section 6 step 2,
results in mgs2/me_step2_notes.md).  Saved 2026-11-03 07:55 UTC from the container trees /home/claude/me14 and /home/claude/me14p6 to
/home/claude/me14p9 (ephemeral).

Files (md5 of each in md5.txt, with the md5 of every file me14.diff makes and of the file it starts from):
- me14.diff: what step 2 changed, as unified diffs against the files it started from:
  ge/xlive.c xlate.c gsfront.c texconv.c texcache.c gelive.c gebatch.c xconv.c vu1conv.c gelist.c gsup2.c and gelive.h gebatch.h
  xconv.h against sb14's (patch_me14.py, and patch_me14_lc.py after it on gsup2.c); ge_e/xlate.c (the take-back's xlate.c) against
  me13's (patch_me14.py's xlate edits); boot/xlive_chk.c against sb14's (patch_me14_glue.py);
  boot/xlive_chk_g.c against me13's (patch_me14g_glue.py); boot/xlive_chk_g2.c against me14's boot/xlive_chk_g.c
  (patch_me14g2_glue.py); psp/xl_psp.c against sb14's (patch_me14_psp.py, v8).  Each patch script makes its file from that base
  byte for byte (verify_patches.sh, run when saved: all the same).  The new files are whole below.
- me_rt.c, me_rt.h (ge/): the picture's own runtime: me_memcpy, me_memset, me_memcmp, me_floorf, me_ceilf, the heap (two-level
  segregated fit over regions, with the host's three hooks: more, less, resize) and ME_CALLP.  me_rt_test.c: its x86 test
  (gcc -O2 -o me_rt_test me_rt_test.c me_rt.c -lm; ./me_rt_test [m h p s r]: the copies, the heap, the pools and hooks, the resize,
  the rounding on all 2^32 floats).
- xlme.c, xlme.h (ge/): the ME's loop: the late walk with the lanes, the switches each record carries, the translator's callbacks,
  a frame's end, list and digest, a frame's start, the GS map's reader of RAM.  lw_lanes.h (ge/): the lanes' recognizers, made by
  mk_lw.py from runtime/ee_io.h.
- patch_me14.py, patch_me14_lc.py, patch_me14_glue.py, patch_me14g_glue.py, patch_me14g2_glue.py, patch_me14_psp.py: the patches
  (each header says from what and what).  patch_me14_psp_v5.py, _v6.py, _v7.py: the glue's earlier versions (me14p6's, me14p7's
  and me14p8's); patch_me14_psp.py is v8 (me14p9's, the final one).  verify_patches.sh: remakes every patched file from its base and compares.
- mk.sh <dir>: twin A (sb14's twin with the ME-safe picture objects; /home/claude/me14/xa).  mk_g.sh: 2a, step 1's split twin with
  them (xg).  mk_g2.sh: 2b, 2a with the ME's loop as one file (xg2).  mkpsp.sh <dir>: the PSP build (gl16's tree without PGO, the
  picture files ME-safe, psp/xl_psp.c with the heap's region, hooks and lines).  mkme_psp.sh <ge dir> <out>: the ME's unit (the 11
  files, xlme.c and me_rt.c with the PSP flags and -fstack-usage, linked by ld -r --gc-sections from the entries in entries.txt).
  mecheck.py <out> [callmap]: the unit's check (relocations, pointer calls, registers, break, the deepest stack); callmap.txt: what
  the unit's pointer calls can reach, by hand, for the stack's walk.
- runs_f.sh: the final x86 set (A and 2b plain, 2a plain, 2b with every switch following its record while four configurations take
  turns, the take-back at six points), each against sb14's x1 runs (cmp13.py).  turns_ref.sh: sb14's own twin with those turns, the
  reference for 2b in turns; turnsdiff.py <ref log> <run log>: a run in turns' differing frames by configuration.  x86tab.py
  <runs_f.log> <runs_r4.log> [turns lines]: the notes' x86 tables.  runs_l.sh <dir> <tag>: one twin on the three routes
  (with MGS2_ME_HEAP_KB=768: the PSP's hooked heap).  run1g.sh, run1g2.sh <d|i|c> <tag> <end vb> [env]: one route of 2a / 2b.
- f6all.sh to f9all.sh: me14p6 to me14p9 on the emulator (gl3.sh emu hw and hwmem); f6all.sh also me14p6's two model windows and
  runs_f.sh.
  emucmp.py <emu log> <reference emu log> <x86 log>: hash and vuh lines, frame digests and the memory lines.  fitcmp.py <fit> <fit>:
  two profcat_fit.py outputs side by side by category.
