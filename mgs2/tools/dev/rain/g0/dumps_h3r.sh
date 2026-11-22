#!/bin/sh
# H3R: EE dumps at vb 23,300 in both modes, one at a time (disk): mode 0 gzipped before mode 1 runs
cd /home/claude/sbg0
sh rundump.sh /home/claude/sbg0/xh3r dh3r0 fffef9df,fffeffdf 23300 23300 && gzip -1 /home/claude/sbg0/dump/dh3r0.23300 && ls -la /home/claude/sbg0/dump/dh3r0.23300.gz
sh rundump.sh /home/claude/sbg0/xh3r dh3r1 fffff9df,ffffffdf 23300 23300 && ls -la /home/claude/sbg0/dump/dh3r1.23300
df -h /home/claude | tail -1
