# tools/dev/sb17: gl17's tree (/home/claude/sb17) and the rain's options (task #158, 2026-11-03)
sb17 = sb15 (Stage C step 4, `tools/dev/sb15/`) + the ME work's local-copy fix on ge/gsup2.c (`tools/dev/me14/patch_me14_lc.py`, 17f7b3c6)
+ the two patches here.  Notes: stagec_notes.md 4.12.
- `patch_none.py`: cfg MGS2_GL_STREAKS=0 leaves the rain's streak calls out whole (before their decode); originals were in the
  thread's scratchpad.  Files after it: fxthin.c 1d8f5562, fxthin.h 43a68d8a, gelive.c c25a25d0, xl_psp.c 8b2e812e, xlive_chk.c fb7e0571.
- `patch_calls.py`: cfg MGS2_GL_STREAK_CALLS=N (1..8) draws 1 in N of the rain's streak calls, whole calls by their block's place in
  the range ((XlCall.src - lo) / 0x1290 a multiple of N); asserts the md5s above and xconv.h 0e78b2ac.  After it: xconv.h 5ac52ff5,
  fxthin.h 4f913fe4, fxthin.c f1a77723, gelive.c bd3864f5, xl_psp.c 81a79761, xlive_chk.c 770a21c3.
- `mkpsp17.sh <new dir>`: the model build (no PGO) from sb17 (sb17m/d: EBOOT 2fd88b9d before patch_calls; sb17m2/d: 00255284 after).
- `models17.sh`, `models17c.sh`: PspProf runs on the indoor route's aft deck (vb 30,810-30,890, 80 steps) per setting; `fit17.sh`,
  `fit17c.sh <A> <B> [functions]`: fitcmp of two of them.
- `masters17.sh`: PPSSPP master pictures (`tools/dev/sb15/c4master.sh`) of the options; the 1/3 by calls one: `B=/home/claude/sb17m2/d
  sh c4master.sh s17c3w ffffffdf MGS2_GL_STREAK_CALLS=3 MGS2_GL_RAIN=1`.
- `x2chk.sh`, `x4chk.sh`: x86 twin checks (sb14/chk14.sh) of the two settings; `fdiff.py <log A> <log B> [<log C>]`: frames that differ.
- `lprobe_xconv.diff` + `lprobe.py`: the x86 probe (MGS2_LPROBE=1, xc_line) of where each rain call's streaks fall on screen.
- `patch_rainspr.py` (task #161, stagec_notes.md 4.13): cfg MGS2_GL_RAIN_SPRITES=0 leaves out the sprite calls of the rain's four
  sprite actors (body_sph, charaspread, rain_gas_pers, line_sph) whole; `patch_rainspr2.py`, `patch_rainspr3.py`, `patch_rainspr4.py`:
  x86 check counts only (boot/xlive_chk.c: calls left out with no census block, by actor and part of the route).  `x5chk.sh`,
  `x7chk.sh`, `x8chk.sh`: the x86 twins x5, x7, x8 (/home/claude/sb17/x8 has every patch here).  `models17r.sh`: the model windows
  of the sprite option; `masters17r.sh`, `masters17r2.sh`: its PPSSPP masters (s17r, s17n2, s17r2: more frames).
- `patch_trim159.py` (task #159, notes 4.14): malloc_trim(0) after the texture table grows at the heap's top (PSP only);
  `master159.sh`: master s17t159 (= s17r but the heap's peak, 88 KB lower).
- `pgo_gl.sh`: gl17's PGO and code-order build (copy of /home/claude/sb17/pgo_gl.sh, e64ef2b4): the training run and both traces
  take gl17's turns (the effects off and on, fffff9df,ffffffdf) and the rain as Ffm chose (no streaks, sheets whole, the rain's
  sprites left out, splash 1/2).  `gl17/`: the drivers (gl17_all.sh, gl17_go.sh, pickseed17.sh, gl17_mk.sh, gl17_chk.sh), the cfg
  header and gl17's own cfg lines (dl_cfg.txt), as tools/dev/sb14/gl16/ for gl16 (task #156; work dir /home/claude/sbg17,
  delivery folder /home/claude/gl17).
Game data passes through these builds: personal use only.
