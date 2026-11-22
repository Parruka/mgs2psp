#!/bin/sh
# turns_ref.sh (task #149 step 2): sb14's own twin (x1) with runs_f.sh's turns (four configurations every 100 vblanks from vb 1,000, the
# lanes following g_opt), the reference for 2b in turns (out_g2*ft); out_<r>trn.log in /home/claude/sb14/x1.  Log /home/claude/me14/turns_ref.log
L=/home/claude/me14/turns_ref.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
T="MGS2_SB_LANE_OPT=1 MGS2_OPT_CYCLE=1000:100:ffffffff,3ff09fff,ffffffff,cfafffff MGS2_OPT2_CYCLE=ffffffdf,fffffe1f,ffffffd8,ffffffdf"
st start
(X=/home/claude/sb14/x1; export X; sh /home/claude/sb14/chk14.sh d dtrn $T; st "d done"; sh /home/claude/sb14/chk14.sh c ctrn $T; st "c done") &
(X=/home/claude/sb14/x1; export X; sh /home/claude/sb14/chk14.sh i itrn $T; st "i done") &
wait; st "all done"
