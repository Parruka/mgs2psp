# SLES_503.83 (PAL ELF): facts

Source: Ffm's upload 2026-10-17, copied to `/mnt/project-files/mgs2/disc/SLES_503.83` (833,792 bytes, SHA-1 bc6911e31282231882a5d7ec2e7744c5e59bca0c). Measured with readelf and Python. Game data: personal use only.

- SYSTEM.CNF: `BOOT2 = cdrom0:\SLES_503.83;1`, `VER = 1.01`, `VMODE = PAL`.
- ELF32 MIPS R5900 (flags 0x20924001: 5900, eabi64, mips3), stripped, statically linked. Entry 0x100010.
- One LOAD segment: file offset 0x80 → vaddr 0x100000, filesz 0xb08b8, memsz 0xedbb4 (image ends 0x1edbb4). **Same 0x100000 / 0x80 layout that TT's `recomp.py` assumes**, so `W()` works unchanged.
- gp = **0x001b88f0** (`.reginfo`, last word).
- Sections: `.text` 0x100000 size 0x7cf78 (512 KB, 127,966 instructions); `.vutext` 0x17cf78 size 0x18c; `.data` 0x17d180 size 0x16cb4; `.vudata` 0x193e50 size 0x19b80; `.rodata` 0x1ada00 size 0x2ea0; `.bss` 0x1b0900 size 0x3d29c.
- `.text` opcode counts: COP1 5,416; COP2 macro 863; LQC2 391; SQC2 221; LQ/SQ 1,358; MMI 229; VCALLMS/VCALLMSR 19 call sites.
- **VU1 microcode:** 14 programs in 56 DVP overlay pieces (102,896 bytes ≈ 12,862 instruction pairs), stored in `.vudata` and uploaded in ≤2 KB pieces up to VU micro address 0x3800 (beyond VU0's 4 KB, so VU1). Table: `.DVP.ovlytab` (12-byte entries: name, RAM address, VU address).
- Debug/assert strings name 22 source files: strctrl.c bmbctrl.c bul_c4_mng.c camera.c camerad.c caption.c dgd.c dumstream.c gaged.c gamed.c gvd.c hzxd.c jimaku.c loader.c map.c mapcnct.c menupri.c sdstream.c sprite_dma.c sunglasses.c target.c vibrate.c.
- Libraries named: `PsIIlibgraph2300` (SCE libgraph 2.3.0), libpad, libmc.
- Disc paths in the ELF: `cdrom0:\MGS2\%s;1` (stage.dat, demo.dat, vox.dat, face.dat, codec.dat, movie.dat), `cdrom0:\MODULES\IOPRP23.IMG;1`, `MODULE.IPK;1`, `PLD.IRX;1`.

## What this means (inferred)
- The ELF holds only 512 KB of code. The NTSC-U notes put per-stage code images (`b` files in STAGE.DAT) right above the ELF image (w00a at 0x1F1480; PAL ELF ends at 0x1edbb4). Much of the gameplay code probably lives in those images, so STAGE.DAT is needed before the recompile set can be sized.
- VU1 microcode is the original renderer; like TT it stays on the presentation side and is replaced by the GE renderer, but the 14 programs show what each draw path does.

## Boot, threads and stage linkage (read from the disassembly, 2026-10-17)
- crt0 at 0x100010 (standard: clear bss, SetupThread gp=0x1b88f0, SetupHeap), then `main` 0x1001a0.
- `main` creates the game thread (entry 0x1000d0, priority 0x12, stack 0x4000) and then sleeps in a loop. The game thread runs init calls, then loops forever on **0x13f140**.
- 0x13f140 is the actor scheduler: 12 actor lists of 0x64 bytes at 0x1b9d30, each skipped when its bit is set in the pause mask at 0x1e6c38; each actor's `act` (offset +4) is called through `jalr`. (Same design as MGS1's GV actor system.)
- Stage image linkage: 0x1060f0 clears the image's bss (header words 1..2) and stores the export-table pointer (header word 3 = 0x1f1c10) at 0x1cc8c0; 0x106130 looks up a function by 24-bit hash in that table. Exports per image: w00a 148, w11a 179, init 10.
- Stage code calls the ELF directly: w00a has 14,398 `jal` into the ELF and 14,960 into itself; w11a 19,396 / 18,790. The ELF reaches stage code only through the hash exports.
- So: ELF = engine and libraries at fixed addresses; each stage image = game code linked against it.

