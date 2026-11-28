---
name: ff8-timing-calibration
description: FF8 option A timing calibration (done 2026-11-10 21:20): what each part matches, Mednafen gotchas, tools, new reference hashes and pad script
metadata:
  type: project
  modified: 2026-11-10T21:00:14.298Z
---

Notes file: /mnt/project-files/ff8/timing_calibration.md (keep current; sections 1-12). Part of [[ff8-port]].

- CD drive model 2 (ref/cdrom.c, Mednafen 1.29's drive facts): commands within ~1% of Mednafen after the hand-off.
- Video clock: 566,204 cycles a frame (NTSC 53,693,175 Hz, lines 3412/3413).
- BIOS chip fetch 24 (DuckStation/hardware-doc formula; Mednafen uses 4). This is why BIOS-side code (boot, card IRQ path, a BIOS routine the pad driver calls) runs slower than in Mednafen: deliberate.
- CPU model 2 (default; --cpu 1 old): within 1-2% per instruction of Mednafen on game code.
- DMA model 2 (default; --dma 1 old). GPU model 2 (default; --gpu 1 old): same cost per command, boot lists within 0.1%.
- SPU status delay done. The old "SPU wait" (0x8003a1f8) is the game's pad byte exchange; it matched once Mednafen had port 2 empty.
- MDEC timing (mdec.c, part of DMA model 2): light movie frames within 0.2%; heavy frames ~4% faster than Mednafen because Mednafen's DMA steps by 128 cycles (MTGRID=1 reproduces it); we keep no step (real refill delay UNKNOWN, needs a real console).
- Controller/card port (sio.c rewritten 2026-11-10 21:00): Mednafen's bit-level model in our words; default = blank card in slot 1, slot 2 empty (--card1/--card2 none|blank|FILE, --cardsave1/2). 2,938 boot port bytes identical to Mednafen; a card adds 71 start-up frames in both.
- Mednafen gotchas: memory card settings only work from medhome/.mednafen/mednafen.cfg (now port1.memcard 1, port2.memcard 0), not the command line; use -psx.input.port2 none and an absolute -psx.bios_na path. medhome/.mednafen/sav/*.0.mcr = blank card (backup cal3/medsav_backup).
- States made before 2026-11-10 21:00 do not load (new ST vars). Movie states: cal3/mv_15100.st is now stale too; Mednafen's cal3/med_mv15000.mcs was made with no card.
- Tools: logging Mednafen /home/claude/ff8/medbuild/src/mednafen (patch + README in /mnt/project-files/ff8/tools/), psxref switches listed in the notes' Tools section (CDLOG, DMALOG, MDLOG, SIOLOG, SIORLOG, PCWIN, GTLOG, GCLOG, --iprof). Shared ref sources copied and md5-checked at 21:27.
- GTE 00h/1Ah: N/A in the hardware notes; FF8 never issued them to frame 40,000; ours warns if one appears.
- DONE 21:20: new pad script (+141, name fixed), hashes (2 runs + resume from state identical), states3/. Left (optional): movie-start and heavy-field comparisons with Mednafen under the new script; battles not run yet.
