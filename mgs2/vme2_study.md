# vme2-next-level study (2026-11-22)

Read only. Source: https://github.com/Th6uD1nk/vme2-next-level, shallow clone of HEAD a9ea35a (2026-11-21). MIT, (c) 2025-2026 m-c/d (mcidclan), Th6uD1nk. Submodules are mcidclan's psp-media-engine-custom-core ("me-core-mapper"), psp-virtual-mobile-engine-ext and psp-media-engine-mist (MIST studied before: me_mist_study.md). No code copied; facts below in our words. [C] = read in its docs/code, [I] = my inference.

## What it is
- A toolkit for the **VME** (Virtual Mobile Engine), the small reconfigurable datapath that sits beside the ME's MIPS core, not for the ME core itself [C README, doc/VMEG.md].
- **vmegc**: a transpiler. Kernels are written in "vmeg", a graph language inside .c/.cpp files, and become plain C over mcidclan's vme-lib [C].
- **vmee**: a Python emulator of the VME (AGUs, DMAC, FUs, 3-cycle op + 3-cycle write pipeline), with traces [C doc/VMEE.md].
- **vmeh / vmegt**: build each `@emulate` kernel into an EBOOT, run it on a real PSP through psplink, dump the buffers, compare word by word with the emulator [C]. Same idea as our emulator-vs-PSP hash test.

## What the VME can do (as documented)
- 4 processing elements, 8 buffers of 2048 32-bit words (TOP_0-3, BASE_0-3), a scratchpad, address generators, a DMAC that loads/stores words, halves (16-bit) or bytes from RAM or the ME eDRAM [C].
- **Integer only**: add/sub, logic, shifts, mul / mac (running inner product), clamp, saturate, small multipliers. No float opcode anywhere in docs or the FU model [C]. Only opcodes the author validated on hardware are exposed (0x000-0x0ff table).
- Kernels can run `async` while the ME core keeps working [C calling.md].
- No throughput figures (cycles per word at a clock, ME-core cost to set up a run) are published [C]. UNKNOWN.

## Does it help MGS2?
- **Not the rain bottleneck [I, high].** The ME time is the walk of PS2 chains: pointer chasing, branches, ~43 of ~58 us per ship group is fixed setup (register, texture, walk, A+D, GS state), PS2 data is float. That is scalar control work; the VME is a fixed-point streaming array with 2048-word buffers. The converter part (4.7 of 74.6 ms ME a ship picture, gl31) is the only streaming part and it is float.
- **Copies [I, low]:** 14.4 ms of the ship's ME work a picture was copies (gl32 split). The VME DMAC could move words without the ME core, but our groups are small (~18 vertices) and the per-run setup cost is unknown; it would need a measurement before any design. Not on the 20 fps plan.
- **Sound mix [I, medium] = the one real fit.** The mix (~7.5 ms a step, in no build yet) is 16-bit integer: multiply by volume, accumulate, clamp. That is what the VME does (Halves transfers, mac, clamp/saturate) and it can run async beside the ME core. Worth a look when sound goes in, as a way to keep the mix off both CPUs. Exactness: integer, so it can be bit-equal to a C reference if the opcodes saturate the same way (their tables give this per opcode).
- **ME eDRAM note [I]:** our start writes Sony's values to 0xBCC00030/40 (VME-side registers, hle_me_mgs2.c ME_SONY_BOOT); vmegc resets the VME to a known state at the start of meLibOnProcess. If we ever use the VME, its start must not disturb our ME setup; read their prologue first.
- **Tooling:** vmee + vmegt is a good pattern (and MIT); we already have our own equal. Their psplink runner would conflict with our PSP rules (no runs without telling Ffm, check the PSP is free) if anyone used it.

## Does it help FF8?
- PS1 is all integer (GTE fixed point, SPU 16-bit), so a better match in principle. But GTE calls come one instruction at a time from game code, too small for a VME run [I]. SPU voice mix (ADPCM decode + volume + clamp) is the same fit as MGS2's sound mix; FF8 field/fight frames are not sound-bound today, so low value now.

## Verdict
Nothing for the current 20 fps work. Keep it as the candidate engine for the sound mix (both ports) once sound is built, after a timing probe (VME run setup cost + words per microsecond on Ffm's PSP-3000). Using the VME is new hardware use on the ME: tell Ffm before a probe runs.