## Frame pacing (read from code; to be confirmed by running it)
- VBLANK-start handler 0x11d930 (AddIntcHandler cause 2) increments counters at 0x18db60 and 0x18db64 and sets event bit 1. VBLANK-end 0x11d9b8.
- 0x11d9d0 (end of frame): stores elapsed vblanks since the last frame at 0x18db6c (minimum 1), then waits:
  - normal path 0x11d720: wait for event bit 1 (the next vblank) or bit 3 (GS done), with a timeout;
  - if the word at 0x18db74 is non-zero, 0x11d7f8 waits until the vblank counter reaches that value. Only stage code writes it (170 sites in 108 stages; in w00a one site at 0x3c7530 restores a saved value).
- The elapsed-vblank count is read in the ELF only at 0x1058e0, where it is added to the play-time clock (+0x130 of the game state). Stage code reads the raw counter 0x18db64 in 15–16 places per image.
- **Conclusion (inferred from the code):** game logic steps once per loop, with no time delta, and the loop waits for at least one vblank. On PAL that is up to 50 logic steps per second; a slow frame slows the game instead of skipping time.

## Boot sequence (seen on the x86 probe, mgs2/boot/boot.c, 2026-10-17)
- crt0 (0x100010): SetupThread(gp 0x1b88f0, stack 0x1fff000, size 0xff0), SetupHeap(0x1edbb4, 0x4000), then libkernel start-up.
- libkernel patch (0x174490): SetSyscall(0x5a, word copy 0x174448), copies 0x740 bytes to 0x80076000 and 0x28 bytes to 0x80082000, sets syscall 0x5b to 0x80076000, then re-points the alarm calls 0xfc/0xfe/0xfd/0xff (and 0x12c, 0x08) to it. The HLE implements the alarm calls itself and ignores the patch.
- libkernel creates a helper thread (id 2, priority 0, stack 0x400) that waits on semaphore 3; the main thread drops to priority 1.
- main (0x1001a0) creates the game thread (id 3, entry 0x1000d0, priority 0x12, stack 0x4000) and loops on SleepThread.
- Game thread: EnableCache, then GS set-up with GIF DMA chains (tag at 0x1b40b0, waits on D1/D2 CHCR STR at 0x1617e0), then SIF set-up: syscall 0x78 sceSifSetDChain from 0x175954. The IOP side starts there.

## Boot to the Tanker start (x86 probe, 2026-10-18)
- Stage order seen: `init` -> `r_title` -> `n_title` (title, menus) -> New Game -> `r_tnk0` (Tanker resident) -> `d00t` (Tanker opening demo).
- Menu input (PAL): START at the title, then CIRCLE confirms (X backs out). Scripted with `MGS2_PAD="1300:0800,1306:0,1500:0020,..."` (vblank:mask, see runtime/iop_host.h pad_script); the probe got from title to `r_tnk0` with START + CIRCLE every 150 vblanks.
- The title waits on the sound driver status word 0x1c6d80+4 (bits 0x80000007), which iop_main updates from its 2.5 ms hardware-timer tick (IOP timer compare 92160 at 36.864 MHz). The runtime now runs IOP timers inside each 20 ms vblank step (runtime/sif.h sif_vblank).
- End of frame waits for GS FINISH (handler 0x123b70 on INTC 0, IMR 0xfdff) with a 13064-hsync timeout; a timeout resets VIF1 / GS as a hang. FINISH comes from an A+D write of register 0x61 in the GIF stream (path 2 VIF1 DIRECT and path 3), modelled in runtime/ee_io.h.
- Alarms as the game's libkernel numbers them (wrappers 0x173560..0x1735d8): SetAlarm 0xfc, ReleaseAlarm 0xfd, iSetAlarm -0xfe, iReleaseAlarm -0xff.
- Deci2Call 0x10 (kputs) is the game's printf; the probe prints it as `kputs:`.
- GetOsdConfigParam2 (0x6f) is called by n_title when the OSD version field is nonzero; answered as zeros (a console setting, like the language).
- mcserv RPC 0x15 = slot max (disc mcman export 26 -> sio2man export 59, returns 1 with no multitap module).
- Movies: the title's intro (MOVIE.DAT offset 814526464, 99,732,160 bytes, chunks {type, size}: 0x10 header, 0x0e video (MPEG-2 PS), 0x01 audio, 0xf0 end) is decoded on the EE main thread through the IPU (start-code search at n_title 0x32dac8 spins without an IPU model). The probe's dev switches MGS2_SKIP_MOVIES / MGS2_SKIP_VOX serve end-of-stream chunks instead. The PSP plan plays movies through PSMF, so the IPU is not modelled.
- Stage export tables (header word 3 -> {hash, address} pairs) can name functions that follow the last `jr ra` (r_tnk0 0x1f2320 ends in a tail `j`); survey.py now extends the code region to them.

