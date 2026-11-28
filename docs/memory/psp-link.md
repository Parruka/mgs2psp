---
name: psp-link
description: PSPLink USB control of Ffm's PSP from the PC: who may use it, setup facts, and the safe way to run an MGS2 harness build through it
metadata:
  type: reference
  modified: 2026-11-27T14:13:00.000Z
---

- Works since 2026-11-19 10:24: PSP 6.61 ARK-4 seen from Ffm's PC through Desktop\mg2 port\utils\psplink (usbhostfs + pspsh, WinUSB driver via Zadig, device "PSP Type B"). Memory stick + USB only, never flash.
- WHO (Ffm 2026-11-19 10:16): only the MGS2 port thread may use it; FF8 only when Ffm says so. [[mgs2-rules]]
- SHARED (Ffm 2026-11-19): FF8 may run ONE build, MGS2 has priority; MGS2 never touches FF8 folders.
- If LOGS\<ver> holds Ffm's marked copies, give `update` a fresh `fetch` dir (LOGS\<ver>\refetch).
- REMOVE ALLOWED (Ffm 2026-11-27 14:11, project chat): "i let you use the psprun.py remove" (old builds off the PSP, no rename trick). The RC session refused it before and did not see this word: put "remove" in the run words Ffm types.
- AFTER A STOP (2026-11-27 gl42in r1): a second start of the build OVERWRITES log.txt/battery.txt/crash.txt. Tell Ffm each time: after a freeze start ONLY PSPLink, never the game. Fix owed in the harness (keep an unfinished log).
- LAUNCH GO-AHEAD: the RC session gets ONLY Ffm's own thread messages (send_message to it is refused, 2026-11-24); its auto-mode needs Ffm's own run word. So any detail it needs (e.g. logs to LOGS\glNN\r2) goes in the words I ask Ffm to type. Still tell Ffm before each launch.
- PSP FREE CHECK (Ffm 16:48, standing rule for both ports): before ANY step on the PSP (update, stage, launch, remove), check it is free: `psprun.py sh "modlist"` shows no game build loaded, and no run in progress on the stick (a log.txt without its end line in any MGS2T_*/FF8 folder). If a build runs, wait.
- WHO RUNS (Ffm 10:46): the link thread only automates PSPLink and hands over its tools + procedure; then each port thread launches its own runs and copies its own logs (MGS2: its own Remote Control session on the PC, briefed with that procedure).
- Run rules: start from ms0:, never host0:; never reset/kill/reload a running build (the ME runs our code; a forced stop can freeze the PSP); the build ends on "Press X to exit"; copy logs only after the end line, copy never move.
- 2026-11-19 11:32 link READY for MGS2 (link thread): `fetch MGS2T_<ver> "...\LOGS\<ver>"` only copies; `remove MGS2T_<ver> "<logs dir>"` deletes ms0:/PSP/GAME/MGS2T_<ver> only, refuses while a build is loaded or unless every non-build file has an md5-matched copy in <logs dir>. In-game plugin now ON (PSPLink up 0.2 s into a build); `plugin-off` for plain runs.
- 11:40 `update MGS2T_<old> MGS2T_<new> "<build dir>" "<logs dir>"` (checks first, then clears run files, renames, copies only changed files per MANIFEST.md5, reads all back; USB hashing 2.7 MB/s), `check`, manifest-aware `stage`. fetch also copies memcard0/*. AUTO RETURN TO PSPLINK DOES NOT WORK (Ffm chose manual, 11:43): after "Press X to exit" Ffm presses X and starts "PSPLink (PC control)". MGS2 timing runs: plugin OFF; tell Ffm before each launch.
- HOW (2026-11-19): utils\psplink\README.md = the procedure; `python -I psprun.py up | sh "<cmd>" | stage <dir> MGS2T_<ver> | launch MGS2T_<ver> | fetch MGS2T_<ver> <pc dir> | plugin-on/off` from utils\psplink (Windows only: a Remote Control session on Desktop\mg2 port; the folder tools' VM can't reach USB; an RC session withholds this thread's folder tools while it runs).
- FF8 LAUNCH (2026-11-19 16:58): pclink.prx launches only ms0:/PSP/GAME/MGS2T_* (pclink\main.c builds[] 36-37). FF8 wrapper FF8\tools\ff8run.py. Reading or writing ms0:/PSP/GAME/PSPLINK_PC from the RC session is blocked by the PC's permission check.
- FOLDER TOOLS vs RC (2026-11-19): folder tools stay off while an RC session runs (often for hours after a stop). WORKAROUND: zip the delivery in /mnt/project-files/_stage_<ver>/, attach it to a reply, Ffm saves it into Builds, the RC session unpacks + md5-checks; delete the staged zip after.
