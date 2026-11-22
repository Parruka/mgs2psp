# ME MIST study: mcidclan's psp-media-engine-mist against our ME sound mix

2026-10-29. Read-and-report only: no project code, build or run was changed. Source: https://github.com/mcidclan/psp-media-engine-mist (MIT, (c) 2026 mcidclan / m-c/d), branch main as served by raw.githubusercontent.com on 2026-10-29 (the GitHub API is blocked here, so no commit id; md5 prefixes: me-mist.c c4d158ac, me-mist-kcall.c 05be7dc8, me-mist-eshot.c be79b174, kernel/kcall-prx.c 0043c634, MIST.md 77f9a365). Files cited as name:line-ish by function. [C] = read in the code, [I] = my inference.

## Is it new to us?

Yes. Earlier notes cite three other repos by the same author: psp-media-engine-custom-core (commit 0197c99, me_startup_study.md and me_switchoff_plan.md: the halt word, the 0xBC100044 wake, the ME interrupt registers), me-feed-graphics-engine (our bring-up's origin, sandstone_review.md:42) and psp-beyond-444mhz (web_research.md:27). web_research.md:45 names "psp-media-engine-safe-task". No note mentions MIST, and it depends on a newer form of custom-core (the "me-core-mapper" headers) than the one we read.

## What MIST is

A way to run your own function on the ME **while Sony's ME firmware keeps running** (MIST.md, README):
- Sony's ME core lives in ME eDRAM; it serves the main CPU's codec requests (sceAudiocodec, sceMpeg, ...) by a "syscall" index read from 0xBFC00600, looked up in a table in eDRAM.
- MIST writes one word into that table (a DMACPLUS word transfer from the main CPU, `mistInjectSyscall`), so an unused index (13 in the sample, 20/21 in the VME sample) points at our function (kernel alias, in main RAM).
- To trigger: fill 0xBFC00600.. with the index, the parameter and a magic word 0xC0FFEE, flush the DDR (0xBD000004), write 1 to 0xBC100044 (`triggerSysCall`). Sony's handler calls our function on the ME.
- The task must end with `meMistFinish()`: it writes -567 to 0xBFC00700 and jumps back into Sony's handler (0x883004C0, hard-coded). `meMistWait` polls 0xBFC00700 for -567, sleeping 1 ms between polls, at most 500 tries.
- `meMistInit` first restarts the ME once in a special way (`mistRefreshMe`): writes 0xBFC0071C = 0x71E6C and 0xBFC00704 = 0x1F, patches three words of Sony's ME boot code at 0xBFC00060-68, pulses the ME/VME/AVC reset bits (0xBC10004C |= 0x34, then 0). This is how it gets the ME's DMACPLUS bus clock on through Sony's own relay word.
- Gains claimed: Sony's ME requests keep working, sleep / resume work (Sony's wrapper is untouched), works over the XMB. Tested on "Slim and Phat"; the VME sample says Slim+ only. Addresses are per ME image (`init`: table 0x003CE870 for the T2 image, 0x0018A210 for the other; two images have none) and one line says "todo: to be tested on phat".
- Kernel mode: either ARK's kuKernelCall (`PRX_FREE=1`, with a full I-cache invalidate before every call) or a tiny kernel PRX it writes to the memory stick and loads (`kernel/kcall-prx.c`, attribute 0x1006, exports kCall_2/kCall_3 as syscalls).
- Also in the repo: "Easy MIST" helpers (macros over the same calls), "eshot" (copies code into the ME core area at 0x88300000 and restarts the ME: a write into Sony's kernel ME area), and a first VME (the ME's DSP) sample: 32 values through a 3-stage Q.23 fixed-point multiply.

## How ours differs (from me_startup_study.md, psp_harness.md, [[mgs2-me-audio]])

- We take the whole ME: Sony's start code at 0xBFC00040 (v30me6b/c), our mix loop runs, it HALTs when idle and the main CPU wakes it with 0xBC100044 = 1 in kernel mode via kuKernelCall (k_kick at its kernel alias). Sony's ME firmware is gone while we run; we hold scePowerLock, so no sleep.
- v30me6c: 3/3 PSP runs to the end, exact. The wake costs 136 us (ARK's kuKernelCall flushes caches twice), 1.07 wakes a step = +0.16 ms a step (psp_harness.md:365, :387).

## What could help the port

**A. Movies (later, when PSMF lands).** The plan plays MOVIE.DAT as PSMF through sceMpeg (feasibility.md:101), and sceMpeg needs Sony's ME firmware, which our takeover removes [I: sceMpeg sends its decode requests to the ME through me_wrapper, me_startup_study.md Q1]. Two ways then:
- A1: give the ME back to Sony for a movie (re-boot Sony's image, play, take it again). Fewer new parts, but a takeover/give-back cycle we have never run [I].
- A2: MIST: Sony's firmware stays, our mix becomes a MIST task. Movies and mix coexist, sleep could work.
- Nothing to do now: movies are not on the road map yet.

**B. A cheaper wake (task #57).** MIST's kcall PRX shows the plain way to run a kernel function from user code without ARK's kuKernelCall: a small kernel module that exports a syscall and calls the function directly. Our k_kick would cost a syscall plus a few instructions, about 5-10 us instead of 136 [I: not measured], about -0.14 ms a step. Small (0.2-0.4% of a step), and a new kernel module needs Ffm's OK. Their PRX also invalidates the whole I-cache on every call; ours would not need that (k_kick is fixed code).

**C. Sleep / resume in play.** Only MIST keeps Sony's sleep path whole. Ffm has not asked for sleep; our power lock blocks it. A feature to note, not a need.

**D. The VME (ME DSP).** A first working pipeline, but undocumented and Slim+ only. Our mix already costs the main CPU nothing but the wake, so the VME would not raise the frame rate. Not useful now.

**Not a speed path for the game.** MIST changes how a task reaches the ME, not what the ME can do. It does not move game logic or drawing off the main CPU (where the 78 / 37 ms steps are).

## Risks if we adopted MIST for the mix

- Many new register writes, each needs Ffm's OK: 0xBFC0071C, 0xBFC00704, three words of Sony's boot code at 0xBFC00060-68, the reset pulse 0x34 (VME and AVC with the ME), DMACPLUS registers, 0xBD000004. Plus a write into Sony's ME syscall table.
- Hard-coded per-image and per-firmware addresses (table base, handler 0x883004C0, the 0x71E6C word) that we cannot check on Ffm's 6.61 PSP-3000 without running it [I]; "todo: to be tested on phat" shows they are still moving.
- Timing: the stock wait sleeps 1 ms per poll (we would write our own); each trigger still goes through kuKernelCall (136 us) unless with the PRX; a mix task blocks Sony's own ME requests while it runs.
- It replaces a design that just passed 3/3 exact runs, and the switch-offs turned out to be the cheap battery pack, not the ME start ([[mgs2-switch-offs]]).
- The authors call it the "author's understanding of the hardware at the time"; the library "is regularly updated".

## Recommendation

1. **Do not change the mix to MIST now.** Ours works, is exact and costs 0.16 ms a step; MIST would add many unverified register writes for no speed gain.
2. **Keep MIST as the main candidate for movies (A2)**, to decide when PSMF work starts, with A1 studied beside it. Re-read the repo then (it changes often).
3. **Optional, small: the cheap wake (B)** as task #57, only with Ffm's OK for a tiny kernel module of our own (no MIST code needed, only the idea). Gain about 0.14 ms a step [I]; worth it only once bigger items are done.

Credit: method and code by mcidclan (m-c/d), MIT License; any code we take keeps their notice and license.
