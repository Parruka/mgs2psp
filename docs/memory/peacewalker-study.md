---
name: peacewalker-study
description: Peace Walker (PSP) engine study for the MGS2 renderer/sound: where inputs are, how to read them, key findings
metadata:
  type: project
  modified: 2026-10-18T19:36:25.353Z
---

Done 2026-10-18. Findings (numbers only) in /mnt/project-files/mgs2/peacewalker_notes.md; no PW code/data in the project (personal use).
- ISO (ULES01372) in Desktop\mg2 port. Stage PRX + kjfs/sound3/zlibdec are plain; main EBOOT decrypted via PPSSPP 1.19 dump (ppsspp.ini `DumpFileTypes = 1`, must edit while PPSSPP is closed; reset to 0 after). Dump + GE frame dumps (.ppdmp v6, zstd) in Documents\PPSSPP\PSP\SYSTEM\DUMP on Ffm's PC.
- NID names: rebuild from pspsdk stub .a files (.rodata.sceNid + .sceStub.text symbols). Parsers were ephemeral (container).
- Key: 30 fps target; 24 MB + 4 MB volatile mem; 5551 double FB + 16-bit Z + ~1 MB VRAM texture pool; vertices in RAM; 15k small skinned strips/frame with HW skinning + lighting; T4/T8 + 8888 CLUT; sceSas 32 voices for SFX.
Related: [[mgs2-port]]
