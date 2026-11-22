#!/bin/sh
# model.sh <tag> <A> <B> <route in | codec> [cfg lines]: one PspProf window of the sb4p/d3 build (sb4 + task #114's thinned rain and splash, ge/fxthin.c; gl3.sh's model step, X = /home/claude/sb4r, d -> /home/claude/sb4p/d3)
t=$1; a=$2; b=$3; r=$4; shift 4
GL3=/home/claude/sb4r sh /mnt/project-files/mgs2/psp/gl3.sh model $t $a $b all $r "$@"
