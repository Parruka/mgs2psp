---
name: ff8-logs-folder-rule
description: Ffm's standing rule (2026-11-19 02:03): with every delivered FF8 build, also create its empty FF8\LOGS\<version> folder on the PC
metadata:
  type: feedback
---

Ffm 2026-11-19 02:03: "always create folder for the logs builds".

**Why:** Ffm copies the run's logs into FF8\LOGS\<version> and wants the folder already there when a build lands.

**How to apply:** in every FF8 delivery step (Builds\FF8T_<ver>\FF8T + md5 check), also `mkdir "$HOME/mnt/mg2 port/FF8/LOGS/<ver>"` on the PC (device_bash) and say in the README / message that it exists. Done for p9 on 2026-11-19 02:05. See [[ff8-p10]], [[ff8-p9]].