### VU0 and the Tanker opening (2026-10-18)
- d00t calls VU0 micro programs (P12cb58 -> P12c908 -> P12bb08, VCALLMS). The micro code arrives by VIF0 DMA (first transfer: normal mode, chcr 0x141, madr 0x194800, 47 qw). ee_io.h now runs VIF0 transfers through vif.h, and maps VU0 micro memory (0x11000000) and data memory (0x11004000) to VUPROG / VUMEM. Before that, every call ran the 200000-step interpreter guard on empty micro memory.
- Split build: the VU0 state in vu0.h (VF, VN, VI, ACC, Q/P/I/R, MAC, STATUS, CLIP, CMSAR0, VUMEM, VUPROG, vu_tpc, g_vmac, vu_prog_clean/hash) is GSV, so all units share one copy.
- DEMO.DAT: with zeros, the stream reader hangs after sector 692524 (synthetic layout). With MGS2_SKIP_DEMO (end chunks {0xf0,0x10,0,0}) the game keeps rendering frames in d00t but never leaves it, START and CIRCLE do nothing, and no further reads are made. The DEMO.DAT chunk format is not known yet, so this needs the real d00t range from the disc.

### Tanker deck w00a and the floor test (x86 probe, 2026-10-18)
- Stage detection: the mkstages.py code signature word is the first instruction of the first translated function at +0x8000 or later (a fixed +0x8000 hit data that w00a overwrites, so JALR failed at 0x2dfdc0).
- Actors: struct +0 next, +4 act, +0x14 die, +0x28 name; lists at 0x1b9d30 + 100*i (i 0..15). Player "raiden.c" at 0x142b640, control at actor+0x40 (pos +0, floor/ceiling levels +0x90/+0x94, map mask +0xa0). Game over = actor gm_over.c (act 0x2c75c8, made by GCL export hash 0x5a4652), VOX #0x76.
- Floor query 0x14d860(mask, ctrl, flags, flags2): HZD grid at *(0x18f4f0); 0x14d1c8 per-polygon bbox; 0x14cf70 / 0x14d000 build a VIF0 chain in SPR 0x70000520 (position to VU qw 0x30, STROW cell origin, header to qw 0x33/0x93, int16 vertices, MSCAL pc 0 first batch / 6 next); 0x14d510 sends it and reads VU0 data 0x11004320 (flags: 1 floor, 2 ceiling), 0x11004200 (floor), 0x11004100 (ceiling).
- Snake fell through the deck (Game Over at vblank 24999) because of four emulation bugs, all fixed: VU R-register ops missing (RNEXT/RGET/RINIT/RXOR); VU pipeline not modelled (vu0.h now lands FMAC results and MAC/status/clip flags 4 cycles after issue, Q after 7/13, with stalls on in-flight VF reads); JR/JALR used VI>>3 instead of the instruction index; COP2 control CMSAR0 is register 27 (not 29), VCFC2 26 = TPC; and ee_io.h mm_map only mapped 0x11004000-0x11004fff when masking with ~0x3fff, so EE reads of VU0 data memory went to the I/O shadow (now ~0x7fff).
- VF00 (2026-10-19): the VU's VF00 is the constant (0, 0, 0, 1.0); boot.c never called vu_reset, so micro programs and the macro ops vuemit.py does not specialize read VF00.w = 0 (every `div q, vf00w, x` gave 0). Fixed (boot.c, `MGS2_NO_VF00=1` for the old state); same stage sequence, 24% fewer VU0 steps, the deck diverges from about vb 23900. New reference `boot/hashes_x86_rz_vf0.txt` (speed_notes.md §6).
- FCGET (2026-10-19): the lower op FCGET (0x1c) reads the clip flags & 0xfff (VU manual; PCSX2 _vuFCGET); vu0.h read the status register. Fixed (`MGS2_OLD_FCGET=1` for the old read). Used by the culling programs 148d7d43 / 8baafbc3. Same stage sequence, hashes differ from vb 4300, VU0 work to vb 25200 9.23 M calls / 598 M steps. Current reference `boot/hashes_x86_rz_fc.txt` (both fixes).
