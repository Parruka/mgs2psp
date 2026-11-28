#!/bin/sh
# lay_one41.sh <stage s1|s2> <out folder name under ord/> (gl41): rt41b.sh's last loop for one stage, into its own folder (so a stage's
# layout files can be made while the next search stage runs; rt41b.sh still writes ord/s1 and ord/s2 itself at its end).
R=/home/claude/main/build_gl41; s=$1; O=$2; [ -n "$s" ] && [ -n "$O" ] || { echo "lay_one41.sh <stage> <out>"; exit 1; }
[ -s $R/lay/order_$s.txt ] || { echo "lay_one41.sh: no lay/order_$s.txt"; exit 1; }
[ -e $R/ord/$O ] && { echo "lay_one41.sh: ord/$O exists"; exit 1; }
PIN=vupv_96936c0f_0_p3; PREF=vupv_8baafbc3_0
mkdir -p $R/ord/$O/w && cd $R/ord/$O/w || exit 1
python3 -I $R/tools/split41.py $R/lay/order_$s.txt order_deck.txt order_w01a.txt || exit 1
for d in deck w01a; do mkdir -p $d; for o in $R/u/*.o; do ln -sf $o $d/; done; done
python3 -I $R/m/tools/layout_order.py order_deck.txt $R/ord/$O/w/deck w00a > lo_deck.out 2>&1; tail -n 2 lo_deck.out
python3 -I $R/m/tools/layout_order.py order_w01a.txt $R/ord/$O/w/w01a w01a > lo_w01a.out 2>&1; tail -n 2 lo_w01a.out
cmp deck/layout_elf.ld w01a/layout_elf.ld && echo "$O: layout_elf.ld equal for both routes"
cp deck/layout_elf.ld deck/layout_w00a.ld w01a/layout_w01a.ld $R/ord/$O/ || exit 1
python3 -I $R/tools/pin41.py $R/ord/$O/layout_elf.ld $PIN $PREF 0x80 || exit 1
( cd $R/ord/$O && md5sum *.ld )
