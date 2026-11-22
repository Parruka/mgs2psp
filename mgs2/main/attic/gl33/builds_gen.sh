#!/bin/sh
# builds_gen.sh: the instrumented builds after g_game, one after the other (m/scripts/psp.sh PGO=gen)
R=/home/claude/main/build_gl33; cd $R/m || exit 1
PSPD=$R/g_rtb PGO=gen PGO_ELF=rtb sh scripts/psp.sh > $R/g_rtb.out 2>&1; echo "g_rtb rc $?"
PSPD=$R/g_pic PGO=gen PGO_ELF=pic sh scripts/psp.sh > $R/g_pic.out 2>&1; echo "g_pic rc $?"
PSPD=$R/g_w00a PGO=gen PGO_STAGE=w00a sh scripts/psp.sh > $R/g_w00a.out 2>&1; echo "g_w00a rc $?"
PSPD=$R/g_w01a PGO=gen PGO_STAGE=w01a sh scripts/psp.sh > $R/g_w01a.out 2>&1; echo "g_w01a rc $?"
echo done
