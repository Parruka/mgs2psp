---
name: ff8-p1
description: FF8 play p1 (delivered 2026-11-17 14:24, THE ONE BUILD OUT, optional for Ffm): first build with the player's own pad, card file, pacing, play-time repair, interp.txt; what was checked, what is UNKNOWN, what to read when files come back
metadata:
  type: project
---

Notes: ff8/stage5_notes.md "2026-11-17 14:24"; sources ff8/src_play_p1; container psp/vw -> psp/rel_p1 (never make there). Follows [[ff8-m7]], [[ff8-m8]], [[ff8-movie-fade-clock]]; layout [[ff8-harness-layout]].

**Why:** Ffm 2026-11-17 13:46 "lts continue". My choice, told to Ffm: first playable build instead of more scripted trials. Told as OPTIONAL (lesson of m8).

**Delivered 14:24** to `FF8\Builds\FF8T_p1\FF8T`: EBOOT.PBP bf06f6ff (9,270,428 bytes), ff8.cfg df012a40, README_p1.txt f0296a95, `mvd\` = the 14 faded d1_*.ffv (284 MB, copied on the PC from FF8\Movies\mvd); md5 list FF8T_p1\md5.txt. Ffm copies it over PSP/GAME/FF8T. Files back to `FF8\LOGS\p1`: log.txt, padlog.txt, interp.txt, a copy of card1.mcd.

**What p1 = m7 +** (cfg keys; all off = m7, proven):
- `padlive 1` PSP keys once a frame (L = L1, R = R1); `nub 1` stick = direction keys, `nub 2` stick left / right = L2 / R2, down = both, `nub 0` off. padlog.txt = scripted-pad format on the frame + pad-shift timeline, plus a `# movie ...` comment line a file movie (pictures, shift, play-time frames).
- `pace 1` no frame under 16,715 us. `card 1` PS1 card slot 1 = card1.mcd (written via card1.new then card1.mcd when the card changed and stood still ~2-4 s, and at HOME; an unusable existing file is never overwritten: saving off). `mvclock 1` play time gets a file movie's length: round(pictures x 3.98843) + 3 frames for disc-made files [I beyond the two measured movies].
- interp.txt at HOME: interpreted blocks by PS1 address (rt.c statics reached through rt_syms.h, written and checked by build.sh); the window's `[pad]` line has the count.
- A last session's log / padlog / interp are renamed to *_N.txt at the next start (sceIoRename: UNKNOWN on the PSP).
- Test keys, never delivered: `padsim <file>`, `cardtest F`, `quitat F`.
- No sound outside file movies; disc 1 only; screen shape 0 (card still open).

**Checked in the emulator on the delivered file [C]:** full route 80/80 (aae53db0), trial route 5a65d45a, model 63.98 / 41.95 / 24.95 / 20.55 (= m7); live path with padsim = scripted hashes, padlog = the script; card written / reloaded / damaged cases; the 14-file set-up (intro from mvd/d1_282220.ffv, 0 late); 3 LOAD segments like m7.

**Settled by the run (see [[ff8-p2]]):** a real save works and the file is whole; fights run at 1.5 fps on the interpreter. Still UNKNOWN: loading a save on the PSP, rename on the PSP.

**When files come back:** read log.txt ([time] per 600 frames, [pad] interp counts, [pace], card lines, movie lines), interp.txt (which modules to translate first; overlays share addresses), record, report. Do not ask Ffm for a run again just to re-check.

**Next (mine, no PSP build needed):** a real save in the PC reference; the PC twin of the play build (runtime with the movie hook, pad shift and play-time count) so a padlog replays exactly and gives module snapshots for gen.py (rt.c pad table = 4,096 lines: enlarge); then translate what interp.txt names.
