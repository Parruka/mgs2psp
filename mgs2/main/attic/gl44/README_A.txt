gl44 BLOCK A: THE HARNESS KEEPS UNFINISHED LOGS AND WRITES A BLACK BOX. (This part goes into gl44's README; the game itself is
gl43's in block A: the same hashes, the same pictures.)

1. THE FILES OF THE RUNS BEFORE ARE KEPT, AT EVERY START. Before it writes anything, the build renames the run before's files next
   to the EBOOT:
       log.txt      -> log_prev1.txt          battery.txt -> battery_prev1.txt     crash.txt -> crash_prev1.txt
       steps.csv    -> steps_prev1.csv        bbox.txt    -> bbox_prev1.txt
   and the _prev1 files of the run before that to _prev2 (log_prev2.txt, ...; the older _prev2 files are deleted). It does this
   whether or not the run before reached its end, so a run that froze keeps its files even if the game is started again. The
   shots (shot_*.bmp, live_*.bmp, zbuf_*.raw) are not renamed. The new log.txt's first lines (after the title) say what was renamed
   and whether the run before's log had its end line ("gl44 H1 ...").
   NEW FILES ON THE STICK (each once a run has made the file it comes from): bbox.txt, log_prev1.txt, log_prev2.txt,
   battery_prev1.txt, battery_prev2.txt, crash_prev1.txt, crash_prev2.txt, steps_prev1.csv, steps_prev2.csv, bbox_prev1.txt,
   bbox_prev2.txt. Room: three runs' files, about 3 x (log + battery + 461 KB), a few MB. Copy them all with the logs (PSPLink).

2. THE BLACK BOX (mgs2.cfg's MGS2_BBOX=1, on in this mgs2.cfg; without the key it is off). bbox.txt is made at its full size at
   the start (3,600 lines of 128 bytes, 461 KB; a run longer than an hour writes over the oldest lines, each line numbered). From
   the first window (vblank 100) a thread above the game's priority writes one line a second into it, with open, seek, write and
   close, so the file's size never changes and a power-off cannot cut it short. Each line says:
     - the line's number and the time since the start; the PSP's own vblank count; the game's vblank and a counter of the game
       thread's steps that moves many times a step;
     - what the game thread was doing (a tag): logic, IOP and sound, the GS walk, the draw pipeline, the translator, the picture,
       waiting for the GE, waiting for the ME (and what for), a disc read (with the sector and size: the reader names the file),
       the log's write, a shot's write, a stage overlay's load, the vblank hook and its hash, the end;
     - the Media Engine's loop heartbeat, its place (the last command) and its own heartbeat, the entries waiting for it;
     - the last GE list's id and its state (drawing, done, ...);
     - the battery: mV, %, charging, external power; and the black box's own lateness and write time.
   After a freeze, on the PC:   python -I rdbbox.py <LOGS\gl44>      (-p1: bbox_prev1.txt; standard library only)
   It prints the last 20 lines and which case the end is:
     - "the game counter STOPPED while the heartbeat went on N s": the system survived a stall of N seconds (a wait, a hang in
       our code or in a system call), and the tag says where the game thread was;
     - "BOTH END IN THE SAME SECOND": the whole chip stopped (interrupts off, a bus lock or the power);
     - or the run's own end, or the program's orderly exit;
   and the last tag.
   What it costs: one 128-byte write a second on its own thread. On the PSP: UNKNOWN until a run; the log's last lines
   ("gl44 black box ...") give its write time and lateness, and every bbox.txt line has them too.

3. AT A FREEZE, PLEASE NOTE (no tool needed; it tells power, heat and a stuck chip apart):
     - the sound: does it stop, go on playing, or buzz (a short sound repeating)?
     - the power LED: steady, blinking, or off?
     - how many seconds from the freeze until the PSP switches off (if it does)?
     - is the back of the PSP hot?
   Afterwards start ONLY "PSPLink (PC control)", NEVER the game, and copy the logs.
