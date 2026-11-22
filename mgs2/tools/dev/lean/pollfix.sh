#!/bin/sh
# pollfix.sh <work dir> <handler list> [<regcache.py>]: the registers a poll must store (recomp/lean_plan.md step 1b) as the least
# fixpoint over the ELF's interprocedural sets.  A handler's IN depends on what the polls inside it read, so: start with polls that read
# nothing, compute the handlers' IN with regcache.py (RC_IPO_ONLY, the ELF unit), take their union less a0-a2, sp and ra
# (tools/pollset.py), and repeat with polls reading that set until it stops growing.  A register enters the set only when some handler
# reads it before writing it, with the polls inside handlers counted at the current set (the least solution: a register held in the set
# only by polls reading polls never reaches the guest's memory).  The handlers must all lie in the ELF (pollset.py checks).
# <handler list>: "khandler: <kind> <hex>" lines (x86 builds with -DK_HANDLER_LOG) or bare hex addresses.  Out: <work dir>/pollfix.txt,
# the last pollset.py output (RC_POLL_R= / RC_POLL_F= lines).  Takes about a minute per round (survey once, then regcache IPO only).
MG=/mnt/project-files/mgs2; SL=${SL:-/home/claude/mgs2/SLES_503.83}; RCPY=${3:-/home/claude/lean/rc/regcache.py}
W=$1; HL=$2; [ -n "$W" ] && [ -f "$HL" ] || { echo "usage: pollfix.sh <work dir> <handler list> [regcache.py]"; exit 1; }
mkdir -p $W && W=$(cd $W && pwd) || exit 1
[ -s $W/pre_elf.h ] || (cd $MG/recomp && env SLUS=$SL VU_VFPU=1 python3 survey.py $W/pre_elf.h > $W/survey.log 2>&1) || { echo "survey failed ($W/survey.log)"; exit 1; }
PR=; PF=; n=0
while :; do
  cp $W/pre_elf.h $W/work.h
  env RC_KEEPMASK=1 RC_IPO_ONLY=1 RC_IPO_SAVE=$W/ipo_$n.json RC_POLL_R="$PR" RC_POLL_F="$PF" python3 $RCPY $W/work.h > $W/rc_$n.log 2>&1 || { echo "regcache failed ($W/rc_$n.log)"; exit 1; }
  python3 $MG/tools/pollset.py $HL $W/ipo_$n.json > $W/ps_$n.txt || { echo "pollset failed ($W/ps_$n.txt)"; exit 1; }
  NR=$(sed -n 's/^RC_POLL_R=//p' $W/ps_$n.txt); NF=$(sed -n 's/^RC_POLL_F=//p' $W/ps_$n.txt)
  echo "round $n: polls read R {$PR} F {$PF} -> handlers read R {$NR} F {$NF}"
  [ "$NR" = "$PR" ] && [ "$NF" = "$PF" ] && break
  PR=$NR; PF=$NF; n=$((n + 1)); [ $n -lt 12 ] || { echo "no fixpoint after 12 rounds"; exit 1; }
done
cp $W/ps_$n.txt $W/pollfix.txt
: > $W/work.h
echo "fixpoint after $n rounds: $W/pollfix.txt"
