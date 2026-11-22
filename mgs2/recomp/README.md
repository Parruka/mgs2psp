# mgs2/recomp: the TT recompiler, re-targeted to MGS2 PAL

Copied from `tt_port/src/model/` on 2026-10-17 (the TT copies are untouched). Runtime headers: `mgs2/runtime/` (TT `guest.h`, `vu0.h`, `vif.h` with the TT-only includes made optional, plus the new `mmi.h`).

## What changed from TT
- `recomp.py`: `W()` reads the ELF (`SLUS`, default `SLES_503.83`) and, with `STAGE=<stage>.bin`, one stage code image at its header load address (0x1F1C00). Same 0x100000 / 0x80 ELF layout as TT.
- New instructions (runtime in `mgs2/runtime/mmi.h`): syscall, break, mthi/mtlo, COP0 (mfc0, mtc0, ei/di/eret as host hooks), LWL/LWR/SWL/SWR/LDL/LDR/SDL/SDR, MULTU1, MTSAB/MTSAH/MFSA/MTSA, and the MMI ops the game uses: PLZCW, PSUBW, PSUBB, PADDH, PADDSH, PADDUH, PADDUB, PMINH, PMAXH, PCGTH, PEXTLW/UW, PEXTLH/UH, PEXTLB/UB, PPACW/H/B, PXOR, PROT3W, PCPYH, PEXEH, PMULTH, PDIVBW, PMFHI, PMFLO, PMFHL.LW/UW, QFSRV, PSLLH/PSRLH/PSRAH/PSLLW/PSRLW/PSRAW.
  Semantics written from the EE instruction definitions and PCSX2's behaviour for LWR; **not yet checked against an oracle.**
- `funcs.py` (new): function discovery for stripped code. Seeds: entry, jal targets (only amid valid instructions), stage hash exports, data words and lui+addiu/ori pairs that point right after a function end, tail-jump targets, and code after a return that nothing before it branches past. Each function's end is the end of its reachable code, so data inside code regions is left out. Switch tables (GCC `lui / addu / lw / jr` and `lui / addiu / addu / lw / jr` forms) are found from the code and kept out of the pointer scan.
- `survey.py` (new): runs discovery + `gen_func` on everything, adds jump tables and register tail jumps automatically, turns jumps out of a function into tail calls (tailfix), and reports what fails. With an output file it writes compilable C: prototypes, the functions, `gen_init()` (switch tables into guest memory) and `gen_call(addr)` (calls through a register).
  `SLUS=SLES_503.83 [STAGE=w00a.bin] python3 survey.py out.h`

## Status (2026-10-17)
- ELF: 2,222 functions (483,268 bytes of code), all translate.
- All 123 stage images, each surveyed together with the ELF: every function translates (w00a + ELF 8,667 functions, w16b + ELF 10,199).
- ELF + w00a C compiles and links on x86 (gcc, -w, -lm) against `mgs2/runtime` (VU0 micro calls fall back to the interpreter until vurecomp runs on MGS2's programs).
- `mgs2/boot/boot.c` runs the recompiled ELF on x86 with `runtime/kernel.h` (threads, semaphores, interrupts, alarms) and `runtime/ee_io.h` (memory map, registers, DMA). It gets through crt0, the libkernel start-up, the thread set-up and the first GIF transfers, and stops at the first SIF call (syscall 0x78, sceSifSetDChain): the IOP side is next.

## Not done yet
- SIF and the IOP side (sifcmd / sifrpc servers, module loading, the game's iop_main RPC), CD reads, GS / VIF execution of the DMA chains.
- VU0 micro programs (`vurecomp.py`) not run on MGS2 yet; VU1 programs stay on the presentation side.
- Oracle check of the new instructions (TT `emu.py`, or PCSX2 over PINE per `mgs2/sandstone_review.md` §2.5).

## Register cache (regcache.py, narrow.py) on MGS2 (2026-10-18)
The x86 probe built with regcache on the boot-path units (ELF, init, r_title, n_title, r_tnk0, d00t, w00a; RC_KEEPMASK=1) gives the
same state hashes as the plain build every 100 vblanks to vb 25200 (Tanker deck; boot/hashes_w00a_x86.txt).  MGS2 fixes:
- MMI helpers M_<op>(rd, rs, rt) and the GLDL/GLDR/GSDL/GSDR/GLWL/GLWR/GSWL/GSWR helpers: register numbers, pure.
- IRQ_POLL is a call that reads every register (an interrupt handler's stack frame saves them: exact values) and writes none.
- A call inside a branch on one line (IRQ_POLL / BREAK in `if (c_) { ... }`) leaves the fallthrough dirty, GPRs and FPRs alike.
- COP0R is pure: a non-pure call used as a value would get its reloads after it in the comma expression (the value becomes the last
  reload); regcache now refuses that case.
- narrow.py: S_ / O_ / M_ / SYSCALL / BREAK / IRQ_POLL / COP0OP count as calls (a dsll32 value passed to an S_ tail call stays 64-bit).
Bisecting: elf_try.sh / stg_try.sh style builds (one unit rc, the rest plain), EE dumps (MGS2_EE_DUMP) to find the first differing
vblank, then a gdb hardware watch on the byte.  MGS2_FN_ALL=1 with MGS2_FN_TRACE_VB=a:b (boot_hook build) prints every ELF function
entry with the virtual time.

## Speed passes (2026-10-19), all exact (x86 hashes identical to vb 25200)
- `vupipe.py`: VU0 micro programs compiled with the vu0.h interpreter's pipeline timing (input: a `-DVU_PROFILE` run's `MGS2_VU_PROF=<dir>:<vb>`
  images and entries.txt). JR / JALR compile when the target is known (BAL / JALR links tracked per path); entries whose compiled body is the
  same in several images share one function. Check with `-DVU_CHECK` (dead flags) or `-DVU_CHECK -DVU_CHECK_FLAGS -DVP_EXACT_FLAGS`.
  Since the FCGET fix (vu0.h: FCGET reads the clip flags) no compiled program on the boot path reads the status flags (no FSEQ / FSAND /
  FSOR, EE code never reads VCFC2 16), so builds leave `VP_EXACT_FLAGS` off.
  Inputs that are not known VU-form values are clamped once per path. `-DVP_FAST` (2026-10-19): no result clamps (the FPU mode, round toward
  zero + flushed denormal results, already gives the clamped value); VU_CHECK on x86 to vb 25200: 8,888,503 calls, 0 differ. PSP code of
  the compiled set (vp9): 993 KB before, 789 KB exact, 341 KB fast.
- `idioms.py`: library routines found by their exact words get `GEN_IDIOM_<NAME>()` on their FN_ENTER line (recomp.py; `tools/idiom_apply.py`
  for existing headers); boot.c runs them natively when it can reproduce the translation exactly. QCOPY: the qword copy in 108 stage images.
- `tools/poll_fast.py` (after regcache / prune): loop back-edge polls become gen_pre.h `IRQ_POLLF`, the inline `k_time` advance with the
  register stores and the kernel work only past kernel.h `k_poll_at`.
- Order for the PSP headers: regcache.py, prune_cov.py, poll_fast.py, idiom_apply.py.
- v16 (2026-10-20), last, on the x86 and the PSP headers alike (both idempotent): `tools/fpu_zero.py` (FPU add / subtract through guest.h
  FADD / FSUB: the PSP's zero rule modelled on x86, plain operators on the PSP) and `tools/vgrp_fcl.py` (the result clamps of vu_vgrp
  fallbacks become vu0.h VU_FCL, valid under the VFPU's rounding). speed_notes.md §24.
