---
name: ff8-disc-modules
description: FF8 code coverage without play (Ffm's rule 2026-11-17 21:06): game code files translated straight from the disc (d_* modules), clusters, PSP code overlays loaded from the memory stick; tools, sizes, what is left (stages, effects)
metadata:
  type: project
  modified: 2026-11-17T22:44:54.414Z
---

**Ffm's rule (2026-11-17 21:06):** Ffm will NOT play the whole game to feed coverage. Coverage comes from my side; their play logs are a bonus. Follows [[ff8-p3]], [[ff8-translate-rounds]]; first build with it: [[ff8-p4]].

**Disc code files [C]:** `xcode.py` (src: /mnt/project-files/ff8/src_p4) reads disc1_user.bin (FF8DISC1.IMG at LBA 826, never written out) by the decompile's lock file, unpacks LZS: 368 code files (25 main + 343 mag effects), every sha1 = the decompile config's, each = the code in PS1 RAM word for word. Container /home/claude/ff8/ovl (game data, container only; mag*.bin deleted for disk, re-made in seconds). Decompile config (CC0) in src_p4/decomp_cfg.
- Battle stages a0stg000-162.x (LZS, load 0x8013E000) hold ~1.4 KB of code each (stg.py). Effects: 343 files, mostly 0x801A0000, 11.6 MB of MIPS (each with copies of shared lib code).

**gen.py:** `--disc ovl/disc_mods.txt` (mkmanifest.py) adds d_<file> modules, translated whole, never pruned or clustered; with --disc the exe is never pruned either (`--prunedexe` off). Seeds: decompile func symbols, code-range starts, stack set-ups after jr/j, run-time seeds, snapshot coverage by content. Guessed pointers only inside the config's code ranges (the config misses some code: field 0x800982F0). Snapshot functions fully covered by disc code are left out. Clusters: split at gaps > 0x4000 and at the load addresses 0x8013E000 0x80190000 0x801A0000 0x801B0000 0x801E2800 0x801E5800 0x801E8000 0x801EF800. 51 modules (limit 64: rt_codemap u64).

**Size [C]:** the whole translation is 31.4 MB of objects. PSP CODE OVERLAYS (`--ovl`): the 7 modules at 0x80098000 each go to ovl/<module>.bin, linked at rt_ovl_area (0x640000 = 6.25 MB, NOBITS section .ovlbss after *(COMMON)). rt_l.c ovl_fn loads a file at the first lookup that needs it, only while the loaded module is invalid; otherwise the block is interpreted (exact). File header RtOvl {magic 0x314C564F, nfn, nict, id = rt_build_id, fn, ict}; the build id is a random --defsym shared with the overlay links. Harness logs `[ovl] name: bytes in us` (the emulator's 75,705 us is fake; PSP time UNKNOWN until p4's log).
- Build: the scratchpad's p1/l5.sh (copy in src_p4) `G=gendkp OVL="d_battle ..." SHIFT=0x10000`, rodata dR 0x21c000; it moves overlay objects to vy/ovlobj (a file named in the link script is linked even when not on the command line).

**Left:** stages and effects as overlay families (one code map bit per load address, because of the 64-module limit); menus as overlays; smaller code-word tables; loading while the PS1 reads the file from CD; a session that casts magic to measure effect cost.
