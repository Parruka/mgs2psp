# gl25 review context (MGS2 PS2 -> PSP static recompile; picture made by the PSP's Media Engine (ME))

Trees: /home/claude/t24b = gl24b (the build on the PSP now), /home/claude/t25 = gl25 (gl24b + the patch).
Patch: /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/l25/gl25.diff (made by gl25_apply.py next to it).
Main CPU side: t25/psp/xl_psp.c (mq_* = the queue to the ME; two_* = the two copy areas; mq_frame = the vblank hook; mq_chk_end = a
DMA record's copy plan as its transfer ends; mq_late = FILL + WALK post; mq_fill; mq_wait; mq_probe; mq_takeback*; mq_turn_hook; mq_sync).
ME side: t25/ge/xme.c (xme_entry, xme_rec, xme_fill, xme_walk, xme_hook, xme_run). Shared struct: t25/ge/xme.h.

## How it works
- The game runs two steps per picture frame. The FULL step runs the PS2 draw pipeline and emits DMA records (mp_rec[]); it ends at
  a vblank hook with records (H1). The LITE step runs game logic only, no records; it ends at a hook without records (H2).
- Each record's DMA chain is copied (an "early copy") into the ME's arena when the ME processes its REC entry (xme_rec: xm_fill copies
  from RAM as it is then). Writes the game makes to marked ranges after the transfer ended are logged (old values); the FILL (posted at
  the next records hook, main CPU waits for it synchronously, MW_FILL) lays the log over the early copies so they hold the bytes as the
  transfers started. Then the WALK entry walks the step's records (VU1/GS emulation into GE lists). The walk ALSO reads from game RAM
  every byte its copies do not hold (image data outside the xc_keep ranges): these are "late reads".
- Queue: a ring, entries processed by the ME strictly in order (tail). mq_post returns the entry's seq; mq_done(seq) = tail passed it;
  mq_wait(seq, why) blocks (serving the ME's requests, with take-back on a lost heartbeat).
- ME turns and main CPU turns alternate every 100 vblanks (MGS2_ME_PIC_TURN); at a turn change a SYNC hand-over empties the queue.
- gl24 (two copy areas, MGS2_ME_PIC_TWO=2: in every other ME turn): the arena and its extents are split in two halves. A step plans
  its copies into one half while the walk posted at the last records hook holds the other (two_pend[h]). gl24 waited at the START of
  EVERY hook for the last walk (mq_arena_waitw(MW_HOOK)). Since the walk is posted at H1, that wait sat at H2 (after the ~20 ms lite
  step) and the walk takes ~67 ms per frame: no gain on the PSP (deck 49.5 ms a step in both).
- gl25: in two-area turns a hook WITHOUT records does not wait (two_poll only). A hook WITH records waits for the walks before
  (mq_arena_waitw(MW_HOOK)), before its own FILL and WALK. The copy plans wait only for the walk that holds the half they need
  (two_seq[h], two_wait). two_poll empties halves whose walk is done (non-blocking) and moves the step to the other half when its
  current half is held and the other is free.
- Exactness argument for late reads: a debug build (lchk3, /home/claude/d24, check at the next records hook) hashes every RAM range a
  hook walk reads outside its copies right after the walk, and hashes it again at the next hook with records. If nothing changed
  there, a walk that runs any time before the next records hook reads the same bytes. gl25's records-hook wait bounds the walk to that.
- Exactness is proven by state hashes (game state only) + frame digests + shots vs the x86 reference; the emulator (PPSSPP) runs the
  ME loop as a low-priority thread of the main CPU and has NO cache model; only the real PSP shows cache/timing bugs.
- Hard rule: never write the PSP's flash. Nothing here does.
