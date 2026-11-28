# gl44 r1 on the PSP (2026-11-28 09:40 UTC): stopped at vb 17,843. First black-box record of a stop.

Source [R]: read on Ffm's PC from LOGS\gl44 (10 files, md5-equal to the stick: bbox.txt 9c7bb5788fd09f6c1d4b97861ec72aeb,
log.txt a43d79893e7872503391849baacec614 2,762 lines, battery.txt 13187156e84751e52bf238ea61be76c0). Marks: [R] read there, [I] inferred.

## Facts [R]
- Start 09:40:02 UTC, pclink start, plugin OFF, 17 files md5-equal. Ffm 09:57: "game crashed"; froze, then off by itself.
- log.txt ends at "disc vb 17800"; HASHES 35/35 equal to hashref (vb 500..17,500); crash.txt start lines only; shots 8760/10516/15761.
- bbox.txt: 928 records (seq 0-927), last t 950.9 s, game vblank 17,843, phase DP. rdbbox: BOTH END IN THE SAME SECOND.
- Battery: external power 0 from t 0 to 674 s (AC adapter plugged at vb 13,016), then 1 to the end; 98 %, 4278 mV, charging; last battery
  line t 945 s. Free memory 40 KB (largest 24 KB) at t 301, 598, 899 s; gl43 at the same times 44 KB (largest 24 KB).
- Black box cost: write mean 18.84 ms, max 70 ms; wake lateness mean 0.01, max 7 ms.
- Turns here are 100 vblanks, alternating main (f C) and ME (f M). Windows: main turns ~59.5 ms a step (logic ~41.3), ME turns ~37.6
  (logic 12.0, GE wait 12.8). The stop: ME turn vb 17,800-17,900, 43+ vblanks in, the 2nd record after the change C to M.
- All records by f's first letter (q, m, h hex; changes since the record before): C n 467, q 0/0/0, dm 0/0/19691, dh 0/538/127125;
  M n 460, q 0/0/23, dm 13/1049/27005, dh 30/1634.5/55933; none n 1. 89 changes C to M in all.

## Reading [I]
- No precursor at 1 s: the last two records (q 08 then 17, m +0x60 then +0xd, h +0x321 then +0x3e1, place 0104 then 010b) are the same as
  the first two records of the M turns before (seq 916/917: m +0x5d, +0xd, q 17 17; 906/907: q 07 17; 896/897; 886/887; 876/877).
  m is the ME's wait counter (psp/xme_me.c: it moves when the ME waits), so a small change means a busy ME, not a stalled one.
- The whole chip stopped within the second after t 950.87 s: the priority-0x10 thread wrote no further record.
- Supply path unlikely: external power on, 98 %. Slow build-up unlikely: memory steady, queues as before.
- One sample says nothing about ME turn against main turn (even odds). Asked from the PC with r2: the turn kind at each earlier stop.

