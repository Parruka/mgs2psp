#!/bin/sh
# runs_f.sh (task #149 step 2, the final code: me_rt.c with pools v4, the three hooks and the resize; the local copy's check before its
# buffer): twin A (xa, rebuilt by mk.sh), the split twins 2a (xg, mk_g.sh) and 2b (xg2, mk_g2.sh) relinked with xa's objects, on the three
# routes; 2b again with every switch following its record (MGS2_XM_FOLLOW=15) while four configurations take turns every 100 vblanks
# from vb 1,000 (g_opt: all, the "rest" bits and eager/mark off, all, the four lanes off with MGS2_SB_LANE_OPT=1; g_opt2: all but C2,
# step 3's trims off, step 0's bits off, all but C2); then the take-back with the whole picture state kept (TBK 15) at step 1's six
# points in 2a.  Each compared with sb14's x1 run (cmp13.py).  Log /home/claude/me14/runs_f.log
E=/home/claude/me14; L=$E/runs_f.log; C=/home/claude/me13/tools/cmp13.py
st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
cmpr() { echo "$1 $3: $(PYTHONDONTWRITEBYTECODE=1 python3 -I $C /home/claude/sb14/x1/out_$2t0.log $E/$1/out_$3.log $4 2>&1 | head -1)" >> $L
         grep -a -h "picture heap" $E/$1/out_$3.log | head -1 | cut -c1-330 >> $L; }
st "start"
sh $E/mk.sh $E/xa > $E/xa_build_f.out 2>&1 || { st "mk.sh failed"; exit 1; }
sh $E/mk_g.sh > $E/xg_build_f.out 2>&1 || { st "mk_g.sh failed"; exit 1; }
sh $E/mk_g2.sh > $E/xg2_build_f.out 2>&1 || { st "mk_g2.sh failed"; exit 1; }
st "built: $(md5sum $E/xa/ge_me_rt.o $E/xa3/ge_me_rt.o $E/xa/boot $E/xg/boot $E/xg2/boot | awk '{printf "%s %s; ", substr($1,1,8), $2}')"
(X=$E/xa; export X; sh /home/claude/sb14/chk14.sh d df0; sh /home/claude/sb14/chk14.sh c cf0) &
(X=$E/xa; export X; sh /home/claude/sb14/chk14.sh i if0) &
(sh $E/run1g2.sh d g2df0 25200; sh $E/run1g2.sh c g2cf0 40000) &
(sh $E/run1g2.sh i g2if0 37800) &
wait; st "A and 2b done"
for r in d i c; do cmpr xa $r ${r}f0; cmpr xg2 $r g2${r}f0; done
T="MGS2_XM_FOLLOW=15 MGS2_SB_LANE_OPT=1 MGS2_OPT_CYCLE=1000:100:ffffffff,3ff09fff,ffffffff,cfafffff MGS2_OPT2_CYCLE=ffffffdf,fffffe1f,ffffffd8,ffffffdf"
(sh $E/run1g.sh d gdf0 25200; sh $E/run1g.sh c gcf0 40000) & (sh $E/run1g.sh i gif0 37800) &
(sh $E/run1g2.sh d g2dft 25200 $T; sh $E/run1g2.sh c g2cft 40000 $T) & (sh $E/run1g2.sh i g2ift 37800 $T) &
wait; st "2a and 2b in turns done"
for r in d i c; do cmpr xg $r g${r}f0; cmpr xg2 $r g2${r}ft; grep -a -h -m3 "xlive mode: task #149\|xlive mode: the lanes follow\|turns)" $E/xg2/out_g2${r}ft.log | cut -c1-200 >> $L; done
run() { sh $E/run1g.sh $1 $2 $4 MGS2_XLIVE_DLF=1 MGS2_XLIVE_TB=$3 MGS2_XLIVE_TBK=$5; cmpr xg $1 $2 $3; grep -a -h 'me13 tb:' $E/xg/out_$2.log | head -1 | cut -c1-200 >> $L; }
run d gd15f15 1500 3000 15 & run d gd240f15 24000 25200 15 & run i gi330f15 33000 37800 15 & wait
run d gd60f15 6000 25200 15 & run d gd158f15 15800 25200 15 & run c gc360f15 36000 40000 15 & wait
st "done"
