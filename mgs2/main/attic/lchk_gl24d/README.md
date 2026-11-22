# lchk (gl24d): do late walks read RAM that the next step changes?

Debug check made 2026-11-19 before gl24 (two copy areas). Not in the mainline.

Question: with two copy areas the ME walks step k while the main CPU already runs step k+1.
The walk reads from game RAM every byte that its copies do not hold (image data minus the
xc_keep ranges, which are always copied). If step k+1 rewrites one of those bytes before the
ME reads it, the picture is no longer exact.

How: build gl24d (cfg MGS2_ME_PIC_GE2=0, MGS2_ME_PIC_LCHK=1). At each hook the walk is made at
once; every byte range it reads from RAM outside its extents (run, map, lane, tag) is recorded
(merged across kinds, plus a 4 KB page bitmap as a fallback) and hashed. At the next hook the
ranges are hashed again and every change is printed with its vb.

- lchk_apply.py = v1 (record per kind; overflowed: 21,162 ranges not recorded).
- lchk_v2.py, applied on top of v1 = merge across kinds + page bitmap (0 not recorded).
- Apply both to a copy of gl23's ge/ and psp/ (debug tree /home/claude/d24, runtime symlinked).

Result [C, emulator, full route to vb 31,400, build_gl24d/emu/log.txt md5 36b790aa]:
- 7,217 hook walks, up to 15,880 RAM ranges a walk, ~2.0 MB read from RAM a walk (3.4 MB max).
- Changed by the next hook: only the boot (vb 1, run 00c00000-00c40000) and one opening step
  (vb 3815, map 013a73f0-013abdc0, inside xc_keep range 1, so copied anyway in real builds).
- Nothing on the deck. So gl21r's ring failure was not caused by late reads (cause UNKNOWN).
- The page bitmap line ("pages changed 6805") is coarse: a page counts when any byte in it
  changed, read or not; the exact ranges above are the answer.
- Limit [I]: an endpoint check cannot see a byte changed and put back inside one step.

## lchk3 and lchk4 (2026-11-19, for gl25: may a walk run through the next step too?)

gl24 on the PSP showed the walk posted at a hook with records (H1, the end of the full step) is waited for at the next hook (H2, the
end of the lite step). gl25 wanted to let it run on through the next full step. Checks on the emulator, full route to vb 31,400, built
from build_gl24d/stage with only xl_psp.o compiled again (scratchpad l25/relink_xl.sh; a build from gl23's stage objects records
nothing: the recording lives in ge_xlme.o / ge_xlive.o):

- lchk3 (check at the next hook WITH records; build_gl24e, log b87f7f67): changes in 23 walks, most across a turn's hand-over (the
  hand-over drains the walk at its own hook, so those do not count).
- lchk4 (lchk4.py here: check at EVERY hook after the walk until the next hook with records, first change per range with its distance
  in hooks; walks whose turn ended at their own hook dropped; build_gl24f, log 83162c4f):
  - hooks from a walk to the next hook with records: 2 in 7,054 walks, 1 in 3, 9 or more in 8; 148 walks dropped at a hand-over.
  - first change 1 hook after: vb 1 (boot) and vb 3815 (inside xc_keep): as gl24d.
  - first change 2 hooks after: vb 20692 (011a09f0-011a53c0 map + run, 18,896 bytes, the talks; seen at the next hook with records)
    and vb 26008 (00c04140-00c04200, four 32-byte map reads, the deck's start; the next hook with records 60 hooks later).
  - 4 hooks after: vb 34 (boot); 6 hooks after: vb 26008 (431 ranges, 011a0000-011a5400).
- So gl25 as first written (wait only at the next hook with records) is NOT exact on the PSP at vb 20692 and 26008 (the emulator
  cannot show it: its stand-in loop walks at the hook's heartbeat probe). gl25b: a walk may lag at most to the second hook after its
  own, and xc_keep copies 0x0c04000-0x0c05000 and 0x11a0000-0x11a6000. Exact on this route by this check; other routes need lchk4 again.
- lchk5 (= lchk4 with gl25b's two extra xc_keep ranges; build_gl24g, d24/ge/xlme.c, scratchpad l25/relink_xl2.sh): still 2 walks with a
  change 2 hooks after (5 ranges): the GS map's lazy reads go through the view of the record being walked, so a keep range copied with
  an earlier record does not cover them. gl25b's emulator run also changed the picture against the x86 twin (1 of 150 frame digests
  equal, 9 of 10 shots differ from a gl24b run): the keep ranges were dropped. gl25 ships with the two-hook bound only and the two
  frames logged as DEVIATIONS.md C7.