## The last 60 records (verbatim)
s0000868 t0891874600 c053629 v16652 na7e37 pMW01 d1a6add+007 fMe h1c05bb a0001 m0ac811 q09 g3551267c:2 b4278,096,cx l000 w020
s0000869 t0892874634 c053689 v16678 na854b pLG00 d1a6add+007 fMe h1c099d a0103 m0ac822 q02 g35511abc:0 b4278,096,cx l000 w023
s0000870 t0893874599 c053749 v16701 na8b7e pIO00 d1a6af9+003 fCe h1c0d38 a0100 m0ac830 q00 g35511dfc:0 b4278,096,cx l000 w026
s0000871 t0894874608 c053809 v16719 na8ff4 pLG00 d1a6af9+003 fCe h1c0ee9 a0100 m0ac830 q00 g3551103c:2 b4278,096,cx l000 w019
s0000872 t0895874617 c053869 v16736 na9454 pLG00 d1a6af9+003 fCe h1c1097 a0313 m0ac830 q00 g3551223c:0 b4278,096,cx l000 w020
s0000873 t0896874604 c053929 v16753 na9898 pLG00 d1a6af9+003 fCe h1c124e a0100 m0ac830 q00 g3551247c:0 b4278,096,cx l000 w020
s0000874 t0897874603 c053989 v16770 na9cd6 pPI00 d1a6b0c+009 fCe h1c13f7 a0311 m0ac830 q00 g3551267c:0 b4278,096,cx l000 w019
s0000875 t0898874608 c054049 v16787 naa130 pLG00 d1a6b0c+009 fCe h1c1591 a0100 m0ac830 q00 g355119bc:2 b4278,097,cx l000 w020
s0000876 t0899874588 c054109 v16806 naa608 pMW09 d1a6b0c+009 fMe h1c17d0 a010e m0ac88b q17 g35511bbc:0 b4278,097,cx l000 w019
s0000877 t0900874590 c054169 v16832 naacf8 pMW01 d1a6b25+007 fMe h1c1baf a0001 m0ac898 q11 g35511dfc:2 b4278,097,cx l000 w020
s0000878 t0901874608 c054229 v16858 nab40c pLG00 d1a6b25+007 fMe h1c1f97 a0103 m0ac8a9 q02 g3551103c:0 b4278,097,cx l000 w022
s0000879 t0902874596 c054289 v16885 nabb17 pGS00 d1a6b25+007 fMe h1c2367 a0103 m0ac8b6 q07 g3551237c:0 b4278,097,cx l000 w020
s0000880 t0903874601 c054348 v16906 nac09b pPI00 d1a6b34+00d fCe h1c2687 a0311 m0ac8c2 q00 g355127bc:0 b4278,097,cx l000 w019
s0000881 t0904874603 c054408 v16923 nac508 pGS00 d1a6b34+00d fCe h1c2839 a0100 m0ac8c2 q00 g35511abc:2 b4278,097,cx l000 w020
s0000882 t0905874651 c054468 v16940 nac967 pLG00 d1a6b34+00d fCe h1c29e8 a0313 m0ac8c2 q00 g35511cbc:0 b4278,097,cx l000 w018
s0000883 t0906874600 c054528 v16957 nacdb2 pGS00 d1a6b51+008 fCe h1c2b99 a0100 m0ac8c2 q00 g35511efc:0 b4278,097,cx l000 w021
s0000884 t0907874605 c054588 v16973 nad1cb pLG00 d1a6b51+008 fCe h1c2d41 a0100 m0ac8c2 q00 g355110fc:0 b4278,097,cx l000 w020
s0000885 t0908874606 c054648 v16991 nad645 pLG00 d1a6b51+008 fCe h1c2ee8 a0100 m0ac8c2 q00 g355122fc:0 b4278,097,cx l000 w019
s0000886 t0909874605 c054708 v17003 nad976 pLG00 d1a6b51+008 fMe h1c3063 a010b m0ac91a q17 g3551243c:0 b4278,097,cx l000 w019
s0000887 t0910874601 c054768 v17029 nae071 pIO00 d1a6b69+005 fMe h1c3435 a010b m0ac927 q17 g3551257c:0 b4278,097,cx l000 w019
s0000888 t0911874706 c054828 v17056 nae769 pMW01 d1a6b69+005 fMe h1c3815 a0311 m0ac934 q17 g355119bc:0 b4278,097,cx l000 w019
s0000889 t0912874634 c054888 v17082 naee5a pGS00 d1a6b6e+010 fMe h1c3c03 a0103 m0ac945 q02 g35511c3c:0 b4278,097,cx l000 w027
s0000890 t0913874607 c054948 v17104 naf44c pDP00 d1a6b6e+010 fCe h1c3f3e a0313 m0ac952 q00 g35511f7c:0 b4278,097,cx l000 w022
s0000891 t0914874606 c055008 v17121 naf897 pLG00 d1a6b8e+006 fCe h1c40f3 a0100 m0ac952 q00 g3551227c:0 b4278,097,cx l000 w019
s0000892 t0915874601 c055068 v17139 nafd13 pLG00 d1a6b8e+006 fCe h1c429f a0100 m0ac952 q00 g3551247c:0 b4278,097,cx l000 w020
s0000893 t0916874605 c055128 v17155 nb0143 pLG00 d1a6b8e+006 fCe h1c4463 a0100 m0ac952 q00 g355127bc:0 b4278,097,cx l000 w019
s0000894 t0917874638 c055188 v17172 nb0578 pGS00 d1a6b8e+006 fCe h1c45ff a0313 m0ac952 q00 g355119bc:0 b4278,097,cx l000 w019
s0000895 t0918874604 c055248 v17189 nb09d5 pLG00 d1a6ba9+003 fCe h1c47a1 a0100 m0ac952 q00 g35511bfc:0 b4278,097,cx l000 w022
s0000896 t0919874728 c055308 v17210 nb0f39 pMW09 d1a6ba9+003 fMe h1c4a32 a010b m0ac9ac q17 g35511dfc:0 b4278,097,cx l000 w019
s0000897 t0920874683 c055367 v17236 nb1626 pMW01 d1a6ba9+003 fMe h1c4e11 a0311 m0ac9b9 q17 g35511f3c:0 b4278,097,cx l000 w019
s0000898 t0921874608 c055427 v17261 nb1cf6 pDP00 d1a6bbc+008 fMe h1c51c8 a010b m0ac9c6 q17 g355123bc:2 b4278,097,cx l000 w021
s0000899 t0922874678 c055487 v17288 nb23f8 pMW09 d1a6bbc+008 fMe h1c5599 a010b m0ac9d3 q17 g355126bc:0 b4278,097,cx l000 w019
s0000900 t0923874603 c055547 v17308 nb2939 pPI00 d1a6bbc+008 fCe h1c588a a0311 m0ac9dd q00 g3551193c:2 b4278,097,cx l000 w019
s0000901 t0924874610 c055607 v17325 nb2d9d pLG00 d1a6bd4+008 fCe h1c5a2c a0100 m0ac9dd q00 g35511cfc:2 b4278,097,cx l000 w020
s0000902 t0925874603 c055667 v17342 nb31e3 pPI00 d1a6bd4+008 fCe h1c5bea a0311 m0ac9dd q00 g35511efc:0 b4278,097,cx l000 w019
s0000903 t0926874609 c055727 v17359 nb3651 pLG00 d1a6bd4+008 fCe h1c5d9e a0100 m0ac9dd q00 g3551103c:2 b4278,097,cx l000 w020
s0000904 t0927874602 c055787 v17375 nb3a6f pLG00 d1a6bd4+008 fCe h1c5f47 a0100 m0ac9dd q00 g3551223c:0 b4278,097,cx l000 w019
s0000905 t0928874605 c055847 v17392 nb3ec6 pDP00 d1a6be4+00f fCe h1c60de a0313 m0ac9dd q00 g3551243c:0 b4278,097,cx l000 w026
s0000906 t0929874600 c055907 v17415 nb44ac pLG00 d1a6be4+00f fMe h1c63df a0103 m0aca39 q07 g3551267c:0 b4278,097,cx l000 w019
s0000907 t0930874605 c055967 v17441 nb4bb4 pLG00 d1a6be4+00f fMe h1c67be a010b m0aca46 q17 g355119fc:0 b4278,097,cx l000 w019
s0000908 t0931874756 c056027 v17468 nb52b8 pMW09 d1a6c03+008 fMe h1c6b8e a010b m0aca53 q17 g35511c3c:0 b4278,097,cx l000 w019
s0000909 t0932874592 c056087 v17494 nb59a5 pMW01 d1a6c03+008 fMe h1c6f6f a0311 m0aca60 q17 g355110bc:2 b4278,097,cx l000 w020
s0000910 t0933874607 c056147 v17505 nb5cb4 pDP00 d1a6c03+008 fCe h1c73c9 a0100 m0accf5 q00 g355123fc:2 b4278,097,cx l000 w020
s0000911 t0934874605 c056207 v17523 nb612f pLG00 d1a6c1f+003 fCe h1c7567 a0100 m0accf5 q00 g3551247c:0 b4278,097,cx l000 w019
s0000912 t0935874603 c056267 v17539 nb655f pLG00 d1a6c1f+003 fCe h1c772a a0100 m0accf5 q00 g355127bc:0 b4278,097,cx l000 w020
s0000913 t0936874601 c056326 v17557 nb69d3 pLG00 d1a6c1f+003 fCe h1c78cd a0100 m0accf5 q00 g355119bc:0 b4278,097,cx l000 w019
s0000914 t0937874604 c056386 v17573 nb6df3 pLG00 d1a6c1f+003 fCe h1c7a76 a0100 m0accf5 q00 g35511bfc:0 b4278,097,cx l000 w019
s0000915 t0938874602 c056446 v17590 nb722f pPI00 d1a6c32+008 fCe h1c7c1e a0311 m0accf5 q00 g35511dfc:0 b4278,097,cx l000 w019
s0000916 t0939874607 c056506 v17611 nb77cf pDP00 d1a6c32+008 fMe h1c7ed3 a010b m0acd52 q17 g35511f3c:2 b4278,098,cx l000 w020
s0000917 t0940874679 c056566 v17638 nb7ed1 pMW09 d1a6c32+008 fMe h1c82a4 a010b m0acd5f q17 g3551217c:0 b4278,098,cx l000 w019
s0000918 t0941874621 c056626 v17664 nb85bf pMW01 d1a6c4a+008 fMe h1c8683 a0311 m0acd6c q17 g355125fc:2 b4278,098,cx l000 w019
s0000919 t0942874607 c056686 v17689 nb8c8c pDP00 d1a6c4a+008 fMe h1c8a38 a010b m0acd79 q17 g3551183c:2 b4278,098,cx l000 w021
s0000920 t0943874598 c056746 v17709 nb91b2 pGS00 d1a6c6a+001 fCe h1c8ce4 a0100 m0acd82 q00 g35511b3c:0 b4278,098,cx l000 w019
s0000921 t0944874605 c056806 v17725 nb95e1 pLG00 d1a6c6a+001 fCe h1c8ea8 a0100 m0acd82 q00 g35511efc:0 b4278,098,cx l000 w019
s0000922 t0945874604 c056866 v17743 nb9a57 pLG00 d1a6c6a+001 fCe h1c904a a0100 m0acd82 q00 g355110fc:0 b4278,098,cx l000 w023
s0000923 t0946874628 c056926 v17760 nb9eaa pGS00 d1a6c6a+001 fCe h1c9208 a0313 m0acd82 q00 g3551223c:0 b4278,098,cx l000 w021
s0000924 t0947874607 c056986 v17777 nba301 pLG00 d1a6c7b+009 fCe h1c93a7 a0100 m0acd82 q00 g3551247c:0 b4278,098,cx l000 w023
s0000925 t0948874605 c057046 v17793 nba730 pDP00 d1a6c7b+009 fCe h1c9569 a0100 m0acd82 q00 g3551267c:0 b4278,098,cx l000 w019
s0000926 t0949874598 c057106 v17817 nbad5d pGS00 d1a6c7b+009 fMe h1c988a a0104 m0acde2 q08 g3551187c:0 b4278,098,cx l000 w019
s0000927 t0950874607 c057166 v17843 nbb46a pDP00 d1a6c97+003 fMe h1c9c6b a010b m0acdef q17 g35511b3c:0 b4278,098,cx l000 w019

