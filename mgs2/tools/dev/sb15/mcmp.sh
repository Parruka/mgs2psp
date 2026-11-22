#!/bin/sh
# mcmp.sh <tag> [<reference tag>, default gl16]: a PPSSPP master run (/home/claude/msm/PSP/GAME/MGS2M_<tag>) against another: the hash
# and vuh lines, the frame digests' line, the list words, the heap at the start and at its lowest, and each shot's md5 (= or differs).
G=/home/claude/msm/PSP/GAME; A=$G/MGS2M_$1; R=$G/MGS2M_${2:-gl16}
for k in '^hash:' '^vuh:'; do
  na=$(grep -c "$k" $A/log.txt); nr=$(grep -c "$k" $R/log.txt)
  if [ "$(grep "$k" $A/log.txt | md5sum)" = "$(grep "$k" $R/log.txt | md5sum)" ]; then echo "$k $na lines, equal"; else echo "$k $na lines against $nr: DIFFER"; fi
done
for f in $A $R; do
  grep '^picture: [0-9]* frames' $f/log.txt | sed 's/.*lists: 0 that did not fit, 0 batches left out, /lists /; s/, the largest aux [0-9]* bytes//'
  grep '^free memory: heap headroom' $f/log.txt | sed 's/; outside.*//'; grep 'headroom was about' $f/log.txt | sed 's/.*the headroom/the headroom/'
  grep '^picture: the largest of each in one frame' $f/log.txt | sed -E 's/.*: ([0-9]+ batches) .*vertices, ([0-9]+ KB data).*/largest in one frame: \1, \2/'
done
for s in $R/shot_*.bmp; do b=$(basename $s); if [ ! -f $A/$b ]; then echo "$b missing"; elif cmp -s $s $A/$b; then printf "%s = " ${b#shot_}; else printf "%s DIFFERS " ${b#shot_}; fi; done; echo
