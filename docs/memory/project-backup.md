---
name: project-backup
description: How and where the project folder is backed up to Ffm's PC (PROJECT\backup_<date>), the 25 MB transfer limit, and what is NOT covered
metadata:
  type: project
  modified: 2026-11-16T21:05:30.348Z
---

- Ffm 2026-11-16 20:54: "save everything useful that cant be deleted to my pc folder, so we dont loose anything if we run out of space in the project".
- First backup 2026-11-16 21:02 UTC: Desktop\mg2 port\PROJECT\backup_2026-11-16\ = files\ (the whole /mnt/project-files except _stage, 2,842 files, 277 MB, every file md5-checked on the PC against MANIFEST.md5), the .tar.gz (89 MB, md5 872aa4bc...), README.txt, parts\ (duplicate pieces; Ffm may delete).
- Method: tar.gz in the scratchpad + md5 manifest; device_commit_files reads at most 25 MB a file, so split -b 24000000 into /mnt/project-files/_stage/backup_<date>/, commit, cat + tar -x on the PC with device_bash, md5sum -c; then delete the staged pieces (game-derived).
- NOT covered: thread containers' own work dirs (e.g. MGS2's /home/claude/gl18me, gl18e1, FF8's psp/rel_s*), which other threads can't reach; each thread must copy its own unique work into the project folder (or straight to the PC) before it is lost.
- WHEN (Ffm 2026-11-16 21:12, replaces 'after each delivery'): NO backup per build. Back up only when the project folder or a thread's container is nearly full; then copy the files to the PC (new dated folder, or only files changed since the last MANIFEST.md5), md5-check them there, and only after that delete them from the cloud to free space. Ffm: "so you can delete stuff without us losing it".