## The turn kind at each earlier stop (read on the PC 2026-11-28 10:10 from LOGS\<ver>\log.txt r1 and each build's mgs2.cfg) [R]
All these builds: MGS2_ME_PIC_TURN=100 (gl17: no turn lines). Even hundreds = ME turn, odd = main turn (ME asleep in its rest).
| run | last window line | last battery vb | the stop's turn |
|---|---|---|---|
| gl27 r1 | vb 16600 (main turn 16500-16600: 59.93 ms, logic 42.36) | 16,551 | ME turn 16600-16700 |
| gl29 r1 | vb 30700 (ME turn: 112.90 ms, GE wait 19.78) | 30,760 | MAIN turn 30700-30800 (ME asleep 18.8 s a turn) |
| gl29x r1 | vb 31000 (main turn: 146.09 ms) | 31,039 | ME turn 31000-31100 |
| gl40 r1 | vb 28400 (34.93 ms; the ME turn before did no work: idle 1928 of 1931 ms) | 28,459 | ME turn 28400-28500 by the schedule |
| gl42 r1 | vb 29500 (76.06 ms) | 29,561 | ME turns back to back there |
| gl44 r1 | vb 17800 (main turn: 59.44 ms) | bbox 17,843 | ME turn 17800-17900 |
| gl17 r1 | vb 33000 (86.16 ms, picture 30.29) | 33,044 | no turns in that build |
Reading [I]: of the 5 runs where main and ME turns alternate, 4 stopped in an ME turn and 1 in a main turn with the ME asleep. ME turns
take about 0.4 of the wall time there (main turns are slower), so 4 of 5 has a chance of about 0.09 under "no link": suggestive, not
proof. A working ME is NOT needed for a stop (gl29 r1; v24 and gl1 stopped with the ME off).

## Idea for the next harness step (lead 2026-11-28): the ME as a witness
The 1 Hz black box cannot say which processor stopped first. A witness that needs no file: a high-priority main thread ticks a shared
word 4 times a second; the ME watches it; when the ticks stop for 2 s (and the run is not at its end screen) the ME paints the shown
frame buffer with its finding (main CPU stopped, the last phase tag, vblank, its own place) for the seconds before the power cut.
Ffm (or a phone filming the screen) reads it. No ME screen at a freeze = the ME or the bus stopped too. Needs: a boot self-test of the
ME's VRAM write, the shown buffer's address in shared memory, a timer wake in the ME's rest for main turns. Harness only.
