#!/bin/sh
# mod41.sh <psp dir> <tag> <cfg> [windows...] (gl41s): one PspProf model run (build_gl39/m/scripts/pgo.sh model, B = this scratch, so
# its game folder ms/<tag> and its counts model/<tag>[.N] land here) with PPSSPPHeadless_fcr (harness rule 4) against hashref_gl38;
# windows default 29,610 (t56 rain wait K0 digest-off) and 30,910 (t69 walk K0 digest-off), ka39.1 / ka39.6's.  The game folder is
# deleted after the run (its literal path).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s; D=$1; T=$2; C=$3; shift 3; W=${*:-29610:29690 30910:30990}
case $T in m_*) ;; *) echo "mod41.sh: tags start with m_"; exit 1 ;; esac
B=$S PPF=/home/claude/ppsspp_prof/ppsspp-1.20.4/build/PPSSPPHeadless_fcr nice -n 10 sh /home/claude/main/build_gl39/m/scripts/pgo.sh model $D $T $C \
  /home/claude/main/build_gl39/cfg/hashref_gl38.txt $W > $S/pgo_$T.out 2>&1
echo "mod41.sh: $T rc $? $(date -u +%H:%M:%S)"; cat $S/pgo_$T.out | cut -c1-220
[ -d "$S/ms/$T" ] && rm -rf "/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s/ms/$T"
