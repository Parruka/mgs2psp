#!/bin/sh
# serve.sh <fork vblank> [par]: the deck route to <fork vblank>, then the fork server reading /home/claude/play/run/fifo (boot.c MGS2_FORK_*)
P=/home/claude/play; R=$P/run; F=$1; N=${2:-3}
cd $R || exit 1
[ -p $R/fifo ] || mkfifo $R/fifo
PAD="$(cat ${PADF:-/home/claude/mgs2/boot/pad_deck_idle.txt})"
exec env MGS2_HASH_EVERY=100 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_DISC=/home/claude/mgs2/disc MGS2_DISC_LSN=/home/claude/psp5/mgs2disc.pak MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
  MGS2_FORK_AT=$F MGS2_FORK_FIFO=$R/fifo MGS2_FORK_DIR=$R MGS2_FORK_PAR=$N $P/x/boot /home/claude/mgs2/SLES_503.83 99999999 > $R/server.log 2>&1
